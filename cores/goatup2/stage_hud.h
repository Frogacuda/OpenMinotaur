#pragma once
#include "stage_runtime.h"
namespace goatup2 {
struct HudText {
    int column, row;
    std::string text;
    neon::colour colour;
};
// Active-stage branch of 0x42f28. Text reads the PRE-update player/score/palette;
// graphical HUD bands are separately prepared after the actor/terrain passes.
class StageHud {
  public:
    void prepare(StageRuntime &, int bestTime, uint32_t frame);
    static std::string timeText(const char *label, int ticks);
    std::vector<int> clearRows;
    std::vector<HudText> text;
};
} // namespace goatup2
