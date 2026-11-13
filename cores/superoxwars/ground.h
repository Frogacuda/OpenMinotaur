#pragma once
#include "world.h"
namespace superoxwars {
// GBE presentation retains the original tile index and publishes a separate
// muzzle position after drawing its armour stack.
struct GroundEnemy : GameEntity {
    int cell = 0, armour = 0, archetype = 0, bias = 0, reward = 10, deathSound = 8;
    int gunState = 0, burstCount = 2, burstLeft = 0, gunMode = 2, gunChance = 64, shotCount = 0;
    // Kind 9 disables layer drawing and does not initialize its layer style;
    // the shared values below are inactive host defaults for that constructor.
    int layerFrame = 2, layerPalette = 4;
    float gunClock = 0, warmup = .04f, cooldown = .01f, interval = .3f, spread = .3926991f,
          turnRate = 0, spin = 0, pulse = 1, layerScale = .5f, layerAlpha = .4f;
    float baseAngle = 0;
    bool active = false, departed = false, wasHit = false;
    Vec2 layerOffset{.02, .015}, muzzle;
    Vec2 emissionOffset, emissionMultiplier;
    Colour baseColour;
    explicit GroundEnemy(World &, int cell, int kind);
    void run() override;
    void prepare(std::vector<ActorDraw> &) override;
    void hitBy(Entity &) override;
    void aimAndFire();
};
struct Crater : GroundEnemy {
    float phase = 0, phaseStep = .01f;
    Vec2 positionDelta;
    const uint32_t *cycle = nullptr;
    Crater(World &, int cell, float radius);
    void run() override;
};
// The eye shares turret combat but publishes its own pupil/muzzle position.
struct Eyeball : GroundEnemy {
    Eyeball(World &w, int tile) : GroundEnemy(w, tile, 5) {}
    void run() override;
    void prepare(std::vector<ActorDraw> &) override;
    void hitBy(Entity &) override;
};
} // namespace superoxwars
