#pragma once
#include "platform.h"
#include "neon/source_random.h"

namespace goatup {
struct PlatformBirth {
    enum class Kind {
        Bonus,
        BillyGoat,
        Sheep,
        Pickup,
        Baddy2049,
        Goody2049,
        MinerWilly,
        CcDude,
        CcCar,
        CcCat,
        PinkiePie,
        RollingSkull,
        Egg,
        Chick,
        CeDude,
        FlyingCat,
        PlatformBit,
        ConveyorCap
    } kind;
    Platform *platform = nullptr;
    float angle = 0;
    int frame = 0, bonusIndex = -1;
    double horizontalSpeed = 0;
    float verticalOffset = 0;
    Vec2 position{}, velocity{};
    double facing = 1;
    int tile = -1;
};
struct GenerationState {
    bool returning = false, pregnant = false, atTop = false;
    int laps = 0, lastTheme = -1, highestGenerated = 0;
    float climbSpeed = .003f, returnSpeed = .003f, targetClimbSpeed = .003f;
    float pickupSpeedThreshold = .0025f;
};
// All nine zones' platform creation path. Each callback executes
// immediately, at the original constructor/init site. It must not defer RNG use.
class PlatformGenerator {
  public:
    PlatformGenerator(neon::DarwinRandom &rng, GenerationState &state)
        : random(rng), state(state) {}
    std::function<void(const PlatformBirth &)> create;
    std::function<void()> enterZone;
    void generate(Platform &);

  private:
    neon::DarwinRandom &random;
    GenerationState &state;
    int slot(const Platform &) const;
    void birth(Platform &, PlatformBirth::Kind, int frame, int tile, int bonus = -1,
               double speed = 0, float verticalOffset = 0);
    void send(const PlatformBirth &);
};
} // namespace goatup
