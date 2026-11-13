#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neon::effects {
enum class Framing { LegacyStretch, ExtendReference, PreserveVertical, PreserveHorizontal };
struct ScreenFraming {
    Framing mode = Framing::ExtendReference;
    float referenceAspect = 4.f / 3;
};
struct ScreenLayout {
    int width = 1, height = 1, turns = 0; // native effect axes, before final clockwise rotation
    float extentX = 1, extentY = 1;       // visible extent relative to authored reference frame
};
inline ScreenLayout screenLayout(int width, int height, int clockwiseTurns,
                                 ScreenFraming framing = {}) {
    if (width < 1 || height < 1 || !std::isfinite(framing.referenceAspect) ||
        framing.referenceAspect < .1f || framing.referenceAspect > 10 || int(framing.mode) < 0 ||
        int(framing.mode) > 3)
        throw std::runtime_error("Invalid effect screen framing");
    ScreenLayout r;
    r.turns = (clockwiseTurns % 4 + 4) % 4;
    r.width = (r.turns & 1) ? height : width;
    r.height = (r.turns & 1) ? width : height;
    const float ratio = float(r.width) / r.height / framing.referenceAspect;
    if (framing.mode == Framing::ExtendReference) {
        r.extentX = std::max(1.f, ratio);
        r.extentY = std::max(1.f, 1 / ratio);
    }
    if (framing.mode == Framing::PreserveVertical)
        r.extentX = ratio;
    if (framing.mode == Framing::PreserveHorizontal)
        r.extentY = 1 / ratio;
    return r;
}
// Normalized top-left coordinates. Matches DisplayLayout::turns in iOX.
inline void rotateScreenPoint(float &x, float &y, int turns) {
    const float a = x, b = y;
    switch ((turns % 4 + 4) % 4) {
    case 1:
        x = 1 - b;
        y = a;
        break;
    case 2:
        x = 1 - a;
        y = 1 - b;
        break;
    case 3:
        x = b;
        y = 1 - a;
        break;
    }
}
} // namespace neon::effects
