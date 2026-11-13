// input.h - turns SDL touch/mouse/gamepad events into core input.
#pragma once
#include "core_api.h"
#include "control_mapping.h"
#include "neon/tilt_calibration.h"

#include <SDL3/SDL.h>
#include <map>

namespace mino {

class Display;

class Input {
  public:
    void init();
    void shutdown();
    bool enableMotion(bool enabled);
    MotionState motion(SDL_Window *window) const;
    MotionState rawMotion() const;
    bool calibrateTilt() {
        return calibration_.set(rawMotion());
    }
    void loadTiltCalibration(const std::string &s) {
        calibration_.decode(s);
    }
    std::string tiltCalibration() const {
        return calibration_.encode();
    }

    // Returns true if the event was consumed. Touch events are forwarded to
    // the core immediately (in order), mapped into surface coordinates.
    bool handleEvent(const SDL_Event &e, SDL_Window *window, const Display &display, Core *core);

    // Poll pad state once per frame (applies dead zone, computes pressed edges).
    void updatePads(float deadzone);
    const PadState *pads() const {
        return pads_;
    }
    int connectedPads() const;
    struct MenuPrompts {
        bool connected = false;
        std::string select, back;
    };
    MenuPrompts menuPrompts() const;
    void setMappings(const std::array<ControlMapping, kMaxPads> &mappings) {
        mappings_ = mappings;
    }
    const PhysicalPad &physical(int player) const {
        return physical_[size_t(player)];
    }
    bool touchEnabled() const {
        return touchEnabled_;
    }
    void setTouchEnabled(bool enabled, Core *core) {
        if (touchEnabled_ && !enabled)
            cancelTouches(core);
        touchEnabled_ = enabled;
    }

    // Release every active touch (e.g. when pausing) so the core never sees a stuck finger.
    void cancelTouches(Core *core);

  private:
    void addPad(SDL_JoystickID id);
    void removePad(SDL_JoystickID id);
    uint32_t touchId(SDL_FingerID f, bool create);

    std::array<ControlMapping, kMaxPads> mappings_{};
    std::array<PhysicalPad, kMaxPads> physical_{};
    bool touchEnabled_ = true;
    neon::input::TiltCalibration calibration_;
    SDL_Sensor *accelerometer_ = nullptr;
    bool sensorSubsystem_ = false;
    SDL_Gamepad *handles_[kMaxPads] = {};
    SDL_JoystickID ids_[kMaxPads] = {};
    int activePad_ = 0;
    PadState pads_[kMaxPads];
    uint32_t prevButtons_[kMaxPads] = {};
    uint32_t pendingReleased_[kMaxPads] = {};

    std::map<SDL_FingerID, uint32_t> fingers_;
    std::map<uint32_t, std::pair<float, float>> lastPos_;
    uint32_t nextTouch_ = 1; // 0 is reserved for the mouse
    bool mouseDown_ = false;
};

} // namespace mino
