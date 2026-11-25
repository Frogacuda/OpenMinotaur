#pragma once
#include "core_api.h"
#include "engine/game_entities.h"
#include <array>
#include <map>

namespace minorescue::module {
// Samples original state at simulation boundaries. Counters belong to the
// presentation adapter; none are written into saves, scores or game rules.
class PresentationObserver {
  public:
    void beforeTick(Neon_Internals::spaceWars &);
    void afterTick(Neon_Internals::spaceWars &);
    mino::PresentationState sample(const Neon_Internals::spaceWars *) const;
    mino::PresentationState widgets(const Neon_Internals::spaceWars *) const;

  private:
    struct CraftState {
        int joy = 0, hp = 0, life = 0;
        float spin = 0;
    };
    struct RescuePoints {
        int64_t earned = 0, reference = 0;
        double efficiency() const {
            return reference > 0 ? double(earned) / reference : -1.;
        }
    };
    std::map<const Neon_Internals::entity *, CraftState> before_;
    std::array<int, 4> lives_{};
    std::array<uint64_t, 4> hits_{};
    uint64_t session_ = 0, center_ = 0, rescued_ = 0;
    bool active_ = false;
    bool betweenLevels_ = false, minesSeen_ = false;
    int64_t livesLost_ = 0, minotaursLost_ = 0;
    RescuePoints levelRescue_, gameRescue_;
    double heading_ = 0, boost_ = 0;
    static bool playing(const Neon_Internals::spaceWars &);
    static bool widgetMode(const Neon_Internals::spaceWars &);
    void beginSession();
    void resetLevelIfStarted(const Neon_Internals::spaceWars &);
    static void contact(void *, const Neon_Internals::entity &, const Neon_Internals::entity &);
    static void shipLost(void *, const Neon_Internals::spaceWars &);
};
} // namespace minorescue::module
