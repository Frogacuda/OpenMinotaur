#pragma once
#include "neon/controller_session.h"
namespace minorescue::module {
class ControllerRouting : public neon::input::ControllerOwnership {
  public:
    void update(const std::array<neon::input::PlayerMotion, 4> &input, int players, bool flipped,
                bool enabled, const bool *touchActive) {
        ControllerOwnership::update(
            input, enabled,
            [&](int pad) {
                int player = players == 2 ? pad % 2 : players >= 3 && players <= 4 ? pad : 0;
                if (flipped && players >= 2 && players <= 4)
                    player ^= 1;
                return player;
            },
            [&](int player) { return touchActive[player]; },
            [&](const auto &in) {
                const double scale = flipped ? -.12 : .12;
                double x = in.x * scale, y = in.y * scale;
                if (x == 0)
                    x = 0.;
                if (y == 0)
                    y = 0.;
                return neon::input::PlayerMotion{true, x, y};
            });
    }
};
} // namespace minorescue::module
