#ifdef NDEBUG
#undef NDEBUG
#endif
#include "control_mapping.h"
#include "display_layout.h"
#include <cassert>
#include <iostream>
using namespace mino;
int main(int argc, char **argv) {
    Settings settings;
    assert(settings.display("gridrunner").aspect == AspectMode::IPad);
    for (const auto *game : {"minotron", "minorescue", "gridrunner", "gridrunner-classic"}) {
        settings.set("global", "screen_mode", "crt");
        settings.set("global", "aspect", "fill");
        settings.set("global", "disable_touch", "on");
        assert(settings.display(game).screenMode == ScreenMode::CRT &&
               settings.display(game).aspect == AspectMode::Fill &&
               settings.input(game).disableTouch);
        assert(!settings.input(game).rotateControls);
    }
    settings.set("global", "screen_mode", "crt2");
    assert(settings.display("gridrunner").screenMode == ScreenMode::CRT2);
    settings.set("gridrunner", "screen_mode", "crt");
    assert(settings.display("gridrunner").screenMode == ScreenMode::CRT &&
           settings.display("goatup").screenMode == ScreenMode::CRT2);
    settings.set("gridrunner", "screen_mode", "smooth");
    assert(settings.display("gridrunner").screenMode == ScreenMode::Smooth);
    Settings phone;
    phone.useNativeOrientationDefault(true);
    assert(phone.display("global").orientation == Orientation::BestFit);
    assert(portraitLauncher(phone.display("global").orientation, 720, 1280));
    phone.set("global", "orientation", "landscape");
    assert(phone.display("minos").orientation == Orientation::Landscape);
    phone.set("minos", "orientation", "bestfit");
    assert(phone.display("minos").orientation == Orientation::BestFit);
    PhysicalPad raw;
    raw.connected = true;
    ControlMapping defaults;
    for (int b = 0; b < 4; ++b) {
        raw = {};
        raw.connected = true;
        raw.buttons[size_t(b)] = true;
        const uint32_t bits[] = {PAD_SOUTH, PAD_EAST, PAD_WEST, PAD_NORTH};
        assert(defaults.apply(raw).buttons == bits[b]);
    }
    raw = {};
    raw.connected = true;
    raw.axes = {-.7f, .4f, .8f, -.2f, 0, 0};
    auto p = defaults.apply(raw);
    assert(p.lx == -.7f && p.ly == .4f && p.rx == .8f && p.ry == -.2f);
    MappingCapture capture;
    capture.begin(2);
    raw = {};
    raw.connected = true;
    raw.buttons[0] = true;
    capture.update(raw);
    assert(capture.step() == 0); // opening press never becomes mapping
    for (int i = 0; i < ControlMapping::Count; ++i) {
        raw = {};
        raw.connected = true;
        capture.update(raw);
        assert(!capture.waitingForRelease());
        if (i < 8)
            raw.buttons[size_t(9 + i % 2)] = true;
        else if (i >= 18)
            raw.buttons[size_t(i == 18 ? SDL_GAMEPAD_BUTTON_START : SDL_GAMEPAD_BUTTON_BACK)] =
                true;
        else if (i >= 16)
            raw.axes[size_t(i - 12)] = 1;
        else
            raw.axes[size_t((i - 8) / 2)] = (i % 2) ? 1.f : -1.f;
        capture.update(raw);
        capture.update(raw);
        assert(capture.step() == i + 1);
    }
    assert(capture.complete() && !capture.active() && capture.player() == 2);
    auto custom = capture.mapping();
    assert(ControlMapping::decode(custom.encode()).sources == custom.sources);
    auto legacy = custom.encode();
    size_t separator = 0;
    for (int n = 0; n < 16; ++n)
        separator = legacy.find(',', separator) + (n == 15 ? 0 : 1);
    legacy.resize(separator);
    auto upgraded = ControlMapping::decode(legacy);
    for (int i = 0; i < 16; ++i)
        assert(upgraded.sources[i] == custom.sources[i]);
    for (int i = 16; i < 20; ++i)
        assert(upgraded.sources[i] == defaults.sources[i]);
    assert(ControlMapping::decode(custom.encode() + ",").sources == defaults.sources);
    assert(ControlMapping::decode("bad").sources == defaults.sources);
    assert(ControlMapping::decode(custom.encode() + ",8").sources == defaults.sources);
    if (argc > 1) {
        Settings stored;
        stored.load(argv[1]);
        stored.set("global", "mapping3", custom.encode());
        stored.set("global", "wallpaper", "2");
        stored.set("global", "rotate_controls", "on");
        assert(stored.save());
        Settings restored;
        assert(restored.load(argv[1]));
        assert(ControlMapping::decode(restored.get("global", "mapping3", "")).sources ==
               custom.sources);
        assert(restored.get("global", "wallpaper", "") == "2" &&
               restored.input("gridrunner").rotateControls);
    }
    auto extra = defaults;
    extra.sources[16] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    extra.sources[17] = ControlMapping::axisSource(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, true);
    extra.sources[18] = SDL_GAMEPAD_BUTTON_BACK;
    extra.sources[19] = SDL_GAMEPAD_BUTTON_START;
    raw = {};
    raw.connected = true;
    raw.buttons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = true;
    raw.buttons[SDL_GAMEPAD_BUTTON_START] = true;
    raw.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = .37f;
    p = extra.apply(raw);
    assert(p.lt == 1 && p.rt == .37f && !(p.buttons & PAD_RB) && (p.buttons & PAD_BACK) &&
           !(p.buttons & PAD_START));
    raw = {};
    raw.connected = true;
    raw.buttons[9] = true;
    p = custom.apply(raw);
    assert((p.buttons & (PAD_UP | PAD_LEFT | PAD_SOUTH | PAD_WEST)) ==
           (PAD_UP | PAD_LEFT | PAD_SOUTH | PAD_WEST));
    custom = defaults;
    custom.sources[4] = 73;
    raw = {};
    raw.connected = true;
    raw.axes[4] = .9f;
    assert(custom.apply(raw).buttons & PAD_SOUTH); // trigger assigned to face
    capture.begin(0);
    raw = {};
    capture.update(raw);
    assert(!capture.active() && !capture.complete()); // disconnect discards draft
    capture.begin(0);
    raw.connected = true;
    raw.buttons[4] = raw.buttons[6] = true;
    for (int i = 0; i < 90; ++i)
        capture.update(raw);
    assert(!capture.active() && !capture.complete());
    capture.begin(1);
    capture.cancel();
    assert(!capture.active() && !capture.complete());
    DisplaySettings d;
    d.orientation = Orientation::BestFit;
    auto layout = displayLayout(1280, 720, {480, 640, .75f, .75f}, d);
    PadState input;
    input.lx = 1;
    input.released = PAD_RIGHT;
    auto rotated = layout.nativePad(input);
    assert(rotated.ly == -1 && rotated.released == PAD_UP);
    std::cout
        << "Options contracts: defaults/overrides, 20-source remap (16-source saves retained), release gating, triggers, cancel/disconnect, serialization and rotated release edges passed\n";
}
