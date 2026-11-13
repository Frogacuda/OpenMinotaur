#pragma once
#include "ship.h"
#include "neon/touch_slots.h"

namespace minos {
// controlBlock::processJoyDown/Move/Up (0xec74 / 0xea3c / 0xde00).
// The original keeps three point arrays: +0x10c is updated on motion;
// +0x204 is an adjustable rotation origin; +0x244 retains the initial hit point.
// Binding these explicitly avoids confusing the menu hit point with ship dragging.
class TouchInput {
  public:
    void down(PlayState &g, unsigned id, Vec2 p) {
        syncPurges(g);
        const neon::vec2d point{p.x, p.y};
        const int i =
            neon::input::beginTouch(slots_, id, point, neon::input::TouchTracking::MoveCounter);
        if (i >= 0) {
            g.touchDown[i] = true;
            g.touchFlag[i] = false;
            g.touchTimer[i] = 0;
            g.touchStartX[i] = g.touchNowX[i] = g.touchPos[i][0] = p.x;
            g.touchStartY[i] = g.touchPos[i][1] = p.y;
        }
        neon::input::claimTouchJoystick(slots_, 0, id, point);
        g.touchPressed[0] = slots_.joyActive[0];
    }
    void move(PlayState &g, unsigned id, Vec2 p) {
        syncPurges(g);
        neon::input::moveTouch(slots_, id, neon::vec2d{p.x, p.y},
                               neon::input::TouchTracking::MoveCounter);
        for (int i = 0; i < 4; ++i)
            if (slots_.touchActive[i] && slots_.touchId[i] == id) {
                g.touchStartX[i] = p.x;
                g.touchStartY[i] = p.y;
                g.touchDeltaX[i] = slots_.touchMove[i].x;
                g.touchFlag[i] = slots_.touchFlag[i];
                g.touchTimer[i] = 0;
            }
    }
    void up(PlayState &g, unsigned id) {
        syncPurges(g);
        neon::input::endTouch(slots_, id);
        for (int i = 0; i < 4; ++i)
            g.touchDown[i] = slots_.touchActive[i];
        g.touchPressed[0] = slots_.joyActive[0];
    }
    void cancel(PlayState &g) {
        neon::input::purgeTouchState(slots_);
        g.purgeTouches();
    }

  private:
    void syncPurges(const PlayState &g) {
        // Death, UI transitions and the source watchdog may release the bindings.
        for (int i = 0; i < 4; ++i) {
            if (!g.touchDown[i])
                slots_.touchActive[i] = false;
            if (!g.touchPressed[i])
                slots_.joyActive[i] = false;
        }
    }
    neon::input::TouchState<neon::vec2d> slots_;
};
} // namespace minos
