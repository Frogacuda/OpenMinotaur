#pragma once
#include "original_frontend.h"
#include "native_draw.h"
namespace gridrunner {
void drawNativeGrid(const gr::reconstruction::ios::DrawFrame &, Neon_Internals::sprite *,
                    Neon_Internals::vec2d);
void drawNativeOverlay(const gr::reconstruction::ios::DrawFrame &, Neon_Internals::sprite *,
                       Neon_Internals::vec2d);
} // namespace gridrunner
