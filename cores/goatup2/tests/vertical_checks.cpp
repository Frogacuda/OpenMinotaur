// Source-rule setups using actual terrain, medium and effect implementations.
// No original-execution or live constructor claim.
#include "vertical.h"
#include "player_history.h"
#include <cmath>
#include <iostream>
#include <limits>
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
struct Actor {
    neon::vec2d local{0, 0}, world{9, 10}, velocity{0, 2}, scale{1, 1};
    bool player = false, hazard = false, resting = false, landed = false, vertical = false,
         opposite259 = false, opposite25a = false;
    uint32_t flags = 0, changed = 0, motion = 0, mask = 0;
    int32_t age = 100, chance = -1, column = -1, row = -1, forced = 0;
    float gravity = 1, buoyancy = -.7f, elasticity = .5f, effective = 0, radius = 1, damping = 1;
    double impulse = 0, travel = 0;
    StageContactPolicy policy;
    VerticalActorView view() {
        MediumActorView medium{player, flags, changed, age,     velocity, impulse, hazard,
                               world,  local, gravity, scale.y, damping,  chance};
        return {medium,      local,       scale,  radius, buoyancy, elasticity, effective,
                travel,      policy,      motion, mask,   resting,  landed,     vertical,
                opposite259, opposite25a, column, row,    forced};
    }
};
struct World {
    std::vector<uint32_t> working = std::vector<uint32_t>(10000), backup = working;
    TerrainBounds bounds{};
    TerrainLayout layout = calculateTerrainLayout(100, 100, 100, 100, {100, 100}, &bounds);
    uint32_t clock = 0;
    TerrainCursor cursor{-9, -9};
    std::array<neon::colour, 32> palette;
    MediumSharedState medium{0, 0, 1, 8, true};
    VerticalTerrainView view() {
        return {working, backup, layout, bounds, clock, cursor, palette};
    }
    uint32_t &tile(int x, int y) {
        return working[y * 100 + x];
    }
};
struct Recorder : MediumOutput, ContactParticleOutput {
    struct Birth {
        char kind;
        neon::vec2d position;
        uint32_t contactTile;
        int32_t sprite;
    };
    World &world;
    std::vector<Birth> births;
    std::vector<int> sounds;
    std::string order;
    unsigned bubbles = 0, splashes = 0;
    explicit Recorder(World &w) : world(w) {}
    void adoptBubble(const BubbleBirth &) override {
        ++bubbles;
        order += 'B';
    }
    void adoptSplash(const SplashBirth &) override {
        ++splashes;
        order += 'S';
    }
    void save(char kind, const StageParticleBirth &b) {
        births.push_back(
            {kind, b.stagePosition, world.tile(world.cursor.column, world.cursor.row), b.sprite});
        order += kind;
    }
    void adoptTileFragment(const TileFragmentBirth &b) override {
        save('F', b);
    }
    void adoptImpact(const StageParticleBirth &b) override {
        save('I', b);
    }
    void playSample(int32_t sample, uint32_t gain, int32_t option) override {
        CHECK(gain == 32768 && option == 0);
        sounds.push_back(sample);
        order += 'A';
    }
};
struct Run {
    World w;
    Actor a;
    neon::DarwinRandom random{1};
    Recorder output{w};
    void vertical() {
        moveVertical(a.view(), w.view(), w.medium, random, output, output);
    }
    void falling() {
        applyFalling(a.view(), w.view(), w.medium, random, output, output);
    }
};
int postRow(double velocity) {
    return velocity < 0 ? 46 : 53;
}
int firstRow(double velocity) {
    return velocity < 0 ? 48 : 51;
}

void simpleContacts() {
    for (int sign : {-1, 1}) {
        Run r;
        r.a.velocity.y = sign * 2.;
        r.a.gravity = float(sign);
        r.vertical();
        CHECK(r.a.local.y == sign * 2. && r.a.travel == 2 && r.a.age == 101);
        CHECK(!r.a.landed && !r.a.vertical && r.output.births.empty());
        CHECK(r.w.cursor.row == postRow(sign));
    }
    {
        Run r;
        r.a.resting = true;
        r.w.tile(50, 51) = 0x90000;
        r.vertical();
        CHECK(r.a.velocity.y == 0 && r.a.local.y == 0 && r.a.travel == 0 && r.a.hazard);
        CHECK(r.a.column == 50 && r.a.row == 51 && !r.a.landed && !r.a.vertical);
    }
    {
        Run r;
        r.a.resting = true;
        r.vertical();
        CHECK(!r.a.resting && r.a.local.y == 2);
    }
    for (int sign : {-1, 1}) {
        Run r;
        r.a.velocity.y = sign * 2.;
        r.a.motion = 8;
        r.w.tile(50, firstRow(sign)) = 0x10000;
        r.vertical();
        CHECK(r.a.velocity.y == -sign * 2. && r.a.local.y == -sign * 2.);
        CHECK(r.a.travel == 0 && r.a.vertical && !r.a.landed);
    }
    for (int sign : {-1, 1})
        for (int mode = 0; mode <= 3; ++mode) {
            Run r;
            r.a.velocity.y = sign * 2.;
            r.a.policy.automaticVerticalHandling = false;
            r.a.policy.solidCollisions = false;
            r.a.policy.airborneResponse = mode;
            r.w.tile(50, postRow(sign)) = 0x10000;
            r.vertical();
            CHECK(r.a.vertical && r.a.travel == 2);
            CHECK(r.a.local.y == (mode == 1 || mode == 2 ? 0 : sign * 2.));
            CHECK(r.a.velocity.y == (mode == 1 ? -sign * 1. : mode == 2 ? 0 : sign * 2.));
        }
    {
        Run r;
        r.a.policy.solidCollisions = false;
        r.w.tile(50, 53) = 0xb0000;
        r.vertical();
        CHECK(r.a.hazard && r.a.local.y == 2 && r.a.velocity.y == 2 && !r.a.vertical);
    }
    for (bool player : {false, true})
        for (int direction : {-1, 1})
            for (uint32_t id : {90U, 91U}) {
                Run r;
                r.a.player = player;
                r.a.velocity.x = direction;
                r.a.policy.automaticVerticalHandling = false;
                r.w.tile(50, 53) = 0x90000 | id;
                r.vertical();
                const bool ignored =
                    player && ((direction > 0 && id == 90) || (direction < 0 && id == 91));
                CHECK(r.a.vertical == !ignored && r.a.hazard == !ignored);
            }
    for (uint32_t mask : {0x400000U, 0x410000U}) {
        Run r;
        r.a.mask = mask;
        r.w.tile(50, 53) = 0x430000;
        r.vertical();
        CHECK(r.a.landed == (mask == 0x410000U));
    }
}

void landingsAndBreaking() {
    for (int gravity : {-1, 1}) {
        Run r;
        r.a.gravity = float(gravity);
        r.a.velocity.y = gravity * 2.;
        const int row = postRow(gravity);
        r.w.tile(50, row) = 0x10000;
        r.vertical();
        CHECK(r.a.landed && r.a.resting && r.a.vertical && r.a.velocity.y == 0);
        CHECK(r.a.travel == 2 && r.a.column == 50 && r.a.row == row && r.output.births.empty());
        // Independent source arithmetic, binary64 cell = 100 * float(1/100).
        if (gravity > 0)
            CHECK(r.a.local.y == 0x1.d99985f000000p+0);
        else
            CHECK(r.a.local.y == -0x1.d999ab7000000p+0);
    }
    {
        Run r;
        r.w.tile(50, 53) = 0x10000;
        r.w.tile(50, 52) = 0x20000;
        r.vertical();
        CHECK(r.a.velocity.y == 0 && !r.a.resting && !r.a.landed && !r.a.vertical);
    }
    for (int gravity : {-1, 1})
        for (uint32_t id : {32U, 33U, 34U, 39U, 40U, 41U, 42U}) {
            Run r;
            r.a.player = true;
            r.a.gravity = float(gravity);
            r.a.velocity.y = gravity * 2.;
            r.a.forced = -7;
            const int row = postRow(gravity);
            r.w.tile(50, row) = 0x10000 | id;
            r.w.tile(49, row) = 32;
            r.w.tile(51, row) = 42;
            r.vertical();
            CHECK(r.a.forced == -1 && r.random.draws() == 20 && r.output.births.size() == 10);
            CHECK(r.w.tile(49, row) == 0 && r.w.tile(50, row) == 0 && r.w.tile(51, row) == 0);
            CHECK(!r.a.landed && !r.a.vertical && !r.a.resting);
            CHECK(r.a.velocity.y == (gravity > 0 ? 0 : -2));
            CHECK(r.output.births.front().position.y == r.a.local.y);
            CHECK(r.output.births.front().contactTile == (0x10000 | id)); // emissions precede clear
            const int expectedSprite = id == 32   ? 145
                                       : id == 33 ? 146
                                       : id == 34 ? 0
                                                  : int(id) + 108;
            CHECK(r.output.births.front().sprite ==
                  expectedSprite); // extracted seven-word source table
            if (gravity < 0)
                CHECK(r.a.local.y == -2); // break before the negative-gravity snap
        }
    {
        Run r;
        r.a.player = true;
        r.a.forced = 1;
        r.w.tile(50, 53) = 0x10000 | 32;
        r.w.tile(49, 53) = 17;
        r.w.tile(51, 53) = 0x9000 | 39;
        r.vertical();
        CHECK(r.w.tile(49, 53) == 17 && r.w.tile(51, 53) == 0 && r.a.forced == 0);
    }
    for (float speed : {.009f, std::nextafter(.009f, 1.f)}) {
        Run r;
        r.a.player = true;
        r.a.velocity.y = double(speed);
        r.w.tile(50, 51) = 0x10000 | 32;
        r.vertical();
        const bool breaks = speed > .009; // float .009 is BELOW the source double threshold
        CHECK(r.output.births.size() == std::size_t(breaks ? 10 : 0));
        CHECK(r.a.landed == !breaks);
    }
    {
        Run r;
        r.a.player = true;
        r.a.forced = 1;
        r.w.tile(50, 53) = 0x110000 | 32;
        r.vertical();
        CHECK(r.output.births.empty() && !r.a.landed && r.a.local.y == 2);
        CHECK(r.w.tile(50, 53) ==
              (0x110000 | 32)); // substituted query does not retain raw surface flags
    }
    {
        Run r;
        r.a.player = true;
        r.a.forced = 1;
        r.a.local = {49.25, 46};
        r.w.tile(99, 99) = 0x10000 | 32;
        r.w.tile(98, 99) = 39;
        r.w.backup[0] = 42;
        r.vertical();
        CHECK(r.w.tile(99, 99) == 0 && r.w.tile(98, 99) == 0 && r.w.backup[0] == 0);
        CHECK(r.output.births.size() == 10); // right neighbour crosses physical grid into backup
    }
    {
        Run r;
        r.a.player = true;
        r.a.forced = 1;
        r.a.local.x = -49.75;
        r.w.tile(0, 53) = 0x10000 | 32;
        r.w.tile(1, 53) = 42;
        r.vertical();
        CHECK(r.w.tile(0, 53) == 0 && r.w.tile(1, 53) == 0);
    }
}

void oppositeContactsAndMedium() {
    for (int gravity : {-1, 1}) {
        Run r;
        r.a.player = true;
        r.a.gravity = float(gravity);
        r.a.velocity.y = -gravity * 2.;
        r.a.forced = 7;
        const int row = postRow(-gravity);
        r.w.tile(50, row) = 0x20000 | 32;
        r.w.medium.splashSoundSuppressed = false;
        r.vertical();
        CHECK(r.a.velocity.y == 0 && r.a.opposite259 && r.a.opposite25a && r.a.forced == 7);
        CHECK(r.output.births.size() == 30 && r.random.draws() == 100 &&
              r.output.sounds == std::vector<int>({43, 43}));
        CHECK(r.output.order == "A" + std::string(10, 'I') + "A" + std::string(20, 'F'));
        CHECK(r.output.births[10].contactTile == (0x20000 | 32) && r.w.tile(50, row) == 0);
    }
    for (int gravity : {-1, 1})
        for (bool wet : {false, true})
            for (bool player : {false, true}) {
                Run r;
                r.a.player = player;
                r.a.gravity = float(gravity);
                r.a.velocity.y = -gravity * 2.;
                r.a.forced = 7;
                r.w.tile(50, postRow(-gravity)) = 0x20000;
                if (wet) {
                    r.w.tile(50, 50) = '+';
                    r.a.flags = 1;
                }
                r.vertical();
                CHECK(r.a.opposite259 && r.a.opposite25a && !r.a.landed);
                CHECK(r.a.velocity.y == (wet ? gravity * .4 : 0));
                CHECK(r.a.forced == (player ? 0 : 7));
                CHECK(r.output.births.size() == std::size_t(player && !wet ? 10 : 0));
                CHECK(r.random.draws() == uint64_t((wet ? 1 : 0) + (player && !wet ? 60 : 0)));
            }
    for (int gravity : {-1, 1}) {
        Run r;
        r.a.gravity = float(gravity);
        r.a.velocity.y = -gravity * .0005;
        r.a.flags = 1;
        r.w.tile(50, 50) = '+';
        r.w.tile(50, firstRow(-gravity)) = 0x20000;
        r.vertical();
        CHECK(r.a.velocity.y == gravity * .001 && r.a.opposite25a);
    }
    {
        Run r;
        r.a.gravity = .03125f;
        r.a.velocity.y = 0;
        r.a.impulse = .0625;
        r.a.flags = 0x81;
        r.falling();
        CHECK(r.a.local.y == .09375 && r.a.impulse == .05625 && r.a.age == 102);
        CHECK(r.a.flags == 0x80 && r.a.changed == 0 && r.random.draws() == 0);
    }
    {
        Run r;
        r.a.gravity = 0;
        r.a.velocity = {2, .125};
        r.a.flags = 1;
        r.a.damping = .5;
        r.w.tile(50, 50) = '+';
        r.falling();
        CHECK(r.a.velocity.x == .5 && r.a.velocity.y == .03125 && r.a.local.y == .03125);
        CHECK(r.random.draws() == 2 && r.a.age == 102 && r.output.splashes == 0);
    }
    {
        Run r;
        r.a.player = true;
        r.a.gravity = 0;
        r.a.velocity = {0, 0};
        r.a.age = 80;
        r.w.tile(50, 50) = '+';
        r.falling();
        // Two wet chance draws + one shared splash colour + 50*3 child draws.
        CHECK(r.output.splashes == 50);
        CHECK(r.random.draws() == 153);
        CHECK(r.a.age == 1);
    }
}
void checkpointHistory() {
    GoatCheckpointHistory history({1.25, -2.5}, -.0002f);
    CHECK(history.cursor == 0 && history.latest().position.x == 1.25 &&
          history.latest().gravity == -.0002f);
    for (unsigned i = 0; i < 64; ++i)
        CHECK(history.positions[i].y == -2.5 && history.gravities[i] == -.0002f);
    for (int i = 1; i <= 65; ++i)
        history.record({double(i), -double(i)}, float(i));
    CHECK(history.cursor == 65 && history.positions[0].x == 65 && history.positions[1].x == 2);
    CHECK(history.latest().position.y == -65 && history.latest().gravity == 65);
    history.cursor = std::numeric_limits<int32_t>::max();
    history.record({9, 8}, -.0f);
    CHECK(history.cursor == std::numeric_limits<int32_t>::min());
    CHECK(history.latest().position.x == 9 && std::signbit(history.latest().gravity));
}
} // namespace
int main() {
    try {
        simpleContacts();
        landingsAndBreaking();
        oppositeContactsAndMedium();
        checkpointHistory();
        std::cout << "Vertical source-rule checks passed: real terrain, both gravity branches, "
                     "breaking, effects, double medium visits.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
