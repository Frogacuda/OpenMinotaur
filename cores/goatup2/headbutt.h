#pragma once
#include "neon/integer.h"
#include <cstdint>

namespace goatup2 {
// 0x58e5c's selection/gameplay dispatch over the actual source target list.
// Target fields: next, serial, headbuttState, dead, category, rewardValue,
// stagePosition, savedStagePosition. No secondary list or actor snapshot.
// Output must synchronously perform the real explosion/score-label factories;
// neither dependency is supplied as a default/no-op gameplay implementation.
template <class Target, class Output>
bool dispatchHeadbutt(Target *targets, bool disabled, int32_t ledgeGrace, int32_t sharedGrace,
                      int32_t &multiplier, int32_t &firstGoatCounter, Output &output) {
    if (disabled)
        return false;
    if (ledgeGrace > 0 || sharedGrace > 0)
        return true;
    int32_t best = -1;
    Target *selected = nullptr;
    for (auto *target = targets; target; target = target->next) {
        if (target->serial > best && target->headbuttState == 1) {
            best = target->serial;
            selected = target;
        }
    }
    if (!selected)
        return false;
    selected->dead = true;
    output.explodeHeadbuttTarget(*selected, 10);
    if (selected->category == 2) {
        multiplier = neon::integers::add32(multiplier, -1);
        selected->rewardValue = multiplier;
        output.createHeadbuttMultiplierLabel(*selected, multiplier);
    }
    if (selected->category == 15) {
        selected->dead = false;
        selected->serial = -1;
        selected->headbuttState = 2;
        selected->stagePosition = selected->savedStagePosition;
    }
    firstGoatCounter = neon::integers::add32(firstGoatCounter, -1);
    return true;
}
} // namespace goatup2
