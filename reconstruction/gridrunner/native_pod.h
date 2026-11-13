#pragma once
#include "native_pickup.h"
#include <vector>
namespace gr::reconstruction::ios {
// Original cell sprite array (ac+13d0) remains authoritative for ordinary pods.
// It is not replaced with health stored independently on every Pod.
struct PodWorld {
    HeadAcState &ac;
    EntityLists &lists;
    Effects &effects;
    RandomStream &random;
    std::vector<int32_t> &cells;
    Ship *ship = nullptr;
    bool decayPulse = false; // ac+26b0
    int32_t clearRow = 0;    // ac+26d8
    float localFeedback = 0;
    float &feedback;
    Rgba dimTint; // game's colour at +eec
    PodWorld(HeadAcState &a, EntityLists &l, Effects &e, RandomStream &r, std::vector<int32_t> &c,
             float *sharedFeedback = nullptr)
        : ac(a), lists(l), effects(e), random(r), cells(c),
          feedback(sharedFeedback ? *sharedFeedback : localFeedback) {
        if (&e.random != &r)
            throw std::logic_error("Pods and effects require the same original RNG stream");
    }
    virtual ~PodWorld() = default;
    virtual void sound(int32_t) = 0;
    virtual void score(int32_t) = 0;
    void boostShip();
    void award100(const Entity &);
    virtual void fireRing(const Entity &, int32_t) {
        throw std::logic_error("Original pod2 projectile binding missing");
    }
    virtual void spawnSwarmers(const Entity &, int32_t) {
        throw std::logic_error("Original spod offspring binding missing");
    }
};
class Pod final : public Entity {
  public:
    explicit Pod(PodWorld &w) : world(w), ac(w.ac) {}
    PodWorld &world;
    HeadAcState &ac;
    int32_t cell = 0, row = 0;
    float baseScale = 1.6f; // +170,+174,+178
    // Parameters already quantized by the original ac::makePod owner.
    void initialize(Vec2, int32_t cellIndex, int32_t gridRow, float halfCellHeight);
    void run() override;           // iOS 0x4cd90
    void hitBy(Entity &) override; // iOS 0x4d100
};
class Bomb final : public Entity {
  public:
    explicit Bomb(PodWorld &w) : world(w), ac(w.ac) {}
    PodWorld &world;
    HeadAcState &ac;
    void initialize(const Entity &);
    void run() override;           // iOS 0x4df38
    void hitBy(Entity &) override; // iOS 0x4e158
};
class Pod2 final : public Entity {
  public:
    explicit Pod2(PodWorld &w) : world(w), ac(w.ac) {}
    PodWorld &world;
    HeadAcState &ac;
    int32_t cell = 0, row = 0;
    float baseRadius = .01f;
    void initialize(Vec2, int32_t cellIndex, int32_t gridRow, float halfCellHeight);
    void run() override;           // iOS 0x4d258
    void hitBy(Entity &) override; // iOS 0x4d6a8
};
class Spod final : public Entity {
  public:
    explicit Spod(PodWorld &w) : world(w), ac(w.ac) {}
    PodWorld &world;
    HeadAcState &ac;
    float acceleration = .0001f;
    int32_t row = 0, offspring = 0;
    void initialize(Vec2, int32_t gridRow, int32_t children);
    void run() override;           // iOS 0x4d7e8
    void hitBy(Entity &) override; // iOS 0x4dcf0
  private:
    void open();
};
} // namespace gr::reconstruction::ios
