// Review-only alternatives. The application's default dock is not changed.
#pragma once
#include "launcher_dock.h"

inline std::vector<uint8_t> dockStylePreview(int style) {
    using Graphic = mino::iox::DockGraphic;
    std::vector<uint8_t> rgba(size_t(Graphic::width) * Graphic::height * 4);
    constexpr float pi = 3.14159265359f;
    for (int y = 0; y < Graphic::height; ++y)
        for (int x = 0; x < Graphic::width; ++x) {
            const float u = float(x) / (Graphic::width - 1), v = float(y) / (Graphic::height - 1);
            const float curve = std::pow(std::max(0.f, std::sin(pi * v)), .7f);
            float shade;
            if (style == 0) {
                // Higher-contrast diagonal carbon under a pronounced curved clearcoat.
                const float twill = .5f + .5f * std::cos(2 * pi * (x + y) / 12);
                const float strands = .5f + .5f * std::cos(2 * pi * (x - y) / 3);
                // Build curvature by darkening the edges/underside, rather than a
                // bright band behind the icons that can read as a lightening shadow.
                shade = 76 + 34 * twill + 4 * strands - 45 * (1 - curve) - 24 * std::pow(v, 3.f);
            } else {
                // Silver micro-perforated metal, with staggered, softly edged holes.
                const int row = y / 7;
                const float dx = std::fmod(x + (row & 1) * 3.5f, 7.f) - 3.5f;
                const float dy = float(y % 7) - 3.5f;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float surface = std::clamp((distance - 1.1f) / .7f, 0.f, 1.f);
                shade = (92 + 65 * curve + 1.5f * std::sin(y * 2.4f)) * (.30f + .70f * surface);
            }
            const float reflectionEdge =
                style == 0 ? .43f : .43f - .065f * (2 * u - 1) * (2 * u - 1);
            if (v < reflectionEdge)
                shade += style == 0 ? 28 - 10 * v / reflectionEdge : 12 * (1 - v / reflectionEdge);
            if (y == 0)
                shade = style == 0 ? 195.f : 190.f;
            else if (y == 1)
                shade = style == 0 ? 65.f : 60.f;
            else if (style == 0 && y == Graphic::height - 2)
                shade = 18;
            else if (y == Graphic::height - 1)
                shade = 25;
            const size_t pixel = size_t(y * Graphic::width + x) * 4;
            rgba[pixel] = uint8_t(std::clamp(shade * .98f, 0.f, 255.f));
            rgba[pixel + 1] = uint8_t(std::clamp(shade, 0.f, 255.f));
            rgba[pixel + 2] = uint8_t(std::clamp(shade * 1.02f, 0.f, 255.f));
            rgba[pixel + 3] = 255;
        }
    return rgba;
}
