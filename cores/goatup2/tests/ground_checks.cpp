#include "ground.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__func__) + ": " + #x);                           \
    } while (false)
using namespace goatup2;
namespace {
template <class B, class V> B bits(V v) {
    B b;
    static_assert(sizeof(B) == sizeof(V));
    std::memcpy(&b, &v, sizeof b);
    return b;
}
float asFloat(const std::string &s) {
    const auto b = uint32_t(std::stoul(s, nullptr, 16));
    float v;
    std::memcpy(&v, &b, 4);
    return v;
}
double asDouble(const std::string &s) {
    const auto b = std::stoull(s, nullptr, 16);
    double v;
    std::memcpy(&v, &b, 8);
    return v;
}
struct Actor {
    neon::vec2d world{9, 10}, local{.125, -.25}, scale{-2, .75}, velocity{0, 0};
    float gravity = .0002f, hunger = .5f, zoom = 0, sink = .25f, jumpSpeed = 0;
    double impulse = 0, verticalImpulse = 0;
    bool resting = true;
    int32_t column = 50, row = 50, jump = 0;
    GoatCheckpointHistory checkpoints{{.125, -.25}, .0002f};
    GroundRewardActorView reward() {
        return {world, local, scale, gravity, hunger};
    }
    GroundActorView view() {
        return {reward(), local,   resting, column,    row,        zoom,
                sink,     impulse, jump,    jumpSpeed, checkpoints};
    }
};
struct Score {
    int32_t score = 100, total = 50, high = 1000, awards = 0, multiplier = 2;
    bool latch = false;
    GroundScoreView view() {
        return {score, total, high, awards, latch, multiplier};
    }
};
struct World {
    std::vector<uint32_t> working = std::vector<uint32_t>(10000), backup = working;
    TerrainLayout layout = calculateTerrainLayout(100, 100, 100, 100, {100, 100}, nullptr);
    MediumSharedState shared{.125f, .125f, .0625, 8, true};
    bool exit = true;
    uint32_t clock = 0;
    uint32_t &tile(int x, int y) {
        return physicalTerrainWord({working, backup}, x, y);
    }
};
struct Recorder : GroundOutput, MediumOutput {
    neon::DarwinRandom &rng;
    std::vector<GroundParticleBirth> births;
    std::vector<uint32_t> states, tiles;
    std::vector<uint64_t> draws, soundDraws;
    std::vector<int> sounds;
    std::string order;
    const uint32_t *observedTile = nullptr;
    explicit Recorder(neon::DarwinRandom &r) : rng(r) {}
    void adoptGroundParticle(const GroundParticleBirth &b) override {
        births.push_back(b);
        states.push_back(rng.state());
        draws.push_back(rng.draws());
        if (observedTile)
            tiles.push_back(*observedTile);
        order += (b.kind == GroundParticleBirth::Kind::Grass ? 'G' : 'N');
    }
    void playSample(int32_t sample, uint32_t gain, int32_t option) override {
        CHECK(gain == 32768 && option == 0);
        sounds.push_back(sample);
        soundDraws.push_back(rng.draws());
        order += 'A';
    }
    void adoptBubble(const BubbleBirth &) override {
        throw std::runtime_error("unexpected bubble in dry ground test");
    }
    void adoptSplash(const SplashBirth &) override {
        throw std::runtime_error("unexpected splash in dry ground test");
    }
};
struct Run {
    Actor a;
    World w;
    Score score;
    neon::DarwinRandom rng{1};
    Recorder output{rng};
    void ground() {
        interactWithGround(a.view(), {w.working, w.backup}, w.layout, w.clock, w.exit, w.shared,
                           score.view(), rng, output);
    }
};
void rewardWitnesses(const char *path) {
    std::ifstream in(path);
    CHECK(in.good());
    Actor a;
    Score score;
    std::unique_ptr<neon::DarwinRandom> rng;
    std::unique_ptr<Recorder> output;
    std::string token;
    unsigned cases = 0, checked = 0;
    while (in >> token) {
        if (token[0] == '#') {
            std::string rest;
            std::getline(in, rest);
            continue;
        }
        if (token == "case") {
            uint32_t seed;
            int suppressed, expectedSound;
            std::string g, h, expectedHunger;
            in >> seed >> g >> h >> suppressed >> expectedHunger >> expectedSound;
            CHECK(in.good());
            a.gravity = asFloat(g);
            a.hunger = asFloat(h);
            score = Score{};
            rng = std::make_unique<neon::DarwinRandom>(seed);
            output = std::make_unique<Recorder>(*rng);
            collectGroundReward(a.reward(), score.view(), {8, suppressed != 0}, *rng, *output);
            CHECK(bits<uint32_t>(a.hunger) == std::stoul(expectedHunger, nullptr, 16));
            CHECK(score.score == 102 && score.total == 52 && score.high == 1000 && !score.latch &&
                  score.awards == 0);
            CHECK(output->sounds ==
                  (suppressed ? std::vector<int>{} : std::vector<int>{expectedSound}));
            if (!suppressed)
                CHECK(output->soundDraws[0] == 1);
            ++cases;
        } else if (token == "birth") {
            unsigned i;
            uint32_t state;
            uint64_t draws;
            std::string x, y, sx, sy, g, radius, fade, angle, spin, colour;
            in >> i >> state >> draws >> x >> y >> sx >> sy >> g >> radius >> fade >> angle >>
                spin >> colour;
            CHECK(in.good());
            const auto &b = output->births.at(i);
            CHECK(output->states[i] == state && output->draws[i] == draws);
            CHECK(bits<uint64_t>(b.velocity.x) == std::stoull(x, nullptr, 16));
            CHECK(bits<uint64_t>(b.velocity.y) == std::stoull(y, nullptr, 16));
            CHECK(bits<uint64_t>(b.savedVelocity.x) == std::stoull(sx, nullptr, 16));
            CHECK(bits<uint64_t>(b.savedVelocity.y) == std::stoull(sy, nullptr, 16));
            CHECK(bits<uint32_t>(b.gravity) == std::stoul(g, nullptr, 16));
            CHECK(bits<uint32_t>(b.radius) == std::stoul(radius, nullptr, 16));
            CHECK(bits<uint32_t>(b.fadePerUpdate) == std::stoul(fade, nullptr, 16));
            CHECK(bits<uint32_t>(b.angle) == std::stoul(angle, nullptr, 16));
            CHECK(bits<uint32_t>(b.spin) == std::stoul(spin, nullptr, 16));
            CHECK(b.worldPosition.x == 9 && b.worldPosition.y == 10 && b.stagePosition.x == .125 &&
                  b.stagePosition.y == -.25);
            CHECK(b.savedDrawScale.x == -2 && b.savedDrawScale.y == .75 && b.opacity == 1);
            CHECK(b.tileInteractionMask == 0x440000 && b.ledgeResponse == 0);
            if (i < 4) {
                CHECK(b.kind == GroundParticleBirth::Kind::Grass && b.sprite == 2 &&
                      b.horizontalDrawScale == .3 && b.colourOverride);
                const auto green = (std::stoul(colour, nullptr, 16) >> 16) & 255U;
                CHECK(b.colourOverride->r == 0 && b.colourOverride->g == double(green) / 255. &&
                      b.colourOverride->b == 0 && b.colourOverride->a == 1);
            } else {
                CHECK(b.kind == GroundParticleBirth::Kind::Nom && b.sprite == 26 &&
                      b.horizontalDrawScale == 3 && !b.colourOverride);
                CHECK(b.savedVelocity.x == output->births[3].velocity.x &&
                      b.savedVelocity.y == output->births[3].velocity.y);
            }
            ++checked;
        } else if (token == "end") {
            uint32_t state;
            uint64_t draws;
            in >> state >> draws;
            CHECK(in.good());
            CHECK(rng->state() == state && rng->draws() == draws && draws == 34 &&
                  output->births.size() == 5);
        } else
            throw std::runtime_error("unexpected reward witness row");
    }
    CHECK(in.eof() && cases == 4 && checked == 20);
}
void collectionAndScore() {
    for (int sign : {-1, 1})
        for (uint32_t id : {4U, 5U, 92U, 93U}) {
            Run r;
            r.a.gravity = sign * .0002f;
            const int adjacentRow = 50 + (sign < 0 ? 1 : -1);
            auto &tile = r.w.tile(50, adjacentRow);
            tile = id;
            r.output.observedTile = &tile;
            r.ground();
            const uint32_t changed = id == 4    ? 0x800006
                                     : id == 5  ? 0x1800007
                                     : id == 92 ? 0x80005e
                                                : 0x80005f;
            CHECK(tile == changed && r.output.births.size() == 5 && r.rng.draws() == 34);
            CHECK(r.output.tiles ==
                  std::vector<uint32_t>(5, changed)); // write precedes all reward output
            CHECK(r.a.checkpoints.cursor == (id >= 92 ? 1 : 0));
            if (id >= 92)
                CHECK(r.a.checkpoints.latest().gravity == r.a.gravity);
        }
    {
        Run r;
        r.w.tile(50, 49) = 0x100003;
        r.ground();
        CHECK(r.w.tile(50, 49) == 0x800006); // raw lookup index 3 -> ID4
    }
    {
        Run r;
        r.a.resting = false;
        r.a.column = -99;
        r.a.row = -99;
        r.a.sink = 1;
        r.ground();
        CHECK(r.a.sink == .8f && r.w.exit && r.rng.draws() == 0); // no read or flag clear
    }
    {
        Run r;
        r.score.high = 100;
        r.w.shared.splashSoundSuppressed = false;
        r.w.tile(50, 49) = 4;
        r.ground();
        CHECK(r.score.high == 102 && r.score.awards == 1 && r.score.latch);
        CHECK(r.output.sounds.size() == 2 && r.output.sounds[0] == 70);
        CHECK(r.output.soundDraws == std::vector<uint64_t>({0, 1}));
        r.w.tile(50, 49) = 4;
        r.ground();
        CHECK(r.score.high == 104 && r.score.awards == 1);
    }
    {
        Run r;
        r.score.score = 0;
        r.score.high = 0;
        r.w.shared.splashSoundSuppressed = false;
        r.w.tile(50, 49) = 4;
        r.ground();
        CHECK(r.score.awards == 1 && r.score.latch && r.output.sounds.size() == 1 &&
              r.output.sounds[0] != 70);
    }
    {
        Run r;
        r.score.score = std::numeric_limits<int32_t>::max();
        r.score.total = r.score.score;
        r.score.multiplier = 1;
        r.w.tile(50, 49) = 4;
        r.ground();
        CHECK(r.score.score == std::numeric_limits<int32_t>::min());
        CHECK(r.score.total == r.score.score && r.score.high == 1000 && !r.score.latch);
    }
}
void surfaces() {
    for (int sign : {-1, 0, 1})
        for (bool protectedState : {false, true}) {
            Run r;
            r.a.gravity = sign * .0002f;
            r.a.zoom = protectedState ? 1.f : 0.f;
            r.w.tile(50, 50) = 89;
            r.w.shared.splashSoundSuppressed = false;
            r.ground();
            CHECK(r.a.jump == (protectedState ? 0 : 10) && r.a.resting == protectedState);
            if (!protectedState)
                CHECK(r.a.jumpSpeed == (sign < 0 ? .0108f : -.0108f) &&
                      r.output.sounds == std::vector<int>({68}));
        }
    for (uint32_t id : {0U, 25U, 26U}) {
        Run r;
        r.w.tile(50, 50) = id;
        r.ground();
        CHECK(r.w.exit == (id == 25));
    }
    for (int sign : {-1, 0, 1})
        for (uint32_t direction : {0U, 0x200000U, 0x400000U, 0x600000U}) {
            Run r;
            r.a.gravity = float(sign);
            const auto word = 0x40000 | direction | (10U << 24);
            r.w.tile(50, 50) = word;
            r.ground();
            const bool allowed =
                (!(direction & 0x200000) || sign >= 0) && (!(direction & 0x400000) || sign <= 0);
            CHECK(r.w.tile(50, 50) == (allowed ? ((word & 0xffffff) | (11U << 24)) : word));
            CHECK(r.a.sink == (allowed ? (sign < 0 ? -.171875f : .171875f) : 0.f));
        }
    for (uint32_t direction : {0U, 0x200000U}) {
        Run r;
        const uint32_t count = direction ? 63U : 32U;
        r.w.tile(50, 50) = 0x40000 | direction | (count << 24);
        r.w.tile(50, 49) = 92;
        r.ground();
        CHECK(r.w.tile(50, 50) == 0 && r.a.local.y == 0);
        CHECK(r.a.sink == (direction ? 1.f : .515625f));   // retained overflow count, not zero
        CHECK(r.a.checkpoints.latest().position.y == 0);   // checkpoint after position adjustment
        CHECK(r.output.births[0].stagePosition.y == -.25); // reward before adjustment
    }
    {
        Run r;
        r.w.layout.displayReciprocal.y = double(0x1.555556p-2f);
        r.w.layout.scale.y = 0x1.1745d1ffffffcp+4;
        r.w.tile(50, 50) = 0x240000 | (10U << 24);
        r.ground();
        CHECK(r.a.sink == 0x1.000002p0f); // source product grouping; reassociation gives 1.0f
    }
    for (uint32_t id : {12U, 13U}) {
        Run r;
        r.w.layout.cellSize.x = 7;
        r.w.tile(50, 50) = 0x200000 | id;
        r.ground();
        CHECK(r.a.impulse ==
              (id == 12 ? -.875 : .875)); // cached cell size, not product recomputation
        CHECK((id == 12 ? r.w.shared.downwardRamp : r.w.shared.upwardRamp) == .131f);
        CHECK((id == 12 ? r.w.shared.upwardRamp : r.w.shared.downwardRamp) == .125f);
    }
    {
        Run r;
        r.w.shared.upwardRamp = .899f;
        r.w.tile(50, 50) = 0x20000d;
        r.ground();
        CHECK(r.w.shared.upwardRamp == .905f);
        r.ground();
        CHECK(r.w.shared.upwardRamp == .905f);
    }
    {
        Run r;
        r.w.tile(50, 50) = 0x20000d;
        r.ground();
        bool hazard = false;
        uint32_t flags = 0, changed = 0;
        int age = 0;
        MediumActorView medium{
            true,   flags,     changed,   age,         r.a.velocity, r.a.verticalImpulse,
            hazard, r.a.world, r.a.local, r.a.gravity, r.a.scale.y,  1,
            -1};
        updateMedium(medium, r.w.shared, '/', r.rng, r.output);
        CHECK(r.w.shared.upwardRamp == .137f && r.a.verticalImpulse == 0x1.ad42c33333334p-11 &&
              r.rng.draws() == 0);
    }
    {
        Run r;
        r.a.zoom = 1;
        r.w.tile(50, 50) = 0x24000d;
        r.ground();
        CHECK(r.w.tile(50, 50) == 0x24000d && r.a.sink == .25f && r.a.impulse == 0);
    }
    {
        Run r;
        r.a.row = 0;
        bool rejected = false;
        try {
            r.ground();
        } catch (const std::out_of_range &) {
            rejected = true;
        }
        CHECK(rejected && r.rng.draws() == 0 && r.output.births.empty() && r.score.score == 100);
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        rewardWitnesses(argv[1]);
        collectionAndScore();
        surfaces();
        std::cout << "Ground checks passed: collection/RNG births, checkpoints, spring/exit, "
                     "sinking and shared conveyors. No live actor claim.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
