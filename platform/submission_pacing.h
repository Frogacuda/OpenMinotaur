#pragma once
#include <cstdint>
namespace mino {
// A minimum spacing between frame starts, without catch-up submissions. The
// host supplies monotonic nanoseconds and performs any requested sleep itself.
class SubmissionPacer {
  public:
    uint64_t remaining(uint64_t now, uint64_t period) const {
        if (!started_ || !period || now < previous_ || now - previous_ >= period)
            return 0;
        return period - (now - previous_);
    }
    void started(uint64_t now) {
        previous_ = now;
        started_ = true;
    }
    void reset() {
        started_ = false;
    }

  private:
    uint64_t previous_ = 0;
    bool started_ = false;
};
} // namespace mino
