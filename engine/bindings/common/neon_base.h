// neon_base.h - shared foundation types of the Neon engine:
// 2D/3D vectors, 4x4 matrices with the D3DX helpers the engine was written
// against, the colour type (with its built-in colour cycler), RNG helpers.
//
// Each game compiles its bindings in a per-core
// namespace (NEON_GAME) to keep cores linkable into one binary.
#pragma once

#include "neon/math.h"
#include <cstdint>

#ifndef NEON_GAME
#error "Define NEON_GAME to the owning game namespace before including Neon bindings"
#endif

namespace NEON_GAME {
namespace Neon_Internals {

// ------------------------------------------------------------- random -----
// The original calls libc rand() and scales by 2147483647.0 (iOS RAND_MAX).
// Windows' RAND_MAX is 32767, so we carry our own copy of the BSD/iOS
// generator (Park-Miller "minimal standard", 16807 multiplier).
constexpr int kRandMax = 0x7fffffff;
void neonSrand(uint32_t seed);
int neonRand();

using neon::clerp;
using neon::colour;
using neon::D3DXMatrixIdentity;
using neon::D3DXMatrixLookAtLH;
using neon::D3DXMatrixOrthoLH;
using neon::D3DXMatrixPerspectiveFovLH;
using neon::D3DXMatrixRotationAxis;
using neon::D3DXMatrixRotationX;
using neon::D3DXMatrixRotationY;
using neon::D3DXMatrixRotationZ;
using neon::D3DXMatrixScaling;
using neon::D3DXMatrixTranslation;
using neon::D3DXMatrixTranspose;
using neon::D3DXVec3Transform;
using neon::D3DXVECTOR3;
using neon::D3DXVECTOR4;
using neon::matrix4;
using neon::matrix4f;
using neon::vec2d;
using neon::vec3d;

// One of 14 flashing palettes keyed off a frame counter (the 8-bit style
// colour flicker used for text and pickups). Unknown types return opaque white.
colour getDynamicColour(int type, float alpha, int frame);

// ------------------------------------------------------------------ RNG ----

struct oxRNG {
    static double rand(double lo, double hi); // uniform in [lo, hi]
    static double rand(double hi);            // uniform in [0, hi]
    static int irand(double hi);              // truncated rand(hi)
};

} // namespace Neon_Internals
} // namespace NEON_GAME
