// Source-rule checks: EAGLView sampling through the translated game and ship.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cores/minos/motion_input.h"
#include "reconstruction/minos/touch_input.h"
#include "reconstruction/minos/game_states.h"
#include "platform/display_layout.h"
#include <cassert>
#include <cmath>
#include <iostream>

struct Host final : mino::Host {
    int starts = 0, stops = 0;
    bool enabled = false, available = true;
    mino::MotionState value{true, 0, 0, -1};
    bool enableMotion(bool on) override {
        on ? ++starts : ++stops;
        enabled = on && available;
        return enabled;
    }
    mino::MotionState motion() const override {
        auto v = value;
        v.available = enabled;
        return v;
    }
    bool readAsset(const std::string &, std::vector<uint8_t> &) const override {
        return false;
    }
    std::string saveDir() const override {
        return {};
    }
    void log(const char *, ...) override {}
    std::string adaptShader(const std::string &s, int) const override {
        return s;
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {}
};
struct Hud final : minos::Hud {
    void setPrintColour(const neon::colour &) override {}
    void printAt(int, int, const char *) override {}
    void cPrintAt(int, const char *, const neon::colour &) override {}
    void cll(int) override {}
    void cls() override {}
    void clattr() override {}
    void setRowDisplacement(int, float) override {}
};
int main() {
    Host host;
    minos::MotionInput input;
    host.available = false;
    assert(input.sample(host, false, false) == 0 && host.starts == 1);
    host.available = true;
    assert(input.sample(host, false, true) == 0 && host.starts == 2); // retry entering play
    for (int turns : {-1, 0, 1, 2})
        for (double x : {-.8, -.38, -.1, -.079, 0., .079, .1, .38, .8}) {
            input.release(host);
            mino::DisplayLayout layout;
            layout.turns = turns;
            host.value = layout.nativeMotion(neon::input::rotateMotion({true, x, 0, -1}, -turns));
            assert(input.sample(host, false, true) == double(float(x * .5)));
            const double filtered = input.sample(host, false, true);
            assert(filtered == double(float(x)));
            assert(input.sample(host, true, true) == -filtered);
            Hud hud;
            minos::PlayState game;
            game.hud = &hud;
            game.runState = 0;
            game.goalState = 0;
            game.lateralTilt = filtered;
            neon::DarwinRandom random(7);
            game.rand = &random;
            minos::TouchInput touch;
            touch.down(game, 17, {-.2, .3});
            touch.down(game, 42, {.2, -.3});
            game.runFrame();
            const float j = float(filtered);
            float expected =
                std::fabs(double(j)) < .08 ? 0 : float(double(j) + (j < 0 ? .08 : -.08));
            expected = std::max(-.3f, std::min(.3f, expected));
            assert(game.shotDrift == expected);
            minos::Ship ship;
            ship.g = &game;
            game.fuel = 1;
            game.shield = 1;
            ship.doControls();
            assert(ship.rotateSlot == 0 && ship.thrustSlot == 1 && !ship.analogDrive);
            assert(game.touchDown[0] && game.touchDown[1]);
            // The optional original tilt-steering mode uses the same physical sample.
            game.prefsControl = 0x16;
            ship.doControls();
            assert(ship.analogDrive == (std::fabs(float(filtered * .5)) > .01f));
            game.iPad = true;
            game.prefsControl = 0;
            game.touchPos[0][0] = -.35;
            game.touchPos[1][0] = .35;
            ship.doControls();
            assert(ship.rotateSlot == 0 && ship.thrustSlot == 1);
            game.prefsControl = 1;
            ship.doControls();
            assert(ship.rotateSlot == 1 && ship.thrustSlot == 0);
        }
    for (float left : {0.f, .3f, 1.f})
        for (float right : {0.f, .3f, 1.f}) {
            mino::PadState pad;
            pad.connected = true;
            pad.lt = left;
            pad.rt = right;
            const float axis = neon::input::worldInfluenceAxis(pad);
            assert(axis == right - left);
            Hud hud;
            minos::PlayState game;
            game.hud = &hud;
            game.runState = 0;
            game.goalState = 0;
            game.tiltEnabled = false;
            game.lateralTilt = .9;
            game.controllerInfluence = double(axis) * .3;
            game.runFrame();
            assert(game.shotDrift == float(double(axis) * .3));
        }
    {
        Hud hud;
        minos::PlayState game;
        game.hud = &hud;
        game.runState = 0;
        game.goalState = 0;
        game.updatedMenus = true;
        game.touchAvailable = game.tiltAvailable = game.controllerAvailable = true;
        neon::DarwinRandom random(3);
        game.rand = &random;
        game.openPause();
        minos::PauseControl *touch = nullptr, *tilt = nullptr;
        int hints = 0;
        for (auto *e = game.debris; e; e = e->next) {
            if (auto *guide = dynamic_cast<minos::Osj *>(e)) {
                assert(guide->pauseGuide);
                ++hints;
            }
            if (auto *b = dynamic_cast<minos::PauseControl *>(e)) {
                if (b->action == minos::PauseControl::Action::Touch)
                    touch = b;
                if (b->action == minos::PauseControl::Action::Tilt)
                    tilt = b;
            }
        }
        assert(hints == 2 && touch && tilt);
        for (int layout = 0; layout < 3; ++layout) {
            game.touchLayout = layout;
            game.iPad = layout != 0;
            for (auto *e = game.debris; e; e = e->next)
                if (auto *hint = dynamic_cast<minos::Osj *>(e)) {
                    hint->run();
                    assert(hint->size == .1f);
                    const bool steer = hint->spriteIndex == 0x49;
                    assert(layout == 0 ? hint->pos.x == 0 && hint->pos.y == (steer ? .43 : -.32)
                                       : hint->pos.x == ((steer == (layout != 2)) ? -.3 : .3));
                }
        }
        game.touchLayout = 0;
        game.iPad = false;
        for (int next : {1, 2, 0, 0}) {
            touch->activate();
            assert(game.touchLayout == next);
        }
        assert(game.touchGameplayEnabled);
        game.controllerAvailable = false;
        game.touchLayout = 2;
        touch->activate();
        assert(game.touchGameplayEnabled && game.touchLayout == 0);
        game.controllerAvailable = true;
        tilt->activate();
        assert(!game.tiltEnabled);
        tilt->activate();
        assert(game.tiltEnabled);
        game.paused = false;
        for (auto *e = game.debris; e; e = e->next)
            if (auto *hint = dynamic_cast<minos::Osj *>(e)) {
                hint->run();
                assert(hint->dead);
            }
        game.resetLists(0);
        for (bool hasTouch : {false, true})
            for (bool hasTilt : {false, true})
                for (bool hasController : {false, true}) {
                    game.touchAvailable = hasTouch;
                    game.tiltAvailable = hasTilt;
                    game.controllerAvailable = hasController;
                    game.openPause();
                    int toggles = 0;
                    for (auto *e = game.debris; e; e = e->next)
                        if (auto *b = dynamic_cast<minos::PauseControl *>(e))
                            if (b->action == minos::PauseControl::Action::Touch ||
                                b->action == minos::PauseControl::Action::Tilt) {
                                ++toggles;
                                const bool both = hasTouch && hasTilt && hasController;
                                assert(
                                    b->pos.x ==
                                    (both ? (b->action == minos::PauseControl::Action::Touch ? -.24
                                                                                             : .24)
                                          : 0));
                                assert(b->pos.y == minos::menuLayout::ControlsY);
                            } else if (b->action == minos::PauseControl::Action::Resume)
                                assert(b->pos.y == minos::menuLayout::PauseY);
                    assert(toggles == int(hasTouch) + int(hasTilt && hasController));
                    game.resetLists(0);
                }
    }
    {
        // A touch on Resume must not hit the co-located gameplay Pause button
        // when pause was entered through a controller.
        Hud hud;
        minos::PlayState game;
        game.hud = &hud;
        game.updatedMenus = true;
        neon::DarwinRandom random(3);
        game.rand = &random;
        minos::PauseButton gameplay;
        gameplay.g = &game;
        gameplay.pos = {0, minos::menuLayout::PauseY};
        gameplay.pressed = false;
        gameplay.size = gameplay.baseSize = .08f;
        gameplay.scale = {2.4, 1};
        game.pausePressed = [&] { game.openPause(); };
        game.openPause();
        minos::PauseControl *resume = nullptr;
        for (auto *e = game.debris; e; e = e->next)
            if (auto *b = dynamic_cast<minos::PauseControl *>(e))
                if (b->action == minos::PauseControl::Action::Resume)
                    resume = b;
        assert(resume && resume->pos.y == gameplay.pos.y);
        resume->run();
        game.touchDown[0] = true;
        game.touchPos[0][0] = 0;
        game.touchPos[0][1] = gameplay.pos.y;
        resume->run();
        assert(!game.paused && !game.touchDown[0]);
        gameplay.run();
        assert(!game.paused);
        game.resetLists(0);
    }
    {
        mino::PadState p;
        p.connected = true;
        p.rx = .8f;
        p.rt = .6f;
        assert(neon::input::worldInfluenceAxis(p) == 1);
        p.lt = .6f;
        assert(neon::input::worldInfluenceAxis(p) == .8f);
        p.connected = false;
        assert(neon::input::worldInfluenceAxis(p) == 0);
    }
    host.value = {true, .8, 0, -1};
    input.release(host);
    assert(input.sample(host, false, true) == double(float(.4)));
    input.release(host);
    const int stopped = host.stops;
    input.release(host);
    assert(host.stops == stopped);
    host.value.x = -.8;
    assert(input.sample(host, false, true) == double(float(-.4))); // resume clears history
    host.enabled = false;
    assert(input.sample(host, false, true) == 0);
    host.enabled = true;
    assert(input.sample(host, false, true) == double(float(-.4)));
    std::cout
        << "Minos motion: sampling, retry, release/resume, four orientations, dead zone/cap, concurrent touch and optional tilt steering passed\n";
}
