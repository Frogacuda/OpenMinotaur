#pragma once
#include "core_api.h"
#include "world.h"
#include <algorithm>

namespace superoxwars {
// The overlay observes the game's retained scores, followers and resolved tilt.
inline mino::PresentationState presentationSnapshot(const World &world, uint64_t session,
                                                    bool active, bool paused) {
    mino::PresentationState state;
    state.gameplay = active;
    state.paused = paused;
    state.session = session;
    const double red = std::max(0, world.redScore), blue = std::max(0, world.blueScore);
    const double total = red + blue;
    const double redShare = total > 0 ? red / total : .5;
    state.values = {{"score.red", int64_t(world.redScore)},
                    {"score.blue", int64_t(world.blueScore)},
                    {"score.redShare", redShare},
                    {"score.blueShare", 1 - redShare},
                    {"player.followers", int64_t(world.player ? world.player->power : 0)},
                    {"world.tilt", world.layerTilt}};
    return state;
}
} // namespace superoxwars
