// App opening: constant acceleration for the image, exact reversed home motion.
#pragma once
#include "launcher_entrance.h"

namespace mino::iox {
class AppLaunch {
  public:
    static constexpr double seconds = .25;
    void start() {
        elapsed_ = 0;
        active_ = true;
    }
    void cancel() {
        active_ = false;
    }
    bool active() const {
        return active_;
    }
    float progress() const {
        return float(std::clamp(elapsed_ / seconds, 0.0, 1.0));
    }
    static float imageProgress(float progress) {
        return progress * progress;
    }
    static float imageScale(float progress) {
        return .25f + .75f * imageProgress(progress);
    }
    static HomeEntrance homePose(float progress) {
        HomeEntrance pose;
        pose.seek(1 - progress);
        return pose;
    }
    void advance(double dt) {
        if (std::isfinite(dt) && dt > 0)
            elapsed_ = std::min(seconds, elapsed_ + dt);
        if (elapsed_ > seconds - 1e-9)
            elapsed_ = seconds;
    }

  private:
    double elapsed_ = 0;
    bool active_ = false;
};
} // namespace mino::iox
