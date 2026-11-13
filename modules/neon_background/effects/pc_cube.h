#pragma once
#include "pc_geometry.h"
#include <array>
#include <vector>
namespace neon::pc {
struct CubeVertex {
    uint32_t rgba;
    float u0, v0, u1, v1, x, y, z;
};
using CubeFace = std::vector<CubeVertex>;
using CubeField = std::array<float, 1024>;
// Emit one deformed local-space face; the caller supplies placement and colour.
CubeFace cubeFace(const ShapeSettings &, const CubeField &, uint32_t rgba, int face);
CubeField cubeField(const ShapeSettings &);
std::array<float, 16> cubeRotation(float a, float b, float c);
} // namespace neon::pc
