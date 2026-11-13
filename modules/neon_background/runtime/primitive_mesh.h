#pragma once
#include "component_scene.h"
#include "pc_cube.h"
namespace neon::effects {
const char *primitiveName(int);
neon::pc::CubeFace primitiveMesh(const Component &, double seconds = 0);
} // namespace neon::effects
