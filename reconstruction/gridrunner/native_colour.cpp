#include "native_colour.h"
#include "neon/foundation_rules.h"
#include <cmath>
namespace gr::reconstruction::ios {
void Rgba::unpack(uint32_t p) {
    r = double(p >> 24) / 255.;
    g = double((p >> 16) & 255) / 255.;
    b = double((p >> 8) & 255) / 255.;
    a = double(p & 255) / 255.;
}
void Rgba::white(double alpha) {
    r = g = b = 1;
    a = neon::whiteAlpha(alpha, neon::WhiteAlphaRule::ClampUnit);
}
void Rgba::generate() {
    for (size_t i = 0; i < 3; ++i)
        phase[i] += speed[i];
    // sinf rounds its result before the double multiply/add. Do not wrap the
    // phase, clamp output, advance alpha, or silently call this on every tint.
    r = double(base[0]) + (double(std::sin(phase[0])) * .5 + .5) * double(amplitude[0]);
    g = double(base[1]) + (double(std::sin(phase[1])) * .5 + .5) * double(amplitude[1]);
    b = double(base[2]) + (double(std::sin(phase[2])) * .5 + .5) * double(amplitude[2]);
}
Rgba dynamicColour(int32_t type, float alpha, uint32_t f) {
    static constexpr neon::PaletteRules rules{neon::WashRule::TriangularRainbow, 12, 4};
    const auto channels = neon::paletteColour(type, alpha, f, rules);
    Rgba c;
    c.r = channels.r;
    c.g = channels.g;
    c.b = channels.b;
    c.a = channels.a;
    return c;
}
} // namespace gr::reconstruction::ios
