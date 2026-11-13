#pragma once
#include <cstdint>
namespace gr::reconstruction::ios {
// Original iOS oxRNG range calls global _rand, ignoring its object receiver.
// Bind one stream for the whole game, including particles and score artwork.
struct RandomStream {
    virtual ~RandomStream() = default;
    virtual uint32_t next() = 0;
    double range(double low, double high); // iOS 0x99f8
};
// Apple's published default rand algorithm, expressed as a 64-bit modular
// product rather than depending on the host libc or its integer word size.
// Runtime identity/seed timing still require original-execution validation.
class DarwinRandom final : public RandomStream {
  public:
    explicit DarwinRandom(uint32_t initialSeed = 1) : state_(initialSeed) {}
    void seed(uint32_t value) {
        state_ = value;
    }
    uint32_t state() const {
        return state_;
    }
    uint32_t next() override;

  private:
    uint32_t state_;
};
} // namespace gr::reconstruction::ios
