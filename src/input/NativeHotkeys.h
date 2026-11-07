#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "state/MapRenderState.h"

// ==========================================
// LeviLamina 原生键位注册 (ll::input::KeyRegistry)
//
// 与旧版 config.json / ImGui 重绑面板的关系：
//   * 默认把 config.json 里的单键绑定作为原版「设置 -> 控制」里的默认值注册进去，
//     玩家可以直接在原版界面里改键，并由原版统一处理与其它 Mod 的按键冲突。
//   * 带 Ctrl/Shift/Alt 的组合键原版无法表达，仍由 ChiyanMap 自带热键系统处理。
//   * 在 ChiyanMap 快捷键面板中改动某一行后，该动作自动切回自带热键，避免双重触发。
// ==========================================
namespace ChiyanMap::Input {

enum class HotkeyAction : uint8_t {
    OpenBigMap,
    OpenWaypointMgr,
    OpenDeathPointMgr,
    ToggleMinimap,
    ToggleMinimapShape,
    ToggleMinimapRot,
    HoldEntities,
    ToggleSeedMap,
    EnlargeMinimap,
    CenterCamera,
    SelectExportArea,
    ToggleSlimeChunks,
    SwitchWaypointSet,
    ToggleHighways,
    ToggleAnarchySettings,
    ToggleDrawingMode,
    ToggleDoubleMinZoom,
    ToggleTransBg,
    MinimapZoomIn,
    MinimapZoomOut,
    TogglePreferOwWaypoints,
    QuickCreateWaypoint,
    ToggleViewDistance,
    ToggleSimulationDistance,
    ToggleBigMapUI,
    ToggleFollowPlayer,
    Count
};

inline constexpr size_t kHotkeyActionCount = static_cast<size_t>(HotkeyAction::Count);

struct NativeHotkeySpec {
    HotkeyAction                                            action;
    char const*                                             registryName;
    MapRenderState::Hotkey MapRenderState::HotkeyBindings::* binding;
};

// 顺序与 HotkeyBindings 字段一一对应，同时是 config.json 里 hotkeys 对象的键名。
inline constexpr std::array<NativeHotkeySpec, kHotkeyActionCount> kNativeHotkeySpecs = {{
    { HotkeyAction::OpenBigMap,              "openBigMap",              &MapRenderState::HotkeyBindings::openBigMap },
    { HotkeyAction::OpenWaypointMgr,         "openWaypointMgr",         &MapRenderState::HotkeyBindings::openWaypointMgr },
    { HotkeyAction::OpenDeathPointMgr,       "openDeathPointMgr",       &MapRenderState::HotkeyBindings::openDeathPointMgr },
    { HotkeyAction::ToggleMinimap,           "toggleMinimap",           &MapRenderState::HotkeyBindings::toggleMinimap },
    { HotkeyAction::ToggleMinimapShape,      "toggleMinimapShape",      &MapRenderState::HotkeyBindings::toggleMinimapShape },
    { HotkeyAction::ToggleMinimapRot,        "toggleMinimapRot",        &MapRenderState::HotkeyBindings::toggleMinimapRot },
    { HotkeyAction::HoldEntities,            "holdEntities",            &MapRenderState::HotkeyBindings::holdEntities },
    { HotkeyAction::ToggleSeedMap,           "toggleSeedMap",           &MapRenderState::HotkeyBindings::toggleSeedMap },
    { HotkeyAction::EnlargeMinimap,          "enlargeMinimap",          &MapRenderState::HotkeyBindings::enlargeMinimap },
    { HotkeyAction::CenterCamera,            "centerCamera",            &MapRenderState::HotkeyBindings::centerCamera },
    { HotkeyAction::SelectExportArea,        "selectExportArea",        &MapRenderState::HotkeyBindings::selectExportArea },
    { HotkeyAction::ToggleSlimeChunks,       "toggleSlimeChunks",       &MapRenderState::HotkeyBindings::toggleSlimeChunks },
    { HotkeyAction::SwitchWaypointSet,       "switchWaypointSet",       &MapRenderState::HotkeyBindings::switchWaypointSet },
    { HotkeyAction::ToggleHighways,          "toggleHighways",          &MapRenderState::HotkeyBindings::toggleHighways },
    { HotkeyAction::ToggleAnarchySettings,   "toggleAnarchySettings",   &MapRenderState::HotkeyBindings::toggleAnarchySettings },
    { HotkeyAction::ToggleDrawingMode,       "toggleDrawingMode",       &MapRenderState::HotkeyBindings::toggleDrawingMode },
    { HotkeyAction::ToggleDoubleMinZoom,     "toggleDoubleMinZoom",     &MapRenderState::HotkeyBindings::toggleDoubleMinZoom },
    { HotkeyAction::ToggleTransBg,           "toggleTransBg",           &MapRenderState::HotkeyBindings::toggleTransBg },
    { HotkeyAction::MinimapZoomIn,           "minimapZoomIn",           &MapRenderState::HotkeyBindings::minimapZoomIn },
    { HotkeyAction::MinimapZoomOut,          "minimapZoomOut",          &MapRenderState::HotkeyBindings::minimapZoomOut },
    { HotkeyAction::TogglePreferOwWaypoints, "togglePreferOwWaypoints", &MapRenderState::HotkeyBindings::togglePreferOwWaypoints },
    { HotkeyAction::QuickCreateWaypoint,     "quickCreateWaypoint",     &MapRenderState::HotkeyBindings::quickCreateWaypoint },
    { HotkeyAction::ToggleViewDistance,      "toggleViewDistance",      &MapRenderState::HotkeyBindings::toggleViewDistance },
    { HotkeyAction::ToggleSimulationDistance,"toggleSimulationDistance",&MapRenderState::HotkeyBindings::toggleSimulationDistance },
    { HotkeyAction::ToggleBigMapUI,          "toggleBigMapUI",          &MapRenderState::HotkeyBindings::toggleBigMapUI },
    { HotkeyAction::ToggleFollowPlayer,      "toggleFollowPlayer",      &MapRenderState::HotkeyBindings::toggleFollowPlayer }
}};

inline MapRenderState::Hotkey& GetMutableHotkey(HotkeyAction action) {
    return MapRenderState::g_hotkeys.*(kNativeHotkeySpecs[static_cast<size_t>(action)].binding);
}

inline MapRenderState::Hotkey const& GetHotkey(HotkeyAction action) {
    return GetMutableHotkey(action);
}

enum class HotkeyDispatchResult : uint8_t {
    NotHandled,   // 未处理（例如长按动作的抬起事件、当前状态不允许触发）
    Handled,      // 已处理，WndProc 应吞掉该按键
    PassThrough   // 已判定但应把按键交还给游戏
};

using HotkeyActionHandler = HotkeyDispatchResult (*)(HotkeyAction action, bool isDown);

// 在 load() 阶段、LanguageManager::Init() 之后调用；必须早于原版 RemappingLayout::assignDefaultMapping。
void RegisterChiyanMapKeys();

// enable()/disable() 控制原生回调是否生效（KeyHandle 没有公开的反注册接口）。
void SetChiyanMapKeysActive(bool active);

// 由 DX11Hook 在静态初始化阶段注入真正的动作执行器。
void SetHotkeyActionHandler(HotkeyActionHandler handler);

bool IsNativeHotkeyManaged(HotkeyAction action);
bool AreNativeHotkeysEnabled();

// SaveConfig() 末尾调用：某一行热键被 config/ImGui 面板改动后切回自带热键。
void OnConfigSaved();

// 快捷键面板「全部改用原版键位」：恢复注册时的默认绑定并重新交由原版管理。
void RestoreNativeKeyManagement();

} // namespace ChiyanMap::Input
