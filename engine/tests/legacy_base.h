// neon_base.h - foundation types of the Neon engine (Minotron's copy):
// 2D/3D vectors, 4x4 matrices with the D3DX helpers the engine was written
// against, the colour type (with its built-in colour cycler), RNG helpers.
//
// Each game ships its own engine copy, so everything lives in a per-core
// namespace (NEON_GAME) to keep cores linkable into one binary.
#pragma once

#include <cmath>
#include <cstdint>

#ifndef NEON_GAME
#define NEON_GAME minotron
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
        double s = std::sin(a), c = std::cos(a), ox = x;
        x = s * y + c * ox;
        y = y * c - ox * s;
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

struct D3DXVECTOR3 {
    float x, y, z;
};
struct D3DXVECTOR4 {
    float x, y, z, w;
};

template <typename T> struct matrix4 {
    T m[4][4];
    T *data() {
        return &m[0][0];
    }
    const T *data() const {
        return &m[0][0];
    }
};
using matrix4f = matrix4<float>;

matrix4f operator*(const matrix4f &a, const matrix4f &b);

void D3DXMatrixIdentity(matrix4f *out);
void D3DXMatrixTranspose(matrix4f *out, const matrix4f *in);
void D3DXMatrixOrthoLH(matrix4f *out, float w, float h, float zn, float zf);
void D3DXMatrixPerspectiveFovLH(matrix4f *out, float fovy, float aspect, float zn, float zf);
void D3DXMatrixLookAtLH(matrix4f *out, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at,
                        const D3DXVECTOR3 *up);
void D3DXMatrixRotationX(matrix4f *out, float a);
void D3DXMatrixRotationY(matrix4f *out, float a);
void D3DXMatrixRotationZ(matrix4f *out, float a);
void D3DXMatrixRotationAxis(matrix4f *out, const D3DXVECTOR3 *axis, float a);
void D3DXMatrixScaling(matrix4f *out, float sx, float sy, float sz);
void D3DXMatrixTranslation(matrix4f *out, float tx, float ty, float tz);
void D3DXVec3Transform(D3DXVECTOR4 *out, const D3DXVECTOR3 *v, const matrix4f *m);

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
        a = alpha < 0.0 ? 0.0 : (alpha > 1.0 ? 1.0 : alpha);
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
    static int irand(double hi);              // truncated rand(hi)
};

} // namespace Neon_Internals
} // namespace NEON_GAME
