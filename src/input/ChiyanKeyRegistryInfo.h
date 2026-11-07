#pragma once

#include "ll/api/input/KeyHandle.h"

#include <string>
#include <vector>

namespace ChiyanMap::Input {

// 记录本模组通过 KeyRegistry 注册的所有快捷键句柄，供“设置-控制-键盘和鼠标”
// 注入独立分区时使用（显示名称 + 当前按键）。
struct ChiyanRegisteredKey {
    std::string           registryName; // 例如 "openBigMap"
    std::string           actionId;     // 例如 "key.ChiyanMap.openBigMap"
    ll::input::KeyHandle* handle;
};

extern std::vector<ChiyanRegisteredKey> gChiyanRegisteredKeys;

} // namespace ChiyanMap::Input
