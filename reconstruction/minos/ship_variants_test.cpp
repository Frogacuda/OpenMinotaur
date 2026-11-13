// Source-rule integration checks. These are not original-execution fixtures.
#include "ship.h"
#include <cmath>
#include <cstdio>
#include <memory>

using namespace minos;
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *rule) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", rule);
        }
    };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-12; };
    // Primary ship::run assigns these two fields before their first reads;
    // neither allocation contents nor a previous flight may affect that tick.
    Vec2 cleanVelocity{}, cleanPosition{};
    for (bool poisoned : {false, true}) {
        PlayState g;
        neon::DarwinRandom random(1);
        g.rand = &random;
        g.scrollLimit = 1000;
        g.frameCount = 2;
        Ship s;
        s.init(g);
        if (poisoned) {
            s.thrustTarget = 123.f;
            s.impact = 123456;
        }
        s.run();
        check(s.thrustTarget == 0 && s.impact == 0,
              "first update defines primary thrust target and contact sum");
        if (!poisoned) {
            cleanVelocity = s.vel;
            cleanPosition = s.mapPos;
        } else
            check(s.vel.x == cleanVelocity.x && s.vel.y == cleanVelocity.y &&
                      s.mapPos.x == cleanPosition.x && s.mapPos.y == cleanPosition.y,
                  "primary movement is independent of supplied inactive allocation fields");
        g.resetLists(0);
    }
    for (int kind = 1; kind <= 3; ++kind) {
        PlayState g;
        neon::DarwinRandom random(1);
        g.rand = &random;
        g.shipKind = kind;
        g.scrollLimit = 1000;
        g.frameCount = 2;
        g.awardEligibilityGate = 1;
        std::unique_ptr<Ship> s;
        if (kind == 1)
            s = std::make_unique<VicLander>();
        else if (kind == 2)
            s = std::make_unique<Taxi>();
        else
            s = std::make_unique<AsteroidsSpaceship>();
        s->init(g);
        check(random.draws() == 0, "ship initialization consumes no RNG");
        int sensors = 0, guns = 0;
        for (auto *e = g.debris; e; e = e->next) {
            sensors += dynamic_cast<Sensor *>(e) != nullptr;
            if (auto *gun = dynamic_cast<Gun *>(e)) {
                ++guns;
                check(gun->ship == s.get() && gun->g == &g, "weapon owner bindings");
                check(gun->burst == (kind == 3 ? 10 : 7) && gun->reload == (kind == 3 ? 5 : 8),
                      "weapon cadence");
            }
        }
        check(sensors == 5 && guns == kind + 2, "five sensors and class-specific weapon count");
        check(s->sensor(0)->weight == (kind == 3 ? 4.0f : 1.0f),
              "class-specific landing impact weights");
        check(s->spriteIndex == (kind == 1 ? 10 : kind == 2 ? 75 : 11), "class-specific artwork");
        s->mapPos = {9.5, 20};
        g.prefsButtons = 0x10;
        const float startFuel = g.fuel;
        s->run();
        float thrust;
        if (kind == 3)
            thrust = float(double(float(0.0009999999776482583)) - 5.555555555555556e-05);
        else
            thrust = float(double(0.008f) * (kind == 1 ? 0.02 : 0.05));
        const double gravity = kind == 1   ? double(0.0005f)
                               : kind == 2 ? double(0.0014f)
                                           : double(0.0002f);
        const double drag = kind == 1 ? 0.9999 : kind == 2 ? 1.0 : 0.999;
        check(s->thrust == thrust, "free-flight first-tick thrust, including overshoot fall");
        check(near(s->vel.y, (-double(thrust) + gravity) * drag), "free-flight gravity/drag order");
        const float idle = kind == 1 ? 0.00005f : kind == 2 ? 0.00015f : 0.0002f;
        const float burn = kind == 1 ? 0.0006f : kind == 2 ? 0.0003f : 0.0002f;
        check(g.fuel == float(float(startFuel - idle) - burn), "idle and thrust fuel stores");

        // The Asteroids ship accepts upside-down landings and awards BATTY;
        // the landers require an almost level attitude.
        g.prefsButtons = 0;
        s->rot = 3.0f;
        s->vel = {0, 0};
        s->sensor(0)->touching = s->sensor(1)->touching = true;
        s->run();
        check(s->isLanded == (kind == 3), "ship-specific landing angle rule");
        check(bool(g.goalBits & 1) == (kind == 3), "upside-down landing achievement");
        s->sensor(0)->touching = s->sensor(1)->touching = false;
        s->rot = 0;
        s->isLanded = false;
        s->launchHold = false;
        s->vel = {0, 0};
        g.analog = 1;
        s->run();
        if (kind == 3)
            check(s->rot == -0.03f, "phone Asteroids fixed-step turning");
        else
            check(0.0 < s->vel.x && (kind != 2 || s->scale.x > 0),
                  "lander steering and taxi facing");
        if (kind == 3) {
            g.iPad = true;
            s->rot = 0;
            s->vel = {0, 0};
            s->run();
            check(s->rot == -0.015f * 5.0f, "tablet Asteroids fixed-step turning");
        }
        g.analog = 0;
        s->rot = 0;
        s->vel = {0, 0};
        s->isLanded = false;
        s->sensor(0)->touching = s->sensor(1)->touching = true;
        s->sensor(0)->hazard = true;
        s->sensor(0)->impact = 10;
        const float shield = g.shield;
        s->run();
        check(!s->isLanded && g.shield == shield - 10.0f * s->damage,
              "hazard contacts damage without landing");
        s->sensor(0)->touching = s->sensor(1)->touching = false;
        s->sensor(0)->hazard = false;

        // The same shentity death timer/respawn path must retain each class.
        g.analog = 0;
        g.shield = 0;
        g.lives = 2;
        s->run();
        check(s->dying && s->type == 3 && s->deathTimer == 100, "shared death entry");
        s->deathTimer = 1;
        g.shipList = s.get();
        g.respawnPos = {9.5, 20};
        s->run();
        check(!s->dying && g.lives == 1 && g.shield == 1.0f && s->type == 2,
              "shared respawn retains variant");
        g.shipList = nullptr; // unique_ptr owns the player; the source list owns its children
        g.resetLists(0);
    }
    std::printf("Minos ship variant source checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
