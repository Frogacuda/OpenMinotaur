#include "stage_controls.h"
#include <algorithm>
#include <cmath>

namespace goatup2 {
bool StageControls::anyReportInput() const {
    for (const auto &pad : pads_.slots())
        if (pad.connected &&
            (pad.buttons || pad.lx != 0 || pad.ly != 0 || pad.rx != 0 || pad.ry != 0))
            return true;
    for (int i = 0; i < 4; ++i)
        if (touches_.touchActive[i])
            return true;
    return false;
}
void StageControls::beginStage() {
    // Reset the per-stage movement/jump producer without manufacturing a
    // release of physical inputs or feeding a menu acceptance into gameplay.
    signal_ = digitalSignal_ = 0;
    touchJump_ = padJump_ = 0;
    fastTouchResponse = false;
}
using namespace neon::input;
float StageControls::gentle(float axis) {
    if (!std::isfinite(axis))
        return 0;
    axis = std::clamp(axis, -1.f, 1.f);
    return std::copysign(
        axis * axis,
        axis); // Gentle response, unit gain: finer center control without limiting full travel
}
void StageControls::pads(const ControllerState *states, int count) {
    pads_.capture(states, count);
}
bool StageControls::ownsTouch(uint32_t id) const {
    for (int i = 0; i < 4; ++i)
        if (touches_.touchActive[i] && touches_.touchId[i] == id)
            return true;
    return false;
}
void StageControls::touch(TouchPhase phase, uint32_t id, neon::vec2d point) {
    if (phase == TouchPhase::Down) {
        const int existing = firstSlot(
            4, [&](int i) { return touches_.touchActive[i] && touches_.touchId[i] == id; });
        if (existing >= 0)
            return;
        const int i = beginTouch(touches_, id, point, TouchTracking::Basic);
        if (i >= 0) {
            floatingOrigin_[i] = point;
            horizontalGesture_[i] = {point.x, point.x, 0};
            verticalGesture_[i] = {point.y, point.y, 0};
            touches_.touchStartPos[i] = point;
        }
    } else if (phase == TouchPhase::Move)
        moveTouch(touches_, id, point, TouchTracking::Basic);
    else
        endTouch(touches_, id);
}
void StageControls::motion(MotionState value, bool enabled) {
    tilt_ = enabled && value.available && std::isfinite(value.x);
    if (!tilt_) {
        previousMotionX_ = tiltX_ = 0;
        return;
    }
    tiltX_ = double(float(-(value.x + previousMotionX_) * .5));
    previousMotionX_ = value.x;
}
void StageControls::cancel() {
    pads_.clear();
    purgeTouchState(touches_);
    signal_ = digitalX_ = digitalSignal_ = 0;
    touchJump_ = padJump_ = 0;
    awaitNeutral_ = true;
}
StageIntent StageControls::tick(StageRuntime &world, bool playerActive) {
    bool left = false, right = false, vertical = false, jump = false;
    float axis = 0;
    for (const auto &p : pads_.slots())
        if (p.connected) {
            left = left || bool(p.buttons & Left);
            right = right || bool(p.buttons & Right);
            vertical = vertical || bool(p.buttons & (Up | Down));
            jump = jump || bool(p.buttons & South);
            if (std::fabs(p.lx) > std::fabs(axis))
                axis = p.lx;
        }
    bool anyTouch = false;
    for (bool active : touches_.touchActive)
        anyTouch = anyTouch || active;
    if (awaitNeutral_) {
        if (left || right || vertical || jump || axis != 0 || anyTouch)
            return {};
        awaitNeutral_ = false;
    }
    // Original outer 0x423c8 digital X ramp, retained for modern d-pad input.
    // Both-held order is left then right; reversal zeroes the prior direction.
    if (!left && !right && !vertical)
        digitalX_ *= .1;
    else {
        if (left) {
            if (digitalX_ > 0)
                digitalX_ = 0;
            digitalX_ = std::max(-1., digitalX_ - double(.03f));
        }
        if (right) {
            if (digitalX_ < 0)
                digitalX_ = 0;
            digitalX_ = std::min(1., digitalX_ + double(.03f));
        }
    }
    if (!playerActive)
        return {}; // outer digital ramp still runs during death/exit
    // 0x4c648 before the touch producer. The modern analog mapping below is
    // a direct-input extension; these friction rules preserve the original behavior.
    if (fastTouchResponse)
        world.inputFriction = world.savedInputFriction;
    else if (double(world.inputFriction) < .99) {
        world.inputFriction = float(double(world.inputFriction) + .0001);
        if (double(world.inputFriction) > .99)
            world.inputFriction = .99f;
    }
    int movementTouch = -1;
    bool touchJump = false;
    for (int i = 0; i < 4; ++i)
        if (touches_.touchActive[i]) {
            if (tilt_) {
                touchJump = true;
                continue;
            } // Original mode 2: every touch is jump.
            // Preserve the original regions, but hold their role until release.
            // The floating origin still follows the original response arithmetic.
            const auto origin = touches_.touchStartPos[i];
            const bool lower = origin.y >= double(-.2f);
            const bool onRight = origin.x >= double(hand == TouchHand::MovementLeft ? .2f : -.2f);
            if (lower && onRight) {
                if (hand == TouchHand::MovementLeft)
                    touchJump = true;
                else if (movementTouch < 0)
                    movementTouch = i;
            } else if (!onRight) {
                if (hand == TouchHand::MovementLeft) {
                    if (movementTouch < 0)
                        movementTouch = i;
                } else
                    touchJump = true;
            }
        }
    touchJump_ = touchJump ? std::min(30, touchJump_ + 1) : 0;
    if (movementTouch < 0)
        signal_ -= signal_ * double(world.inputFriction);
    else {
        auto &origin = floatingOrigin_[movementTouch];
        const auto current = touches_.touchPos[movementTouch];
        constexpr double scale = .1, deadzone = .0005;
        auto delta = (current - origin) * scale;
        // Remember return progress within a stroke. Repeated/jittering reports
        // must not ratchet the floating origin toward a held finger.
        auto follow = [&](auto &stroke, double point, double &anchor, bool enabled) {
            const double offset = point - anchor;
            const int direction = offset < 0 ? -1 : 1;
            if (std::fabs(offset) < deadzone / scale || stroke.direction != direction) {
                stroke = {point, point, direction};
                return;
            }
            if (direction * (point - stroke.peak) >= deadzone / scale) {
                stroke.peak = stroke.retreat = point;
            } else if (direction * (stroke.retreat - point) >= deadzone / scale) {
                if (enabled)
                    anchor = anchor + (point - anchor) * .1;
                stroke.retreat = point;
            }
        };
        follow(horizontalGesture_[movementTouch], current.x, origin.x, !fastTouchResponse);
        if (fastTouchResponse && world.goat) {
            const auto sign = [](double value) { return value < 0 ? -1 : 1; };
            world.savedInputFriction =
                sign(float(delta.x)) == sign(float(world.goat->velocity.x)) ? .05f : .2f;
        }
        follow(verticalGesture_[movementTouch], current.y, origin.y, true);
        const float length = float(delta.length());
        if (length > .009f)
            delta = delta * double(.009f / length);
        if (std::fabs(delta.x) < deadzone)
            delta.x = 0;
        if (std::fabs(delta.y) < deadzone)
            delta.y = 0;
        const double requested = double(float(delta.x * 10.));
        signal_ = signal_ + double(world.inputFriction) * (requested - signal_);
    }
    // Original 0x4c648 mode 2 overrides the touch movement signal.
    if (tilt_) {
        const float stickAxis = float(tiltX_ * .5);
        signal_ = std::abs(stickAxis) <= .01f ? signal_ * .9 : double(stickAxis) * .7;
    }
    // Accepted native analog profile uses the same bounded .09 signal. Read
    // the canonical dead-zoned axis directly, leaving all actor physics intact.
    if (movementTouch < 0 && axis != 0)
        signal_ = double(gentle(axis)) * .09;
    if (!left && !right && !vertical)
        digitalSignal_ -= digitalSignal_ * double(world.inputFriction);
    else if (std::fabs(digitalX_) > .001)
        digitalSignal_ = digitalX_ * .1;
    if (std::fabs(signal_) < std::fabs(digitalSignal_))
        signal_ = digitalSignal_;
    padJump_ = jump ? std::min(30, padJump_ + 1) : 0;
    // Source uses bitwise OR of the two independent held counters, not max.
    return {signal_, touchJump_ | padJump_};
}
} // namespace goatup2
