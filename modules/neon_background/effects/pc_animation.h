#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace neon::pc {
struct Clock {
    float phase = 0, delta = 0;
};
// Original fixed-tick clock chain. Caller controls tick scheduling; no audio or
// gameplay dependency. Sequence phase is sampled before its length division.
class Sequencer {
  public:
    void tick(float tempo);
    const std::array<Clock, 4> &clocks() const {
        return clocks_;
    }

  private:
    float phase_ = 0;
    std::array<unsigned, 4> steps_{};
    std::array<Clock, 4> clocks_{};
};
struct Channel {
    float rate = 0, offset = 0, amplitude = 1;
    uint8_t flags = 0, clock = 0, waveform = 0;
};
using Channels = std::array<Channel, 32>;
using Values = std::array<std::optional<float>, 32>;
Channels loadChannels(const uint8_t *record, size_t size);
// Unsupported external/noise sources propagate as unavailable; never silently
// substitute a sine wave or zero. Lane dependencies follow original order.
Values evaluate(const Channels &, const std::array<Clock, 4> &, const Values &previous = {},
                std::optional<unsigned> inputMode = {}, bool jukebox = false);
struct Intervention {
    bool frozen = false;
    std::optional<float> overrideValue;
    float amplitude = 1, offset = 0;
    float apply(float original, float previous) const;
};
float mapFloat(float normalized, float minimum, float maximum);
int32_t mapInteger(float normalized, int32_t minimum, int32_t maximum);
} // namespace neon::pc
