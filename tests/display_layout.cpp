#ifdef NDEBUG
#undef NDEBUG
#endif
#include "display_layout.h"
#include "tube_layout.h"
#include "game_format.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
using namespace mino;
bool near(float a, float b) {
    return std::abs(a - b) < .003f;
}
int main() {
    Settings settings;
    assert(settings.display("gridrunner").orientation == Orientation::Landscape);
    settings.set("global", "orientation", "portrait_right");
    assert(settings.display("minotron").orientation == Orientation::PortraitRight);
    NativeFormat portrait{320, 480, 2.f / 3, 2.f / 3}, landscape{480, 320, 1.5f, 1.5f};
    DisplaySettings s;
    s.aspect = AspectMode::IPad;
    for (auto orientation : {Orientation::BestFit, Orientation::Auto}) {
        assert(portraitLauncher(orientation, 720, 1280));
        assert(!portraitLauncher(orientation, 1280, 720));
        assert(std::string(orientationHint(orientation, true, portrait)) ==
               "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
        assert(std::string(orientationHint(orientation, false, portrait)) ==
               "Portrait PortraitUpsideDown MinotaurLocked");
        assert(std::string(orientationHint(orientation, false, landscape)) ==
               "LandscapeLeft LandscapeRight MinotaurLocked");
    }
    assert(!portraitLauncher(Orientation::Landscape, 720, 1280));
    for (auto orientation : {Orientation::Portrait, Orientation::PortraitRight})
        assert(portraitLauncher(orientation, 1280, 720));
    for (auto mode : {Orientation::Portrait, Orientation::PortraitRight}) {
        s.orientation = mode;
        auto gr = displayLayout(960, 720, portrait, s), mt = displayLayout(960, 720, landscape, s);
        assert(gr.view.x == 0 && gr.view.y == 0 && gr.view.w == 960 && gr.view.h == 720);
        assert(mt.view.x == 210 && mt.view.y == 0 && mt.view.w == 540 && mt.view.h == 720);
        assert(gr.turns == (mode == Orientation::Portrait ? 1 : -1) && mt.turns == gr.turns);
        gr = displayLayout(720, 960, portrait, s);
        mt = displayLayout(720, 960, landscape, s);
        assert(gr.turns == 0 && gr.view.w == 720 && gr.view.h == 960);
        assert(mt.turns == 0 && mt.view.x == 0 && mt.view.y == 210 && mt.view.w == 720 &&
               mt.view.h == 540);
    }
    s.orientation = Orientation::Landscape;
    auto horizontal = displayLayout(960, 720, portrait, s);
    assert(horizontal.turns == 0 && horizontal.view.w == 540 && horizontal.view.h == 720);
    auto tube = tubeLayout(horizontal.view.w, horizontal.view.h, horizontal.turns);
    assert(tube.clockwise && tube.inputTurns == -1);
    s.orientation = Orientation::BestFit;
    auto native = displayLayout(960, 720, portrait, s);
    assert(native.turns == 1 && native.view.w == 960 && native.view.h == 720);
    MotionState reversed{true, .4, -.7, .2};
    auto halfTurn = neon::input::rotateMotion(reversed, 2);
    assert(halfTurn.x == -.4 && halfTurn.y == .7 && halfTurn.z == .2 && halfTurn.available);
    int cases = 0;
    for (auto orientation : {Orientation::Landscape, Orientation::BestFit, Orientation::Auto,
                             Orientation::Portrait, Orientation::PortraitRight})
        for (auto window : {std::pair<int, int>{1280, 720}, {720, 1280}, {1001, 703}})
            for (auto format : {portrait, landscape,
                                gridrunner::nativeFormat(gridrunner::GameVersion::ClassicVic20)})
                for (auto aspect : {AspectMode::IPad, AspectMode::Original, AspectMode::Fill})
                    for (auto mode : {ScreenMode::HiRes, ScreenMode::Smooth, ScreenMode::Sharp,
                                      ScreenMode::Scanlines, ScreenMode::CRT}) {
                        s.orientation = orientation;
                        s.aspect = aspect;
                        s.screenMode = mode;
                        auto l = displayLayout(window.first, window.second, format, s);
                        assert(l.view.w > 0 && l.view.h > 0 && l.view.x >= 0 && l.view.y >= 0 &&
                               l.view.x + l.view.w <= window.first &&
                               l.view.y + l.view.h <= window.second);
                        if (aspect == AspectMode::Fill)
                            assert(l.view.x == 0 && l.view.y == 0 && l.view.w == window.first &&
                                   l.view.h == window.second);
                        if (mode != ScreenMode::HiRes) {
                            assert(l.width == (format.width >= format.height ? 480 : 320));
                            assert(l.height == (format.width >= format.height ? 320 : 480));
                        }
                        for (auto point : {std::pair<float, float>{0.f, 0.f},
                                           {1.f, 0.f},
                                           {0.f, 1.f},
                                           {1.f, 1.f},
                                           {.2f, .7f},
                                           {-.2f, 1.2f}}) {
                            float u = l.turns > 0   ? 1 - point.second
                                      : l.turns < 0 ? point.second
                                                    : point.first;
                            float v = l.turns > 0   ? point.first
                                      : l.turns < 0 ? 1 - point.first
                                                    : point.second;
                            float x, y;
                            l.windowToSurface(l.view.x + u * l.view.w, l.view.y + v * l.view.h, &x,
                                              &y);
                            assert(near(x / l.width, point.first) &&
                                   near(y / l.height, point.second));
                        }
                        PadState p;
                        p.lx = .7f;
                        p.ly = -.3f;
                        p.rx = -.2f;
                        p.ry = .6f;
                        p.buttons = PAD_UP | PAD_A;
                        p.pressed = PAD_LEFT | PAD_B;
                        p.released = PAD_RIGHT;
                        auto q = l.nativePad(p);
                        assert((q.buttons & PAD_A) && (q.pressed & PAD_B));
                        if (l.turns > 0) {
                            assert(q.lx == p.ly && q.ly == -p.lx && q.released == PAD_UP &&
                                   (q.buttons & PAD_LEFT) && (q.pressed & PAD_DOWN));
                        } else if (l.turns < 0) {
                            assert(q.lx == -p.ly && q.ly == p.lx && q.released == PAD_DOWN &&
                                   (q.buttons & PAD_RIGHT) && (q.pressed & PAD_UP));
                        } else
                            assert(q.lx == p.lx && q.ly == p.ly && q.released == p.released);
                        MotionState motion{true, .4, -.7, .2};
                        auto m = l.nativeMotion(motion);
                        assert(m.available && m.z == .2);
                        if (l.turns > 0) {
                            assert(m.x == .7 && m.y == .4);
                        } else if (l.turns < 0) {
                            assert(m.x == -.7 && m.y == -.4);
                        } else {
                            assert(m.x == .4 && m.y == -.7);
                        }
                        auto back = neon::input::rotateMotion(m, -l.turns);
                        assert(back.x == motion.x && back.y == motion.y);
                        auto integerTube =
                            tubeLayout(l.view.w, l.view.h, l.turns, l.width, l.height);
                        assert(integerTube.clockwise == (l.view.h > l.view.w));
                        assert(integerTube.width >= integerTube.height);
                        assert(integerTube.sourceWidth ==
                               ((integerTube.inputTurns % 2) != 0 ? l.height : l.width));
                        assert(integerTube.sourceHeight ==
                               ((integerTube.inputTurns % 2) != 0 ? l.width : l.height));
                        ++cases;
                    }
    std::cout
        << cases
        << " presentation cases passed: Horizontal/Native, left/right cabinets, native portrait OS, letterbox examples, linear touch inverses, both controller turns and tube axes\n";
}
