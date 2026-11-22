// Optional presentation analysis of already-mixed audio; never reads game state.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
namespace mino {
class AudioLevel {
  public:
    void submit(const float *samples, size_t count) {
        double energy = 0;
        for (size_t i = 0; i < count; ++i) {
            const float value = std::isfinite(samples[i]) ? std::clamp(samples[i], -1.f, 1.f) : 0.f;
            energy += double(value) * value;
        }
        level_.store(count ? float(std::sqrt(energy / count)) : 0.f, std::memory_order_relaxed);
    }
    float value() const { return level_.load(std::memory_order_relaxed); }

  private:
    std::atomic<float> level_{0};
};
} // namespace mino
