#pragma once
#include "neon/input.h"
#include <array>
namespace frozen_mechanics {
// Preserve existing assignment: any pad can control solo play, alternating
// pads share the two player slots, and four-player play uses matching slots.
// A held control keeps its player until release; touches and pads claim slots
// independently, without manufacturing touch identities or positions.
class ControllerRouting {
  public:
    ControllerRouting() {
        cancel();
    }
    const neon::input::PlayerMotion &motion(int player) const {
        return motion_[player];
    }
    void clearOwners() {
        owner_.fill(-1);
        motion_.fill({});
    }
    void cancel() {
        clearOwners();
        held_.fill(false);
    }
    void update(const std::array<neon::input::PlayerMotion, 4> &input, int players, bool flipped,
                bool enabled, const bool *touchActive) {
        for (int pad = 0; pad < 4; ++pad) {
            bool want = enabled && input[pad].active;
            // Releases and claims occur in pad order, matching the previous
            // control path when several devices contend for one player.
            if (!want && held_[pad])
                for (int player = 0; player < 4; ++player)
                    if (owner_[player] == pad) {
                        owner_[player] = -1;
                        motion_[player] = {};
                    }
            if (want && !held_[pad]) {
                int player = players == 2 ? pad % 2 : players >= 3 && players <= 4 ? pad : 0;
                if (flipped && players >= 2 && players <= 4)
                    player ^= 1;
                if (!touchActive[player] && owner_[player] < 0)
                    owner_[player] = pad;
            }
            held_[pad] = want;
        }
        for (int player = 0; player < 4; ++player)
            if (owner_[player] >= 0) {
                const auto &in = input[owner_[player]];
                const double scale = flipped ? -.12 : .12;
                double x = in.x * scale, y = in.y * scale;
                // The previous displacement subtraction produced positive zero.
                // Keep its cardinal-heading convention when controls are flipped.
                if (x == 0)
                    x = 0.;
                if (y == 0)
                    y = 0.;
                motion_[player] = {true, x, y};
            }
    }

  private:
    std::array<int, 4> owner_;
    std::array<bool, 4> held_{};
    std::array<neon::input::PlayerMotion, 4> motion_{};
};
} // namespace frozen_mechanics
