#pragma once
#include "neon/math.h"
#include <cstdint>

namespace goatup2 {
struct MovementAnimationView {
    int32_t freezeTicks;
    const neon::vec2d &travelled;
    const neon::vec2d &weights;
    int32_t mode, baseSprite;
    double savedDrawScaleX, localX;
    uint8_t sharedFrameFlags;
    double &drawScaleX;
    float &heading;
    int32_t &sprite;
};
// Complete 0x58b48 finite-state operation; inputs are actual original fields.
void updateMovementAnimation(MovementAnimationView actor);
} // namespace goatup2
