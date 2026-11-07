#pragma once

#include "mc/world/actor/player/Player.h"

namespace ChiyanMap::Input {

// 注册客户端命令 /chiyanmap，打开「赤岩地图 · 快捷键」表单，
// 将所有由本模组注册的按键以「独立、标注归属」的方式列出（因原版设置界面
// 在 26.60.0 未导出相关符号，无法注入独立分区，故提供此等价视图）。
void RegisterKeybindingViewerCommand();
void UnregisterKeybindingViewerCommand();

// 直接向指定玩家发送快捷键查看表单。
void ShowKeybindingForm(Player& player);

} // namespace ChiyanMap::Input
