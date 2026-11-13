#pragma once
#include "native_effects.h"
namespace gr::reconstruction::ios {
struct EnemyWorld {
    HeadWorld &game;
    HeadAcState &ac;
    EntityLists &lists;
    Effects &effects;
    RandomStream &random;
    float feedback = 0;
    int32_t clearRow = 0, barrelChance = 0;
    EnemyWorld(HeadWorld &g, EntityLists &l, Effects &e, RandomStream &r)
        : game(g), ac(g.ac), lists(l), effects(e), random(r) {}
    virtual ~EnemyWorld() = default;
    virtual void sound(int32_t, int32_t volume = 7 << 12) = 0;
    void fireRing(const Entity &, int32_t count); // iOS pod2::shoot 0x4d4f8
    void enemyBullet(const Entity &, float angle, double speed);
    void specialPickup(Vec2); // original inline triangular selection, Survival guard
    void spawnSwarmers(const Entity &, int32_t count);
};
class EnemyBullet final : public Entity {
  public:
    explicit EnemyBullet(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    void initialize(const Entity &, float angle, double speed);
    void run() override;           // 0x53730
    void hitBy(Entity &) override; // 0x53820
};
class Shrapnel final : public Entity {
  public:
    explicit Shrapnel(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    float spin = 0;
    void run() override; // 0x51ea0
    void hitBy(Entity &other) override {
        other.didHit(*this);
    } // 0x52118
};
class ShrapnelBomb final : public Entity {
  public:
    explicit ShrapnelBomb(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    float detonationY = 0;
    int32_t fanCount = 0, smart = 0, fireTimer = 0, firePeriod = 0, armour = 0;
    void initialize(int32_t count, int32_t cycle);
    void run() override;           // 0x51528
    void hitBy(Entity &) override; // 0x51bb8
    void foom();                   // 0x51840
};
class Cube final : public Entity {
  public:
    explicit Cube(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    bool entered = false;
    float acceleration = 0;
    int32_t armour = 0, fragmentCount = 0; // +178, +17c; distinct from ShrapnelBomb's fan count
    void initialize(int32_t count, int32_t cycle, int32_t fragments);
    void run() override;           // iOS 0x50d10
    void hitBy(Entity &) override; // iOS 0x50f08
};
class AnalSurprise final : public Entity {
  public:
    explicit AnalSurprise(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    int32_t armour = 0, fireTimer = -1, firePeriod = 0;
    void initialize(int32_t count, int32_t cycle);
    void run() override;           // iOS 0x52508
    void hitBy(Entity &) override; // iOS 0x52a30
};
class Swarmer final : public Entity {
  public:
    explicit Swarmer(EnemyWorld &w) : world(w) {}
    EnemyWorld &world;
    float acceleration = 0;
    void initialize(const Entity &); // iOS 0x4db50
    void run() override;             // iOS 0x52130
    void hitBy(Entity &) override;   // iOS 0x522d8
};
} // namespace gr::reconstruction::ios
