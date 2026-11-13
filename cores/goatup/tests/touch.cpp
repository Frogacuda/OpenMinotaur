// Exercise real core touch events and gameplay input selection without GL.
#include "../goatup_core.cpp"
#include <iostream>
#include <stdexcept>
namespace mino {
namespace {
struct GoatUpTouchChecks {
    static void check(bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    }
    static void run() {
        using Method = goatup::ClimbInput::Method;
        GoatUpCore core;
        core.surface_.width = 320;
        core.surface_.height = 480;
        core.world_ = std::make_unique<goatup::World>(core.records_, 17, false);
        core.front_.screen = goatup::Frontend::Screen::Playing;
        auto event = [&](TouchPhase phase, unsigned id, float x, float y) {
            core.touch({phase, id, x * 320, y * 480});
        };
        for (int mode = 0; mode < 2; ++mode) {
            core.front_.control = mode;
            float jumpX = mode ? .15f : .85f, moveX = mode ? .85f : .15f;
            for (float y : {.15f, .5f, .85f}) {
                event(TouchPhase::Down, 1, jumpX, y);
                check(core.input().jump && core.input().method == Method::None,
                      "Jump side must be independent of height");
                event(TouchPhase::Move, 1, moveX, 1 - y);
                check(core.input().jump && core.input().method == Method::None,
                      "Crossing regions must retain the jump assignment");
                event(TouchPhase::Down, 2, moveX, y);
                event(TouchPhase::Move, 2, jumpX, 1 - y);
                auto input = core.input();
                check(input.jump && input.method == Method::RelativeTouch,
                      "Movement and jump must coexist across regions");
                check(std::abs(input.horizontal - double(moveX - jumpX)) < 1e-6,
                      "Steering uses its own original drag origin");
                core.fingers_[1].origin = core.fingers_[1].x;
                check(core.input().method == Method::RelativeTouch && core.input().horizontal == 0,
                      "Rebasing steering must retain its role");
                event(TouchPhase::Up, 1, moveX, 1 - y);
                check(!core.input().jump && core.input().method == Method::RelativeTouch,
                      "Jump release must retain steering");
                event(TouchPhase::Cancel, 2, jumpX, y);
                check(!core.input().jump && core.input().method == Method::None,
                      "Cancel clears all real touches");
            }
            for (double start : {-.5, -.200001, -.2, -.199999, 0., .199999, .2, .200001, .5}) {
                GoatUpCore::Finger finger;
                finger.startX = start;
                check(core.jumpFinger(finger) == (mode == 0 ? start >= .2 : start <= -.2),
                      "Original inclusive region boundaries");
            }
        }
        core.front_.control = 0;
        event(TouchPhase::Down, 1, .12f, .70f);
        core.tickPauseButton();
        check(core.pauseAlpha_ == float(.98) && !core.world_->paused,
              "Pause fades from its original reveal");
        event(TouchPhase::Down, 2, .1f, .10f);
        const float previousAlpha = core.pauseAlpha_;
        core.tickPauseButton();
        check(core.pauseAlpha_ < previousAlpha, "Second touch does not reveal Pause");
        event(TouchPhase::Move, 2, .5f, .17f);
        core.tickPauseButton();
        check(core.world_->paused, "Sliding a live finger into Pause opens the menu");
        core.world_->paused = false;
        core.tickPauseButton();
        check(!core.world_->paused, "A held touch does not reopen Pause");
        core.world_->paused = true;
        event(TouchPhase::Move, 2, .5f, float(.5 + GoatUpCore::PauseY[1]));
        core.tickPauseButton();
        check(core.front_.screen == goatup::Frontend::Screen::Info && core.front_.infoFromPause,
              "Pause menu buttons accept a live finger entering their bounds");
        core.front_.screen = goatup::Frontend::Screen::Playing;
        core.world_->paused = false;
        event(TouchPhase::Cancel, 2, 0, 0);
        core.front_.control = 2;
        event(TouchPhase::Down, 3, .5f, .5f);
        check(core.input().jump && core.input().method == Method::None,
              "Tilt mode accepts any touch for jump");
        event(TouchPhase::Cancel, 3, 0, 0);
        PadState pad{};
        pad.connected = true;
        pad.lx = .5f;
        pad.buttons = PAD_A;
        core.pads_.capture(&pad, 1);
        check(core.input().method == Method::DirectAxis && core.input().jump,
              "Native controller input remains independent");
        core.world_->game.gameOver = true;
        core.world_->game.ticks = 120;
        core.gameOverArmed_ = true;
        core.commands(PAD_A);
        check(core.front_.screen == goatup::Frontend::Screen::Playing && !core.gameOverArmed_,
              "Confirm during the game-over hold is consumed");
        core.world_->game.ticks = 121;
        event(TouchPhase::Down, 4, .5f, .5f);
        check(core.front_.screen == goatup::Frontend::Screen::Playing,
              "Dismissal requires a fresh press after release");
        event(TouchPhase::Up, 4, .5f, .5f);
        core.gameOverArmed_ = true;
        event(TouchPhase::Down, 4, .5f, .5f);
        check(core.front_.screen == goatup::Frontend::Screen::Resetting &&
                  core.front_.bootTicks == 0,
              "A fresh game-over press enters the timed owner transition");
    }
};
} // namespace
} // namespace mino
int main() {
    try {
        mino::GoatUpTouchChecks::run();
        std::cout << "GoatUp original left/right regions, touch-down role latch, drag rebasing, "
                     "multitouch, release/cancel, tilt and controller checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
