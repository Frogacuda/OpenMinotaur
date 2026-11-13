#pragma once
#include "world.h"
namespace superoxwars {
// iOS settings: highest unlocked absolute area; independent blue/red start
// scores and best life count for each of the 24 selectable starting areas.
struct ResumePoint {
    int blue = 0, red = 0, lives = 1;
};
struct Records {
    int unlocked = 0, blueBest = 0, redBest = 0, pureBest = 0, blueCollected = 0, redCollected = 0;
    uint32_t achievements = 0;
    int touchMode = 0; // original settings +c88: relative (0) or anchored stick (1)
    bool tiltEnabled = true;
    std::array<ResumePoint, 24> starts{};
    Records() {
        starts[0].lives = 3;
    }
    void checkpoint(const World &, int nextLevel);
    void captureTotals(const World &);
    void bank(const World &, bool fromBeginning);
    bool load(const std::string &);
    bool save(const std::string &) const;
};
} // namespace superoxwars
