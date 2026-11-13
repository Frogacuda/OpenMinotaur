#include "world.h"
#include "original_tables.h"
#include "neon/foundation_rules.h"
namespace superoxwars {
Colour mixColour(Colour a, Colour b, float t) {
    const auto channel = [&](double low, double high) {
        return double(int(((high - low) * double(t) + low) * 255.) & 255) / 255.;
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}
Colour sequenceColour(float phase, const uint32_t *sequence) {
    const double length = double(float(sequence[0] - 1u)) + .99;
    const float scaled = float(double(phase) * length);
    const int index = int(scaled);
    const double fraction = double(float(scaled - float(index)));
    const uint32_t a = sequence[index + 1], b = sequence[index + 2];
    uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const double low = double((a >> shift) & 255) / 255.,
                     high = double((b >> shift) & 255) / 255.;
        result |= uint32_t(int(((high - low) * fraction + low) * 255.) & 255) << shift;
    }
    return unpack(result);
}
void World::updatePalette() {
    // theGame::run 418c4: spectrum, five dynamic profiles, eight literal cycles.
    palette[0] = sequenceColour(float((ticks & 31) << 1) / 63.f, original::SpectrumSequence.data());
    constexpr int types[] = {8, 5, 0, 4, 2};
    for (int n = 0; n < 5; ++n) {
        const auto c = neon::paletteColour(types[n], 1.f, uint32_t(ticks),
                                           {neon::WashRule::TriangularRainbow, 12, 4});
        palette[size_t(n + 1)] = {c.r, c.g, c.b, c.a};
    }
    for (int n = 0; n < 8; ++n)
        palette[size_t(n + 6)] = unpack(original::FlashSequences[size_t(n)][size_t(ticks & 15)]);
}
} // namespace superoxwars
