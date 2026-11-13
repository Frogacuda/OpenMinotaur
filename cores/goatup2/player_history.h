#pragma once
#include "neon/integer.h"
#include "neon/math.h"
#include <array>

namespace goatup2 {
struct CheckpointSample {
    neon::vec2d position;
    float gravity;
};

// Original goat's 64-slot checkpoint history: factory 0x4d9a8, checkpoint
// branch of 0x5a280 and state-3 reset in 0x59238. Not follower history.
struct GoatCheckpointHistory {
    std::array<neon::vec2d, 64> positions;
    std::array<float, 64> gravities;
    int32_t cursor;

    GoatCheckpointHistory(neon::vec2d spawn, float gravity) : cursor(0) {
        positions.fill(spawn);
        gravities.fill(gravity);
    }
    void record(neon::vec2d position, float gravity) {
        const auto index = uint32_t(cursor) & 63U;
        positions[index] = position;
        gravities[index] = gravity;
        cursor = neon::integers::add32(cursor, 1);
    }
    CheckpointSample latest() const {
        const auto index = (uint32_t(cursor) + 63U) & 63U;
        return {positions[index], gravities[index]};
    }
};
} // namespace goatup2
