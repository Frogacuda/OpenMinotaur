#include "minotron_presentation.h"
#include <algorithm>
#include <cmath>
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
        loveDuration_ = 0;
        active_ = true;
    }
    beforeState_ = g.state;
    beforeBombs_ = g.bombsCollected;
    beforeLives_ = g.lives[0];
    beforeLove_ = g.beastLoveFrames;
}
void PresentationObserver::afterTick(const theGame &g) {
    const bool active = playing(g);
    if (active && !active_) {
        ++session_;
        completed_ = bombs_ = extraLives_ = 0;
        loveDuration_ = 0;
    }
    active_ = active;
    if (active && !g.paused) {
        if (g.beastLoveFrames <= 0 || g.beastLoveFrames > beforeLove_ || loveDuration_ == 0)
            loveDuration_ = g.beastLoveFrames;
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
mino::PresentationState PresentationObserver::widgets(const theGame *g) const {
    mino::PresentationState state;
    if (!g || !playing(*g))
        return state;
    state.gameplay = true;
    state.paused = g->paused || g->state == 7;
    state.session = session_;
    const auto mode = std::clamp(g->gameMode, 0, 3);
    const auto stage = std::clamp(g->level - int(g->state == 9), 0, 99);
    const auto lives = g->saveData ? g->best(mode, stage, 0) : 0;
    const int board = mode + (g->endurance ? 4 : 0);
    const auto score = std::max<int64_t>(0, g->score[0]);
    const auto nextLife = score < 10000 ? 10000 : (score / 20000 + 1) * 20000;
    int64_t following = 0;
    double invincible = 0;
    for (auto *e = g->players; e; e = e->next) {
        if (e->type != 3 || e->dead)
            continue;
        const auto &player = *static_cast<const playerMinotaur *>(e);
        following += player.rescued;
        if (player.invincible) {
            // The pickup runs through a growth and shrink phase at the same speed.
            const double remaining = player.invDT > 0 ? 20. - player.invT : player.invT;
            invincible = std::max(invincible, std::clamp(remaining, 0., 20.));
        }
    }
    state.values = {
        {"highScore", int64_t(board < g->hs.boards() ? g->hs.entry(board, 0).score : 0)},
        {"stageBestScore", int64_t(lives > 0 ? g->best(mode, stage, 1) : -1)},
        {"stageBestLives", int64_t(lives)},
        {"shotPattern", int64_t(g->spreadShots ? (g->fiveWayShots ? 5 : 3) : 1)},
        {"reflective", g->reflectiveShots},
        {"explosive", (g->levelFlags & 8) != 0},
        {"beasties", following},
        {"beastiesKilled", int64_t(g->f1198[0])},
        {"pickupsThisLevel", int64_t(g->pickupsThisLevel)},
        {"nextExtraLife", nextLife - score},
        {"lifeDropPending", g->forcePowerup},
        {"lifeDropAvailable", !g->practice && g->level <= 98 && g->lives[0] <= 98},
        {"loveFrames", int64_t(g->beastLoveFrames)},
        {"loveDuration", int64_t(std::max(loveDuration_, g->beastLoveFrames))},
        {"invincibleFraction", invincible / 20.},
    };
    auto &history = state.collections["pickupHistory"];
    for (int i = 0; i < g->pickupHistoryCount; ++i)
        history.push_back({uint64_t(i), 0, g->pickupHistory[i]});
    auto &expiring = state.collections["expiringPickups"];
    for (auto *e = g->beasts; e; e = e->next) {
        if (e->type != 2 || e->dead)
            continue;
        const auto &pickup = *static_cast<const powerup *>(e);
        expiring.push_back(
            {expiring.size(), std::lround(std::clamp(pickup.life, 0.f, 1.f) * 10000), pickup.kind});
    }
    std::stable_sort(expiring.begin(), expiring.end(),
                     [](const auto &a, const auto &b) { return a.value < b.value; });
    return state;
}
} // namespace minotron::module
