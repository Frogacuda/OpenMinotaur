#pragma once
#include "native_ship.h"
namespace gr::reconstruction::ios {
struct LaserWorld {
    HeadAcState &ac;
    EntityLists &lists;
    Effects &effects;
    RandomStream &random;
    Rgba tint, activeBeamTint;
    float localFeedback = 0;
    float &feedback;
    int32_t barrelChance = 0, zakatoCount = 0;
    float zakatoSpeed = .003f;
    LaserWorld(HeadAcState &a, EntityLists &l, Effects &e, RandomStream &r,
               float *sharedFeedback = nullptr)
        : ac(a), lists(l), effects(e), random(r),
          feedback(sharedFeedback ? *sharedFeedback : localFeedback) {}
    virtual ~LaserWorld() = default;
    virtual void sound(int32_t) = 0;
    virtual void makePod(Vec2, bool) = 0;
    virtual void barrel(Vec2) {
        throw std::logic_error("Original Barrel factory is not bound");
    }
    virtual void zakato(Vec2, Vec2) {
        throw std::logic_error("Original Zakato factory is not bound");
    }
};
class LaserZap final : public Entity {
  public:
    explicit LaserZap(LaserWorld &w) : world(w) {}
    LaserWorld &world;
    void initialize(const Tao &);
    void run() override; // iOS 0x4c7f8
};
class LaserBolt final : public Entity {
  public:
    explicit LaserBolt(LaserWorld &w) : world(w) {}
    LaserWorld &world;
    uint32_t seed = 0;
    std::array<int32_t, 30> frames{};
    void initialize(const Tao &);
    void run() override;   // iOS 0x4c5d8
    void prepareDraw();    // iOS 0x4c680; one global draw, local reseed/restore
    Rgba drawTint() const; // draw starts with gcol[6]; charging overrides with gcol[4]
};
class LeftLaser final : public Tao {
  public:
    explicit LeftLaser(LaserWorld &);
    LaserWorld &world;
    void run() override; // iOS 0x4bc48
};
class BottomLaser final : public Tao {
  public:
    explicit BottomLaser(LaserWorld &);
    LaserWorld &world;
    void run() override; // iOS 0x4c168
};
} // namespace gr::reconstruction::ios
