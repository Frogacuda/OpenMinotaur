#pragma once
#include "neon/input.h"
#include <array>
namespace neon::input {
// A module-owned snapshot of the canonical platform input. Missing devices are
// neutral, slot identity is stable, and interpretation never creates touches.
class ControllerFrame {
  public:
    void capture(const ControllerState *states, int count) {
        std::array<ControllerState, MaxControllers> captured{};
        if (states)
            for (int i = 0; i < std::clamp(count, 0, MaxControllers); ++i)
                captured[std::size_t(i)] = states[i];
        slots_ = captured; // also safe when capturing a view into this frame
    }
    bool connected() const {
        return std::any_of(slots_.begin(), slots_.end(), [](const auto &p) { return p.connected; });
    }
    void clear() {
        slots_.fill({});
    }
    // Fixed-step clients may receive several host snapshots before a tick.
    // Preserve press edges until explicitly consumed, without repeating holds.
    void captureLatched(const ControllerState *states, int count) {
        const auto prior = slots_;
        capture(states, count);
        for (std::size_t i = 0; i < slots_.size(); ++i)
            if (slots_[i].connected && prior[i].connected)
                slots_[i].pressed |= prior[i].pressed;
    }
    std::array<ControllerState, MaxControllers> consume() {
        const auto snapshot = slots_;
        for (auto &state : slots_)
            state.pressed = 0;
        return snapshot;
    }
    const ControllerState &operator[](int i) const {
        return slots_[std::size_t(i)];
    }
    const std::array<ControllerState, MaxControllers> &slots() const {
        return slots_;
    }

  private:
    std::array<ControllerState, MaxControllers> slots_{};
};
// Edge-based device ownership. Assignment and response are supplied by a module;
// this class owns only slot/held state. Clearing owners deliberately preserves
// held edges; cancel additionally rearms them (pause/disconnect lifecycle).
class ControllerOwnership {
  public:
    ControllerOwnership() {
        cancel();
    }
    const PlayerMotion &motion(int player) const {
        return motion_[std::size_t(player)];
    }
    void clearOwners() {
        owner_.fill(-1);
        motion_.fill({});
    }
    void cancel() {
        clearOwners();
        held_.fill(false);
    }
    template <class Player, class Busy, class Transform>
    void update(const std::array<PlayerMotion, MaxControllers> &input, bool enabled,
                Player selectPlayer, Busy busy, Transform transform) {
        for (int pad = 0; pad < MaxControllers; ++pad) {
            const bool want = enabled && input[std::size_t(pad)].active;
            if (!want && held_[std::size_t(pad)])
                for (int p = 0; p < MaxControllers; ++p)
                    if (owner_[std::size_t(p)] == pad) {
                        owner_[std::size_t(p)] = -1;
                        motion_[std::size_t(p)] = {};
                    }
            if (want && !held_[std::size_t(pad)]) {
                const int p = selectPlayer(pad);
                if (p >= 0 && p < MaxControllers && !busy(p) && owner_[std::size_t(p)] < 0)
                    owner_[std::size_t(p)] = pad;
            }
            held_[std::size_t(pad)] = want;
        }
        for (int p = 0; p < MaxControllers; ++p)
            if (owner_[std::size_t(p)] >= 0)
                motion_[std::size_t(p)] = transform(input[std::size_t(owner_[std::size_t(p)])]);
    }

  private:
    std::array<int, MaxControllers> owner_{};
    std::array<bool, MaxControllers> held_{};
    std::array<PlayerMotion, MaxControllers> motion_{};
};
} // namespace neon::input
