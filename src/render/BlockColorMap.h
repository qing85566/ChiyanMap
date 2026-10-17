#pragma once

#include <string>
#include <string_view>
#include <mc/deps/core/math/Color.h>

namespace BlockColorMap {

bool IsInvisibleOrTechnicalOverlay(std::string_view rawName) noexcept;
bool IsPillarLogOrStem(std::string_view rawName) noexcept;
std::string GetHorizontalWoodName(std::string_view name);
bool IsFlowerBlock(std::string_view rawName) noexcept;
bool IsGlassOrStainedGlassBlock(std::string_view rawName) noexcept;
bool GetTranslucentOverlayColor(std::string_view rawName, mce::Color& outColor, float& outAlpha) noexcept;
uint8_t GetIntrinsicBlockLight(std::string_view rawName) noexcept;
mce::Color GetVanillaBlockColor(std::string_view rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol, bool useBiomeColors);
mce::Color GetBlockColor(std::string_view rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol);

} // namespace BlockColorMap
