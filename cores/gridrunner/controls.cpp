#include "controls.h"
namespace gridrunner {
namespace {
bool signChange(double a, double b) {
    return (a < 0) != (b < 0);
}
} // namespace
void TouchControl::begin(Axis p) {
    cancel();
    active_ = true;
    current_ = previous_ = origin_ = p;
}
void TouchControl::move(Axis p) {
    if (!active_)
        return;
    motion_ = {p.x - current_.x, p.y - current_.y};
    current_ = p;
    moved_ = true;
}
void TouchControl::release() {
    active_ = false;
}
void TouchControl::cancel() {
    active_ = moved_ = false;
    current_ = previous_ = origin_ = motion_ = velocity_ = {};
}
Axis TouchControl::tick(TouchMode mode, TouchTarget target) {
    // Classic consumes getvJoy directly; the modern ship applies an additional
    // .09 multiplier. Trackpad's source multiplier is 25/3 before that boundary.
    const double trackpadGain = target == TouchTarget::Classic ? 25. / 3. : .75;
    const double expertGain = target == TouchTarget::Classic ? 1. : .09;
    if (mode == TouchMode::Normal) {
        if (active_ || moved_) {
            Axis delta{current_.x - previous_.x, current_.y - previous_.y};
            previous_ = current_;
            velocity_ = std::hypot(delta.x, delta.y) < .001
                            ? Axis{}
                            : Axis{delta.x * trackpadGain, delta.y * trackpadGain};
        } else {
            velocity_.x *= .9;
            velocity_.y *= .9;
        }
    } else if (active_) {
        // iOS getvJoy resets one origin axis on a deliberate direction reversal.
        Axis delta{current_.x - origin_.x, current_.y - origin_.y};
        if (moved_ && std::abs(motion_.x) > .006 && signChange(motion_.x, delta.x))
            origin_.x = current_.x;
        if (moved_ && std::abs(motion_.y) > .006 && signChange(motion_.y, delta.y))
            origin_.y = current_.y;
        velocity_ = {(current_.x - origin_.x) * expertGain, (current_.y - origin_.y) * expertGain};
    } else
        velocity_ = {};
    moved_ = false;
    return velocity_;
}

} // namespace gridrunner
