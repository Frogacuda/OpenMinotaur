#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace neon::pc {
struct Point {
    float x = 0, y = 0, z = 0;
};
struct ShapeVertex {
    Point position;
    uint32_t colour = 0;
};
using Mesh = std::vector<ShapeVertex>;
struct ShapeSettings {
    std::array<int32_t, 16> integers{};
    std::array<float, 16> floats{};
};
// Recovered ENEMY_SHAPE cases used by r2-c8. Colours are supplied separately:
// palette sequencing belongs to the not-yet-recovered surrounding renderer.
Mesh generateShapes(const ShapeSettings &, const std::array<uint32_t, 4> &colours);
} // namespace neon::pc
