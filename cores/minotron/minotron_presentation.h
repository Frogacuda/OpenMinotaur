#pragma once
#include "core_api.h"
#include "engine/game_entities.h"

namespace minotron::module {
// Read-only values and transient counters owned by the presentation adapter.
class PresentationObserver {
  public:
    void beforeTick(const Neon_Internals::theGame &);
    void afterTick(const Neon_Internals::theGame &);
    mino::PresentationState sample(const Neon_Internals::theGame *) const;
    mino::PresentationState widgets(const Neon_Internals::theGame *) const;

  private:
    uint64_t session_ = 0, completed_ = 0, bombs_ = 0, extraLives_ = 0;
    bool active_ = false;
    int beforeState_ = 0, beforeBombs_ = 0, beforeLives_ = 0;
    int beforeLove_ = 0, loveDuration_ = 0;
    static bool playing(const Neon_Internals::theGame &);
};
} // namespace minotron::module
