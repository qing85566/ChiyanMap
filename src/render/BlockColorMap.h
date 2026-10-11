#pragma once

#include <string>
#include <mc/deps/core/math/Color.h>

namespace BlockColorMap {

bool IsInvisibleOrTechnicalOverlay(std::string const& rawName) noexcept;
bool IsPillarLogOrStem(std::string const& rawName) noexcept;
std::string GetHorizontalWoodName(std::string const& name);
bool IsFlowerBlock(std::string const& rawName) noexcept;
bool IsGlassOrStainedGlassBlock(std::string const& rawName) noexcept;
bool GetTranslucentOverlayColor(std::string const& rawName, mce::Color& outColor, float& outAlpha) noexcept;
uint8_t GetIntrinsicBlockLight(std::string const& rawName) noexcept;
mce::Color GetVanillaBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol, bool useBiomeColors);
mce::Color GetBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol);

} // namespace BlockColorMap
