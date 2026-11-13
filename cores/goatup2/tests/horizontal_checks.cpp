// Explicit state setups and source-derived expectations for 0x58680.
// Uses real terrain lookup; does not exercise original execution or construction.
#include "horizontal.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
using namespace goatup2;
namespace {
struct Actor {
    neon::vec2d position{0, 0}, velocity{2, 0}, scale{1, 1};
    double impulse = 0, travel = 0, hopX = 3, hopY = 4;
    int32_t freeze = 0, hop = -1, period = 2, ledge = 0, column = -1, row = -1;
    uint32_t medium = 0, mask = 0;
    float gravity = -1, effectiveGravity = -1, radius = 1;
    StageContactPolicy policy;
    bool player = false, resting = false, hazard = false, turn = false, contact = false;
    HorizontalActorView view() {
        return {position, velocity, impulse,          travel, freeze, hop,    period, hopX, hopY,
                medium,   gravity,  effectiveGravity, radius, scale,  policy, player, mask, resting,
                hazard,   turn,     contact,          ledge,  column, row};
    }
};
struct World {
    std::vector<uint32_t> working = std::vector<uint32_t>(10000), backup = working;
    TerrainBounds bounds{};
    TerrainLayout layout = calculateTerrainLayout(100, 100, 100, 100, {100, 100}, &bounds);
    uint32_t clock = 0;
    TerrainCursor cursor{-9, -9};
    uint32_t &tile(int x, int y) {
        return working[y * 100 + x];
    }
    void run(Actor &a) {
        moveHorizontal(a.view(), {{working, backup}, layout, bounds, clock, cursor});
    }
};
} // namespace
int main() {
    try {
        {
            World w;
            Actor a;
            a.freeze = -1;
            a.contact = true;
            a.impulse = 1;
            w.run(a);
            CHECK(!a.contact && a.impulse == 1 && a.position.x == 0 && a.velocity.x == 2);
            CHECK(w.cursor.column == -9 && w.cursor.row == -9);
        }
        {
            World w;
            Actor a;
            a.policy.solidCollisions = false;
            a.resting = true;
            a.position.x = 0x1.00000001p0;
            a.impulse = .5;
            w.run(a);
            CHECK(a.position.x == 0x1.c00000008p1 && a.velocity.x == 2.5);
            CHECK(a.impulse == .45 && a.travel == 2.5 && !a.resting);
            CHECK(w.cursor.column == -9);
        }
        {
            World w;
            Actor a;
            a.policy.solidCollisions = false;
            a.hop = 2;
            w.run(a);
            CHECK(a.hop == 2 && a.velocity.x == 2); // dry: no increment
            a.medium = 1;
            w.run(a);
            CHECK(a.hop == 0 && a.velocity.x == -3 && a.velocity.y == 4 && a.position.x == -1);
            a.scale.x = 0;
            a.gravity = 1;
            a.hop = 1;
            w.run(a);
            CHECK(a.hop == 2 && a.velocity.x == -3); // strict > period
            w.run(a);
            CHECK(a.hop == 0 && a.velocity.x == 3 && a.velocity.y == -4);
            a.hop = std::numeric_limits<int32_t>::max();
            w.run(a);
            CHECK(a.hop == std::numeric_limits<int32_t>::min());
        }
        {
            World w;
            Actor a;
            a.resting = true;
            a.velocity.x = 1.75;
            w.tile(52, 48) = 0x10000; // support; body row49, centre row50
            w.tile(51, 48) = 0x80000; // final support probe at committed X
            w.run(a);
            CHECK(a.position.x == 1.75 && a.travel == 1.75 && !a.contact && a.hazard);
            CHECK(a.column == 51 && a.row == 48 && a.ledge == 0);
        }
        for (int mode = 0; mode <= 3; ++mode) {
            World w;
            Actor a;
            a.resting = true;
            a.policy.ledgeResponse = mode;
            w.run(a);
            CHECK(a.ledge == 10 && !a.contact && a.turn == (mode == 3));
            CHECK(a.position.x == ((mode == 1 || mode == 2) ? -2 : 2));
            CHECK(a.velocity.x == (mode == 1 ? -2 : mode == 2 ? 0 : 2));
            CHECK(a.travel == (mode == 2 ? 0 : 2));
        }
        for (bool grounded : {false, true})
            for (int mode = 0; mode <= 3; ++mode) {
                World w;
                Actor a;
                a.resting = grounded;
                w.tile(52, 49) = 0x30000;
                w.tile(52, 48) = 0x10000;
                a.policy.groundedWallResponse = mode;
                a.policy.airborneResponse = mode;
                w.run(a);
                CHECK(a.contact && a.turn == (mode == 3));
                CHECK(a.position.x == (mode == 1 ? -2 : mode == 2 ? 0 : 2));
                CHECK(a.velocity.x == (mode == 1 || mode == 3 ? -2 : mode == 2 ? 0 : 2));
                CHECK(a.travel == (mode == 2 ? 0 : 2));
            }
        for (uint32_t bit : {0x10000U, 0x20000U})
            for (bool automatic : {false, true}) {
                World w;
                Actor a;
                a.policy.automaticVerticalHandling = automatic;
                w.tile(52, 49) = bit;
                w.run(a);
                CHECK(a.contact == !automatic);
            }
        for (bool player : {false, true})
            for (int direction : {-1, 1})
                for (uint32_t id : {90U, 91U}) {
                    World w;
                    Actor a;
                    a.player = player;
                    a.velocity.x = 2 * direction;
                    const int probe = direction > 0 ? 52 : 47;
                    w.tile(probe, 49) = 0x30000 | id;
                    w.run(a);
                    const bool ignored =
                        player && ((direction > 0 && id == 90) || (direction < 0 && id == 91));
                    CHECK(a.contact == !ignored);
                }
        {
            World w;
            Actor a;
            a.mask = 0x410000;
            w.tile(52, 49) = 0x420000;
            w.tile(52, 50) = 0x10000;
            w.run(a);
            CHECK(a.contact); // OR BEFORE exact mask: 0x410000 != 0x400000
        }
        for (uint32_t mask : {0x400000U, 0x410000U}) {
            World w;
            Actor a;
            a.mask = mask;
            w.tile(52, 49) = 0x430000;
            w.run(a);
            CHECK(a.contact == (mask == 0x410000));
        }
        {
            World w;
            Actor a;
            a.resting = true;
            a.mask = 0x10000;
            w.tile(47, 49) = 0xb0000;
            w.run(a);
            CHECK(!a.contact && !a.hazard && a.velocity.x == -2); // ledge nonzero mask rejects all
        }
        {
            World w;
            Actor a;
            a.policy.airborneResponse = 1;
            a.mask = 0x10000;
            w.tile(52, 49) = 0x30000;
            w.tile(47, 49) = 0x90000;
            w.run(a);
            CHECK(a.contact && a.hazard); // wall reverse query has NO mask
        }
        for (int row : {49, 50}) {
            World w;
            Actor a;
            w.tile(52, row) = 0x80000;
            w.run(a);
            CHECK(a.hazard && !a.contact);
        }
        {
            World w;
            Actor a;
            a.velocity.x = 0;
            w.tile(49, 49) = 0x80000;
            w.run(a);
            CHECK(a.hazard); // zero selects the negative-radius probe
        }
        {
            World w;
            Actor a;
            a.resting = true;
            a.policy.groundedWallResponse = 1;
            w.tile(47, 49) = 0x30000; // ledge reversal discovers an opposite wall
            w.run(a);
            CHECK(a.contact && a.ledge == 10 && a.velocity.x == 2 && a.position.x == 2);
            CHECK(a.travel == 2 && a.column == 52 && a.row == 48);
        }
        {
            World w;
            Actor a;
            a.hazard = true;
            a.turn = true;
            a.velocity.x = 0;
            a.position.x = 0x1.00000001p0;
            w.run(a);
            CHECK(a.position.x == 1 && a.travel == 0 && a.turn && a.hazard);
        }
        {
            World w;
            Actor a;
            a.effectiveGravity = 1;
            a.resting = true;
            w.tile(52, 51) = 0x90000;
            w.run(a);
            CHECK(a.ledge == 0 && a.hazard && a.row == 51);
        }
        std::cout << "Horizontal source-rule checks passed (real terrain, ledge/wall modes, masks, "
                     "hops, precision).\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
