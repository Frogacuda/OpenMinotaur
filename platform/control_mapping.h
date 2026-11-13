#pragma once
#include "core_api.h"
#include <SDL3/SDL_gamepad.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <sstream>
namespace mino {
inline const char *gamepadFaceLabel(SDL_GamepadButtonLabel label) {
    switch (label) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:
        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        return "Triangle";
    default:
        return nullptr;
    }
}
// Physical sources are SDL's hardware-neutral positional buttons/axes, before
// user mapping. Axis codes encode each direction independently (triggers too).
struct PhysicalPad {
    bool connected = false;
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{};
};
struct ControlMapping {
    static constexpr int Count = 20;
    static constexpr int axisSource(SDL_GamepadAxis axis, bool positive) {
        return 64 + 2 * int(axis) + int(positive);
    }
    std::array<int, Count> sources{{SDL_GAMEPAD_BUTTON_DPAD_UP,
                                    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
                                    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
                                    SDL_GAMEPAD_BUTTON_SOUTH,
                                    SDL_GAMEPAD_BUTTON_EAST,
                                    SDL_GAMEPAD_BUTTON_WEST,
                                    SDL_GAMEPAD_BUTTON_NORTH,
                                    axisSource(SDL_GAMEPAD_AXIS_LEFTX, false),
                                    axisSource(SDL_GAMEPAD_AXIS_LEFTX, true),
                                    axisSource(SDL_GAMEPAD_AXIS_LEFTY, false),
                                    axisSource(SDL_GAMEPAD_AXIS_LEFTY, true),
                                    axisSource(SDL_GAMEPAD_AXIS_RIGHTX, false),
                                    axisSource(SDL_GAMEPAD_AXIS_RIGHTX, true),
                                    axisSource(SDL_GAMEPAD_AXIS_RIGHTY, false),
                                    axisSource(SDL_GAMEPAD_AXIS_RIGHTY, true),
                                    axisSource(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, true),
                                    axisSource(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, true),
                                    SDL_GAMEPAD_BUTTON_START,
                                    SDL_GAMEPAD_BUTTON_BACK}};
    static bool valid(int code) {
        return (code >= 0 && code < SDL_GAMEPAD_BUTTON_COUNT) ||
               (code >= 64 && code < 64 + SDL_GAMEPAD_AXIS_COUNT * 2);
    }
    static ControlMapping decode(const std::string &text) {
        ControlMapping out;
        if (text.empty())
            return out;
        std::istringstream in(text);
        std::array<int, Count> values = out.sources;
        int count = 0;
        while (count < Count) {
            int v;
            if (!(in >> v) || !valid(v))
                return {};
            values[count++] = v;
            in >> std::ws;
            if (in.eof())
                break;
            char c;
            if (!(in >> c) || c != ',' || count == Count)
                return {};
            in >> std::ws;
            if (in.eof())
                return {};
        }
        in >> std::ws;
        if (!in.eof() || (count != 16 && count != Count))
            return {};
        out.sources = values;
        return out;
    }
    std::string encode() const {
        std::ostringstream s;
        for (int i = 0; i < Count; ++i) {
            if (i)
                s << ',';
            s << sources[i];
        }
        return s.str();
    }
    static float value(const PhysicalPad &pad, int code) {
        if (!pad.connected || !valid(code))
            return 0;
        if (code < 64)
            return pad.buttons[size_t(code)] ? 1.f : 0.f;
        const int a = (code - 64) / 2;
        const bool positive = (code - 64) % 2;
        return std::max(0.f, pad.axes[size_t(a)] * (positive ? 1.f : -1.f));
    }
    PadState apply(const PhysicalPad &raw) const {
        PadState p{};
        p.connected = raw.connected;
        if (!raw.connected)
            return p;
        constexpr uint32_t bits[] = {PAD_UP,    PAD_DOWN, PAD_LEFT, PAD_RIGHT,
                                     PAD_SOUTH, PAD_EAST, PAD_WEST, PAD_NORTH};
        for (int i = 0; i < 8; ++i)
            if (value(raw, sources[i]) > .5f)
                p.buttons |= bits[i];
        p.lx = value(raw, sources[9]) - value(raw, sources[8]);
        p.ly = value(raw, sources[11]) - value(raw, sources[10]);
        p.rx = value(raw, sources[13]) - value(raw, sources[12]);
        p.ry = value(raw, sources[15]) - value(raw, sources[14]);
        // Stick clicks and shoulders retain their auxiliary bindings.
        if (value(raw, sources[18]) > .5f)
            p.buttons |= PAD_START;
        if (value(raw, sources[19]) > .5f)
            p.buttons |= PAD_BACK;
        const auto auxiliary = [&](int button) {
            return raw.buttons[size_t(button)] &&
                   std::find(sources.begin(), sources.end(), button) == sources.end();
        };
        if (auxiliary(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
            p.buttons |= PAD_LB;
        if (auxiliary(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
            p.buttons |= PAD_RB;
        if (auxiliary(SDL_GAMEPAD_BUTTON_LEFT_STICK))
            p.buttons |= PAD_LSTICK;
        if (auxiliary(SDL_GAMEPAD_BUTTON_RIGHT_STICK))
            p.buttons |= PAD_RSTICK;
        p.lt = value(raw, sources[16]);
        p.rt = value(raw, sources[17]);
        return p;
    }
};
inline constexpr const char *kMappingPrompts[] = {"D-pad Up",
                                                  "D-pad Down",
                                                  "D-pad Left",
                                                  "D-pad Right",
                                                  "Bottom face button",
                                                  "Right face button",
                                                  "Left face button",
                                                  "Top face button",
                                                  "Left stick Left",
                                                  "Left stick Right",
                                                  "Left stick Up",
                                                  "Left stick Down",
                                                  "Right stick Left",
                                                  "Right stick Right",
                                                  "Right stick Up",
                                                  "Right stick Down",
                                                  "Left trigger (LT)",
                                                  "Right trigger (RT)",
                                                  "Start",
                                                  "Select / Back"};
class MappingCapture {
  public:
    void begin(int player) {
        player_ = player;
        step_ = 0;
        cancelFrames_ = 0;
        armed_ = false;
        active_ = true;
        complete_ = false;
        draft_ = {};
    }
    void cancel() {
        active_ = complete_ = false;
    }
    void update(const PhysicalPad &raw) {
        if (!active_)
            return;
        if (!raw.connected) {
            cancel();
            return;
        }
        if (raw.buttons[SDL_GAMEPAD_BUTTON_BACK] && raw.buttons[SDL_GAMEPAD_BUTTON_START]) {
            if (++cancelFrames_ >= 90)
                cancel();
            return;
        }
        cancelFrames_ = 0;
        int source = -1;
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
            if (raw.buttons[b]) {
                source = b;
                break;
            }
        if (source < 0)
            for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
                if (std::abs(raw.axes[a]) > .65f) {
                    source = 64 + 2 * a + (raw.axes[a] > 0);
                    break;
                }
        bool neutral = true;
        for (bool b : raw.buttons)
            neutral &= !b;
        for (float v : raw.axes)
            neutral &= std::abs(v) < .25f;
        if (!armed_) {
            if (neutral)
                armed_ = true;
            return;
        }
        if (source >= 0) {
            draft_.sources[size_t(step_++)] = source;
            armed_ = false;
            if (step_ == ControlMapping::Count) {
                active_ = false;
                complete_ = true;
            }
        }
    }
    bool active() const {
        return active_;
    }
    bool complete() const {
        return complete_;
    }
    int player() const {
        return player_;
    }
    int step() const {
        return step_;
    }
    bool waitingForRelease() const {
        return !armed_;
    }
    const ControlMapping &mapping() const {
        return draft_;
    }

  private:
    int player_ = 0, step_ = 0, cancelFrames_ = 0;
    bool armed_ = false, active_ = false, complete_ = false;
    ControlMapping draft_;
};
} // namespace mino
