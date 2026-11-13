#pragma once
#include "neon/controller_session.h"
namespace minotron::module {
struct ControllerInput {
    neon::input::DigitalDirections move, aim;
    bool fire = false, advance = false;
    bool any() const {
        return move.any() || fire;
    }
};
// Preserve the existing digital thresholds and positive-direction priority.
// Original correspondence: move at iCade bits 0..3, aim at 4/6/7/8,
// fire/auto-aim at bit9. The running game no longer reads that protocol word.
inline ControllerInput controllerInput(const neon::input::ControllerFrame &frame) {
    using namespace neon::input;
    ControllerInput out;
    for (const auto &p : frame.slots()) {
        if (!p.connected)
            continue;
        out.move.left |= p.lx < -.4f || bool(p.buttons & Left);
        out.move.right |= p.lx > .4f || bool(p.buttons & Right);
        out.move.up |= p.ly < -.4f || bool(p.buttons & Up);
        out.move.down |= p.ly > .4f || bool(p.buttons & Down);
        out.aim.left |= p.rx < -.4f || bool(p.buttons & West);
        out.aim.right |= p.rx > .4f || bool(p.buttons & East);
        out.aim.up |= p.ry < -.4f || bool(p.buttons & North);
        out.aim.down |= p.ry > .4f || bool(p.buttons & South);
        out.fire |=
            bool(p.buttons & (Start | LeftShoulder | RightShoulder)) || p.lt > .5f || p.rt > .5f;
    }
    out.fire |= out.aim.any();
    out.advance = out.move.left || out.move.right || out.fire;
    return out;
}
inline ControllerInput controllerInput(const neon::input::ControllerState *pads, int count) {
    neon::input::ControllerFrame frame;
    frame.capture(pads, count);
    return controllerInput(frame);
}
} // namespace minotron::module
