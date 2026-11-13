#include "pc_animation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace neon::pc {
namespace {
float bigFloat(const uint8_t *p) {
    uint32_t bits = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
    float value;
    std::memcpy(&value, &bits, 4);
    if (!std::isfinite(value))
        throw std::runtime_error("Non-finite modulation parameter");
    return value;
}
std::optional<float> wave(unsigned id, float phase, std::optional<unsigned> inputMode,
                          bool jukebox) {
    // 00410960..00410ac0 -> 004108e0: saved mode4 bypasses input/history
    // lookup and returns the initialized zero. Unknown modes remain unavailable.
    // 00410929..00410950 also bypasses mode 0 when context+554 is set.
    // The menu/Jukebox loop sets that flag to 1; gameplay case 6 clears it.
    if (inputMode && (*inputMode == 4 || (jukebox && *inputMode == 0)) && id >= 11 && id <= 22)
        return 0.f;
    // Table initialization: 00411810, float increment at 004c4dac.
    // Lookup index conversion truncates as in the original ftol helper.
    constexpr double step = double(0.00038349518f);
    auto sine = [step](int index) { return float(std::sin(double(index & 16383) * step)); };
    const int index = int(phase * 16384.f);
    switch (id) {
    case 0:
        return sine(index);
    case 1:
        return std::abs(sine(index));
    case 2:
        return sine(4096 + int(phase * -16384.f));
    case 3:
        return std::abs(sine(4096 + int(phase * -16384.f)));
    case 4:
        return phase * 2.f - 1.f;
    case 5:
        return (phase * 4.f >= 2.f ? 4.f - phase * 4.f : phase * 4.f) - 1.f;
    case 6:
        return phase < .5f ? -1.f : 1.f;
    // 7 is a random lookup table. Its original state is not reconstructed yet.
    case 8:
        return 1.f - phase;
    case 9:
        return phase;
    case 10:
        return 1.f;
    default:
        return {};
    }
}
float shape(float value, bool wrap) {
    if (!wrap)
        return std::clamp(value, -1.f, 1.f);
    if (value < -1.f)
        value += 2.f * std::ceil((-1.f - value) / 2.f);
    if (value > 1.f)
        value -= 2.f * std::ceil((value - 1.f) / 2.f);
    return value;
}
} // namespace
void Sequencer::tick(float tempo) {
    if (!std::isfinite(tempo) || tempo < 0 || tempo > 3600)
        throw std::runtime_error("Invalid original tick tempo");
    phase_ += tempo * (1.f / 3600.f);
    bool pulse = phase_ >= 1.f;
    if (pulse)
        phase_ -= 1.f;
    float upstream = phase_;
    constexpr std::array<unsigned, 4> lengths = {4, 4, 8, 2};
    for (size_t i = 0; i < clocks_.size(); ++i) {
        bool nextPulse = false;
        if (pulse && ++steps_[i] >= lengths[i]) {
            steps_[i] = 0;
            nextPulse = true;
        }
        const float old = clocks_[i].phase;
        clocks_[i].phase = upstream;
        clocks_[i].delta = upstream - old;
        if (clocks_[i].delta < 0)
            clocks_[i].delta += 1.f;
        upstream = (float(steps_[i]) + upstream) / float(lengths[i]);
        pulse = nextPulse;
    }
}
Channels loadChannels(const uint8_t *record, size_t size) {
    if (size != 0x86c)
        throw std::runtime_error("Expected original v5 module record");
    Channels result;
    for (size_t i = 0; i < result.size(); ++i) {
        auto &c = result[i];
        c.rate = bigFloat(record + 0x68c + 4 * i);
        c.offset = bigFloat(record + 0x70c + 4 * i);
        c.amplitude = bigFloat(record + 0x78c + 4 * i);
        c.flags = record[0x80c + i];
        c.clock = record[0x82c + i];
        c.waveform = record[0x84c + i];
    }
    return result;
}
Values evaluate(const Channels &channels, const std::array<Clock, 4> &clocks,
                const Values &previous, std::optional<unsigned> inputMode, bool jukebox) {
    Values result = previous;
    for (size_t i = 0; i < channels.size(); ++i) {
        const auto &c = channels[i];
        if (c.clock >= clocks.size()) {
            result[i].reset();
            continue;
        }
        const auto &clock = clocks[c.clock];
        float integral;
        const float phase = std::modf(clock.phase + clock.delta * c.rate, &integral);
        auto value = wave(c.waveform, phase, inputMode, jukebox);
        if (!value) {
            result[i].reset();
            continue;
        }
        if (c.flags & 2)
            *value = -*value;
        *value = *value * c.amplitude + c.offset;
        const size_t lane = i % 4, other = (c.flags >> 5) & 3, group = i - lane;
        if (other != lane) {
            // A recovered zero times an unavailable finite source is still exactly zero.
            // Mode-4 external lanes use this with the original history/user-wave lanes.
            if (!result[group + other]) {
                if (!(c.flags & 1) && *value == 0.f) {
                    result[i] = 0.f;
                    continue;
                }
                result[i].reset();
                continue;
            }
            if (c.flags & 1)
                *value += *result[group + other];
            else
                *value *= *result[group + other];
        }
        result[i] = shape(*value, (c.flags & 4) != 0);
    }
    return result;
}
float Intervention::apply(float original, float previous) const {
    if (overrideValue)
        return *overrideValue;
    if (frozen)
        return previous;
    return original * amplitude + offset;
}
float mapFloat(float normalized, float minimum, float maximum) {
    return (normalized + 1.f) * (maximum - minimum) * .5f + minimum;
}
int32_t mapInteger(float normalized, int32_t minimum, int32_t maximum) {
    return int32_t((normalized + 1.f) * float(int64_t(maximum) - minimum) * .5f) + minimum;
}
} // namespace neon::pc
