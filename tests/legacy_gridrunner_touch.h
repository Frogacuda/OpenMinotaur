// Saved reference slot/selection binding, using the unchanged gesture physics.
#pragma once
#include "cores/gridrunner/controls.h"
namespace gridrunner {
class LegacyMultiTouchControl {
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
        for (auto &f : fingers_)
            if (!f.down) {
                f.id = id;
                f.down = true;
                f.age = ++age_;
                f.control.begin(p);
                return;
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
    Axis tick(TouchMode mode) {
        int selected = -1;
        for (int i = 0; i < 4; ++i)
            if (fingers_[size_t(i)].down &&
                (selected < 0 || mode == TouchMode::Expert ||
                 fingers_[size_t(i)].age < fingers_[size_t(selected)].age))
                selected = i;
        if (selected >= 0)
            last_ = selected;
        Axis output{};
        for (int i = 0; i < 4; ++i) {
            const auto d = fingers_[size_t(i)].control.tick(mode);
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
        for (int i = 0; i < 4; ++i)
            if (fingers_[size_t(i)].down && fingers_[size_t(i)].id == id)
                return i;
        return -1;
    }
    std::array<Finger, 4> fingers_{};
    uint64_t age_ = 0;
    int last_ = -1;
};
} // namespace gridrunner
