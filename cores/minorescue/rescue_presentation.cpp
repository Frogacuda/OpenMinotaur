#include "rescue_presentation.h"
#include <algorithm>
#include <cmath>
namespace minorescue::module {
using namespace Neon_Internals;
namespace {
constexpr double turn = 6.283185307179586;
}
bool PresentationObserver::playing(const spaceWars &g) {
    return !g.demo && g.running && (g.state == 4 || g.state == 6 || g.state == 7);
}
void PresentationObserver::beginSession() {
    ++session_;
    hits_ = {};
    center_ = rescued_ = 0;
    heading_ = boost_ = 0;
}
void PresentationObserver::contact(void *context, const entity &source, const entity &target) {
    auto &self = *static_cast<PresentationObserver *>(context);
    const auto *g = source.game;
    if (!g || !playing(*g) || g->paused || source.type != 1)
        return;
    if (g->subMode == 0 && &target == g->sun && g->blackHole)
        ++self.center_;
    // Tanks do not update the ship modes' rescued statistic. An accepted tank
    // pickup marks this minotaur dead; refused/invulnerable contacts do not.
    if (g->subMode == 1 && target.type == 4 && target.dead)
        ++self.rescued_;
}
void PresentationObserver::beforeTick(spaceWars &g) {
    g.presentationContext = this;
    g.presentationContact = contact;
    if (playing(g) && !active_) {
        beginSession();
        active_ = true;
    }
    before_.clear();
    std::copy(std::begin(g.lives), std::end(g.lives), lives_.begin());
    if (!active_ || g.subMode == 0)
        return;
    for (auto *e = g.players; e; e = e->next) {
        if (e->type != 1)
            continue;
        auto &c = *static_cast<craft *>(e);
        if (g.subMode == 1) {
            auto &t = static_cast<tank &>(c);
            before_[e] = {c.joy, e->hp, 0, t.spin};
        } else {
            auto &j = static_cast<jet &>(c);
            before_[e] = {c.joy, e->hp, j.life, j.spin};
        }
    }
}
void PresentationObserver::afterTick(spaceWars &g) {
    const bool active = playing(g);
    if (active && !active_)
        beginSession();
    active_ = active;
    if (!active || g.paused)
        return;
    std::array<unsigned, 4> detected{};
    const craft *primary = nullptr;
    for (auto *e = g.players; e; e = e->next) {
        if (e->type != 1)
            continue;
        auto &c = *static_cast<craft *>(e);
        if (c.joy < 0 || c.joy >= 4)
            continue;
        if (c.joy == 0 && !e->dead &&
            (!primary || (g.subMode == 2 && static_cast<const jet &>(c).slot <
                                                     static_cast<const jet *>(primary)->slot)))
            primary = &c;
        auto old = before_.find(e);
        if (old == before_.end())
            continue;
        if (g.subMode == 1) {
            auto &t = static_cast<const tank &>(c);
            if (old->second.spin <= 0 && t.spin > 0)
                ++detected[c.joy];
        } else if (g.subMode == 2) {
            auto &j = static_cast<const jet &>(c);
            if (j.hp < old->second.hp || j.life > old->second.life)
                ++detected[c.joy];
        }
    }
    for (size_t joy = 0; joy < hits_.size(); ++joy) {
        // The last destroyed jet may already have left the entity list.
        if (g.subMode == 2 && g.lives[joy] < lives_[joy])
            detected[joy] = std::max(detected[joy], unsigned(lives_[joy] - g.lives[joy]));
        hits_[joy] += detected[joy];
    }
    boost_ = 0;
    if (primary) {
        heading_ = std::fmod(double(primary->drawAngle), turn);
        if (heading_ < 0)
            heading_ += turn;
        if (g.subMode == 0) {
            // Ship reload's distance-dependent factor bottoms out at 5:
            // max(5, 30 - 2 * weapon), reached at weapon level 13.
            auto &s = static_cast<const spaceCraft &>(*primary);
            boost_ = std::clamp(s.weapon / 13., 0., 1.);
        } else if (g.subMode == 1) {
            auto &t = static_cast<const tank &>(*primary);
            boost_ = std::clamp(t.powerLevel * 8. / 60., 0., 1.);
        }
    }
}
mino::PresentationState PresentationObserver::sample(const spaceWars *g) const {
    mino::PresentationState s;
    if (!g)
        return s;
    s.gameplay = playing(*g);
    s.paused = g->paused || g->state == 7;
    s.session = session_;
    s.values = {{"player.orientation", heading_},
                {"player.fireBoost", boost_},
                {"level", int64_t(g->level)},
                {"minotaurs.rescued", g->subMode == 0 ? uint64_t(std::max(0, g->rescued)) : rescued_},
                {"center.contacts", center_},
                {"player1.hits", hits_[0]},
                {"player2.hits", hits_[1]}};
    return s;
}
} // namespace minorescue::module
