#include "input.h"
#include "display.h"

#include <algorithm>
#include <cmath>

namespace mino {

void Input::init() {
    // Handle touch and mouse separately to avoid duplicate synthetic events.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    int n = 0;
    SDL_JoystickID *list = SDL_GetGamepads(&n);
    for (int i = 0; i < n; i++)
        addPad(list[i]);
    SDL_free(list);
}

void Input::shutdown() {
    enableMotion(false);
    for (int i = 0; i < kMaxPads; i++) {
        if (handles_[i])
            SDL_CloseGamepad(handles_[i]);
        handles_[i] = nullptr;
        ids_[i] = 0;
        pads_[i] = {};
        prevButtons_[i] = pendingReleased_[i] = 0;
        physical_[size_t(i)] = {};
    }
}

bool Input::enableMotion(bool enabled) {
    if (!enabled) {
        if (accelerometer_)
            SDL_CloseSensor(accelerometer_);
        accelerometer_ = nullptr;
        if (sensorSubsystem_)
            SDL_QuitSubSystem(SDL_INIT_SENSOR);
        sensorSubsystem_ = false;
        return false;
    }
    if (accelerometer_)
        return true;
    if (!sensorSubsystem_) {
        sensorSubsystem_ = SDL_InitSubSystem(SDL_INIT_SENSOR);
        if (!sensorSubsystem_)
            return false;
    }
    int count = 0;
    auto *sensors = SDL_GetSensors(&count);
    for (int n = 0; sensors && n < count; ++n)
        if (SDL_GetSensorTypeForID(sensors[n]) == SDL_SENSOR_ACCEL) {
            accelerometer_ = SDL_OpenSensor(sensors[n]);
            if (accelerometer_)
                break;
        }
    SDL_free(sensors);
    if (!accelerometer_) {
        enableMotion(false);
        return false;
    }
    return true;
}
MotionState Input::rawMotion() const {
    float axes[3];
    if (!accelerometer_ || !SDL_GetSensorData(accelerometer_, axes, 3))
        return {};
    for (float axis : axes)
        if (!std::isfinite(axis))
            return {};
    return {true, -double(axes[0]) / SDL_STANDARD_GRAVITY, -double(axes[1]) / SDL_STANDARD_GRAVITY,
            -double(axes[2]) / SDL_STANDARD_GRAVITY};
}
MotionState Input::motion(SDL_Window *window) const {
    const auto value = calibration_.apply(rawMotion());
    const auto id = SDL_GetDisplayForWindow(window);
    const auto turns = [](SDL_DisplayOrientation orientation) {
        switch (orientation) {
        case SDL_ORIENTATION_PORTRAIT:
            return 0;
        case SDL_ORIENTATION_LANDSCAPE:
            return 1;
        case SDL_ORIENTATION_PORTRAIT_FLIPPED:
            return 2;
        case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
            return 3;
        default:
            return 0;
        }
    };
    const auto current = SDL_GetCurrentDisplayOrientation(id),
               natural = SDL_GetNaturalDisplayOrientation(id);
    if (current == SDL_ORIENTATION_UNKNOWN || natural == SDL_ORIENTATION_UNKNOWN)
        return value;
    return neon::input::rotateMotion(value, turns(current) - turns(natural));
}
void Input::addPad(SDL_JoystickID id) {
    for (int i = 0; i < kMaxPads; i++)
        if (handles_[i] && ids_[i] == id)
            return;
    for (int i = 0; i < kMaxPads; i++) {
        if (handles_[i])
            continue;
        handles_[i] = SDL_OpenGamepad(id);
        if (!handles_[i])
            return;
        ids_[i] = id;
        prevButtons_[i] = pendingReleased_[i] = 0;
        pads_[i] = PadState{};
        pads_[i].connected = true;
        SDL_Log("gamepad %d connected: %s", i, SDL_GetGamepadName(handles_[i]));
        return;
    }
}

void Input::removePad(SDL_JoystickID id) {
    for (int i = 0; i < kMaxPads; i++) {
        if (!handles_[i] || ids_[i] != id)
            continue;
        pendingReleased_[i] |= pads_[i].buttons;
        SDL_CloseGamepad(handles_[i]);
        handles_[i] = nullptr;
        pads_[i] = PadState{};
        prevButtons_[i] = 0;
        SDL_Log("gamepad %d disconnected", i);
    }
}

int Input::connectedPads() const {
    int n = 0;
    for (auto &p : pads_)
        n += p.connected;
    return n;
}

Input::MenuPrompts Input::menuPrompts() const {
    int slot = activePad_;
    if (slot < 0 || slot >= kMaxPads || !handles_[slot]) {
        for (slot = 0; slot < kMaxPads && !handles_[slot]; ++slot) {
        }
        if (slot == kMaxPads)
            return {};
    }
    const auto name = [&](int source) -> std::string {
        if (source >= 64) {
            const int axis = (source - 64) / 2;
            const bool positive = (source - 64) % 2;
            if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
                return "Left Trigger";
            if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                return "Right Trigger";
            return std::string(axis >= SDL_GAMEPAD_AXIS_RIGHTX ? "Right Stick " : "Left Stick ") +
                   (axis % 2 ? (positive ? "Down" : "Up") : (positive ? "Right" : "Left"));
        }
        const auto button = SDL_GamepadButton(source);
        if (const char *label = gamepadFaceLabel(SDL_GetGamepadButtonLabel(handles_[slot], button)))
            return label;
        switch (button) {
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return "Left Shoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return "Right Shoulder";
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "South";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "East";
        default:
            const char *label = SDL_GetGamepadStringForButton(button);
            return label ? label : "Button";
        }
    };
    return {true, name(mappings_[size_t(slot)].sources[4]),
            name(mappings_[size_t(slot)].sources[5])};
}

static void applyDeadzone(float &x, float &y, float dz) {
    float m = std::sqrt(x * x + y * y);
    if (m <= dz) {
        x = y = 0;
        return;
    }
    float scaled = std::min((m - dz) / (1.0f - dz), 1.0f);
    x = x / m * scaled;
    y = y / m * scaled;
}

void Input::updatePads(float deadzone) {
    for (int i = 0; i < kMaxPads; i++) {
        SDL_Gamepad *g = handles_[i];
        if (!g) {
            physical_[size_t(i)] = {};
            pads_[i] = {};
            pads_[i].released = pendingReleased_[i];
            pendingReleased_[i] = 0;
            continue;
        }
        PadState &p = pads_[i];
        auto axis = [&](SDL_GamepadAxis a) {
            return std::clamp(SDL_GetGamepadAxis(g, a) / 32767.0f, -1.0f, 1.0f);
        };
        auto &raw = physical_[size_t(i)];
        raw = {};
        raw.connected = true;
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
            raw.axes[size_t(a)] = axis(SDL_GamepadAxis(a));
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; ++b)
            raw.buttons[size_t(b)] = SDL_GetGamepadButton(g, SDL_GamepadButton(b));
        p = mappings_[size_t(i)].apply(raw);
        if (p.buttons || std::abs(p.lx) > .5f || std::abs(p.ly) > .5f)
            activePad_ = i;
        applyDeadzone(p.lx, p.ly, deadzone);
        applyDeadzone(p.rx, p.ry, deadzone);
        const uint32_t b = p.buttons;
        p.released = prevButtons_[i] & ~b;
        p.pressed = b & ~prevButtons_[i];
        p.buttons = b;
        prevButtons_[i] = b;
    }
}

uint32_t Input::touchId(SDL_FingerID f, bool create) {
    auto it = fingers_.find(f);
    if (it != fingers_.end())
        return it->second;
    if (!create)
        return 0;
    uint32_t id = nextTouch_++;
    if (nextTouch_ == 0)
        nextTouch_ = 1;
    fingers_[f] = id;
    return id;
}

bool Input::handleEvent(const SDL_Event &e, SDL_Window *window, const Display &display,
                        Core *core) {
    // Device lifecycle is independent of any window or touch geometry.
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        addPad(e.gdevice.which);
        return true;
    }
    if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        removePad(e.gdevice.which);
        if (connectedPads() == 0)
            touchEnabled_ = true;
        return true;
    }
    if (!touchEnabled_ && (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
                           e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED ||
                           e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                           e.type == SDL_EVENT_MOUSE_BUTTON_UP || e.type == SDL_EVENT_MOUSE_MOTION))
        return true;
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    float density = SDL_GetWindowPixelDensity(window);

    auto send = [&](TouchPhase phase, uint32_t id, float wx, float wy) {
        TouchEvent t{phase, id, 0, 0};
        display.windowToSurface(wx, wy, &t.x, &t.y);
        if (phase == TouchPhase::Up || phase == TouchPhase::Cancel)
            lastPos_.erase(id);
        else
            lastPos_[id] = {t.x, t.y};
        if (core)
            core->touch(t);
    };

    switch (e.type) {
    case SDL_EVENT_FINGER_DOWN:
        send(TouchPhase::Down, touchId(e.tfinger.fingerID, true), e.tfinger.x * pw,
             e.tfinger.y * ph);
        return true;
    case SDL_EVENT_FINGER_MOTION: {
        uint32_t id = touchId(e.tfinger.fingerID, false);
        if (id)
            send(TouchPhase::Move, id, e.tfinger.x * pw, e.tfinger.y * ph);
        return true;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        uint32_t id = touchId(e.tfinger.fingerID, false);
        if (id) {
            send(e.type == SDL_EVENT_FINGER_UP ? TouchPhase::Up : TouchPhase::Cancel, id,
                 e.tfinger.x * pw, e.tfinger.y * ph);
            fingers_.erase(e.tfinger.fingerID);
        }
        return true;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT)
            return false;
        mouseDown_ = true;
        send(TouchPhase::Down, 0, e.button.x * density, e.button.y * density);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID || !mouseDown_)
            return false;
        send(TouchPhase::Move, 0, e.motion.x * density, e.motion.y * density);
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT ||
            !mouseDown_)
            return false;
        mouseDown_ = false;
        send(TouchPhase::Up, 0, e.button.x * density, e.button.y * density);
        return true;
    default:
        return false;
    }
}

void Input::cancelTouches(Core *core) {
    if (core) {
        for (auto &kv : lastPos_)
            core->touch(
                TouchEvent{TouchPhase::Cancel, kv.first, kv.second.first, kv.second.second});
    }
    lastPos_.clear();
    fingers_.clear();
    mouseDown_ = false;
}

} // namespace mino
