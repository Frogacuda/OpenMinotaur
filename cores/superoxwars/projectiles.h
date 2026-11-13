#pragma once
#include "world.h"
namespace superoxwars {
float heading(Vec2);
bool emitEnemyShots(World &, Entity &, int mode, float speed, int count, float spread,
                    bool deathShot = false);
struct EnemyBullet : GameEntity {
    int lifetime = 1000, paletteSlot = -1;
    EnemyBullet(World &, Vec2 position, Vec2 velocity);
    void run() override;
    void hitBy(Entity &) override;
};
struct ShotSpark : GameEntity {
    int paletteSlot = 0;
    float fade = .1f, turn = 0, clock = 0;
    explicit ShotSpark(World &w) : GameEntity(w) {}
    explicit ShotSpark(World &, const Entity &, bool blue);
    void run() override;
};
struct HomingMissile : GameEntity {
    int tracking = 0;
    float turnRate = 0, speed = .005f;
    explicit HomingMissile(World &w) : GameEntity(w) {}
    HomingMissile(World &, Vec2);
    void run() override;
    void hitBy(Entity &) override;
};
void flowerSparks(World &, const Entity &);
struct Butterfly : HomingMissile {
    float wingPhase = 0;
    Butterfly(World &, const Entity &);
    void run() override;
    void hitBy(Entity &) override;
};
} // namespace superoxwars
