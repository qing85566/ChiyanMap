#include "mod/ChiyanMap.h"
#include "hooks/HookRegistry.h"
#include "state/MapRenderState.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"
#include "state/DeathPointManager.h"
#include "state/LanguageManager.h"
#include "state/SeedMapManager.h"
#include "state/AnarchyManager.h"
#include "state/SpecialGridManager.h"
#include "state/DatabaseManager.h"
#include "state/DrawingManager.h"
#include "state/MultiworldManager.h"
#include "input/NativeHotkeys.h"
#include "input/KeyBindingViewer.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/utils/SystemUtils.h"
#include "ll/api/io/FileUtils.h"
#include "ll/api/data/Version.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include <windows.h>
#include <chrono>
#include <atomic>
#include <vector>
#include <MinHook.h>
#include <string>
#include <algorithm>

// 定义于 MemoryOperators.cpp：置位后本 DLL 的所有 operator delete 变为 no-op
namespace ChiyanMapMem {
extern std::atomic<bool> g_skipDeleteOnShutdown;
}

namespace {

// 纯 Win32 文件写入，避免退出期经过 CRT 堆 / MallocRedirect。
void TraceShutdown(const char* step) {
    HANDLE h = CreateFileA(
        "ChiyanMap_Shutdown.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(h, step, static_cast<DWORD>(lstrlenA(step)), &written, nullptr);
    WriteFile(h, "\r\n", 2, &written, nullptr);
    CloseHandle(h);
}

std::atomic<bool> g_disableCompleted{false};
std::atomic<bool> g_exitGuardInstalled{false};

using ExitProcess_t = void(WINAPI*)(UINT);
// RtlExitUserProcess 的真实参数是 NTSTATUS；x64 下调用约定与 32 位整数参数
// 完全等价，这里统一用 UINT/WINAPI 以免依赖未随 windows.h 引入的 NTSTATUS 定义。
using RtlExitUserProcess_t = void(WINAPI*)(UINT);
ExitProcess_t g_originalExitProcess = nullptr;
RtlExitUserProcess_t g_originalRtlExitUserProcess = nullptr;

// [Arke 兼容性·退出崩溃] 崩溃栈确认 amdxc64.dll 的 0xC0000409 fastfail 发生在
// LdrShutdownProcess 阶段：arke.dll 的静态析构释放 D3D12 对象时，AMD 驱动已进入
// 半拆卸状态。该阶段只负责释放资源，对存档与用户数据没有任何贡献（所有模组的
// disable() 与 CRT atexit 回调都已在 ExitProcess 之前执行完毕）。
// 因此当 arke.dll 同时加载时，在这里直接终止进程，跳过整个 DLL detach 阶段。
// 进程终止时操作系统会回收全部内存、句柄与 GPU 对象，不会留下残留状态。
void WINAPI HookedExitProcess(UINT exitCode) {
    HMODULE hArke = GetModuleHandleW(L"arke.dll");
    TraceShutdown(hArke ? "ExitProcess: arke present, skipping LdrShutdownProcess"
                        : "ExitProcess: normal shutdown");
    if (hArke) {
        TraceShutdown(g_disableCompleted.load(std::memory_order_acquire)
                          ? "ExitProcess: ChiyanMap disable completed"
                          : "ExitProcess: WARNING disable did not complete");
        TerminateProcess(GetCurrentProcess(), exitCode);
        // TerminateProcess 永不返回；若被外部干预，仍走原函数以免卡死。
    }
    if (g_originalExitProcess) {
        g_originalExitProcess(exitCode);
    } else {
        TerminateProcess(GetCurrentProcess(), exitCode);
    }
}

// 兜底路径：部分运行时直接调用 ntdll!RtlExitUserProcess 而不经过 kernel32!ExitProcess，
// 它内部才会调用 LdrShutdownProcess，因此同样在这里截断。
void WINAPI HookedRtlExitUserProcess(UINT status) {
    if (GetModuleHandleW(L"arke.dll")) {
        TraceShutdown("RtlExitUserProcess: arke present, skipping LdrShutdownProcess");
        TerminateProcess(GetCurrentProcess(), status);
    }
    if (g_originalRtlExitUserProcess) {
        g_originalRtlExitUserProcess(status);
    }
}

void InstallExitProcessGuard() {
    // 只有与 Arke 同时加载才需要；单独加载时保持原版优雅退出流程。
    if (!GetModuleHandleW(L"arke.dll")) return;
    // DX11Hook::init() 已初始化 MinHook；重复调用返回 ALREADY_INITIALIZED，无副作用。
    MH_Initialize();
    bool installed = false;

    if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll")) {
        if (void* target = reinterpret_cast<void*>(GetProcAddress(hKernel32, "ExitProcess"))) {
            if (MH_CreateHook(target, reinterpret_cast<void*>(&HookedExitProcess),
                              reinterpret_cast<void**>(&g_originalExitProcess)) == MH_OK) {
                if (MH_EnableHook(target) == MH_OK) {
                    installed = true;
                } else {
                    MH_RemoveHook(target);
                }
            }
        }
    }

    if (HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll")) {
        if (void* target = reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlExitUserProcess"))) {
            if (MH_CreateHook(target, reinterpret_cast<void*>(&HookedRtlExitUserProcess),
                              reinterpret_cast<void**>(&g_originalRtlExitUserProcess)) == MH_OK) {
                if (MH_EnableHook(target) == MH_OK) {
                    installed = true;
                } else {
                    MH_RemoveHook(target);
                }
            }
        }
    }

    if (installed) {
        g_exitGuardInstalled.store(true, std::memory_order_release);
        TraceShutdown("enable: exit guard installed");
    } else {
        TraceShutdown("enable: WARNING exit guard install failed");
    }
}

} // namespace

ll::event::ListenerPtr g_mouseListener = nullptr;

float g_playerX       = 0.0f;
float g_playerY       = 0.0f;
float g_playerZ       = 0.0f;
float g_playerYaw     = 0.0f;
float g_playerPitch   = 0.0f;
bool  g_hasPlayer     = false;

class LocalPlayer;
class ClientInstance;

LocalPlayer* g_localPlayer    = nullptr;
ClientInstance* g_clientInstance = nullptr;

#include <mc/deps/core/math/Color.h>
#include <mc/deps/core/math/Vec3.h>

mce::Color g_mapColors[MAP_DATA_SIZE][MAP_DATA_SIZE]  = {};
float      g_mapHeights[MAP_DATA_SIZE][MAP_DATA_SIZE] = {};

Vec3 g_prevPhysicsPos{0.0f, 0.0f, 0.0f};
Vec3 g_currPhysicsPos{0.0f, 0.0f, 0.0f};
std::chrono::steady_clock::time_point g_lastPhysicsTime = std::chrono::steady_clock::now();

int g_playerBlockX = 0;
int g_playerBlockZ = 0;

std::atomic<bool> g_radarUpdated{false};
std::vector<RadarEntity> g_radarEntities;
std::unordered_map<std::string, PlayerSkinHead> g_playerSkinHeads;
std::string g_localPlayerUuid;
// 注意：旧的 g_mapDataUpdated 已经删除以修复 LNK2001

namespace chiyan_map {

ChiyanMap& ChiyanMap::getInstance() {
    static ChiyanMap instance;
    return instance;
}

bool ChiyanMap::load() {
    // 提前初始化语言引擎，确保拦截器可以使用多语言控制台日志
    LanguageManager::Init();

    // [版本拦截防崩系统] 检测游戏客户端版本，防止因结构体/虚表偏移改变导致的进档崩溃
    auto exePathOpt = ll::sys_utils::getModulePath(nullptr);
    std::optional<ll::data::Version> gameVer = std::nullopt;
    if (exePathOpt) {
        gameVer = ll::file_utils::getVersion(*exePathOpt);
    }
    
    if (gameVer) {
        // 手动构造原始版本字符串 (LeviLamina 输出格式如 "1.26.60-5")
        std::string rawVer = fmt::format("{}.{}.{}-{}", gameVer->major, gameVer->minor, gameVer->patch, gameVer->build.value_or("5"));
        std::string displayVer = rawVer;

        // 将 1.26.60-5 格式美化为玩家熟悉的 1.26.60.05 用于日志友好输出
        size_t dashPos = displayVer.find('-');
        if (dashPos != std::string::npos) {
            std::string tail = displayVer.substr(dashPos + 1);
            if (tail.length() == 1) {
                displayVer.replace(dashPos, 1, ".0"); // 单个数字补零
            } else {
                displayVer.replace(dashPos, 1, ".");  // 双数直接换点
            }
        }

        // 严格限定游戏版本为 1.26.60.05，不匹配则不加载
        if (displayVer != "1.26.60.05" && rawVer != "1.26.60-5" && rawVer != "1.26.60-05") {
            getSelf().getLogger().error("{}: {}", LanguageManager::GetText("LOG_VERSION_MISMATCH"), displayVer);
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_STRICT"));
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
            return false;
        }
        getSelf().getLogger().info("{}: {}", LanguageManager::GetText("LOG_VERSION_PASS"), displayVer);
    } else {
        // 无法识别游戏客户端版本，严格拒绝加载以防崩溃
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_UNKNOWN"));
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
        return false;
    }

    // 在 load 阶段注册 LeviLamina 原生键位：必须早于原版 RemappingLayout::assignDefaultMapping，
    // 否则键位不会出现在「设置 -> 控制」中，也无法与其它 Mod 统一去重。
    ::ChiyanMap::Input::RegisterChiyanMapKeys();

    registerAllHooks();
    MapCacheManager::Init();
    WaypointManager::Init(); // 初始化地标 JSON 引擎
    DeathPointManager::Init(); // 初始化死亡记录引擎
    SeedMapManager::Init();
    AnarchyManager::Init();
    SpecialGridManager::Init();
    DatabaseManager::Init();
    DrawingManager::Init();
    MultiworldManager::Init();
    return true;
}

bool ChiyanMap::enable() {
    // [退出崩溃修复] 将本 DLL 钉在进程中，直到进程终止才卸载。
    // 退出阶段我们刻意保留 user32/DXGI 的 inline hook 跳板与 WndProc 链
    // （主动回写原始机器码会破坏 BetterRenderDragon 等模组的链式 hook），
    // 若 LeviLamina 在 disable() 之后提前 FreeLibrary(ChiyanMap.dll)，
    // 这些跳板就会指向已释放的代码页，游戏 CRT 收尾时必然 0xC0000005。
    static int s_moduleAnchor = 0;
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&s_moduleAnchor),
        &hSelf
    );

    // [性能优化] 提前在模组启用阶段完成 DXGI 和输入底层挂钩，
    // 避免将耗时初始化推迟到主菜单首次渲染帧导致 ~1s 卡顿 (Stutter)
    initDX11Hook();

    // [兼容性关键] 延迟在 enable() 阶段挂钩 ClientInstance::$update
    // 确保 BetterRenderDragon 启动扫描其原始机器码序言顺利完成
    hookClientInstanceUpdate();

    ::ChiyanMap::Input::SetChiyanMapKeysActive(true);
    ::ChiyanMap::Input::RegisterKeybindingViewerCommand();

    g_mouseListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::input::MouseInputEvent>(
        [](ll::event::input::MouseInputEvent& ev) {
            if (MapRenderState::g_isShuttingDown.load()) return;
            if (MapRenderState::IsUIActive()) {
                ev.cancel();
            }
        }
    );

    // arke.dll 在 load 阶段就已加载，此处可稳定检测到共载场景。
    InstallExitProcessGuard();
    return true;
}

bool ChiyanMap::disable() {
    TraceShutdown("disable: enter");
    // [退出崩溃修复] 整个退出流程必须严格幂等：LeviLamina 在进程退出时可能多次触发，
    // 重复 join / 重复释放正是堆损坏与死锁的来源之一。
    // [退出崩溃修复·第二轮] 必须最先置位：此后本 DLL 所有堆释放（含 CRT 在
    // DLL_PROCESS_DETACH 阶段自动执行的全局对象析构）都不再经过已半拆卸的
    // LeviLamina allocator / MallocRedirect，避免 free-list 链表被破坏。
    ChiyanMapMem::g_skipDeleteOnShutdown.store(true, std::memory_order_relaxed);

    static std::atomic<bool> s_disableStarted{false};
    if (s_disableStarted.exchange(true)) {
        TraceShutdown("disable: duplicate call ignored");
        return true;
    }

    // (1) 立即冻结所有钩子入口：置位后每个 hook 都直接 pass-through 原函数，
    //     不再触碰 ImGui 上下文、D3D 资源与玩家数据。
    ::ChiyanMap::Input::SetChiyanMapKeysActive(false);
    ::ChiyanMap::Input::UnregisterKeybindingViewerCommand();
    MapRenderState::g_isShuttingDown.store(true);
    ClipCursor(NULL);
    if (g_mouseListener) {
        ll::event::EventBus::getInstance().removeListener(g_mouseListener);
        g_mouseListener = nullptr;
    }
    g_hasPlayer = false;
    g_localPlayer = nullptr;
    g_clientInstance = nullptr;

    // (2) 先拆掉 IME 窗口过程并终止其常驻线程，防止窗口销毁期间回调进入本 DLL。
    shutdownInputIME();

    // (3) 落盘用户数据。此阶段只做文件 IO / JSON 序列化，不释放任何堆资源。
    AnarchyManager::SaveHighlights();
    WaypointManager::SaveWaypoints();
    DeathPointManager::SaveDeathPoints();
    DrawingManager::SaveDrawings();
    MultiworldManager::SaveConfig();
    SpecialGridManager::Shutdown();
    TraceShutdown("disable: user data saved");

    // (4) 停止并 join 全部后台线程（种子 worker / 缓存写 worker / 磁盘 IO / PNG 导出）。
    //     线程入口代码位于本 DLL，绝不能在 DLL 卸载或 CRT 静态析构后继续运行。
    SeedMapManager::Shutdown();
    shutdownCacheWriteThread();
    MapCacheManager::Shutdown();
    TraceShutdown("disable: background threads joined");

    // (5) 关闭数据库，并冻结 DX11 渲染（等待在飞帧、恢复 WndProc、停止烘焙线程、
    //     按依赖顺序 Release 全部 D3D COM 对象）。
    DatabaseManager::Shutdown();
    shutdownDX11Hook();
    TraceShutdown("disable: dx shutdown done");

    // [退出崩溃修复·关键] 堆内存（ImGui 上下文对象、RegionData、双缓冲、STL 容器）
    // 在此刻意泄漏：它们由 CRT 在 DLL_PROCESS_DETACH 阶段自动析构，而那时
    // LeviLamina allocator / MallocRedirect 已半拆卸，_free_base -> RtlFreeHeap
    // 会读到损坏的堆块头而 0xC0000005（详见 MemoryOperators.cpp）。
    // D3D COM 对象走 IUnknown::Release、不经过该堆，已在上面完整释放。
    // 但实测（16:21 / 16:26 / 18:59 三次崩溃栈逐帧一致）证明：仅靠提前释放
    // 并不能阻止 arke.dll 在 LdrShutdownProcess 阶段的静态析构触发 AMD 驱动
    // fastfail，因此 enable() 中额外安装了 ExitProcess 拦截来跳过该阶段。
    g_disableCompleted.store(true, std::memory_order_release);
    TraceShutdown("disable: complete");

    // 兜底：若 ExitProcess 钩子因系统 DLL 前缀过短等原因安装失败，就在清理完成后
    // 直接终止进程，仍然跳过必然崩溃的 LdrShutdownProcess 阶段。
    if (!g_exitGuardInstalled.load(std::memory_order_acquire) && GetModuleHandleW(L"arke.dll")) {
        TraceShutdown("disable: exit guard missing, fallback TerminateProcess");
        TerminateProcess(GetCurrentProcess(), 0);
    }
    return true;
}

}

LL_REGISTER_MOD(chiyan_map::ChiyanMap, chiyan_map::ChiyanMap::getInstance());
