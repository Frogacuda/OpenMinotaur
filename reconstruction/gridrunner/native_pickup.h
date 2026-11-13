#pragma once
#include "native_effects.h"
namespace gr::reconstruction::ios {
struct PickupWorld {
    HeadAcState &ac;
    EntityLists &lists;
    Effects &effects;
    RandomStream &random;
    const std::array<Rgba, 8> &tints;
    Ship *ship = nullptr;
    PickupWorld(HeadAcState &a, EntityLists &l, Effects &e, RandomStream &r,
                const std::array<Rgba, 8> &p)
        : ac(a), lists(l), effects(e), random(r), tints(p) {
        if (&e.random != &r)
            throw std::logic_error("Pickups and effects require the same original RNG stream");
    }
    virtual ~PickupWorld() = default;
    virtual void sound(int32_t) = 0;
    virtual void score(int32_t) = 0;
};
// Source fhbon, the animated artwork emitted when a pickup is collected.
class Fhbon final : public Entity {
  public:
    int32_t baseSprite = 68; // +170
    void initialize(const Entity &, RandomStream &);
    void run() override; // iOS 0x52cf0
};
class Powerup final : public Entity {
  public:
    explicit Powerup(PickupWorld &w) : world(w), ac(w.ac) {}
    PickupWorld &world;
    HeadAcState &ac;
    int32_t kind = 0; // +170; distinct from Entity::type
    // Selection/RNG is performed by the original caller (Head, cube, etc.).
    void initialize(Vec2 position, int32_t selectedKind);
    void run() override;           // iOS 0x4fed0
    void hitBy(Entity &) override; // iOS 0x4fff0, ignores attacker type
};
} // namespace gr::reconstruction::ios
