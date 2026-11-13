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

colour getDynamicColour(int type, float alpha, int frame) {
    // Original palette rules selected by this adapter, not a game-ID switch.
    static constexpr neon::PaletteRules rules{neon::WashRule::TriangularRainbow, 12, 4};
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

int oxRNG::irand(double hi) {
    return int((double(neonRand()) / 2147483647.0) * hi);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
