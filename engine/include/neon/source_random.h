#pragma once
#include <cstdint>

namespace neon {
// iOS global rand stream. Receivers of oxRNG::range do not own separate RNGs.
// The caller supplies the stream state at the reconstruction boundary.
class DarwinRandom {
  public:
    explicit DarwinRandom(uint32_t seed = 1) : state_(seed) {}
    void seed(uint32_t value) {
        state_ = value;
        draws_ = 0;
    }
    uint32_t state() const {
        return state_;
    }
    uint64_t draws() const {
        return draws_;
    }
    uint32_t next() {
        const uint64_t value = state_ ? state_ : 123459876u;
        state_ = uint32_t(value * 16807u % 2147483647u);
        ++draws_;
        return state_;
    }
    double range(double low, double high) {
        // GoatUp 2 0xa6a0 converts signed integer directly to binary64.
        const double fraction = double(next()) / 2147483647.;
        return (high - low) * fraction + low;
    }

  private:
    uint32_t state_;
    uint64_t draws_ = 0;
};
} // namespace neon
