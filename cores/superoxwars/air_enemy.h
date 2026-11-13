#pragma once
#include "waves.h"
namespace superoxwars {
// ABE's four source phases use independent steering, acceleration, damping,
// gates and guns. This is its original phase interpreter, not a universal AI.
struct AirPhase {
    uint32_t gates = 0;
    Vec2 target, towards, cruise, acceleration{.0004, .0004}, damping{1, 1};
    float clock = 0, spin = 0, spinDamping = 1, wobble = 0;
    // forwardSpeed is written before any source phase enables its consumer;
    // zero is an inactive host default, not a recovered constructor assignment.
    float signClock = 0, signStep = 0, targetX = 0, targetY = 0, nearX = .1f, nearY = .1f,
          forwardSpeed = 0;
    float gunSpeed = 1, gunRate = .01f;
    int gunType = 0, gunChance = 0, eventChance = 0;
};
struct AirEnemy : GameEntity {
    std::array<AirPhase, 4> phases{};
    Vec2 previousStep, originScale, externalAcceleration;
    int shotCount = 0, deathShotCount = 0, deathShotType = 0;
    float shotSpread = .31415927f, shotJitter = .1f, speedVariation = .2f, deathShotSpread = 0;
    int phase = 0, archetype = 0, lifeTicks = 0, armour = 0, reward = 10, groupIndex = 0,
        paletteSlot = -1;
    float displayScale = 1, wobble = 0, wobblePhase = 0, wobbleStep = 0, gunClock = 1,
          lastTravel = 0;
    bool wasHit = false;
    explicit AirEnemy(World &, const LaunchPod &);
    void run() override;
    void hitBy(Entity &) override;
    void destroy();
    void fire(const AirPhase &);
};
} // namespace superoxwars
