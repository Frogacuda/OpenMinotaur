// neon_base.h - foundation types of the Neon engine (Minotaur Rescue's copy):
// 2D/3D vectors, 4x4 matrices with the D3DX helpers the engine was written
// against, the colour type (with its built-in colour cycler), RNG helpers.
//
// Each game ships its own engine copy, so everything lives in a per-core
// namespace (NEON_GAME) to keep cores linkable into one binary.
#pragma once

#include "neon/foundation_rules.h"
#include "neon/math.h"
#include <cmath>
#include <cstdint>

#ifndef NEON_GAME
#error "Define NEON_GAME to the owning game namespace"
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

// --------------------------------------------------------------- vec2d -----

struct vec2d {
    double x = 0, y = 0;

    vec2d() = default;
    constexpr vec2d(double x_, double y_) : x(x_), y(y_) {}

    vec2d operator+(const vec2d &o) const {
        return {x + o.x, y + o.y};
    }
    vec2d operator-(const vec2d &o) const {
        return {x - o.x, y - o.y};
    }
    vec2d operator*(const vec2d &o) const {
        return {x * o.x, y * o.y};
    }
    vec2d operator*(double s) const {
        return {x * s, y * s};
    }
    vec2d operator/(double s) const {
        return {x / s, y / s};
    }
    vec2d &operator=(double s) {
        x = y = s;
        return *this;
    }
    vec2d &operator+=(const vec2d &o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    vec2d &operator-=(const vec2d &o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    void swap() {
        double t = x;
        x = y;
        y = t;
    }
    double length() const {
        return std::sqrt(x * x + y * y);
    }

    // Linear interpolation towards o; t is clamped to [-1, 1].
    vec2d lerp(const vec2d &o, float t) const {
        double k = t < -1.0f ? -1.0 : (t > 1.0f ? 1.0 : double(t));
        return {x + (o.x - x) * k, y + (o.y - y) * k};
    }

    // Rotates in place by `a` radians (clockwise in screen space).
    void rotateBy(double a) {
        neon::rotateClockwise(x, y, a, neon::ZeroAngleRule::EvaluateRotation);
    }

    // Unit vector in the same direction, or (0,0) for a zero vector.
    vec2d normalize() const {
        double l = std::sqrt(y * y + x * x);
        if (l > 0.0)
            return {x / l, y / l};
        return {0, 0};
    }
};

struct vec3d {
    double x = 0, y = 0, z = 0;
    vec3d() = default;
    constexpr vec3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    vec3d operator+(const vec3d &o) const {
        return {x + o.x, y + o.y, z + o.z};
    }
};

// ------------------------------------------------------------ matrices -----
// D3DX conventions: row-major, row vectors (v * M), left-handed.

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

// --------------------------------------------------------------- colour ----
// RGBA as doubles, plus a three-channel sine colour cycler: each frame
// generate() advances phase by speed and sets
//   channel = base + (sin(phase) * 0.5 + 0.5) * amp.
// (0x50 bytes in the original; entities and sprites copy whole colours.)

struct colour {
    double r = 1, g = 1, b = 1, a = 1;
    float phase[3] = {0, 0, 0};
    float speed[3] = {0, 0, 0};
    float amp[3] = {0, 0, 0};
    float base[3] = {0, 0, 0};

    colour() = default;
    colour(double r_, double g_, double b_, double a_) : r(r_), g(g_), b(b_), a(a_) {}

    // Named colours keep the cycler state and take an alpha.
    colour &red(double alpha) {
        r = 1.0;
        g = 0.0;
        b = 0.0;
        a = alpha;
        return *this;
    }
    colour &orange(double alpha) {
        r = 1.0;
        g = 0.5;
        b = 0.0;
        a = alpha;
        return *this;
    }
    colour &green(double alpha) {
        r = 0.0;
        g = 1.0;
        b = 0.0;
        a = alpha;
        return *this;
    }
    colour &blue(double alpha) {
        r = 0.0;
        g = 0.0;
        b = 1.0;
        a = alpha;
        return *this;
    }
    colour &yellow(double alpha) {
        r = 1.0;
        g = 1.0;
        b = 0.0;
        a = alpha;
        return *this;
    }
    colour &purple(double alpha) {
        r = 0.3;
        g = 0.0;
        b = 0.7;
        a = alpha;
        return *this;
    }
    colour &white(double alpha) {
        a = neon::whiteAlpha(alpha, neon::WhiteAlphaRule::Unclamped);
        r = g = b = 1.0;
        return *this;
    }

    uint32_t packRGBA() const; // 0xRRGGBBAA
    uint32_t packARGB() const; // 0xAABBGGRR as stored in the original's vertex colours
    void unpackRGBA(uint32_t v);
    void unpackARGB(uint32_t v);

    void generate(); // advance the colour cycler one step

    // Returns this + (o - this) * t (no clamping), for r, g, b, a.
    colour lerp(const colour &o, float t) const;
    // Scales r, g, b and a.
    colour operator*(float s) const;
};

// Returns a + (b - a) * t for r, g, b, a (free-function variant of lerp).
colour clerp(const colour &a, const colour &b, float t);

// One of 14 flashing palettes keyed off a frame counter (the 8-bit style
// colour flicker used for text and pickups). Unknown types return opaque white.
colour getDynamicColour(int type, float alpha, int frame);

// ------------------------------------------------------------------ RNG ----

struct oxRNG {
    static double rand(double lo, double hi); // uniform in [lo, hi]
    static double rand(double hi);            // uniform in [0, hi]
    static double rand();                     // uniform in [0, 1]
    static int irand(double hi);              // truncated rand(hi)
};

} // namespace Neon_Internals
} // namespace NEON_GAME
