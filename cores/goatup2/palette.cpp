#include "palette.h"
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace goatup2 {
uint32_t sampleColourRamp(float phase, ColourRampView ramp) {
    if (!std::isfinite(phase) || phase < 0 || phase > 1 || !ramp.stops || ramp.segments <= 0)
        throw std::invalid_argument("unsupported particle colour ramp");
    const float scaled = float(double(phase) * (double(float(ramp.segments - 1)) + .99));
    // The particle palettes are small; reject a rounded endpoint that would
    // require an extra stop rather than making an unchecked host read.
    if (scaled >= float(ramp.segments))
        throw std::out_of_range("particle colour ramp endpoint");
    const int32_t index = int32_t(scaled);
    const float fraction = scaled - float(index);
    const auto low = ramp.stops[index], high = ramp.stops[index + 1];
    uint32_t packed = 0;
    for (unsigned shift : {24U, 16U, 8U, 0U}) {
        const double a = double((low >> shift) & 255U) / 255.;
        const double b = double((high >> shift) & 255U) / 255.;
        const double delta = (b - a) * double(fraction);
        const double value = (a + delta) * 255.;
        packed |= (uint32_t(int32_t(value)) & 255U) << shift;
    }
    return packed;
}

neon::PaletteChannels unpackParticleColour(uint32_t packed) {
    return {double(packed >> 24) / 255., double((packed >> 16) & 255U) / 255.,
            double((packed >> 8) & 255U) / 255., double(packed & 255U) / 255.};
}

ColourRampView impactColourRamp() {
    // Original data 0x7e770. The last white stop closes the three segments.
    static constexpr uint32_t stops[] = {0xffffffffU, 0xff0000ffU, 0xffff00ffU, 0xffffffffU};
    return {stops, 3};
}
ColourRampView headbuttColourRamp() {
    // Source 0x7beb4 count=11. The sampled extra word is the following table's
    // header (14), not an invented wrap-to-red stop. Preserve the actual read.
    static constexpr uint32_t stops[] = {0xff0000ffU, 0xff8000ffU, 0xffff00ffU, 0x80ff00ffU,
                                         0x00ff00ffU, 0x00ff80ffU, 0x00ffffffU, 0x0080ffffU,
                                         0x0000ffffU, 0x000080ffU, 0x008040ffU, 0x0000000eU};
    return {stops, 11};
}
} // namespace goatup2
