#pragma once
#include "native_bullet.h"
#include "native_effects.h"
namespace gr::reconstruction::ios {
// Original virtual joy and the intentional direct-control extension meet at
// the displacement boundary. Direct input never synthesizes a touch.
struct ShipInput {
    Vec2 value;
    bool directDisplacement = false;
};
struct ShipWorld {
    HeadAcState &ac;
    EntityLists &lists;
    TouchCollider &collision;
    Effects &effects;
    RandomStream &random;
    bool alternateLayout = false, demo = false;
    uint32_t clock = 0;
    float localFeedback = 0;
    float &feedback;
    Rgba dyingTint;
    ShipWorld(HeadAcState &a, EntityLists &l, TouchCollider &c, Effects &e, RandomStream &r,
              float *sharedFeedback = nullptr)
        : ac(a), lists(l), collision(c), effects(e), random(r),
          feedback(sharedFeedback ? *sharedFeedback : localFeedback) {
        if (&e.random != &r)
            throw std::logic_error("Ship and effects require the same original RNG stream");
    }
    virtual ~ShipWorld() = default;
    // Binding belongs to the game, not the Ship: shots can outlive the Ship.
    void bindShip(Ship &);
    virtual void unbindShip(Ship &);
    BulletWorld &bulletBindings();
    virtual ShipInput readInput() = 0; // getVJoy at entry, including dying/departing
    virtual void sound(int32_t) {
        pending();
    }
    virtual void stopSound(int32_t) {
        pending();
    }
    virtual void achievement(int32_t) {
        pending();
    }
    virtual void livesChanged(int32_t) {
        pending();
    }
    // Original score/achievement bookkeeping; bind local scores deliberately.
    virtual void finishGame() {
        pending();
    }

  private:
    std::unique_ptr<BulletWorld> bullets_;
    [[noreturn]] static void pending() {
        throw std::logic_error("Original Ship game service not bound");
    }
};
} // namespace gr::reconstruction::ios
