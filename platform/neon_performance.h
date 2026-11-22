#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace mino {
// Collect only during the existing loading presentation, then make one immutable choice.
class NeonCadence {
  public:
    void reset() {
        *this = NeonCadence{};
    }
    void observe(double workMs, double intervalMs) {
        if (locked_ || !std::isfinite(workMs) || !std::isfinite(intervalMs) || intervalMs <= 0)
            return;
        if (warmup_++ < 4)
            return;
        if (work_.size() < 240) {
            work_.push_back(workMs);
            missed_ += intervalMs > 22.;
        }
    }
    void finish() {
        if (locked_)
            return;
        locked_ = true;
        if (work_.size() >= 12) {
            std::sort(work_.begin(), work_.end());
            if (work_[work_.size() * 9 / 10] > 14.5 ||
                missed_ > std::max<size_t>(2, work_.size() / 5))
                divisor_ = 2;
        }
    }
    bool finished() const {
        return locked_;
    }
    size_t sampleCount() const {
        return work_.size();
    }
    unsigned divisor() const {
        return divisor_;
    }

  private:
    bool locked_ = false;
    unsigned divisor_ = 1, warmup_ = 0, missed_ = 0;
    std::vector<double> work_;
};
// Count presented game frames. A delayed frame never creates extra updates or
// changes the phase of the every-other-frame schedule.
class NeonPacer {
  public:
    void reset() {
        frames_ = 0;
    }
    bool due(unsigned divisor) {
        if (divisor <= 1) {
            reset();
            return true;
        }
        frames_ = (frames_ + 1) % divisor;
        return frames_ == 0;
    }

  private:
    unsigned frames_ = 0;
};

// Measures CPU frame-work duration during the loading assessment.
class NeonFrameTimer {
  public:
    struct Sample {
        double workMs, intervalMs;
    };
    void begin(double interval) {
        interval_ = interval * 1000;
        start_ = SDL_GetTicksNS();
        running_ = true;
    }
    std::optional<Sample> end() {
        if (!running_)
            return {};
        running_ = false;
        return Sample{double(SDL_GetTicksNS() - start_) / 1e6, interval_};
    }
    void reset() {
        running_ = false;
    }

  private:
    Uint64 start_ = 0;
    double interval_ = 0;
    bool running_ = false;
};
} // namespace mino
