#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_head.h"
#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
using namespace gr::reconstruction::ios;
struct ShipProbe : Ship {
    void run() override {
        throw std::logic_error("Ship update pending");
    }
};
struct Attacker : Entity {
    int notifications = 0;
    Head *h = nullptr;
    bool killTarget = false;
    void didHit(Entity &e) override {
        ++notifications;
        if (killTarget)
            e.dead = true;
    }
};
struct World : HeadWorld {
    ShipProbe actualShip;
    std::vector<std::string> events;
    std::deque<uint32_t> draws;
    HeadExplosion explosion{};
    HeadBonus award{};
    HeadOutline ring{};
    int32_t scoreValue = 0, pickupKind = -1;
    Vec2 pickupPosition;
    double angle = 1.23456789;
    World() {
        ship = &actualShip;
        shipPosition = &actualShip.position;
    }
    void touchEnemyList(Head &) override {}
    void checkSweep(Head &) override {}
    void makePod(Vec2, bool fromLaser) override {
        assert(!fromLaser);
        events.push_back("pod");
    }
    void score(int32_t n) override {
        scoreValue += n;
        events.push_back("score" + std::to_string(n));
    }
    void sound(int32_t n) override {
        events.push_back("sound" + std::to_string(n));
    }
    void explode(const HeadExplosion &e) override {
        explosion = e;
        events.push_back("exp");
    }
    void bonus(const HeadBonus &b) override {
        award = b;
        events.push_back("bonus");
    }
    uint32_t globalRandom() override {
        assert(!draws.empty());
        const auto n = draws.front();
        draws.pop_front();
        events.push_back("rand");
        return n;
    }
    double headRandomRange(Head &, double min, double max) override {
        assert(min == -3.141592654 && max == 3.141592654);
        events.push_back("angle");
        return angle;
    }
    void pickup(Vec2 p, int32_t kind) override {
        pickupPosition = p;
        pickupKind = kind;
        events.push_back("pickup");
    }
    void outline(const HeadOutline &e) override {
        ring = e;
        events.push_back("outline");
    }
};
void eligible(Head &h, int type = 2) {
    h.type = type;
    h.age = 30;
    h.opacity = 1;
    h.position = {.2, -.1};
    h.mode = 0;
}
int main() {
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 5;
        eligible(h);
        h.age = 29;
        h.hitBy(a);
        assert(a.notifications == 0);
        h.age = 30;
        h.opacity = .99f;
        h.hitBy(a);
        assert(a.notifications == 0);
        h.opacity = 1;
        h.mode = 2;
        h.hitBy(a);
        assert(a.notifications == 1 && w.events.empty());
        h.mode = 0;
        h.type = 6;
        h.hitBy(a);
        assert(a.notifications == 2 && w.events.empty());
        h.type = 2;
        h.dead = true;
        h.hitBy(a);
        assert(a.notifications == 3 && w.events.empty());
        h.dead = false;
        a.killTarget = true;
        h.hitBy(a);
        assert(a.notifications == 4 && w.events.empty());
        a.killTarget = false;
        h.dead = false;
        for (int type : {1, 2, 3, 4, 7, 9}) {
            a.type = type;
            h.hitBy(a);
            assert(w.events.empty());
        }
    }
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 5;
        eligible(h);
        h.armour = 2;
        w.actualShip.fireBoost = 9;
        h.hitBy(a);
        assert(h.armour == 1 && h.radius == .08f && h.type == 2 && h.deathCountdown == 0);
        assert(w.actualShip.fireBoost == 11 && w.ac.hitFlash == 1 &&
               w.events == std::vector<std::string>{"sound62"});
        w.events.clear();
        h.hitBy(a);
        assert(h.armour == 0 && w.actualShip.fireBoost == 11 && w.events.size() == 1);
        // Float threshold is a gate, not a clamp to 10.
    }
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 5;
        eligible(h);
        w.ac.dropChance = 0;
        w.ac.dropIncrement = 17;
        w.draws = {0};
        h.hitBy(a);
        assert((w.events ==
                std::vector<std::string>{"pod", "score30", "sound36", "exp", "bonus", "rand"}));
        assert(w.ac.dropChance == 17 && w.scoreValue == 30 && w.actualShip.fireBoost == 2);
        assert(w.explosion.count == 20 && w.explosion.colour == 2 &&
               w.explosion.minSpeed == .002f && w.explosion.maxSpeed == .01f);
        assert(w.award.sprite == 25 && w.award.radius == .02f && w.award.colour == 4);
        assert(h.mode == 2 && h.type == 6 && h.deathCountdown == 5 && h.tint.a == 0 && !h.dead);
        for (int i = 0; i < 4; ++i) {
            h.run();
            assert(!h.dead);
        }
        h.run();
        assert(h.dead);
    }
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 5;
        eligible(h, 7);
        w.ac.powers[1] = 1;
        h.hitBy(a);
        assert((w.events == std::vector<std::string>{"score10", "sound37", "exp", "bonus", "angle",
                                                     "outline"}));
        assert(w.explosion.count == 2 && w.explosion.colour == 0x00ffffff && w.award.sprite == 24 &&
               w.award.colour == 5);
        assert(w.ring.angle == float(w.angle) && w.ring.radius == .03f && w.ring.growth == 1.03f &&
               w.ring.fade == .05f);
        assert(w.draws.empty() && h.type == 6);
    }
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 8;
        eligible(h);
        h.armour = 99;
        w.ac.powers[1] = 1;
        w.ac.dropChance = 100;
        w.ac.dropReset = 500;
        w.ac.level = 2;
        w.draws = {0, 3};
        h.hitBy(a); // Ship contact bypasses armour and does not increase fire boost.
        assert(h.type == 6 && h.armour == 99 && w.actualShip.fireBoost == 0 && w.ac.hitFlash == 0);
        assert(w.explosion.count == 4 && w.pickupKind == 3 && w.ac.dropChance == 500);
        assert((w.events == std::vector<std::string>{"score30", "sound36", "exp", "bonus", "rand",
                                                     "rand", "sound43", "pickup", "angle",
                                                     "outline"}));
        assert(w.ring.angle == 0 && w.ring.radius == .06f && w.ring.growth == 1.1f &&
               w.ring.velocity.x == 0);
    }
    {
        World w;
        Head h(w);
        eligible(h);
        w.ac.powers[7] = 20;
        h.armour = 999;
        h.age = 29;
        h.checkSweep();
        assert(w.events.empty());
        h.age = 30;
        h.opacity = .9f;
        h.checkSweep();
        assert(w.events.empty());
        h.opacity = 1;
        h.checkSweep();
        assert((w.events == std::vector<std::string>{"score30", "sound36", "exp", "bonus", "angle",
                                                     "outline"}));
        assert(h.type == 6 && h.mode == 2 && h.deathCountdown == 5 && h.armour == 999 &&
               w.explosion.count == 20);
        assert(w.ring.colour == 1 && w.ring.radius == .06f && w.ring.growth == 1.2f &&
               w.draws.empty());
    }
    {
        // Every triangular drop value; warp remaps happen after both draws.
        for (uint32_t a = 0; a < 4; ++a)
            for (uint32_t b = 0; b < 4; ++b)
                for (int warp = 0; warp < 3; ++warp) {
                    World w;
                    Head h(w);
                    Attacker attacker;
                    attacker.type = 5;
                    eligible(h);
                    w.ac.dropChance = 1;
                    w.ac.dropReset = 123;
                    w.draws = {0, a, b};
                    w.ac.warpUsed = warp == 1;
                    w.ac.timeScale = warp == 2 ? .5f : 1;
                    int kind = int(a + b + 1);
                    if (kind == 6) {
                        if (warp == 1)
                            kind = 7;
                        else if (warp == 2)
                            kind = 4;
                    }
                    h.hitBy(attacker);
                    assert(w.pickupKind == kind && w.draws.empty() && w.ac.dropChance == 123);
                }
    }
    {
        World w;
        Head h(w);
        Attacker a;
        a.type = 5;
        eligible(h);
        w.survival = true;
        w.ac.level = 6;
        w.ac.dropChance = 100;
        w.ac.dropReset = 500;
        w.draws = {0};
        h.hitBy(a);
        assert(w.pickupKind == -1 && w.ac.dropChance == 500 && w.draws.empty());
        assert(w.events[w.events.size() - 2] == "angle" && w.events.back() == "outline");
        // Drop threshold equal is failure; include original level*8 term.
        World v;
        Head other(v);
        eligible(other);
        v.ac.level = 2;
        v.ac.dropChance = 21;
        v.draws = {5};
        v.ac.dropIncrement = 7;
        other.hitBy(a);
        assert(v.ac.dropChance == 28 && v.pickupKind == -1);
    }
    std::cout << "Original Head notification, armour, fire boost, scores, power-dependent effects, "
                 "drops and delayed death passed\n";
}
