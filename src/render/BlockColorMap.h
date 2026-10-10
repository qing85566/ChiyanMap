#pragma once

#include <string>
#include <mc/deps/core/math/Color.h>

namespace BlockColorMap {

bool IsInvisibleOrTechnicalOverlay(std::string const& rawName) noexcept;
bool IsPillarLogOrStem(std::string const& rawName) noexcept;
std::string GetHorizontalWoodName(std::string const& name);
mce::Color GetBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol);

} // namespace BlockColorMap
