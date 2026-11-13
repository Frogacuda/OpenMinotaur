#pragma once
#include "pc_cube.h"
#include "pc_render_state.h"
namespace neon::pc {
// FLOWER 004205f0 / 00446b60: two textured annular strips, optional
// reflected depth and folded angular UVs. No particle simulation or GPU state.
CubeFace flowerMesh(const ShapeSettings &);
Matrix4 flowerModelView(const ShapeSettings &, const Matrix4 &parent);
} // namespace neon::pc
