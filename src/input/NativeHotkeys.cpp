#include "input/NativeHotkeys.h"

#include <atomic>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "input/ChiyanKeyRegistryInfo.h"
#include "ll/api/input/KeyRegistry.h"
#include "ll/api/mod/NativeMod.h"

namespace ChiyanMap::Input {

std::vector<ChiyanRegisteredKey> gChiyanRegisteredKeys;

namespace {

std::atomic<bool>                    s_enabled{false};
std::atomic<bool>                    s_registered{false};
std::atomic<bool>                    s_active{false};
std::atomic<uint32_t>                s_managedMask{0};
std::atomic<HotkeyActionHandler>     s_handler{nullptr};

std::mutex                           s_stateMutex;
std::array<MapRenderState::Hotkey, kHotkeyActionCount> s_snapshot{};
std::array<MapRenderState::Hotkey, kHotkeyActionCount> s_nativeDefaults{};
std::array<bool, kHotkeyActionCount>                   s_isRegistered{};

constexpr uint32_t Bit(HotkeyAction action) {
    return 1u << static_cast<uint32_t>(action);
}

HotkeyDispatchResult InvokeHandler(HotkeyAction action, bool isDown) {
    auto handler = s_handler.load(std::memory_order_acquire);
    return handler ? handler(action, isDown) : HotkeyDispatchResult::NotHandled;
}

void OnNativeButton(HotkeyAction action, bool isDown) {
    if (!s_enabled.load(std::memory_order_relaxed) || !s_active.load(std::memory_order_relaxed)) return;
    if ((s_managedMask.load(std::memory_order_relaxed) & Bit(action)) == 0) return;
    if (MapRenderState::g_isShuttingDown.load(std::memory_order_relaxed)) return;
    InvokeHandler(action, isDown);
}

} // namespace

void SetHotkeyActionHandler(HotkeyActionHandler handler) {
    s_handler.store(handler, std::memory_order_release);
}

bool AreNativeHotkeysEnabled() {
    return s_enabled.load(std::memory_order_relaxed);
}

bool IsNativeHotkeyManaged(HotkeyAction action) {
    return s_enabled.load(std::memory_order_relaxed)
        && s_active.load(std::memory_order_relaxed)
        && (s_managedMask.load(std::memory_order_relaxed) & Bit(action)) != 0;
}

void SetChiyanMapKeysActive(bool active) {
    s_active.store(active, std::memory_order_relaxed);
}

void RegisterChiyanMapKeys() {
    if (s_registered.exchange(true)) return;

    s_enabled.store(MapRenderState::useNativeKeyRegistry, std::memory_order_relaxed);
    if (!s_enabled.load(std::memory_order_relaxed)) return;

    // KeyRegistry 会把 "modName.name" 作为键位动作 ID，交由原版 RemappingLayout 统一去重与改键。
    auto  mod      = ll::mod::NativeMod::current();
    auto& registry = ll::input::KeyRegistry::getInstance();

    // KeyRegistry 内部用 "默认键码 -> 动作名" 的单值表分发回调，因此每个已注册动作
    // 必须拥有互不相同的非零默认键码：
    //   * 组合键原版无法表达 -> 不注册，继续由自带热键系统处理；
    //   * 多个动作配置了同一个键 -> 全部保留为自带热键，避免错误分发；
    //   * 未绑定的动作 -> 分配一个不会被物理按键产生的保留 VK 码作为占位，
    //     玩家在原版设置里改键后仍能正确回调到对应动作。
    constexpr int kPlaceholderKeys[] = {
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xB8, 0xB9,
        0xC1, 0xC2, 0xE7, 0xE8, 0xF6, 0xF7, 0xF8, 0xF9
    };

    std::unordered_map<int, int> keyUsage;
    for (size_t i = 0; i < kHotkeyActionCount; ++i) {
        auto const& hk = GetHotkey(static_cast<HotkeyAction>(i));
        if (hk.key != 0 && hk.modifiers == 0) ++keyUsage[hk.key];
    }
    std::unordered_set<int> usedKeys;
    for (auto const& [key, count] : keyUsage) {
        if (count > 1) usedKeys.insert(key); // 重复键：相关动作全部走自带热键
    }
    size_t nextPlaceholder = 0;

    std::lock_guard<std::mutex> lock(s_stateMutex);
    uint32_t mask = 0;
    for (size_t i = 0; i < kHotkeyActionCount; ++i) {
        auto const  action = static_cast<HotkeyAction>(i);
        auto const& hk     = GetHotkey(action);

        s_snapshot[i]       = hk;
        s_nativeDefaults[i] = hk;

        bool const isChord    = (hk.modifiers != 0);
        bool const isConflict = (hk.key != 0 && usedKeys.count(hk.key) != 0);
        if (isChord || isConflict) continue; // 不改 s_managedMask，保持自带热键接管

        int defaultKey = hk.key;
        if (defaultKey == 0) {
            while (nextPlaceholder < std::size(kPlaceholderKeys)
                   && keyUsage.count(kPlaceholderKeys[nextPlaceholder]) != 0) {
                ++nextPlaceholder;
            }
            if (nextPlaceholder >= std::size(kPlaceholderKeys)) continue;
            defaultKey = kPlaceholderKeys[nextPlaceholder++];
        }

        auto& handle = registry.getOrCreateKey(
            kNativeHotkeySpecs[i].registryName,
            std::vector<int>{defaultKey},
            true,
            mod
        );
        handle.registerButtonDownHandler([action](::FocusImpact, ::IClientInstance&) {
            OnNativeButton(action, true);
        });
        handle.registerButtonUpHandler([action](::FocusImpact, ::IClientInstance&) {
            OnNativeButton(action, false);
        });
        std::string const regName  = kNativeHotkeySpecs[i].registryName;
        std::string const actionId = std::string("key.ChiyanMap.") + kNativeHotkeySpecs[i].registryName;
        gChiyanRegisteredKeys.push_back(ChiyanRegisteredKey{regName, actionId, &handle});
        s_isRegistered[i] = true;
        mask |= Bit(action);
    }
    s_managedMask.store(mask, std::memory_order_relaxed);
    s_active.store(true, std::memory_order_relaxed);
}

void OnConfigSaved() {
    if (!s_registered.load(std::memory_order_relaxed) || !s_enabled.load(std::memory_order_relaxed)) return;

    std::lock_guard<std::mutex> lock(s_stateMutex);
    for (size_t i = 0; i < kHotkeyActionCount; ++i) {
        auto const  action = static_cast<HotkeyAction>(i);
        auto const& cur    = GetHotkey(action);
        if (cur == s_snapshot[i]) continue;

        // 用户在本模组面板/配置里改了这一行：从此只走自带热键，避免原版回调双重触发。
        s_snapshot[i] = cur;
        s_managedMask.fetch_and(~Bit(action), std::memory_order_relaxed);
    }
}

void RestoreNativeKeyManagement() {
    if (!s_registered.load(std::memory_order_relaxed) || !s_enabled.load(std::memory_order_relaxed)) return;

    std::lock_guard<std::mutex> lock(s_stateMutex);
    uint32_t mask = 0;
    for (size_t i = 0; i < kHotkeyActionCount; ++i) {
        // 只有真正注册进 KeyRegistry 的动作才能交回原版管理，否则会变成"既无原生回调、又被禁用自带热键"；
        // 组合键/重复键等未被注册的动作保持玩家在面板里设置的值不动。
        if (!s_isRegistered[i]) continue;
        auto const action = static_cast<HotkeyAction>(i);
        GetMutableHotkey(action) = s_nativeDefaults[i];
        s_snapshot[i]            = s_nativeDefaults[i];
        mask |= Bit(action);
    }
    s_managedMask.store(mask, std::memory_order_relaxed);
}

} // namespace ChiyanMap::Input
