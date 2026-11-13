#include "animation.h"
#include "neon/integer.h"
#include <cmath>
#include <limits>

namespace goatup2 {
namespace {
int32_t truncateTravel(float value) {
    // VCVT.S32.F32 saturates a positive finite result outside signed range.
    if (double(value) >= double(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    return int32_t(value);
}
} // namespace
void updateMovementAnimation(MovementAnimationView a) {
    if (a.freezeTicks > 0)
        return;
    const double x = a.travelled.x * a.weights.x, y = a.travelled.y * a.weights.y;
    const float travel = float(std::sqrt(x * x + y * y));
    int32_t frame = 0;
    switch (a.mode) {
    case 1:
        frame = int32_t(uint32_t(truncateTravel(travel)) & 1U);
        break;
    case 2:
        a.drawScaleX = (a.sharedFrameFlags & 0x40) ? a.savedDrawScaleX : -a.savedDrawScaleX;
        return;
    case 3:
        frame = int32_t(uint32_t(truncateTravel(travel)) & 3U);
        if (frame == 3)
            frame = 1;
        break;
    case 4:
        a.heading = float(double(std::sin(float(double(travel) * .6))) * .3);
        break;
    case 5:
        a.heading = float(double(travel) * .4);
        break;
    case 6:
        a.drawScaleX =
            (uint32_t(truncateTravel(travel)) & 1U) ? a.savedDrawScaleX : -a.savedDrawScaleX;
        return;
    case 7:
        frame = int32_t((uint32_t(truncateTravel(travel * 6.f)) & 0xffffU) % 7U);
        if (frame > 3)
            frame = 6 - frame;
        break;
    case 8:
        frame = int32_t((uint32_t(truncateTravel(travel)) & 0xffffU) % 7U);
        break;
    case 9:
        a.heading = float(a.localX * -20.);
        break;
    default:
        return;
    }
    a.sprite = neon::integers::add32(a.baseSprite, frame);
}
} // namespace goatup2
