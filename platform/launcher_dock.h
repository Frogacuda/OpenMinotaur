// One canonical portrait dock graphic, uploaded once and rotated by UVs in
// landscape. High-contrast diagonal carbon sits under curved shading and a straight glass
// reflection.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mino::iox {
struct DockGraphic {
    static constexpr int width = 960, height = 248;
    static std::vector<uint8_t> pixels() {
        std::vector<uint8_t> rgba(size_t(width) * height * 4);
        constexpr float pi = 3.14159265359f;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const float v = float(y) / (height - 1);
                const float curve = std::pow(std::max(0.f, std::sin(pi * v)), .7f);
                const float twill = .5f + .5f * std::cos(2 * pi * (x + y) / 12);
                const float strands = .5f + .5f * std::cos(2 * pi * (x - y) / 3);
                float shade =
                    76 + 34 * twill + 4 * strands - 45 * (1 - curve) - 24 * std::pow(v, 3.f);
                constexpr float reflectionEdge = .43f;
                if (v < reflectionEdge)
                    shade += 28 - 10 * v / reflectionEdge;
                if (y == 0)
                    shade = 65;
                else if (y == 1)
                    shade = 65;
                else if (y == height - 2)
                    shade = 18;
                else if (y == height - 1)
                    shade = 25;
                const size_t pixel = size_t(y * width + x) * 4;
                rgba[pixel] = uint8_t(std::clamp(shade * .98f, 0.f, 255.f));
                rgba[pixel + 1] = uint8_t(std::clamp(shade, 0.f, 255.f));
                rgba[pixel + 2] = uint8_t(std::clamp(shade * 1.02f, 0.f, 255.f));
                rgba[pixel + 3] = 255;
            }
        return rgba;
    }
};
} // namespace mino::iox
