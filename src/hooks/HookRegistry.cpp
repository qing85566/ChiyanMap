#include "hooks/HookRegistry.h"
#include "hooks/PlayerHook.h"
#include "hooks/UIRenderHook.h"
#include "hooks/DX11Hook.h"
#include "hooks/BeaconRenderHook.h"

void registerAllHooks() {
    // 注册除 ClientInstance::$update 之外的所有原生游戏逻辑与 UI 钩子
    LevelUpdateWeatherHook::hook();
    UIRenderContextFlushTextHook::hook();
    BeaconRenderHook::hook();

    LocalPlayerApplyTurnDeltaHook::hook();
    GameModeStartDestroyBlockHook::hook();
    GameModeUseItemHook::hook();
    GameModeAttackHook::hook();

    LocalPlayerSwingHook::hook();
    LocalPlayerMissedSwingHook::hook();
    GameModeInteractHook::hook();
    GameModeUseItemAsAttackHook::hook();
    GameModeUseItemOnHook::hook();
    GameModeContinueDestroyBlockHook::hook();

    MobSetCarriedItemHook::hook();
    GameModeBaseUseItemHook::hook();

    PlayerInventorySelectSlotHook::hook();
    PlayerInventorySetItemHook::hook();
    LocalPlayerPickBlockHook::hook();
    LocalPlayerJumpFromGroundHook::hook();
    LocalPlayerSetSneakingHook::hook();
    LocalPlayerIsImmobileHook::hook();
}

void hookClientInstanceUpdate() {
    // [兼容性关键修复] 延迟在模组 enable() 阶段挂钩 ClientInstance::$update
    // BetterRenderDragon 通过扫描 ClientInstance::update 原始机器码序言 (55 41 57 41 56 41...) 来定位并挂钩。
    // 如果 ChiyanMap 在 load() 阶段抢先挂钩，函数开头会被覆盖为 JMP 指令，
    // 导致 BetterRenderDragon 扫描失败而无法获取 ResourcePackManager，进而使其光影/材质替换完全失效。
    // 将此处推迟至 enable() 执行，让 BetterRenderDragon 在启动期顺利完成挂钩，之后两者形成链式调用完美共存。
    ClientInstanceUpdateHook::hook();
}

void unregisterAllHooks() {
    ClientInstanceUpdateHook::unhook();
    LevelUpdateWeatherHook::unhook();
    UIRenderContextFlushTextHook::unhook();
    BeaconRenderHook::unhook();

    LocalPlayerApplyTurnDeltaHook::unhook();
    GameModeStartDestroyBlockHook::unhook();
    GameModeUseItemHook::unhook();
    GameModeAttackHook::unhook();

    LocalPlayerSwingHook::unhook();
    LocalPlayerMissedSwingHook::unhook();
    GameModeInteractHook::unhook();
    GameModeUseItemAsAttackHook::unhook();
    GameModeUseItemOnHook::unhook();
    GameModeContinueDestroyBlockHook::unhook();

    MobSetCarriedItemHook::unhook();
    GameModeBaseUseItemHook::unhook();

    PlayerInventorySelectSlotHook::unhook();
    PlayerInventorySetItemHook::unhook();
    LocalPlayerPickBlockHook::unhook();
    LocalPlayerJumpFromGroundHook::unhook();
    LocalPlayerSetSneakingHook::unhook();
    LocalPlayerIsImmobileHook::unhook();
}

void shutdownCacheWriteThread() {
    ShutdownCacheWriteThread();
}

bool initDX11Hook() {
    return DX11Hook::init();
}

void shutdownDX11Hook() {
    DX11Hook::shutdown();
}
