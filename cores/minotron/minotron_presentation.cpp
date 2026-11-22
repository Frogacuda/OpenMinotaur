#include "minotron_presentation.h"
namespace minotron::module {
using namespace Neon_Internals;
bool PresentationObserver::playing(const theGame &g) {
    return g.running && !g.demo &&
           (g.state == 4 || g.state == 6 || g.state == 7 || g.state == 8 || g.state == 9);
}
void PresentationObserver::beforeTick(const theGame &g) {
    if (playing(g) && !active_) {
        ++session_;
        completed_ = bombs_ = extraLives_ = 0;
        active_ = true;
    }
    beforeState_ = g.state;
    beforeBombs_ = g.bombsCollected;
    beforeLives_ = g.lives[0];
}
void PresentationObserver::afterTick(const theGame &g) {
    const bool active = playing(g);
    if (active && !active_) {
        ++session_;
        completed_ = bombs_ = extraLives_ = 0;
    }
    active_ = active;
    if (active && !g.paused) {
        if (beforeState_ != 9 && g.state == 9)
            ++completed_;
        int bombDelta =
            beforeState_ == 8 && g.state == 4 ? g.bombsCollected : g.bombsCollected - beforeBombs_;
        if (bombDelta > 0)
            bombs_ += uint64_t(bombDelta);
        if (g.lives[0] > beforeLives_)
            extraLives_ += uint64_t(g.lives[0] - beforeLives_);
    }
}
mino::PresentationState PresentationObserver::sample(const theGame *g) const {
    mino::PresentationState state;
    if (!g)
        return state;
    bool invincible = false;
    for (auto *e = g->players; e; e = e->next)
        if (e->type == 3 && !e->dead)
            invincible |= static_cast<const playerMinotaur *>(e)->invincible;
    state.gameplay = playing(*g);
    state.paused = g->paused || g->state == 7;
    state.session = session_;
    state.values = {{"level", int64_t(g->level)},
                    {"levels.completed", completed_},
                    {"bombs.used", bombs_},
                    {"extraLives.gained", extraLives_},
                    {"player.shots", int64_t(g->spreadShots ? (g->fiveWayShots ? 5 : 3) : 1)},
                    {"player.invincible", invincible}};
    return state;
}
} // namespace minotron::module
