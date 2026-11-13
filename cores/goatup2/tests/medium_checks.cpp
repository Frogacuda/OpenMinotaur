#include "medium.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace {
using namespace goatup2;
template <class Bits, class Value> Bits bits(Value value) {
    Bits result;
    static_assert(sizeof(Bits) == sizeof(Value));
    std::memcpy(&result, &value, sizeof result);
    return result;
}
float asFloat(const std::string &value) {
    const uint32_t u = uint32_t(std::stoul(value, nullptr, 16));
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
double asDouble(const std::string &value) {
    const uint64_t u = std::stoull(value, nullptr, 16);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
struct Actor {
    bool player = true, hazard = false;
    uint32_t flags = 0, changed = 99;
    int32_t age = 0;
    neon::vec2d velocity{1, -2}, world{9, 10}, local{1.25, -2.5};
    double impulse = .123456789;
    float gravity = .006f, damping = .5f;
    double drawScale = 1;
    int32_t chance = -1;
    MediumActorView view() {
        return {player, flags, changed, age,       velocity, impulse, hazard,
                world,  local, gravity, drawScale, damping,  chance};
    }
};
struct Recorder final : MediumOutput {
    neon::DarwinRandom &random;
    std::vector<BubbleBirth> bubbles;
    std::vector<SplashBirth> splashes;
    std::vector<char> order;
    std::vector<uint64_t> draws;
    std::vector<uint32_t> states;
    int sounds = 0;
    uint32_t gain = 0;
    explicit Recorder(neon::DarwinRandom &r) : random(r) {}
    void adoptBubble(const BubbleBirth &b) override {
        bubbles.push_back(b);
        order.push_back('B');
    }
    void adoptSplash(const SplashBirth &b) override {
        splashes.push_back(b);
        order.push_back('P');
        draws.push_back(random.draws());
        states.push_back(random.state());
    }
    void playSample(int32_t index, uint32_t level, int32_t option) override {
        CHECK(index == 25 && option == 0);
        gain = level;
        ++sounds;
        order.push_back('S');
    }
};
void scalarAndOrder() {
    Actor a;
    MediumSharedState shared{.1f, .1f, .0625, 8, false};
    neon::DarwinRandom rng(1);
    Recorder out(rng);
    a.flags = 0x80;
    a.age = 80;
    updateMedium(a.view(), shared, 0xab00002bU, rng, out);
    CHECK(a.flags == 0x81 && a.changed == 1 && a.age == 0);
    CHECK(a.velocity.x == .5 && a.velocity.y == -1 && rng.draws() == 152);
    CHECK(out.bubbles.empty() && out.splashes.size() == 50 && out.sounds == 1 && out.gain == 32768);
    CHECK(out.order.front() == 'S' && out.order.back() == 'P');
    updateMedium(a.view(), shared, '+', rng, out);
    CHECK(a.age == 1 && a.changed == 0 && rng.draws() == 153 && out.splashes.size() == 50);
    CHECK(a.velocity.x == .25 && a.velocity.y == -.5); // repeated visits are not deduplicated
    updateMedium(a.view(), shared, ',', rng, out);
    CHECK(a.hazard && a.flags == 0x80 && a.changed == 1 && rng.draws() == 153);

    a = Actor{};
    a.age = 399;
    updateMedium(a.view(), shared, ' ', rng, out);
    CHECK(a.age == 400);
    updateMedium(a.view(), shared, ' ', rng, out);
    CHECK(a.age == 400);
    a.age = 900;
    updateMedium(a.view(), shared, ' ', rng, out);
    CHECK(a.age == 900); // no clamp of caller state
    a.age = -11;
    updateMedium(a.view(), shared, ' ', rng, out);
    CHECK(a.age == -10); // no assumed zero initialization
    a.age = 79;
    updateMedium(a.view(), shared, '+', rng, out);
    CHECK(a.age == 0 && out.splashes.size() == 50);

    a = Actor{};
    shared = {.125f, .125f, .0625, 8, true};
    updateMedium(a.view(), shared, '.', rng, out);
    CHECK(a.impulse == -.00078125 && shared.downwardRamp == .131f && shared.upwardRamp == .125f);
    updateMedium(a.view(), shared, '/', rng, out);
    CHECK(a.impulse == .00078125 && shared.upwardRamp == .131f);
    shared.downwardRamp = .199f;
    shared.cachedVerticalCellSize = 1;
    a.impulse = .123456789;
    updateMedium(a.view(), shared, '.', rng, out);
    CHECK(a.impulse == 0x1.f9add4p-4 &&
          shared.downwardRamp == .205f); // old impulse narrowed through float
    a.player = false;
    a.hazard = false;
    a.impulse = .75;
    const float oldRamp = shared.downwardRamp;
    updateMedium(a.view(), shared, '.', rng, out);
    CHECK(a.impulse == .75 && shared.downwardRamp == oldRamp);
    updateMedium(a.view(), shared, ',', rng, out);
    CHECK(!a.hazard);
}
void bubbleAndEntryOrder() {
    Actor a;
    a.chance = 4096;
    a.gravity = 0;
    a.drawScale = -1;
    a.age = 80;
    MediumSharedState shared{0, 0, .05, 8, false};
    neon::DarwinRandom rng(1);
    Recorder out(rng);
    updateMedium(a.view(), shared, '+', rng, out);
    CHECK(out.bubbles.size() == 1 && out.splashes.size() == 50 && rng.draws() == 153);
    CHECK(out.order[0] == 'B' && out.order[1] == 'S' && out.order[2] == 'P');
    const auto &b = out.bubbles[0];
    CHECK(b.gravity == -.007f && b.mediumFlags == 1 && b.changedMediumFlags == 1);
    CHECK(b.worldPosition.x == 9 && b.worldPosition.y == 10 && b.stagePosition.x == 1.25 &&
          b.stagePosition.y == -2.5);
    CHECK(b.sprite == 151 && b.bubbleThreshold == -1 && !b.restingContact &&
          b.savedDrawScale.x == 1);
    a.player = false;
    a.flags = 1;
    a.chance = 4096;
    a.drawScale = 0;
    neon::DarwinRandom rng2(1);
    Recorder out2(rng2);
    updateMedium(a.view(), shared, '+', rng2, out2);
    CHECK(out2.bubbles[0].gravity == .007f && rng2.draws() == 2);
}
void births(const char *path) {
    std::ifstream in(path);
    CHECK(in.good());
    std::string token;
    Actor a;
    MediumSharedState shared{0, 0, .05, 8, false};
    std::unique_ptr<neon::DarwinRandom> rng;
    std::unique_ptr<Recorder> out;
    unsigned checked = 0, cases = 0, bubbleCases = 0;
    int count = 0;
    uint32_t sprite = 0;
    while (in >> token) {
        if (token[0] == '#') {
            std::string rest;
            std::getline(in, rest);
            continue;
        }
        if (token == "case") {
            uint32_t seed;
            std::string gravity;
            int suppressed;
            in >> count >> seed >> gravity >> suppressed >> shared.sampleVolume >> sprite;
            a.gravity = asFloat(gravity);
            shared.splashSoundSuppressed = suppressed != 0;
            rng = std::make_unique<neon::DarwinRandom>(seed);
            out = std::make_unique<Recorder>(*rng);
            emitSplash(a.view(), shared, count, sprite, *rng, *out);
            ++cases;
            CHECK(out->sounds == (suppressed ? 0 : 1));
            if (!suppressed)
                CHECK(out->gain == (uint32_t(shared.sampleVolume) << 12));
        } else if (token == "birth") {
            unsigned index;
            uint32_t state;
            uint64_t draws;
            std::string blue, fade, g, spin, rate, x, y;
            in >> index >> state >> draws >> blue >> fade >> g >> spin >> rate >> x >> y;
            CHECK(index < out->splashes.size());
            const auto &b = out->splashes[index];
            CHECK(out->states[index] == state && out->draws[index] == draws);
            CHECK(bits<uint64_t>(b.colour.b) == std::stoull(blue, nullptr, 16));
            CHECK(bits<uint32_t>(b.fadePerUpdate) == std::stoul(fade, nullptr, 16));
            CHECK(bits<uint32_t>(b.gravity) == std::stoul(g, nullptr, 16));
            CHECK(bits<uint32_t>(b.spin) == std::stoul(spin, nullptr, 16));
            CHECK(bits<uint32_t>(b.phaseRate) == std::stoul(rate, nullptr, 16));
            CHECK(std::fabs(b.velocity.x - asDouble(x)) <= 2e-16 &&
                  std::fabs(b.velocity.y - asDouble(y)) <= 2e-16);
            CHECK(b.savedVelocity.x == b.velocity.x && b.savedVelocity.y == b.velocity.y);
            CHECK(b.stagePosition.x == 1.25 && b.stagePosition.y == -2.5 &&
                  b.sprite == int32_t(sprite & 65535U));
            CHECK(b.colour.r == 0 && b.colour.g == 0 && b.colour.a == 1 && b.opacity == 1 &&
                  b.radius == .01f);
            CHECK(b.phase == 0 && !b.colourRamp && b.tileInteractionMask == 0x440000U);
            CHECK(b.ledgeResponse == 0 && b.projectileContactGate == 0 && !b.solidCollisionEnabled);
            ++checked;
        } else if (token == "end") {
            uint32_t state;
            uint64_t draws;
            in >> state >> draws;
            CHECK(rng->state() == state && rng->draws() == draws);
            CHECK(out->splashes.size() == std::size_t(count > 0 ? count : 0));
        } else if (token == "bubble") {
            uint32_t seed, flags, state;
            std::string g, drawScale, radius, childGravity;
            in >> seed >> g >> drawScale >> flags >> radius >> childGravity >> state;
            Actor childSource;
            childSource.player = false;
            childSource.flags = flags;
            childSource.gravity = asFloat(g);
            childSource.drawScale = asDouble(drawScale);
            childSource.chance = 4096;
            neon::DarwinRandom bubbleRandom(seed);
            Recorder bubbleOutput(bubbleRandom);
            updateMedium(childSource.view(), shared, '+', bubbleRandom, bubbleOutput);
            CHECK(bubbleOutput.bubbles.size() == 1 && bubbleRandom.draws() == 2 &&
                  bubbleRandom.state() == state);
            const auto &b = bubbleOutput.bubbles[0];
            CHECK(bits<uint32_t>(b.radius) == std::stoul(radius, nullptr, 16));
            CHECK(bits<uint32_t>(b.gravity) == std::stoul(childGravity, nullptr, 16));
            CHECK(b.mediumFlags == (flags | 1U) &&
                  b.changedMediumFlags == ((flags & 1U) ? 0U : 1U));
            CHECK(bubbleOutput.splashes.empty() && bubbleOutput.sounds == 0);
            ++bubbleCases;
        } else
            throw std::runtime_error("unknown medium witness row");
        CHECK(bool(in));
    }
    CHECK(cases == 6 && checked == 56 && bubbleCases == 4);
}
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        scalarAndOrder();
        bubbleAndEntryOrder();
        births(argv[1]);
        std::cout << "Medium scalar/birth witnesses passed: entry/current/damping, output order, "
                     "shared RNG, "
                     "6 splash cases / 56 particles and 4 bubble births. Actor construction and "
                     "movement integration remain separate.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
