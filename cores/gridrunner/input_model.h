// Proportional input experiment. This is a port extension, not recovered iCade physics.
#pragma once
#include <algorithm>
#include <cmath>

namespace gridrunner {
struct Axis {
    double x = 0, y = 0;
};
struct InputTuning {
    double gain = 1, curve = 1, touchTravel = 48;
};

inline Axis unitDisk(Axis v) {
    if (!std::isfinite(v.x) || !std::isfinite(v.y))
        return {};
    double length = std::hypot(v.x, v.y);
    if (length > 1) {
        v.x /= length;
        v.y /= length;
    }
    return v;
}
// PadState is already dead-zoned by the shared input layer: do not apply a
// second dead zone here. Normalize direction before applying curve and gain.
// The result is a probe movement multiplier and may exceed 1: gain scales
// maximum speed, rather than saturating small inputs at a fixed speed cap.
inline Axis response(Axis v, const InputTuning &tuning) {
    v = unitDisk(v);
    double length = std::hypot(v.x, v.y);
    if (length == 0)
        return {};
    double magnitude =
        std::max(0.0, tuning.gain) * std::pow(length, std::clamp(tuning.curve, 0.25, 4.0));
    return {v.x * magnitude / length, v.y * magnitude / length};
}
inline Axis relativeTouch(Axis origin, Axis current, const InputTuning &tuning) {
    double travel = std::max(tuning.touchTravel, 1.0);
    return response({(current.x - origin.x) / travel, (current.y - origin.y) / travel}, tuning);
}
} // namespace gridrunner
