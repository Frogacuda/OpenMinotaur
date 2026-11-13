#pragma once
#include "ground.h"
namespace superoxwars {
struct ColourOrb : GameEntity {
    float clock = 0, clockRate = .04f;
    explicit ColourOrb(World &, const Entity &);
    void run() override;
    void hitBy(Entity &) override;
};
struct BonusTile : GroundEnemy {
    explicit BonusTile(World &, int cell, int kind);
    void hitBy(Entity &) override;
};
struct WeaponPickup : GroundEnemy {
    WeaponPickup(World &, int cell, int frame);
    void hitBy(Entity &) override;
};
struct Follower : GameEntity {
    int slot = 0;
    float lifetime = 1, decay = .0013f;
    Follower(World &, int frame);
    void run() override;
};
struct LifePickup : GroundEnemy {
    bool collected = false;
    float fade = 0;
    LifePickup(World &, int cell, int frame);
    void run() override;
    void hitBy(Entity &) override;
};
struct LifeToken : GameEntity {
    int side = 0, slot = 0, state = 0;
    float progress = 1, targetX = 0;
    LifeToken(World &, int frame);
    void run() override;
};
struct OxBlock : GroundEnemy {
    OxBlock(World &, int cell);
    void prepare(std::vector<ActorDraw> &) override;
    void hitBy(Entity &) override;
};
} // namespace superoxwars
