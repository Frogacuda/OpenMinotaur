#include "deflector_controls.h"

namespace deflex {
void DeflectorInput::captureControllers(const neon::input::ControllerState *pads, int count) {
    nativeHeld = {false, false};
    for (int i = 0; i < count; ++i)
        if (pads[i].connected) {
            const auto &p = pads[i];
            nativeHeld[0] |=
                (p.buttons & (neon::input::South | neon::input::Left)) != 0 || p.lx < -0.5f;
            nativeHeld[1] |=
                (p.buttons & (neon::input::East | neon::input::Right)) != 0 || p.lx > 0.5f;
        }
}
DeflectorButton::DeflectorButton(int side) : orientation(side) {
    position = {side ? 0.4 : -0.4, 0.4};
    spriteScale.x = side ? -1.0 : 1.0;
}
void DeflectorButton::run(DeflectorInput &input, DeflectorServices &services) {
    // deflectorButton::run 5ec10; completion marks retirement but does not skip
    // the remainder of the current invocation.
    if (input.levelFinished)
        dead = true;
    if (flashFrames < 1) {
        colour = input.palette[4];
        colour.a = 0.5;
    } else {
        colour = input.palette[0];
        --flashFrames;
    }
    bool contact = input.nativeHeld[size_t(orientation)];
    for (const auto &touch : input.touches)
        if (touch.down && (touch.position - position).length() < double(0.2f))
            contact = true;
    const bool activated = contact && !contactLatch;
    if (activated)
        flashFrames = 20;
    contactLatch = contact;
    if (phase == 0) {
        size = float(double(entranceAmplitude) * 0.3 + 0.08);
        entranceAmplitude = float(double(entranceAmplitude) * 0.9);
        if (double(entranceAmplitude) < 0.01) {
            entranceAmplitude = 0;
            phase = 1;
        }
    } else if (phase == 1) {
        if (double(size) > 0.08)
            size = float(double(size) * 0.95);
        if (activated) {
            size = 0.2f;
            if (!input.pending) {
                input.pending = orientation;
                if (input.particleCost > 0)
                    services.derezAllBalls(input.particleCost, 0.1f, 0.975f);
            }
        }
    }
}
} // namespace deflex
