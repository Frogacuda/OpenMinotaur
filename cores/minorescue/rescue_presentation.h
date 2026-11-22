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

  private:
    struct CraftState {
        int joy = 0, hp = 0, life = 0;
        float spin = 0;
    };
    std::map<const Neon_Internals::entity *, CraftState> before_;
    std::array<int, 4> lives_{};
    std::array<uint64_t, 4> hits_{};
    uint64_t session_ = 0, center_ = 0, rescued_ = 0;
    bool active_ = false;
    double heading_ = 0, boost_ = 0;
    static bool playing(const Neon_Internals::spaceWars &);
    void beginSession();
    static void contact(void *, const Neon_Internals::entity &, const Neon_Internals::entity &);
};
} // namespace minorescue::module
