#pragma once
#include "input_model.h"
#include "neon/touch_slots.h"
#include <array>
#include <cstdint>
namespace gridrunner {
// Deliberate direct controller profiles, shared by modern and Classic.
inline constexpr InputTuning kFastAnalog{3, 3, 48};
inline constexpr InputTuning kDpad{2, 1, 48};
// Shared direct-controller modifier: preserve the selected curve and select half/full
// output gain (Mode 1: A slows; Mode 2: A speeds). Applied only to direct pad input, never touch
// displacement.
inline Axis controllerResponse(Axis axis, bool digital, bool expertMode, bool buttonHeld) {
    const InputTuning linear{3, 1, 48};
    auto v = response(axis, digital ? kDpad : expertMode ? kFastAnalog : linear);
    const bool slow = expertMode ? !buttonHeld : buttonHeld;
    if (slow) {
        v.x *= .5;
        v.y *= .5;
    }
    return v;
}

enum class TouchMode { Normal, Expert };
enum class TouchTarget { Enhanced, Classic };
// iOS trackpad multiplier/decay and expert origin-displacement path. Produces
// displacement at the selected ship boundary: modern applies .09, Classic does not.
class TouchControl {
  public:
    void begin(Axis p);
    void move(Axis p);
    void release();
    void cancel();
    Axis tick(TouchMode mode, TouchTarget target = TouchTarget::Enhanced);
    bool active() const {
        return active_;
    }

  private:
    bool active_ = false, moved_ = false;
    Axis current_, previous_, origin_, motion_, velocity_;
};

// The original control block retains four touch slots. Keep each origin and
// delta independent so lifting one finger does not transfer its displacement
// to another. Menus still use their own press/release hit testing.
class MultiTouchControl {
  public:
    bool contains(uint32_t id) const {
        return slot(id) >= 0;
    }
    bool active() const {
        for (const auto &f : fingers_)
            if (f.down)
                return true;
        return false;
    }
    void down(uint32_t id, Axis p) {
        if (contains(id))
            return;
        const int i = neon::input::firstSlot(4, [&](int k) { return !fingers_[size_t(k)].down; });
        if (i >= 0) {
            auto &f = fingers_[size_t(i)];
            f.id = id;
            f.down = true;
            f.age = ++age_;
            f.control.begin(p);
        }
    }
    void move(uint32_t id, Axis p) {
        const int i = slot(id);
        if (i >= 0)
            fingers_[size_t(i)].control.move(p);
    }
    void up(uint32_t id, Axis p, bool cancelled = false) {
        const int i = slot(id);
        if (i < 0)
            return;
        auto &f = fingers_[size_t(i)];
        f.down = false;
        if (cancelled)
            f.control.cancel();
        else {
            f.control.move(p);
            f.control.release();
        }
    }
    Axis tick(TouchMode mode, TouchTarget target = TouchTarget::Enhanced) {
        const int selected = neon::input::selectSlot(
            4, [&](int i) { return fingers_[size_t(i)].down; },
            [&](int i, int best) {
                return mode == TouchMode::Expert ||
                       fingers_[size_t(i)].age < fingers_[size_t(best)].age;
            });
        if (selected >= 0)
            last_ = selected;
        Axis output{};
        for (int i = 0; i < 4; ++i) {
            const auto d = fingers_[size_t(i)].control.tick(mode, target);
            if (i == last_)
                output = d;
        }
        return output;
    }
    void cancel() {
        for (auto &f : fingers_) {
            f.down = false;
            f.control.cancel();
        }
        last_ = -1;
    }
    // Used only by the legacy one-pointer menu path.
    void begin(Axis p) {
        down(0, p);
    }
    void move(Axis p) {
        move(0, p);
    }
    void release() {
        const int i = slot(0);
        if (i >= 0) {
            fingers_[size_t(i)].down = false;
            fingers_[size_t(i)].control.release();
        }
    }

  private:
    struct Finger {
        uint32_t id = 0;
        uint64_t age = 0;
        bool down = false;
        TouchControl control;
    };
    int slot(uint32_t id) const {
        return neon::input::firstSlot(
            4, [&](int i) { return fingers_[size_t(i)].down && fingers_[size_t(i)].id == id; });
    }
    std::array<Finger, 4> fingers_{};
    uint64_t age_ = 0;
    int last_ = -1;
};

} // namespace gridrunner
