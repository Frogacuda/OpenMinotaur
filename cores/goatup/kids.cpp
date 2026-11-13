#include "kids.h"
#include "original_tables.h"
#include "neon/entity_list.h"
#include <algorithm>
#include <stdexcept>

namespace goatup {
namespace {
template <class T> void dispose(T *p) {
    delete p;
}
} // namespace
struct Kids::Kid {
    Kid *prev = nullptr;
    Kid *next = nullptr;
    bool dead = false, released = false, touching = false;
    Kids &owner;
    ActorDraw image;
    Vec2 velocity{};
    int ordinal = 0, platform = -1, remaining = 0;
    explicit Kid(Kids &k) : owner(k) {}
    void run() {
        auto &c = owner.context_;
        auto &state = owner.state_;
        if (!released) {
            if (state.tally && state.releaseOrdinal == ordinal) {
                velocity = {c.random.range(-.003, .003), c.random.range(-.01, -.008)};
                released = true;
                remaining = int(c.random.next() & 31) + 15;
                return;
            }
            const auto &h = owner.history(ordinal);
            image.position = h.position;
            for (const auto &p : c.game.tower.platforms)
                if (h.platform == p.level) {
                    image.position.y = double(float(p.y + h.platformOffset));
                    break;
                }
            platform = h.platform;
            image.scale = h.scale * .75;
            image.frame = h.frame;
            image.angle = h.angle;
            if (image.frame == 12 || image.frame == 8 || image.frame == 10)
                image.frame = 1;
            else if (image.frame > 5)
                image.frame = 5;
            if (!owner.touch)
                throw std::logic_error("GoatUp kid contact traversal not connected");
            owner.touch(image.position, image.radius, 2, touching);
            if (c.pendingKidLoss == ordinal || c.game.gameOver) {
                dead = true;
                const auto colour = original::KidColours[size_t(ordinal & 7)];
                c.effects.splash(150, -.019f, 3.8f, image.position, colour, colour);
                --c.goat.kids;
                c.pendingKidLoss = 0;
                if (!c.game.gameOver) {
                    state.statusKind = 4;
                    state.statusTicks = 60;
                }
            }
        } else {
            image.position += velocity;
            image.angle = float(velocity.y * 50.);
            velocity.y += .00015;
            if (velocity.x >= 0)
                image.angle = -image.angle;
            if (velocity.y > 0 && --remaining < 1) {
                dead = true;
                const int points = state.tallyMultiplier * 10000 * (c.game.laps + 1);
                ++state.tallyMultiplier;
                if (!c.game.gameOver) {
                    c.showScore(points, image.position, float(image.position.x * -.01), -.002f,
                                .05f);
                    c.score += points;
                }
                const auto first = c.random.next(), second = c.random.next();
                constexpr int noises[] = {0, 1, 2, 3, 4, 4};
                c.play(noises[(first & 3) + (second & 1)]);
                c.flash = 1;
                const auto colour = original::KidColours[size_t(ordinal & 7)];
                c.effects.splash(150, -.019f, 3.8f, image.position, colour, colour);
            }
        }
    }
};
Kids::~Kids() {
    reset();
}
const ClimbHistory &Kids::history(int ordinal) const {
    const auto &goat = context_.goat;
    const uint32_t index = (goat.historyCursor - uint32_t(ordinal) * 9u) & 1023;
    if (goat.historyCount < 1024 && index >= goat.historyCount)
        throw std::logic_error("GoatUp kid requested an unwritten history slot");
    return goat.history[index];
}
void Kids::create() {
    auto &c = context_;
    const int ordinal = c.goat.kids + 1;
    const auto &h = history(ordinal); // Resolve dependency before changing live state.
    auto kid = std::make_unique<Kid>(*this);
    kid->ordinal = ordinal;
    ++c.goat.kids;
    state_.mostInRun = std::max(state_.mostInRun, ordinal);
    c.records.mostKids = std::max(c.records.mostKids, ordinal);
    if (ordinal == 10)
        c.award(0);
    if (ordinal == 20)
        c.award(1);
    kid->image.position = h.position;
    kid->image.position.y += .02;
    kid->image.scale = h.scale * .75;
    kid->image.radius = .03f;
    kid->image.frame = h.frame;
    kid->image.angle = h.angle;
    kid->image.colour.unpackRGBA(original::KidColours[size_t(ordinal & 7)]);
    for (int n = 0; n < 20; ++n)
        c.effects.loveHeart(c.goat.heading, c.goat.position.y, .002, .006, c.game.tower.angle);
    state_.statusKind = 3;
    state_.statusTicks = 120;
    neon::entities::prepend(kids_, *kid);
    kid.release();
}
void Kids::tick() {
    neon::entities::runAndCull(kids_, dispose<Kid>);
}
void Kids::draw(std::vector<ActorDraw> &out) const {
    for (auto *k = kids_; k; k = k->next)
        out.push_back(k->image);
}
void Kids::reset() {
    neon::entities::destroyList(kids_, dispose<Kid>);
}
int Kids::count() const {
    int n = 0;
    for (auto *k = kids_; k; k = k->next)
        ++n;
    return n;
}
void treadOnTile(Platform &platform, int tile, DenizenContext &c, Kids &kids) {
    if (tile < 0 || tile >= 40)
        throw std::out_of_range("GoatUp tread cell");
    const int original = platform.tiles[size_t(tile)];
    if (original == 101 || original == 105) {
        if (!c.game.demo)
            c.play(30);
        const int first = tile & ~1;
        platform.tiles[size_t(first)] = platform.tiles[size_t(first + 1)] =
            original == 101 ? 106 : 107;
        if (!c.checkBridge)
            throw std::logic_error("GoatUp bridge callback missing");
        if (c.checkBridge(platform)) {
            c.game.bridgeJustRemoved = true;
            if (!c.game.gameOver) {
                const int points = (c.goat.kids + 1) * 100;
                c.showScore(points, c.goat.position, 0, -.002f, .05f);
                c.score += points;
            }
        }
        return;
    }
    if (original == 100) {
        if (c.game.returning)
            c.checkBridge(platform);
        return;
    }
    if (original == 204) {
        if (!c.game.summitVisited) {
            c.award(2);
            c.game.summitVisited = true;
            c.game.pregnant = false;
            c.goat.pregnancy = 0;
            if (!c.game.demo)
                c.play(48);
            c.game.ticks = 0;
        }
        return;
    }
    const bool crumble = original >= 16 && original < 24;
    const bool revenge = original == 98 || (original >= 108 && original <= 110);
    if (original != 4 && original != 14 && !crumble && !revenge && original != 177 &&
        original != 182 && original != 188 && original != 197)
        return;
    if (crumble) {
        if ((c.game.ticks & 3) == 0) {
            platform.tiles[size_t(tile)] = original == 23 ? 0 : original + 1;
            if (!c.game.demo)
                c.play(16);
            c.effects.splash(1, -.006f, 1.8f, c.goat.position, 0xffc000ffu, 0xff2000ffu);
        }
    } else {
        platform.tiles[size_t(tile)] = revenge           ? 99
                                       : original == 4   ? 2
                                       : original == 14  ? 15
                                       : original == 182 ? 186
                                                         : original + 1;
        if (!c.game.demo)
            c.play(16, original == 4 ? 11 : 12);
    }
    if (!c.game.gameOver)
        c.score += 1 + (platform.level >> 4);
    c.game.climbSpeed = float(double(c.game.climbSpeed) * (crumble           ? 1.0003
                                                           : original == 182 ? 1.004
                                                                             : 1.003));
    if (!crumble) {
        const uint32_t from = original == 14    ? 0x00ffffffu
                              : original == 188 ? 0xff0000ffu
                              : original == 182 ? 0xff0000ffu
                              : original == 197 ? 0x50ff00ffu
                              : original == 177 ? 0xffffffffu
                                                : 0x00ff40ffu;
        const uint32_t to = original == 14    ? 0x0000ffffu
                            : original == 188 ? 0x800000ffu
                            : original == 182 ? 0x4000cfffu
                            : original == 197 ? 0x00ff00ffu
                            : original == 177 ? 0x00ff4fffu
                                              : 0x008000ffu;
        c.effects.splash(4, original == 4 || revenge ? -.005f : -.006f, .8f, c.goat.position, from,
                         to);
    }
    if (c.game.pregnant) {
        const double increment = original == 4 ? .04 : original == 177 ? .01 : .02;
        c.goat.pregnancy = float(double(c.goat.pregnancy) + increment);
        if (c.goat.pregnancy >= 4) {
            c.goat.pregnancy = 0;
            c.game.pregnant = false;
            if (c.pendingKidLoss == 0) {
                kids.create();
                c.game.climbSpeed = float(double(c.game.climbSpeed) * .8);
                if (!c.game.demo)
                    c.play(34);
                ++c.runBirths;
            }
        }
    }
}
} // namespace goatup
