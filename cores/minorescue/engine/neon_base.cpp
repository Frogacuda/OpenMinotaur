#include "neon_base.h"

namespace NEON_GAME {
namespace Neon_Internals {

// ------------------------------------------------------------- random -----

static uint32_t gRandState = 1;

void neonSrand(uint32_t seed) {
    gRandState = seed;
}

// Park-Miller minimal standard generator, as in BSD/iOS libc rand():
// x = 16807 * x mod (2^31 - 1), computed with Schrage's method.
int neonRand() {
    int32_t x = int32_t(gRandState % 0x7ffffffe) + 1; // keep x in [1, 2^31-2]
    int32_t hi = x / 127773, lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    gRandState = uint32_t(x - 1); // libc stores and returns x - 1
    return x - 1;
}

// --------------------------------------------------------------- colour ----

static uint32_t to8(double v) {
    return uint32_t(int(v * 255.0)) & 0xff;
}

uint32_t colour::packRGBA() const {
    return (to8(r) << 24) | (to8(g) << 16) | (to8(b) << 8) | to8(a);
}

uint32_t colour::packARGB() const {
    return (to8(a) << 24) | (to8(b) << 16) | (to8(g) << 8) | to8(r);
}

void colour::unpackRGBA(uint32_t v) {
    r = (v >> 24) / 255.0;
    g = ((v >> 16) & 0xff) / 255.0;
    b = ((v >> 8) & 0xff) / 255.0;
    a = (v & 0xff) / 255.0;
}

void colour::unpackARGB(uint32_t v) {
    a = (v >> 24) / 255.0;
    b = ((v >> 16) & 0xff) / 255.0;
    g = ((v >> 8) & 0xff) / 255.0;
    r = (v & 0xff) / 255.0;
}

void colour::generate() {
    for (int i = 0; i < 3; i++)
        phase[i] += speed[i];
    r = base[0] + (std::sin(phase[0]) * 0.5 + 0.5) * amp[0];
    g = base[1] + (0.5 + std::sin(phase[1]) * 0.5) * amp[1];
    b = base[2] + (0.5 + std::sin(phase[2]) * 0.5) * amp[2];
}

colour colour::lerp(const colour &o, float t) const {
    colour c = *this;
    double k = t;
    c.r = r + (o.r - r) * k;
    c.g = g + k * (o.g - g);
    c.b = b + k * (o.b - b);
    c.a = a + k * (o.a - a);
    return c;
}

colour colour::operator*(float s) const {
    colour c = *this;
    double k = s;
    c.r = r * k;
    c.g = k * g;
    c.b = k * b;
    c.a = k * a;
    return c;
}

colour clerp(const colour &a, const colour &b, float t) {
    return a.lerp(b, t);
}

colour getDynamicColour(int type, float alpha, int frame) {
    // Original palette rules selected by this adapter, not a game-ID switch.
    static constexpr neon::PaletteRules rules{neon::WashRule::PastelRamp, 3, 8};
    const auto channels = neon::paletteColour(type, alpha, uint32_t(frame), rules);
    return colour(channels.r, channels.g, channels.b, channels.a);
}

// ------------------------------------------------------------------ RNG ----

double oxRNG::rand(double lo, double hi) {
    return lo + (double(neonRand()) / 2147483647.0) * (hi - lo);
}

double oxRNG::rand(double hi) {
    return (double(neonRand()) / 2147483647.0) * hi;
}

double oxRNG::rand() {
    return double(neonRand()) / 2147483647.0;
}

int oxRNG::irand(double hi) {
    return int((double(neonRand()) / 2147483647.0) * hi);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
