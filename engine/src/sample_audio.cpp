#include "neon/sample_audio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace neon::audio {
namespace {
int32_t signed32(uint32_t x) {
    return x <= 0x7fffffffU ? int32_t(x) : int32_t(int64_t(x) - 0x100000000LL);
}
int16_t signed16(uint16_t x) {
    return x <= 0x7fff ? int16_t(x) : int16_t(int(x) - 0x10000);
}
int q16(int16_t sample, int volume) {
    const int64_t product = int64_t(sample) * volume;
    // ARM SMULW's arithmetic shift, followed by the packed low halfword.
    const int64_t scaled = product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
    return signed16(uint16_t(uint64_t(scaled) & 0xffff));
}
int32_t lateAdd(int32_t sum, int16_t sample, int volume) {
    const auto doubled = signed32(uint32_t(uint64_t(int64_t(sample) * volume) * 2));
    const float contribution = float(doubled) * (1.f / 65536.f);
    const float mixed = float(sum) + contribution;
    // NEON VCVT.S32.F32 truncates toward zero and saturates on overflow.
    if (double(mixed) >= 2147483647.)
        return 2147483647;
    if (double(mixed) <= -2147483648.)
        return (-2147483647 - 1);
    return int32_t(mixed);
}
} // namespace
NativeSampleMixer::NativeSampleMixer(MixerRules rules) : rules_(rules), voices_(rules.voiceLimit) {
    if (rules.voiceLimit < 1)
        throw std::invalid_argument("Audio voice limit must be positive");
}
void NativeSampleMixer::setSamples(std::vector<Sample> samples) {
    stopAll();
    samples_ = std::move(samples);
    group_ = 0;
    for (auto &sample : samples_) {
        sample.active = 0;
        sample.pcm.resize(sample.pcm.size() & ~std::size_t(7));
    }
}
void NativeSampleMixer::start(std::size_t slot, int sample, int volume, int loop) {
    voices_[slot] = {sample, volume, loop, 0};
    ++samples_[std::size_t(sample)].active;
}
void NativeSampleMixer::play(int n, int volume, int loop) {
    if (n < 0 || std::size_t(n) >= samples_.size())
        return;
    auto &sample = samples_[std::size_t(n)];
    if (sample.pcm.empty() || (group_ != 0 && sample.group == 0))
        return;
    if (sample.active == sample.maxInstances) {
        if (rules_.retrigger == RetriggerRule::RequireSampleRestart && !sample.restart)
            return;
        for (auto &voice : voices_)
            if (voice.sample == n) {
                voice.position = 0;
                return;
            }
        return;
    }
    for (std::size_t i = 0; i < voices_.size(); ++i)
        if (voices_[i].sample < 0) {
            start(i, n, volume, loop);
            return;
        }
    // Original scan chooses the FIRST nonnegative priority, then compares only
    // that slot. It does not search for the lowest priority or any stealable one.
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto &old = samples_[std::size_t(voices_[i].sample)];
        if (old.priority < 0)
            continue;
        if (sample.priority < old.priority) {
            --old.active;
            start(i, n, volume, loop);
        }
        return;
    }
    // Reject the original's out-of-range all-negative case. Rescue's unrolled
    // replacement index is also unsafe; use the bounded shared slot above.
}
void NativeSampleMixer::stop(int n) {
    if (n < 0 || std::size_t(n) >= samples_.size())
        return;
    for (auto &voice : voices_)
        if (voice.sample == n) {
            voice = {};
            samples_[std::size_t(n)].active = 0;
            return;
        }
}
void NativeSampleMixer::stopAll() {
    for (auto &v : voices_)
        v = {};
    for (auto &s : samples_)
        s.active = 0;
}
void NativeSampleMixer::setGroup(int group) {
    group_ = group;
    if (group != 1)
        return;
    for (auto &voice : voices_)
        if (voice.sample >= 0) {
            auto &s = samples_[std::size_t(voice.sample)];
            if (s.group == 0) {
                s.active = 0;
                voice = {};
            }
        }
}
void NativeSampleMixer::mixMono(int16_t *out, int frames) {
    mixFrames(out, nullptr, frames);
}
void NativeSampleMixer::mixFloat(float *out, int frames) {
    mixFrames(nullptr, out, frames);
}
void NativeSampleMixer::mixFrames(int16_t *out, float *clean, int frames) {
    if (frames <= 0)
        return;
    const bool early = rules_.arithmetic == MixArithmetic::ReverseQ16Saturating;
    // Sample-major traversal is equivalent to the original voice-major loops:
    // preserve voice order, exact endpoint release and unchanged nonzero loops.
    for (int frame = 0; frame < frames; ++frame) {
        int32_t sum = 0;
        float floatSum = 0;
        for (std::size_t k = 0; k < voices_.size(); ++k) {
            auto &voice = voices_[early ? voices_.size() - 1 - k : k];
            if (voice.sample < 0)
                continue;
            auto &sample = samples_[std::size_t(voice.sample)];
            if (voice.position >= sample.pcm.size()) {
                if (voice.loop != 0)
                    voice.position = 0;
                else {
                    --sample.active;
                    voice.sample = -1;
                    continue;
                }
            }
            const int16_t value = sample.pcm[voice.position++];
            if (clean)
                floatSum +=
                    (float(value) / 32768.f) * (float(voice.volume) / (early ? 65536.f : 32768.f));
            else if (early)
                sum = std::clamp(sum + q16(value, voice.volume), -32768, 32767);
            else
                sum = lateAdd(sum, value, voice.volume);
            // Early callback releases a one-shot even when its last sample
            // lands EXACTLY on the callback boundary; late code checks next visit.
            if (early && voice.position == sample.pcm.size()) {
                if (voice.loop != 0)
                    voice.position = 0;
                else {
                    --sample.active;
                    voice.sample = -1;
                }
            }
        }
        if (clean)
            clean[frame] = floatSum;
        else
            out[frame] = int16_t(early ? sum : std::clamp(sum, -32767, 32767));
    }
}
SampleMixer::SampleMixer(MixerRules rules, int outputRate, float outputGain, PlaybackMix playback)
    : native_(rules), outputRate_(outputRate), outputGain_(outputGain), playback_(playback) {
    if (!std::isfinite(outputGain) || outputGain < 0 || outputGain > 1)
        throw std::invalid_argument("Host output gain must be between zero and one");
    if (outputRate < NativeRate)
        throw std::invalid_argument("Host audio rate must be at least 11025 Hz");
}
void SampleMixer::setSamples(std::vector<Sample> samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    native_.setSamples(std::move(samples));
    phase_ = 0;
    previous_ = current_ = 0;
    previousFloat_ = currentFloat_ = 0;
    started_ = false;
}
void SampleMixer::play(int n, int volume, int loop) {
    std::lock_guard<std::mutex> lock(mutex_);
    native_.play(n, volume, loop);
}
void SampleMixer::stop(int n) {
    std::lock_guard<std::mutex> lock(mutex_);
    native_.stop(n);
}
void SampleMixer::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    native_.stopAll();
    previous_ = current_ = 0;
    previousFloat_ = currentFloat_ = 0;
}
void SampleMixer::setGroup(int group) {
    std::lock_guard<std::mutex> lock(mutex_);
    native_.setGroup(group);
}
void SampleMixer::mix(float *out, int frames) {
    if (frames <= 0)
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < frames; ++i) {
        if (playback_ == PlaybackMix::CleanFloat) {
            if (!started_) {
                native_.mixFloat(&currentFloat_, 1);
                started_ = true;
            }
            const float value =
                float(double(previousFloat_) +
                      (double(currentFloat_) - previousFloat_) * double(phase_) / outputRate_);
            out[2 * i] = out[2 * i + 1] = std::clamp(value * outputGain_, -1.f, 1.f);
        } else {
            if (!started_) {
                native_.mixMono(&current_, 1);
                started_ = true;
            }
            const float value = float((double(previous_) + (double(current_) - previous_) *
                                                               double(phase_) / outputRate_) /
                                      32768.);
            out[2 * i] = out[2 * i + 1] = value * outputGain_;
        }
        phase_ += NativeRate;
        if (phase_ >= outputRate_) {
            phase_ -= outputRate_;
            previous_ = current_;
            previousFloat_ = currentFloat_;
            started_ = false;
        }
    }
}
bool parseWav(const std::vector<uint8_t> &d, Sample &out) {
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4))
        return false;
    auto u16 = [&](std::size_t p) { return uint16_t(uint16_t(d[p]) | (uint16_t(d[p + 1]) << 8)); };
    auto u32 = [&](std::size_t p) {
        return uint32_t(d[p]) | (uint32_t(d[p + 1]) << 8) | (uint32_t(d[p + 2]) << 16) |
               (uint32_t(d[p + 3]) << 24);
    };
    int channels = 0, bits = 0, rate = 0;
    std::size_t dataAt = 0, dataSize = 0;
    for (std::size_t at = 12; at <= d.size() - 8;) {
        const auto length = std::size_t(u32(at + 4)), body = at + 8;
        if (length > d.size() - body)
            return false;
        if (!std::memcmp(d.data() + at, "fmt ", 4)) {
            if (length < 16 || u16(body) != 1)
                return false;
            channels = u16(body + 2);
            rate = int(u32(body + 4));
            bits = u16(body + 14);
            if (channels < 1 || channels > 2 || rate <= 0 || (bits != 8 && bits != 16))
                return false;
        } else if (!std::memcmp(d.data() + at, "data", 4)) {
            dataAt = body;
            dataSize = length;
        }
        if (length == d.size() - body)
            break;
        at = body + length + (length & 1);
    }
    if (channels == 0 || dataAt == 0)
        return false;
    const std::size_t stride = std::size_t(channels * (bits / 8)), count = dataSize / stride;
    if (count == 0)
        return false;
    std::vector<int16_t> decoded(count);
    for (std::size_t i = 0; i < count; ++i) {
        int sum = 0;
        for (int c = 0; c < channels; ++c) {
            const auto p = dataAt + i * stride + std::size_t(c) * (bits / 8);
            sum += bits == 16 ? signed16(u16(p)) : (int(d[p]) - 128) * 256;
        }
        decoded[i] = int16_t(sum / channels);
    }
    const uint64_t nativeCount = uint64_t(count) * NativeRate / unsigned(rate);
    if (nativeCount > std::numeric_limits<std::size_t>::max())
        return false;
    std::vector<int16_t> converted(std::size_t(nativeCount) & ~std::size_t(7));
    for (std::size_t i = 0; i < converted.size(); ++i) {
        const uint64_t position = uint64_t(i) * unsigned(rate);
        const auto p = std::size_t(position / NativeRate);
        const double fraction = double(position % NativeRate) / NativeRate;
        converted[i] =
            int16_t(double(decoded[p]) +
                    (double(decoded[std::min(p + 1, count - 1)]) - decoded[p]) * fraction);
    }
    out.pcm = std::move(converted);
    out.rate = NativeRate;
    out.active = 0;
    return true;
}
} // namespace neon::audio
