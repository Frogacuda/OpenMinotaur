#define NEON_GAME minotron
#include "cores/minotron/engine/game.h"
#include "cores/minotron/engine/game_entities.h"
#undef NEON_GAME
#define NEON_GAME minorescue
#include "cores/minorescue/engine/game.h"
#undef NEON_GAME
#include "reconstruction/gridrunner/native_entity.h"
#include "legacy_controller_routing.h"
#include "legacy_mechanics.h"
#include "legacy_gridrunner_touch.h"
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace mt = minotron::Neon_Internals;
namespace mr = minorescue::Neon_Internals;
namespace native = gr::reconstruction::ios;
namespace {
uint32_t seed = 0x44810;
uint32_t randomWord() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
template <class T> bool same(const T &a, const T &b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}
template <class Point> struct OriginalTouch {
    using PointType = Point;
    Point joyOrigin[4]{}, joyDelta[4]{};
    bool joyActive[4]{};
    unsigned joyTouchId[4]{};
    Point touchPos[4]{};
    unsigned touchId[4]{};
    int touchIdleFrames[4]{};
    bool touchActive[4]{}, touchFlag[4]{};
    Point touchLast[4]{}, touchMove[4]{};
    int touchStartFrame[4]{};
    Point touchStartPos[4]{}, touchPrevPos[4]{};
    int frameCount = 0, f2d4 = 0, pauseTapTimer = 0, joyZones = 1;
    bool inputFlipped = false, paused = false, pauseArmed = false, demo = false;
    float flash = 0;
    frozen_mechanics::ControllerRouting controllers;
};
template <class A, class B> void compareTouches(const A &a, const B &b) {
    for (int i = 0; i < 4; ++i) {
#define FIELD(name) CHECK(same(a.name[i], b.name[i]))
        FIELD(joyOrigin);
        FIELD(joyDelta);
        FIELD(joyActive);
        FIELD(joyTouchId);
        FIELD(touchPos);
        FIELD(touchId);
        FIELD(touchIdleFrames);
        FIELD(touchActive);
        FIELD(touchFlag);
        FIELD(touchLast);
        FIELD(touchMove);
        FIELD(touchStartFrame);
        FIELD(touchStartPos);
        FIELD(touchPrevPos);
#undef FIELD
    }
}
template <int Kind, class Actual> void touchTrace() {
    auto actual = std::make_unique<Actual>();
    using Point = typename Actual::PointType;
    OriginalTouch<Point> old;
    const std::array<double, 9> coordinates = {-.5, -.4, -.05, -0., 0., .0001, .05, .4, .5};
    for (int step = 0; step < 12000; ++step) {
        actual->inputFlipped = old.inputFlipped = bool(step & 16);
        actual->pauseArmed = old.pauseArmed = bool(step & 32);
        actual->frameCount = old.frameCount = step;
        actual->paused = old.paused = bool(step & 64);
        if constexpr (Kind == 1)
            actual->f2d4 = old.f2d4 = 17;
        if constexpr (Kind == 2) {
            actual->joyZones = old.joyZones = 1 + step % 4;
            actual->demo = old.demo = bool(step & 128);
            actual->pauseTapTimer = old.pauseTapTimer = step % 31;
            actual->flash = old.flash = .75f;
        }
        const unsigned ids[] = {0, 1, 2, 3, 99, 0x3d0dead};
        const unsigned id = ids[randomWord() % 6];
        const Point point{coordinates[randomWord() % coordinates.size()],
                          coordinates[randomWord() % coordinates.size()]};
        switch (randomWord() % 5) {
        case 0:
            actual->processJoyDown(id, point);
            if constexpr (Kind == 0)
                frozen_mechanics::mtBaseDown(old, id, point);
            if constexpr (Kind == 1)
                frozen_mechanics::mtGameDown(old, id, point);
            if constexpr (Kind == 2)
                frozen_mechanics::rescueDown(old, id, point);
            break;
        case 1:
            actual->processJoyMove(id, point);
            if constexpr (Kind == 0)
                frozen_mechanics::mtBaseMove(old, id, point);
            if constexpr (Kind == 1)
                frozen_mechanics::mtGameMove(old, id, point);
            if constexpr (Kind == 2)
                frozen_mechanics::rescueMove(old, id, point);
            break;
        case 2:
            actual->processJoyUp(id);
            frozen_mechanics::release(old, id);
            break;
        case 3:
            actual->purgeTouches();
            for (int i = 0; i < 4; ++i)
                old.joyActive[i] = old.touchActive[i] = false;
            if constexpr (Kind == 2)
                old.controllers.clearOwners();
            break;
        case 4:
            if constexpr (Kind == 2) {
                std::array<neon::input::PlayerMotion, 4> pads{};
                for (auto &pad : pads) {
                    pad.active = bool(randomWord() & 1);
                    pad.x = double(int(randomWord() % 3) - 1);
                    pad.y = double(int(randomWord() % 3) - 1);
                }
                actual->controllers.update(pads, actual->joyZones, actual->inputFlipped,
                                           !actual->demo, actual->joyActive);
                old.controllers.update(pads, old.joyZones, old.inputFlipped, !old.demo,
                                       old.joyActive);
            }
            break;
        }
        compareTouches(*actual, old);
        CHECK(actual->paused == old.paused);
        if constexpr (Kind == 1)
            CHECK(actual->f2d4 == old.f2d4);
        if constexpr (Kind == 2) {
            CHECK(actual->flash == old.flash && actual->pauseTapTimer == old.pauseTapTimer);
            for (int i = 0; i < 4; ++i) {
                const auto &a = actual->controllers.motion(i);
                const auto &b = old.controllers.motion(i);
                CHECK(a.active == b.active && same(a.x, b.x) && same(a.y, b.y));
            }
        }
    }
    std::cout
        << "Touch binding " << Kind
        << ": 12000 ordered events match the saved reference, including IDs, pause gestures and separate controller owners\n";
}
template <class Point> struct Motion {
    Point pos{}, vel{}, drag{};
    int32_t age = 0;
    bool noWrap = false, wrapped = false;
    double wrapWidth = 1, wrapHeight = 1;
};
void motionTrace() {
    mt::entity m;
    mr::entity r;
    native::Entity g;
    Motion<neon::vec2d> old;
    for (int i = 0; i < 16000; ++i) {
        const double widths[] = {1., .3, 1.00000003, 3., -.5};
        old.wrapWidth = widths[i % 5];
        old.wrapHeight = widths[(i + 1) % 5];
        old.pos = {double(int(randomWord() % 6001) - 3000) / 1000.,
                   double(int(randomWord() % 6001) - 3000) / 1000.};
        old.vel = {double(int(randomWord() % 101) - 50) / 1000.,
                   double(int(randomWord() % 101) - 50) / 1000.};
        old.drag = {.98, (i & 1) ? 1. : -.5};
        if (i % 16 == 0) {
            old.pos.x = double(float(old.wrapWidth * .5));
            old.vel.x = 0;
        }
        if (i % 16 == 1) {
            old.pos.y = std::nextafter(double(float(old.wrapHeight * .5)), 1e9);
            old.vel.y = -0.;
        }
        old.age = i;
        old.noWrap = bool(i & 2);
        old.wrapped = bool(i & 4);
        m.pos = old.pos;
        m.vel = old.vel;
        m.drag = old.drag;
        m.age = old.age;
        m.noWrap = old.noWrap;
        m.wrapped = old.wrapped;
        m.wrapWidth = old.wrapWidth;
        m.wrapHeight = old.wrapHeight;
        r.pos = {old.pos.x, old.pos.y};
        r.vel = {old.vel.x, old.vel.y};
        r.drag = {old.drag.x, old.drag.y};
        r.age = old.age;
        r.noWrap = old.noWrap;
        r.wrapped = old.wrapped;
        r.wrapWidth = old.wrapWidth;
        r.wrapHeight = old.wrapHeight;
        g.position = {old.pos.x, old.pos.y};
        g.velocity = {old.vel.x, old.vel.y};
        g.damping = {old.drag.x, old.drag.y};
        g.age = old.age;
        g.noWrap = old.noWrap;
        g.wrapped = old.wrapped;
        g.wrapExtent = {old.wrapWidth, old.wrapHeight};
        frozen_mechanics::minotronMotion(old);
        m.run();
        r.run();
        g.run();
        CHECK(same(m.pos.x, old.pos.x) && same(m.pos.y, old.pos.y) && same(m.vel.x, old.vel.x) &&
              same(m.vel.y, old.vel.y) && m.age == old.age && m.wrapped == old.wrapped);
        CHECK(same(r.pos.x, old.pos.x) && same(r.pos.y, old.pos.y) && same(r.vel.x, old.vel.x) &&
              same(r.vel.y, old.vel.y) && r.age == old.age && r.wrapped == old.wrapped);
        CHECK(same(g.position.x, old.pos.x) && same(g.position.y, old.pos.y) &&
              same(g.velocity.x, old.vel.x) && same(g.velocity.y, old.vel.y) && g.age == old.age &&
              g.wrapped == old.wrapped);
    }
    m.age = r.age = g.age = INT32_MAX;
    m.run();
    r.run();
    g.run();
    CHECK(m.age == INT32_MIN && r.age == INT32_MIN && g.age == INT32_MIN);
    std::cout
        << "48000 actual base-entity updates match source-order integration/wrapping; defined 32-bit age rollover passed\n";
}
void modernTouchTrace() {
    gridrunner::MultiTouchControl actual;
    gridrunner::LegacyMultiTouchControl old;
    for (int step = 0; step < 16000; ++step) {
        const uint32_t id = randomWord() % 6;
        const gridrunner::Axis point{double(int(randomWord() % 101) - 50) / 100.,
                                     double(int(randomWord() % 101) - 50) / 100.};
        switch (randomWord() % 6) {
        case 0:
            actual.down(id, point);
            old.down(id, point);
            break;
        case 1:
            actual.move(id, point);
            old.move(id, point);
            break;
        case 2:
            actual.up(id, point);
            old.up(id, point);
            break;
        case 3:
            actual.up(id, point, true);
            old.up(id, point, true);
            break;
        case 4:
            actual.cancel();
            old.cancel();
            break;
        case 5:
            actual.begin(point);
            old.begin(point);
            break;
        }
        const auto mode = step & 8 ? gridrunner::TouchMode::Expert : gridrunner::TouchMode::Normal;
        const auto a = actual.tick(mode), b = old.tick(mode);
        CHECK(same(a.x, b.x) && same(a.y, b.y));
        CHECK(actual.active() == old.active());
        for (uint32_t i = 0; i < 6; ++i)
            CHECK(actual.contains(i) == old.contains(i));
    }
    std::cout
        << "16000 modern/Classic multi-touch events match the saved reference across both modes, replacement, cancellation and inertia\n";
}
void originalTouchSelectors() {
    mt::controlBlock control;
    control.frameCount = 100;
    control.touchActive[0] = control.touchActive[1] = control.touchActive[2] = true;
    control.touchStartFrame[0] = control.touchStartFrame[1] = 90;
    control.touchStartFrame[2] = 110;
    control.touchStartPos[0] = {0, 0};
    control.touchStartPos[1] = {-.1, 0};
    control.touchStartPos[2] = {.1, 0};
    CHECK(control.findOldestTouch(-1) == 0 && control.findOldestTouch(0) == 1 &&
          control.findOldestTouch(1) == 0);
    CHECK(control.findLeftTouch() == 1 && control.findRightTouch() == 2);
    control.touchIdleFrames[0] = 240;
    control.touchIdleFrames[1] = 1;
    control.touchIdleFrames[2] = 240;
    control.joyWatchdog();
    CHECK(control.touchActive[0] && control.touchIdleFrames[0] == 241);
    for (int i = 0; i < 3; ++i) {
        control.touchIdleFrames[i] = 240;
        control.joyActive[i] = true;
        control.joyTouchId[i] = unsigned(100 + i);
    }
    control.joyWatchdog();
    for (int i = 0; i < 3; ++i)
        CHECK(!control.touchActive[i] && !control.joyActive[i] &&
              control.joyTouchId[i] == unsigned(100 + i));
    control.touchActive[0] = true;
    control.touchStartFrame[0] = INT32_MAX;
    control.touchId[0] = 77;
    control.touchFlag[0] = false;
    control.processJoyMove(77, {.1, .2});
    CHECK(control.touchStartFrame[0] == INT32_MIN && !control.touchFlag[0]);
    control.frameCount = INT32_MIN;
    control.touchStartFrame[0] = INT32_MAX;
    CHECK(control.findOldestTouch(-1) == 0);
}
template <class Base> struct Contact : Base {
    int tag = 0, action = 0;
    std::vector<int> *trace = nullptr;
    void wasShot(Base *) override {
        trace->push_back(tag * 10 + 1);
    }
    void hitBy(Base *by) override {
        trace->push_back(tag * 10 + 2);
        if (action == 1) {
            by->pos.x += .01;
            this->pos.y -= .02;
        }
        if (action == 2)
            by->dead = true;
        if (action == 3 && this->next)
            this->next->dead = true;
        if (action == 4) {
            this->mass = 3;
            this->size = .04f;
        }
        if (action == 5 && this->next)
            this->next = this->next->next;
    }
};
template <bool Minotron, class Base, class Control> void contactTrace() {
    Control control;
    for (int test = 0; test < 2500; ++test) {
        Contact<Base> a[5], b[5];
        std::vector<int> actual, expected;
        for (int i = 0; i < 5; ++i) {
            auto &n = a[i];
            n.tag = i;
            n.trace = &actual;
            n.action = (test + i) % 6;
            n.pos = {double(int(randomWord() % 41) - 20) / 1000.,
                     double(int(randomWord() % 41) - 20) / 1000.};
            n.vel = {double(i - 2) * .01, double(2 - i) * .02};
            n.size = .02f;
            n.mass = float(i + 1);
            n.elasticity = .75f;
            n.dead = (i == 3) && (test % 3 == 0);
            n.touching = test % 7 == 0;
            n.group = test % 9 == 0 ? 2 : 0;
            n.flags = test % 4;
            if constexpr (Minotron)
                n.drawDelay = (test % 11 == 0 && i == 1) ? 2 : 0;
            auto &old = b[i];
            old.tag = i;
            old.trace = &expected;
            old.action = n.action;
            old.pos = n.pos;
            old.vel = n.vel;
            old.size = n.size;
            old.mass = n.mass;
            old.elasticity = n.elasticity;
            old.dead = n.dead;
            old.touching = n.touching;
            old.group = n.group;
            old.flags = n.flags;
            if constexpr (Minotron)
                old.drawDelay = n.drawDelay;
            if (i > 0 && i < 4) {
                n.next = &a[i + 1];
                old.next = &b[i + 1];
            }
        }
        const bool bounce = test & 1;
        const int result = control.collide(&a[0], &a[1], bounce);
        int oldResult;
        if constexpr (Minotron)
            oldResult = frozen_mechanics::minotronCollide<Base>(&b[0], &b[1], bounce);
        else
            oldResult = frozen_mechanics::minorescueCollide<Base>(&b[0], &b[1], bounce);
        CHECK(result == oldResult && actual == expected);
        for (int i = 0; i < 5; ++i) {
            CHECK(same(a[i].pos.x, b[i].pos.x) && same(a[i].pos.y, b[i].pos.y) &&
                  same(a[i].vel.x, b[i].vel.x) && same(a[i].vel.y, b[i].vel.y));
            CHECK(a[i].dead == b[i].dead && a[i].touching == b[i].touching && a[i].hit == b[i].hit);
        }
    }
}
} // namespace
int main() {
    try {
        // entity::entity (iOS 0x42054) stores binary64 0.98 on both axes.
        // Check construction before the motion traces supply per-case damping.
        mt::entity inherited;
        mt::hlaser laser;
        CHECK(inherited.drag.x == .98 && inherited.drag.y == .98);
        CHECK(laser.drag.x == .98 && laser.drag.y == .98);
        inherited.vel = {.1, -.2};
        inherited.run();
        CHECK(inherited.vel.x == .1 * .98 && inherited.vel.y == -.2 * .98);
        touchTrace<0, mt::controlBlock>();
        touchTrace<1, mt::theGame>();
        touchTrace<2, mr::controlBlock>();
        modernTouchTrace();
        originalTouchSelectors();
        motionTrace();
        contactTrace<true, mt::entity, mt::controlBlock>();
        contactTrace<false, mr::entity, mr::controlBlock>();
        std::cout
            << "5000 actual early contact/bounce traversals match the saved reference callback order, mutations, eligibility and numeric results\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
