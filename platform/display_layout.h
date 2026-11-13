// Pure presentation/input policy. The game always renders in native axes;
// only the final presentation and its inverse input mapping are rotated.
#pragma once
#include "core_api.h"
#include "settings.h"
#include <algorithm>
#include <cmath>

namespace mino {
inline bool portraitLauncher(Orientation orientation, int windowW, int windowH) {
    if (orientation == Orientation::Auto || orientation == Orientation::BestFit)
        return windowH > windowW;
    return orientation != Orientation::Landscape;
}
inline const char *orientationHint(Orientation orientation, bool launcher, NativeFormat format) {
    const bool native = orientation == Orientation::Auto || orientation == Orientation::BestFit;
    if (launcher && native)
        return "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown";
    const bool landscape =
        orientation == Orientation::Landscape || (native && format.width >= format.height);
    if (launcher)
        return landscape ? "LandscapeLeft LandscapeRight" : "Portrait PortraitUpsideDown";
    // Android's activity locks this allowed axis while a game is active.
    return landscape ? "LandscapeLeft LandscapeRight MinotaurLocked"
                     : "Portrait PortraitUpsideDown MinotaurLocked";
}
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};
struct DisplayLayout {
    Rect view;
    int width = 2, height = 2;
    int turns = 0; // quarter-turns clockwise: -1, 0, +1
    void windowToSurface(float wx, float wy, float *sx, float *sy) const {
        const float u = (wx - view.x) / std::max(1, view.w),
                    v = (wy - view.y) / std::max(1, view.h);
        float a = turns > 0 ? v : turns < 0 ? 1 - v : u, b = turns > 0 ? 1 - u : turns < 0 ? u : v;
        *sx = a * width;
        *sy = b * height;
    }
    MotionState nativeMotion(MotionState value) const {
        // Unlike sticks/touches, motion's Y axis points upward.
        return neon::input::rotateMotion(value, turns);
    }
    PadState nativePad(PadState p) const {
        if (!turns)
            return p;
        const auto rotate = [&](uint32_t b) {
            const uint32_t mask = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT;
            if (turns > 0)
                return (b & ~mask) | ((b & PAD_UP) ? PAD_LEFT : 0u) |
                       ((b & PAD_DOWN) ? PAD_RIGHT : 0u) | ((b & PAD_LEFT) ? PAD_DOWN : 0u) |
                       ((b & PAD_RIGHT) ? PAD_UP : 0u);
            return (b & ~mask) | ((b & PAD_UP) ? PAD_RIGHT : 0u) |
                   ((b & PAD_DOWN) ? PAD_LEFT : 0u) | ((b & PAD_LEFT) ? PAD_UP : 0u) |
                   ((b & PAD_RIGHT) ? PAD_DOWN : 0u);
        };
        const float sign = turns > 0 ? 1.f : -1.f;
        float x = p.lx;
        p.lx = sign * p.ly;
        p.ly = -sign * x;
        x = p.rx;
        p.rx = sign * p.ry;
        p.ry = -sign * x;
        p.buttons = rotate(p.buttons);
        p.pressed = rotate(p.pressed);
        p.released = rotate(p.released);
        return p;
    }
};
inline DisplayLayout displayLayout(int windowW, int windowH, NativeFormat native,
                                   const DisplaySettings &s) {
    const int ww = std::max(1, windowW), wh = std::max(1, windowH);
    const bool landscape = native.width >= native.height;
    DisplayLayout result;
    const bool vertical =
        s.orientation == Orientation::Portrait || s.orientation == Orientation::PortraitRight;
    if (vertical) {
        // A wide framebuffer on a physically turned cabinet is a virtual
        // portrait canvas. Already portrait OS output needs no extra turn.
        if (ww >= wh)
            result.turns = s.orientation == Orientation::Portrait ? 1 : -1;
    } else if (s.orientation == Orientation::BestFit || s.orientation == Orientation::Auto) {
        // Native turns the game to match the available screen orientation.
        if (landscape != (ww >= wh))
            result.turns = 1;
    }
    // Horizontal keeps original game axes upright; never rotates to fill.
    // Output aspect is a presentation choice, independent of game coordinates.
    const float nativeAspect = float(native.width) / std::max(1, native.height);
    float aspect = s.aspect == AspectMode::IPad ? (landscape ? 4.f / 3 : 3.f / 4)
                                                : (landscape ? 3.f / 2 : 2.f / 3);
    if (s.aspect == AspectMode::Fill)
        aspect = result.turns != 0 ? float(wh) / ww : float(ww) / wh;
    const float shownAspect = result.turns != 0 ? 1 / aspect : aspect;
    int vh = std::max(1, int(std::min(ww / shownAspect, float(wh))));
    int vw = std::max(1, std::min(ww, int(std::lround(vh * shownAspect))));
    if (s.aspect == AspectMode::Fill) {
        vw = ww;
        vh = wh;
    }
    if (s.matchViewportPixels) {
        result.width = result.turns != 0 ? vh : vw;
        result.height = result.turns != 0 ? vw : vh;
        result.view = {(ww - vw) / 2, (wh - vh) / 2, vw, vh};
        return result;
    }
    const bool low = s.screenMode != ScreenMode::HiRes;
    result.height = low ? (landscape ? 320 : 480)
                        : std::max(2, (s.internalScale == 0 ? (result.turns != 0 ? vw : vh)
                                                            : native.height * s.internalScale) &
                                          ~1);
    result.width = low ? (landscape ? 480 : 320)
                       : std::max(2, int(std::lround(result.height * nativeAspect)) & ~1);
    if (s.scale == ScaleMode::Integer && s.internalScale != 0) {
        const int pw = result.turns != 0 ? result.height : result.width;
        const int ph = result.turns != 0 ? result.width : result.height;
        const int k = std::min(ww / pw, wh / ph);
        if (k >= 1) {
            vw = pw * k;
            vh = ph * k;
        }
    }
    result.view = {(ww - vw) / 2, (wh - vh) / 2, vw, vh};
    return result;
}
} // namespace mino
