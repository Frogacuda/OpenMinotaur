#include "particles.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace {
using namespace goatup2;
template <class Bits, class Value> Bits bits(Value v) {
    Bits result;
    static_assert(sizeof(Bits) == sizeof(Value));
    std::memcpy(&result, &v, sizeof v);
    return result;
}
float asFloat(const std::string &s) {
    auto u = uint32_t(std::stoul(s, nullptr, 16));
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
double asDouble(const std::string &s) {
    auto u = std::stoull(s, nullptr, 16);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
struct Recorder : ContactParticleOutput {
    neon::DarwinRandom &rng;
    std::vector<StageParticleBirth> births;
    std::vector<neon::colour> colours;
    std::vector<uint64_t> draws;
    std::vector<uint32_t> states;
    std::string order;
    int sounds = 0;
    uint32_t gain = 0;
    explicit Recorder(neon::DarwinRandom &r) : rng(r) {}
    void record(const StageParticleBirth &b, char kind) {
        births.push_back(b);
        draws.push_back(rng.draws());
        states.push_back(rng.state());
        order += kind;
    }
    void adoptTileFragment(const TileFragmentBirth &b) override {
        record(b, 'F');
        colours.push_back(b.colour);
    }
    void adoptImpact(const StageParticleBirth &b) override {
        record(b, 'I');
    }
    void playSample(int32_t sample, uint32_t level, int32_t option) override {
        CHECK(sample == 43 && option == 0 && rng.draws() == 0 && order.empty());
        ++sounds;
        gain = level;
        order += 'S';
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        // The shared value copy preserves all 80 supplied bytes. This does not
        // accept its host constructor defaults or generate() as source behavior.
        static_assert(sizeof(neon::colour) == 80 && std::is_trivially_copyable_v<neon::colour>);
        std::array<neon::colour, 32> palette;
        for (unsigned n = 0; n < palette.size(); ++n) {
            auto &c = palette[n];
            c.r = n + .125;
            c.g = n + .25;
            c.b = n + .5;
            c.a = n + .75;
            for (int k = 0; k < 3; ++k) {
                c.phase[k] = float(n * 16 + k + 1);
                c.speed[k] = float(n * 16 + k + 4);
                c.amp[k] = float(n * 16 + k + 7);
                c.base[k] = float(n * 16 + k + 10);
            }
        }
        const auto initialPalette = palette;
        neon::vec2d position{1.25, -2.5}, scale{-2, 3};
        std::ifstream in(argv[1]);
        CHECK(in.good());
        std::unique_ptr<neon::DarwinRandom> rng;
        std::unique_ptr<Recorder> out;
        std::vector<uint32_t> stops;
        std::string token, kind;
        int count = 0, suppressed = 0, volume = 0;
        uint32_t sprite = 0;
        unsigned cases = 0, births = 0, samples = 0;
        while (in >> token) {
            if (token[0] == '#') {
                std::string rest;
                std::getline(in, rest);
                continue;
            }
            if (token == "case") {
                uint32_t seed;
                std::string gravity;
                in >> kind >> count >> seed >> gravity >> suppressed >> volume >> sprite;
                CHECK(in.good());
                rng = std::make_unique<neon::DarwinRandom>(seed);
                out = std::make_unique<Recorder>(*rng);
                ParticleSourceView actor{position, scale, .125f, asFloat(gravity)};
                ParticleSoundSettings sound{volume, suppressed != 0};
                if (kind == "fragment")
                    emitTileFragments(actor, sound, palette, count, sprite, *rng, *out);
                else {
                    CHECK(kind == "impact");
                    emitImpactParticles(actor, sound, count, *rng, *out);
                }
                CHECK(out->sounds == (suppressed ? 0 : 1));
                if (!suppressed)
                    CHECK(out->gain == (uint32_t(volume) << 12));
                CHECK(position.x == 1.25 && position.y == -2.5 && scale.x == -2 && scale.y == 3);
                CHECK(std::memcmp(palette.data(), initialPalette.data(), sizeof palette) == 0);
                ++cases;
            } else if (token == "birth") {
                unsigned index;
                uint32_t state;
                uint64_t draws;
                std::string radius, fade, gravity, spin, rate, x, y;
                in >> index >> state >> draws >> radius >> fade >> gravity >> spin >> rate >> x >>
                    y;
                CHECK(in.good());
                CHECK(index < out->births.size());
                const auto &b = out->births[index];
                CHECK(out->states[index] == state && out->draws[index] == draws);
                CHECK(bits<uint32_t>(b.radius) == std::stoul(radius, nullptr, 16));
                CHECK(bits<uint32_t>(b.fadePerUpdate) == std::stoul(fade, nullptr, 16));
                CHECK(bits<uint32_t>(b.gravity) == std::stoul(gravity, nullptr, 16));
                CHECK(bits<uint32_t>(b.spin) == std::stoul(spin, nullptr, 16));
                CHECK(bits<uint32_t>(b.phaseRate) == std::stoul(rate, nullptr, 16));
                CHECK(std::fabs(b.velocity.x - asDouble(x)) <= 2e-16 &&
                      std::fabs(b.velocity.y - asDouble(y)) <= 2e-16);
                CHECK(b.savedVelocity.x == b.velocity.x && b.savedVelocity.y == b.velocity.y);
                CHECK(b.savedDrawScale.x == 1 && b.savedDrawScale.y == 1 && b.opacity == 1 &&
                      b.phase == 0);
                CHECK(b.tileInteractionMask == 0x440000 && b.ledgeResponse == 0 &&
                      b.projectileContactGate == 0);
                CHECK(b.solidCollisionEnabled == (kind == "impact"));
                if (kind == "fragment") {
                    CHECK(!b.colourRamp && b.sprite == int32_t(sprite & 65535U));
                    CHECK(b.stagePosition.x == 1.25 && b.stagePosition.y == -2.5);
                    const auto &expected = palette[(sprite & 0x1fffffU) >> 16];
                    CHECK(std::memcmp(&out->colours[index], &expected, 80) == 0);
                } else {
                    CHECK(b.colourRamp && b.colourRamp->segments == 3 && b.sprite == 51);
                    CHECK(b.colourRamp->stops[0] == 0xffffffff &&
                          b.colourRamp->stops[3] == 0xffffffff);
                    CHECK(b.stagePosition.x == 1.5 && b.stagePosition.y == -2.515);
                }
                ++births;
            } else if (token == "end") {
                uint32_t state;
                uint64_t draws;
                in >> state >> draws;
                CHECK(in.good());
                CHECK(rng->state() == state && rng->draws() == draws);
                CHECK(out->births.size() == std::size_t(count > 0 ? count : 0));
                CHECK(out->order ==
                      std::string(suppressed ? "" : "S") +
                          std::string(out->births.size(), kind == "fragment" ? 'F' : 'I'));
            } else if (token == "ramp") {
                int segments;
                in >> segments;
                CHECK(segments > 0);
                stops.clear();
                for (int i = 0; i <= segments; ++i) {
                    std::string colour;
                    in >> colour;
                    stops.push_back(uint32_t(std::stoul(colour, nullptr, 16)));
                }
            } else if (token == "colour") {
                std::string phase, expected;
                in >> phase >> expected;
                CHECK(in.good());
                CHECK(sampleColourRamp(asFloat(phase), {stops.data(), int32_t(stops.size() - 1)}) ==
                      std::stoul(expected, nullptr, 16));
                ++samples;
            } else
                throw std::runtime_error("unexpected witness row: " + token);
        }
        CHECK(in.eof() && cases == 12 && births == 68 && samples == 207);
        const auto rgba = unpackParticleColour(0x12345678);
        CHECK(rgba.r == 18. / 255 && rgba.g == 52. / 255 && rgba.b == 86. / 255 &&
              rgba.a == 120. / 255);
        // This phase produces exactly 3/128 after source float narrowing.
        // Normalized interpolation truncates 32.99999999999999 to 32;
        // direct byte interpolation instead gives 33. Keep the source rule.
        const uint32_t boundaryStops[] = {0x1e0000ff, 0x9e0000ff};
        CHECK(sampleColourRamp(0x1.83e0f8p-6f, {boundaryStops, 1}) == 0x200000ff);
        for (float invalid : {-.01f, 1.01f, std::numeric_limits<float>::quiet_NaN()}) {
            bool rejected = false;
            try {
                sampleColourRamp(invalid, impactColourRamp());
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            CHECK(rejected);
        }
        std::cout << "Particle witnesses passed: 12 cases / 68 births, RNG and colour copy, 207 "
                     "ramp samples. No live construction claim.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
