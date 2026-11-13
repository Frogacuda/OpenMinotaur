#pragma once
#include "pc_geometry.h"
#include <cstddef>
#include <string>
namespace neon::pc {
constexpr std::size_t ShapeVertexLimit = 65536;
bool recoveredShapeKind(int);
const char *shapeKindName(int);
// ENEMY_SHAPE geometry, with the original packed RGBA palette/alpha convention.
// Kept separate from the historical experimental generateShapes() implementation.
Mesh generatedShapeMesh(const ShapeSettings &, const std::array<uint32_t, 256> &palette,
                        float gateDrive = 0);
} // namespace neon::pc
