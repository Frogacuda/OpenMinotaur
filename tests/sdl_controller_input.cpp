#ifdef NDEBUG
#undef NDEBUG
#endif
#include "input.h"
#include "display.h"
#include <cassert>
#include <cmath>
#include <iostream>
// No GL/window is needed for ordered delivery and disable/recovery checks.
void mino::Display::windowToSurface(float x, float y, float *sx, float *sy) const {
    *sx = x;
    *sy = y;
}
struct TouchProbe final : mino::Core {
    std::vector<mino::TouchPhase> events;
    const mino::CoreInfo &info() const override {
        static const mino::CoreInfo i = {"probe", "probe", 2, 2, 1, 1, nullptr};
        return i;
    }
    bool init(mino::Host &, const mino::SurfaceInfo &) override {
        return true;
    }
    void resize(const mino::SurfaceInfo &) override {}
    void frame(double) override {}
    void touch(const mino::TouchEvent &t) override {
        events.push_back(t.phase);
    }
    void pads(const mino::PadState *, int) override {}
    void mixAudio(float *, int) override {}
    void pause(bool) override {}
    void save() override {}
    void shutdown() override {}
};
int main() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "Neon input contract";
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    const auto id = SDL_AttachVirtualJoystick(&desc);
    assert(id && SDL_IsGamepad(id));
    auto *joystick = SDL_OpenJoystick(id);
    assert(joystick);
    mino::Input input;
    input.init();
    mino::Display display;
    assert(input.connectedPads() > 0);
    assert(input.menuPrompts().connected && !input.menuPrompts().select.empty() &&
           !input.menuPrompts().back.empty());
    assert(std::string(mino::gamepadFaceLabel(SDL_GetGamepadButtonLabelForType(
               SDL_GAMEPAD_TYPE_PS5, SDL_GAMEPAD_BUTTON_SOUTH))) == "Cross");
    assert(std::string(mino::gamepadFaceLabel(SDL_GetGamepadButtonLabelForType(
               SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_EAST))) == "A");
    assert(std::string(mino::gamepadFaceLabel(SDL_GetGamepadButtonLabelForType(
               SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_SOUTH))) == "A");
    // Test device will be the last connected slot in the usual no/full-pad
    // inventory. Locate it by its unique axis state rather than hardware name.
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, -32768));
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 0));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    int slot = -1;
    for (int i = 0; i < mino::kMaxPads; ++i)
        if (input.pads()[i].connected && input.pads()[i].lx == -1)
            slot = i;
    assert(slot >= 0);
    assert(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    assert((input.pads()[slot].buttons & mino::PAD_SOUTH) &&
           (input.pads()[slot].pressed & mino::PAD_SOUTH));
    input.updatePads(.2f);
    assert(!(input.pads()[slot].pressed & mino::PAD_SOUTH));
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 0));
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 5000));
    assert(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    assert(input.pads()[slot].lx == 0 && input.pads()[slot].ly == 0);
    assert(input.pads()[slot].released & mino::PAD_SOUTH);
    assert(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, true));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    assert(input.pads()[slot].pressed & mino::PAD_EAST);
    TouchProbe probe;
    SDL_Event finger{};
    finger.type = SDL_EVENT_FINGER_DOWN;
    finger.tfinger.fingerID = 42;
    input.handleEvent(finger, nullptr, display, &probe);
    assert(probe.events.size() == 1);
    input.setTouchEnabled(false, &probe);
    assert(probe.events.back() == mino::TouchPhase::Cancel);
    input.handleEvent(finger, nullptr, display, &probe);
    assert(probe.events.size() == 2);
    std::array<mino::ControlMapping, mino::kMaxPads> custom{};
    custom[size_t(slot)].sources[4] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    custom[size_t(slot)].sources[8] = 64 + 2 * SDL_GAMEPAD_AXIS_RIGHTX;
    custom[size_t(slot)].sources[9] = 65 + 2 * SDL_GAMEPAD_AXIS_RIGHTX;
    input.setMappings(custom);
    assert(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true));
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, 26000));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    assert(input.pads()[slot].pressed & mino::PAD_SOUTH);
    assert(input.pads()[slot].lx > .7f);
    assert(input.menuPrompts().select == "Left Shoulder");
    custom[size_t(slot)].sources[4] = 65 + 2 * SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    input.setMappings(custom);
    assert(input.menuPrompts().select == "Left Trigger");
    assert(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767));
    SDL_UpdateGamepads();
    input.updatePads(.2f);
    assert(input.pads()[slot].buttons & mino::PAD_SOUTH);
    input.setMappings({});
    input.updatePads(.2f);
    assert(input.pads()[slot].released & mino::PAD_SOUTH);
    SDL_CloseJoystick(joystick);
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = id;
    input.handleEvent(removed, nullptr, display, nullptr);
    input.updatePads(.2f);
    assert(!input.pads()[slot].connected && input.pads()[slot].buttons == 0);
    if (!input.connectedPads())
        assert(!input.menuPrompts().connected);
    assert(input.pads()[slot].released & mino::PAD_EAST);
    if (input.connectedPads() == 0) {
        input.handleEvent(finger, nullptr, display, &probe);
        assert(probe.events.size() == 3 && probe.events.back() == mino::TouchPhase::Down);
        input.cancelTouches(&probe);
    }
    input.updatePads(.2f);
    assert(input.pads()[slot].released == 0);
    const auto replacement = SDL_AttachVirtualJoystick(&desc);
    assert(replacement);
    SDL_Event added{};
    added.type = SDL_EVENT_GAMEPAD_ADDED;
    added.gdevice.which = replacement;
    input.handleEvent(added, nullptr, display, nullptr);
    input.updatePads(.2f);
    assert(input.pads()[slot].connected && input.pads()[slot].pressed == 0);
    input.shutdown();
    input.shutdown();
    assert(SDL_DetachVirtualJoystick(replacement));
    SDL_Quit();
    std::cout
        << "SDL virtual gamepad: positional mapping, limits, dead zone, press/release, unplug, replug and shutdown passed\n";
}
