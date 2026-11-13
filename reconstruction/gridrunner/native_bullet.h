#pragma once
#include "native_collision.h"
namespace gr::reconstruction::ios {
// Explicit game bindings until the original theGame/ac are reconstructed.
// No ownership of Ship, and no touch emulation or controller interpretation.
struct BulletWorld {
    EntityLists &lists;
    TouchCollider &collision;
    Ship *ship;                     // current game+3b0, cleared when that record is retired
    std::array<int32_t, 8> &powers; // bind original ac+26b8..26d4; never a copy
    Rgba power0Tint, power2Tint, power1Tint;
};
// Original myBull, distinct from the generic engine bull and hostile ebull.
class MyBull final : public Entity {
  public:
    explicit MyBull(BulletWorld &sourceWorld) : world(sourceWorld) {}
    BulletWorld &world;
    void initialize(const Entity &source);
    void run() override;                 // iOS 0x4bac8
    void didHit(Entity &other) override; // iOS 0x4bc00
};
} // namespace gr::reconstruction::ios
