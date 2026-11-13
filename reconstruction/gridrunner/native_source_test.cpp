#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_head.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
struct World : HeadWorld {
    Vec2 ship{0, .1};
    int contacts = 0, sweeps = 0;
    uint32_t frameAtContact = 0;
    int typeAtContact = 0;
    World() {
        shipPosition = &ship;
    }
    void touchEnemyList(Head &h) override {
        contacts++;
        frameAtContact = h.frame;
        typeAtContact = h.type;
    }
    void checkSweep(Head &) override {
        sweeps++;
    }
};
struct ShipInitProbe : Ship {
    void run() override {
        throw std::logic_error("Ship update not reconstructed");
    }
};
int main() {
    {
        Entity e;
        assert(e.sprite == 140 && e.mass == 1 && e.radius == .02f && e.damping.x == .98);
        e.position = {.49, -.49};
        e.velocity = {.02, -.03};
        e.run();
        assert(e.age == 1 && e.wrapped && e.position.x == .49 + .02 - 1 &&
               e.position.y == -.49 - .03 + 1);
        assert(e.velocity.x == .02 * .98 && e.velocity.y == -.03 * .98);
        e.position = {4.5, 0};
        e.velocity = {};
        e.run();
        assert(e.position.x == 3.5); // one wrap, not modulo
        e.noWrap = true;
        e.position = {8, 9};
        e.wrapped = true;
        e.run();
        assert(e.position.x == 8 && e.wrapped);
        e.age = std::numeric_limits<int32_t>::max();
        e.run();
        assert(e.age == std::numeric_limits<int32_t>::min());
        Vec2 v{-0.0, 1};
        v.rotateBy(0);
        assert(std::signbit(v.x) && v.y == 1);
    }
    {
        Tao t;
        t.position = {.2, -.3};
        t.age = 9;
        t.initTrail();
        t.run();
        assert(t.trailFrame == 1 && t.trail[0].x == .2 && t.age == 9 && t.arrival == .005f);
        assert(t.arrivalStep == float(double(.005f) * 1.01));
        t.arrival = .999f;
        t.arrivalStep = .005f;
        t.run();
        assert(t.arrival == 1 && t.arrivalStep == 0);
        ShipInitProbe s;
        s.initialize(false);
        assert(s.type == 8 && s.sprite == 4 && s.position.y == .1 && s.scale.x == 1.33);
        s.fireTimer = 7;
        s.fireBoost = 2;
        s.rerez(true);
        assert(s.position.y == .35 && s.fireTimer == 7 && s.fireBoost == 2);
        assert(s.trail[511].y == .35 && s.state == Ship::State::Arriving);
    }
    {
        EntityLists lists;
        auto entity = std::make_unique<Entity>();
        auto *e = entity.get();
        e->velocity = {.1, 0};
        lists.prepend(List::L3c4, std::move(entity));
        lists.runUnpaused(1);
        assert(e->age == 1 && e->position.x == .1);
    }
    {
        World w;
        Head parent(w), child(w);
        HeadSpawn s;
        s.x = .25f;
        s.y = -.3f;
        s.cycle = 2;
        s.headArmour = 3;
        s.bodyArmour = 4;
        parent.targets.fill(17);
        parent.initialize(s, nullptr, 0);
        assert(parent.type == 2 && parent.armour == 5 && parent.originalArmour == 5);
        assert(parent.speed == (2.f * .3f + 1.f) * .004f && parent.positions[0].x == .25);
        assert(parent.opacities[31] == 0 && parent.modes[31] == -1 && parent.targets[31] == 17);
        child.initialize(s, &parent, 0);
        assert(child.type == 7 && child.armour == 4 && child.parent == &parent);
        parent.frame = 19;
        parent.positions[2] = {.125, .25};
        parent.opacities[2] = .7f;
        parent.modes[2] = 1;
        parent.returnModes[2] = 4;
        parent.targets[2] = 1.5f;
        parent.turnSteps[2] = -.125f;
        parent.turnCounts[2] = 3;
        parent.angles[2] = .25f;
        child.separation = 8;
        w.ac.timeScale = .4f;
        child.run();
        assert(child.position.x == .125 && child.position.y == .25 && child.mode == 1 &&
               child.turning == 3 && child.movementAngle == .25f);
        assert(w.sweeps == 0 && w.frameAtContact == 1 && child.historyAt(0).position.y == .25);
        parent.type = 6;
        parent.originalArmour = 9;
        child.run();
        assert(child.type == 2 && child.armour == 9 && child.originalArmour == 4 &&
               child.parent == &parent);
        assert(child.position.x == .125 && child.mode == 1); // still samples on promotion tick
        child.type = 7;
        parent.type = 2;
        parent.dead = true;
        child.run();
        assert(child.type == 7); // type 6, not dead flag, promotes
    }
    {
        for (int vx : {-1, 1})
            for (bool repel : {false, true}) {
                World w;
                w.ac.deathRepel = repel ? 1 : 0;
                Head h(w);
                h.position = {0, 0};
                h.velocity = {double(vx), 0};
                h.turnTicks = 16;
                h.normalTurn();
                assert(h.mode == 1 && h.turning == 16 && h.returnMode == 0);
                assert(h.target == (vx > 0 ? -1.5707964f : 1.5707964f));
                assert(h.turnStep == (vx > 0 ? -3.1415927f : 3.1415927f) / 16.f * (repel ? -1 : 1));
            }
        struct Boundary {
            Vec2 position, velocity;
            float target, step;
        };
        const float q = .7853982f, tq = 2.3561945f, hp = 1.5707964f / 16.f;
        const Boundary cases[] = {{{.42, 0}, {1, 1}, -q, -hp},  {{.42, 0}, {1, -1}, -tq, hp},
                                  {{-.4, 0}, {-1, 1}, q, hp},   {{-.4, 0}, {-1, -1}, tq, -hp},
                                  {{0, -.33}, {1, -1}, q, -hp}, {{0, -.33}, {-1, -1}, -q, hp},
                                  {{0, .43}, {1, 1}, tq, hp},   {{0, .43}, {-1, 1}, -tq, -hp}};
        for (const auto &c : cases) {
            World w;
            Head h(w);
            h.mode = 4;
            h.position = c.position;
            h.velocity = c.velocity;
            h.diagonalBoundary();
            assert(h.mode == 1 && h.returnMode == 4 && h.target == c.target &&
                   h.turnStep == c.step);
        }
    }
    {
        World w;
        Head h(w);
        HeadSpawn s;
        s.x = .2f;
        s.y = -.3f;
        h.initialize(s, nullptr, 0);
        h.radius = .0301f;
        h.run();
        assert(h.radius == float(double(.0301f) * .95) && h.radius < h.baseRadius);
        h.mode = 1;
        h.turning = 1;
        h.returnMode = 4;
        h.target = .5f;
        h.turnStep = .25f;
        h.opacity = .3f;
        h.headCooldown = 3;
        h.run();
        assert(h.mode == 4 && h.movementAngle == .5f && h.opacity == .3f && h.headCooldown == 3);
        h.mode = 0;
        w.ac.timeScale = .4f;
        h.bump = true;
        h.run();
        assert(h.headCooldown == 5 && h.podCooldown == 5 && !h.bump);
    }
    {
        World w;
        Head h(w);
        Entity other;
        other.type = 2;
        h.didHit(other);
        assert(h.bump && h.headCooldown == 30);
        h.didHit(other);
        assert(!h.bump);
        h.mode = 1;
        h.bump = false;
        other.type = 1;
        h.didHit(other);
        assert(!h.bump && h.podCooldown == 0);
        h.mode = 4;
        h.didHit(other);
        assert(h.bump && h.podCooldown == 10);
        h.age = 29;
        h.opacity = 1;
        h.hitBy(other);
        h.age = 30;
        other.type = 5; // mature bullet requires the not-yet-bound Ship service
        bool pending = false;
        try {
            h.hitBy(other);
        } catch (const std::logic_error &) {
            pending = true;
        }
        assert(pending);
    }
    {
        World w;
        Head h(w);
        HeadSpawn s;
        s.x = .7f;
        s.y = -.3f;
        h.initialize(s, nullptr, 0);
        h.run();
        assert(h.type == 6 && h.mode == 2 && h.deathCountdown == 5 && !h.dead &&
               w.typeAtContact == 2);
        assert(h.historyAt(0).mode == 0); // escape transition happens after history/contact
        for (int i = 0; i < 4; i++) {
            h.run();
            assert(!h.dead);
        }
        h.run();
        assert(h.dead && h.deathCountdown == 0 && w.contacts == 6);
    }
    std::cout << "Source Entity/Tao/Ship initialization and Head histories, turns, promotion, warp "
                 "and dying-record order passed\n";
}
