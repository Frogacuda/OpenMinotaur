#pragma once
#include "ground.h"
namespace superoxwars {
struct Nose : GroundEnemy {
    Nose(World &w, int tile) : GroundEnemy(w, tile, 7) {}
    void run() override;
    void hitBy(Entity &) override;
};
struct Horn : GroundEnemy {
    Horn(World &, int tile);
    void run() override;
    void hitBy(Entity &) override;
};
struct Ear : GroundEnemy {
    int hingeState = 0;
    float hingeStep = 0;
    bool queued = false;
    Ear(World &, int tile);
    void run() override;
    void hitBy(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct Tooth : GroundEnemy {
    Tooth(World &, int tile, bool inverted);
    void run() override;
};
struct Flower : GroundEnemy {
    Flower(World &, int tile);
    void hitBy(Entity &) override;
};
struct Fox : GroundEnemy {
    Fox(World &, int tile);
};
struct Fence : GroundEnemy {
    uint32_t material = 3;
    Fence(World &, int tile, int variant);
    void run() override;
    void hitBy(Entity &) override;
};
struct Mine : GroundEnemy {
    int countdown = 0, column = 0, scriptRow = 0;
    Mine(World &, int tile, int row);
    void run() override;
    void hitBy(Entity &) override;
    void detonate();
};
struct Pyramid : GroundEnemy {
    bool activated = false;
    int countdown = 3, period = 3, missiles = 0, spawnPhase = 0;
    Pyramid(World &, int tile);
    void run() override;
    void hitBy(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
} // namespace superoxwars
