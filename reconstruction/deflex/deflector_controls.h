// Deflex's two original deflector buttons. Native controllers enter the same
// button action directly; they do not manufacture touches.
#pragma once
#include "neon/math.h"
#include "neon/input.h"
#include <array>
#include <optional>

namespace deflex {
struct DeflectorInput {
    struct Touch {
        bool down = false;
        neon::vec2d position;
    };
    std::array<Touch, 4> touches{};
    std::array<bool, 2> nativeHeld{};
    std::array<neon::colour, 10> palette{};
    bool levelFinished = false;
    int particleCost = 0;       // level +600
    std::optional<int> pending; // original +89c and +8dc, capacity one
    // Controller actions: South/Left = left
    // deflector, East/Right = right. Platform remapping applies before this binding.
    void captureControllers(const neon::input::ControllerState *pads, int count);
};
struct DeflectorServices {
    virtual ~DeflectorServices() = default;
    virtual void derezAllBalls(int count, float velocityScale, float decay) = 0;
};
struct DeflectorButton {
    int orientation = 0; // +14, left = 0, right = 1
    neon::vec2d position, spriteScale{1, 1};
    neon::colour colour;
    float size = 0.1f;           // original entity +130
    float entranceAmplitude = 1; // +16c
    int phase = 0;               // +168, entrance then active
    int flashFrames = 50;        // +10
    bool contactLatch = false, dead = false;

    explicit DeflectorButton(int side);
    void run(DeflectorInput &input, DeflectorServices &services);
};
} // namespace deflex
