#pragma once
#include <array>
#include <cstdint>
namespace neon::pc {
using RootPixels = std::array<uint32_t, 64 * 64>;
// Original 64x64 procedural sprite/template and coloured stamp mechanisms.
// Root 2/3/4 definitions from 0043f360; template 0043ef40; stamp 0043e430.
RootPixels rootTexture(unsigned selector);
} // namespace neon::pc
