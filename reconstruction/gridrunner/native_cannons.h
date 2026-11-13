#pragma once
#include "native_enemies.h"
namespace gr::reconstruction::ios {
// OGE keeps physical motion separate from the grid-stepped drawing/collision
// position. Bridging a cell corner pauses physical integration until the
// visible position catches up on the other axis (iOS 0x52e28).
class OGE : public Entity {
  public:
    explicit OGE(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    Vec2 physical;
    int32_t cellX = -1, cellY = -1, mode = -1;
    float bridgeVelocity = 0;
    void initialize(Vec2);
    void pog();
    void run() override;
};
class Zakato final : public OGE {
  public:
    explicit Zakato(EnemyWorld &w) : OGE(w) {}
    std::array<Vec2, 128> trail{};
    uint32_t trailFrame = 0;
    bool fading = false;
    float fade = 1;
    int32_t armour = 0, variant = 0;
    void initialize(Vec2, Vec2);   // iOS 0x4c4e8
    void run() override;           // 0x530e8
    void hitBy(Entity &) override; // 0x536c8
    void foom();                   // 0x532f8
};
class Barrel final : public Entity {
  public:
    explicit Barrel(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    int32_t armour = 0;
    float spin = 0, baseRadius = 0;
    void initialize(Vec2);         // inline in LeftLaser::run 0x4bc48
    void run() override;           // 0x539f8
    void hitBy(Entity &) override; // 0x53b58
};
} // namespace gr::reconstruction::ios
