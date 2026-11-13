#pragma once
#include "device_motion.h"
#include <cmath>
#include <cstdio>
#include <string>
namespace neon::input {
// Sensor-native coordinates keep calibration independent of screen rotation.
// A shortest-arc quaternion moves the chosen neutral gravity to (0,0,-1).
class TiltCalibration {
  public:
    bool set(MotionState neutral) {
        const double n =
            std::sqrt(neutral.x * neutral.x + neutral.y * neutral.y + neutral.z * neutral.z);
        if (!neutral.available || !std::isfinite(n) || n < .5 || n > 1.5)
            return false;
        neutral_ = neutral;
        const double ax = neutral.x / n, ay = neutral.y / n, az = neutral.z / n;
        w_ = 1 - az;
        x_ = -ay;
        y_ = ax;
        z_ = 0;
        const double length = std::sqrt(w_ * w_ + x_ * x_ + y_ * y_);
        if (length < 1e-8) {
            w_ = 0;
            x_ = 1;
            y_ = z_ = 0;
        } else {
            w_ /= length;
            x_ /= length;
            y_ /= length;
        }
        return true;
    }
    MotionState apply(MotionState v) const {
        if (!v.available)
            return v;
        const double tx = 2 * (y_ * v.z - z_ * v.y), ty = 2 * (z_ * v.x - x_ * v.z),
                     tz = 2 * (x_ * v.y - y_ * v.x);
        return {true, v.x + w_ * tx + y_ * tz - z_ * ty, v.y + w_ * ty + z_ * tx - x_ * tz,
                v.z + w_ * tz + x_ * ty - y_ * tx};
    }
    std::string encode() const {
        char s[128];
        std::snprintf(s, sizeof s, "%.17g,%.17g,%.17g", neutral_.x, neutral_.y, neutral_.z);
        return s;
    }
    void decode(const std::string &s) {
        MotionState v;
        v.available = true;
        char extra = 0;
        if (std::sscanf(s.c_str(), "%lf,%lf,%lf%c", &v.x, &v.y, &v.z, &extra) == 3)
            set(v);
    }

  private:
    MotionState neutral_{true, 0, 0, -1};
    double w_ = 1, x_ = 0, y_ = 0, z_ = 0;
};
} // namespace neon::input
