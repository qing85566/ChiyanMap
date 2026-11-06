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
#include <string>
#include <algorithm>

// 定义于 MemoryOperators.cpp：置位后本 DLL 的所有 operator delete 变为 no-op
namespace ChiyanMapMem {
extern std::atomic<bool> g_skipDeleteOnShutdown;
}

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

    g_mouseListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::input::MouseInputEvent>(
        [](ll::event::input::MouseInputEvent& ev) {
            if (MapRenderState::g_isShuttingDown.load()) return;
            if (MapRenderState::IsUIActive()) {
                ev.cancel();
            }
        }
    );
    return true;
}

bool ChiyanMap::disable() {
    // [退出崩溃修复] 整个退出流程必须严格幂等：LeviLamina 在进程退出时可能多次触发，
    // 重复 join / 重复释放正是堆损坏与死锁的来源之一。
    // [退出崩溃修复·第二轮] 必须最先置位：此后本 DLL 所有堆释放（含 CRT 在
    // DLL_PROCESS_DETACH 阶段自动执行的全局对象析构）都不再经过已半拆卸的
    // LeviLamina allocator / MallocRedirect，避免 free-list 链表被破坏。
    ChiyanMapMem::g_skipDeleteOnShutdown.store(true, std::memory_order_relaxed);

    static std::atomic<bool> s_disableStarted{false};
    if (s_disableStarted.exchange(true)) return true;

    // (1) 立即冻结所有钩子入口：置位后每个 hook 都直接 pass-through 原函数，
    //     不再触碰 ImGui 上下文、D3D 资源与玩家数据。
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

    // (4) 停止并 join 全部后台线程（种子 worker / 缓存写 worker / 磁盘 IO / PNG 导出）。
    //     线程入口代码位于本 DLL，绝不能在 DLL 卸载或 CRT 静态析构后继续运行。
    SeedMapManager::Shutdown();
    shutdownCacheWriteThread();
    MapCacheManager::Shutdown();

    // (5) 关闭数据库，并冻结 DX11 渲染（等待在飞帧、恢复 WndProc、停止烘焙线程）。
    DatabaseManager::Shutdown();
    shutdownDX11Hook();

    // [退出崩溃修复·关键] 进程即将终止，此处刻意不再主动 delete / Release /
    // DestroyContext / clear 大容器。此前在 LdrShutdownProcess 与 CRT 静态析构阶段
    // 集中释放 ImGui 上下文、D3D COM 对象、数百个 RegionData 和 11MB 双缓冲，
    // 会与 LeviLamina allocator、MallocRedirect 已半拆卸的堆交互，
    // 使后续 _free_base -> RtlFreeHeap 读到损坏的堆块头而 0xC0000005。
    // 这些内存由操作系统在进程终止时统一回收，代价仅为退出瞬间的“有意泄漏”。
    return true;
}

}

LL_REGISTER_MOD(chiyan_map::ChiyanMap, chiyan_map::ChiyanMap::getInstance());
