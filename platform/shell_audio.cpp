#include "shell_audio.h"

#include "core_api.h"
#include "shell_arcade.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mino {
namespace {
constexpr double kPi = 3.141592653589793;
constexpr double kStep = 1.0 / kAudioRate;
constexpr int kControlFrames = 64;
constexpr int kFirstNote = 35, kLastNote = 69;
constexpr size_t kNoteCount = kLastNote - kFirstNote + 1;
constexpr size_t kWaveSize = 2048;
// Apply the shell gain after summing the bed, arcade, and cue buses.
constexpr float kShellLevel = 1.25f;
constexpr float kMenuCueLevel = 1.10f;
constexpr float kPadLevel = .75f;
constexpr float kPadFadeIn = 3.4f;

struct Chord {
    std::array<int, 6> notes;
    std::array<int, 2> support;
    int root;
};
// Open Orbit: Dmaj9, Gmaj9(#11), Bm11, Em9.
constexpr Chord kChords[]{
    {{38, 45, 49, 54, 57, 64}, {50, 57}, 62},
    {{43, 50, 54, 59, 61, 69}, {55, 50}, 55},
    {{35, 54, 57, 61, 62, 64}, {47, 54}, 59},
    {{40, 47, 55, 59, 62, 66}, {52, 47}, 52},
};
constexpr double kCycle = 32.2, kHalfFade = 1.4;
constexpr std::array<double, 4> kChanges{7.5, 15.8, 24.0, kCycle};

float smooth(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}
float approach(float current, float target, float seconds) {
    const float step = float(kStep) / seconds;
    return current + std::clamp(target - current, -step, step);
}
double frequency(int note) {
    return 440.0 * std::exp2((note - 69) / 12.0);
}

struct Harmony {
    int from = 0, to = 0;
    float blend = 0;
};
Harmony harmonyAt(double time) {
    const double at = std::fmod(time, kCycle);
    if (time >= kCycle && at < kHalfFade)
        return {3, 0, smooth(float((at + kHalfFade) / (2 * kHalfFade)))};
    for (size_t i = 0; i < kChanges.size(); ++i) {
        const double offset = at - kChanges[i];
        if (offset < kHalfFade)
            return {int(i), int((i + 1) % kChanges.size()),
                    smooth(float((offset + kHalfFade) / (2 * kHalfFade)))};
    }
    return {};
}

struct Band {
    float a = 0, b = 0, c = 0;
    std::array<float, 2> integrator1{}, integrator2{};
    void set(float hz, float damping) {
        const float g = std::tan(float(kPi / kAudioRate) * hz);
        a = 1.f / (1.f + g * (g + damping));
        b = g * a;
        c = g * b;
    }
    float mix(int channel, float input) {
        auto &i1 = integrator1[channel];
        auto &i2 = integrator2[channel];
        const float residual = input - i2;
        const float v1 = a * i1 + b * residual;
        const float v2 = i2 + b * i1 + c * residual;
        i1 = 2.f * v1 - i1;
        i2 = 2.f * v2 - i2;
        return v1;
    }
};

enum class Tone { Melodic, Chord, Snare, Kick };

struct CueVoice {
    Tone type = Tone::Melodic;
    double hz = 0, phase = 0;
    int age = 0, length = 0;
    float attack = 0, gain = 0, decayStep = 0, pan = 0;
    float low = 0, high = 0;
};
struct PadVoice {
    double hz = 0, phase = 0;
    float gain = 0, gainStep = 0, support = 0, supportStep = 0, drift = 0;
};
} // namespace

struct ShellAudio::State {
    std::array<float, kWaveSize + 1> sine{}, mist{}, shimmer{};
    std::array<PadVoice, kNoteCount> pad{};
    std::array<CueVoice, 24> cues{};
    std::array<std::array<float, 4096>, 2> reflections{};
    ShellArcade arcade;
    Band lowBand, highBand;
    double seconds = 0, lastTick = -1;
    size_t reflectionHead = 0, moves = 0;
    int control = 0, repeatedTicks = 0;
    uint32_t noise = 0xa03ed52bu;
    float ambienceGain = 0, padReveal = 0, cueGain = 0;
    bool suspended = false;

    State() {
        for (size_t i = 0; i <= kWaveSize; ++i) {
            const double phase = 2 * kPi * i / kWaveSize;
            sine[i] = float(std::sin(phase));
            mist[i] = float(.76 * std::sin(phase) + .37 * std::sin(2 * phase + .2) +
                            .20 * std::sin(3 * phase + .45) + .059 * std::sin(4 * phase));
            shimmer[i] = float(.40 * std::sin(2 * phase) + .36 * std::sin(3 * phase + .33) +
                               .14 * std::sin(4 * phase - .4));
        }
        for (size_t i = 0; i < pad.size(); ++i) {
            pad[i].hz = frequency(int(i) + kFirstNote);
            pad[i].phase = .137 * i + .19;
        }
    }

    static float wave(const std::array<float, kWaveSize + 1> &table, double cycles) {
        const double position = (cycles - std::floor(cycles)) * kWaveSize;
        const size_t index = size_t(position);
        return table[index] + (table[index + 1] - table[index]) * float(position - index);
    }

    void install(Tone type, int midi, float gain, float pan, float attack, float decay,
                 float delay = 0) {
        auto slot = std::find_if(cues.begin(), cues.end(),
                                 [](const CueVoice &voice) { return voice.length == 0; });
        if (slot == cues.end())
            slot = std::min_element(
                cues.begin(), cues.end(),
                [](const CueVoice &a, const CueVoice &b) { return a.gain < b.gain; });
        const int frames = int(6.f * decay * kAudioRate);
        *slot = {type,   type == Tone::Kick ? 62.0 : frequency(midi),
                 0,      -int(delay * kAudioRate),
                 frames, attack,
                 gain,   float(std::exp(-kStep / decay)),
                 pan};
    }

    void updatePad() {
        // Sum the weights of shared notes so they sustain through chord changes.
        const auto harmony = harmonyAt(seconds + kStep * kControlFrames);
        std::array<float, kNoteCount> targets{}, support{};
        auto add = [&](int chord, float gain) {
            for (int note : kChords[chord].notes)
                targets[note - kFirstNote] += gain;
            for (int note : kChords[chord].support)
                support[note - kFirstNote] += gain;
        };
        add(harmony.from, 1.f - harmony.blend);
        add(harmony.to, harmony.blend);
        for (size_t i = 0; i < pad.size(); ++i) {
            auto &voice = pad[i];
            voice.gainStep = (targets[i] - voice.gain) / kControlFrames;
            voice.supportStep = (support[i] - voice.support) / kControlFrames;
            voice.drift = wave(sine, seconds * (.043 + .003 * (i % 11)) + voice.phase);
        }
        const float low = 335.f * std::exp2(.32f * wave(sine, .063 * seconds + .16) +
                                            .14f * wave(sine, .110 * seconds + .68));
        const float high = 790.f * std::exp2(.39f * wave(sine, .051 * seconds + .55) +
                                             .12f * wave(sine, .097 * seconds + .16));
        lowBand.set(low, .91f);
        highBand.set(high, 1.12f);
    }

    void renderPad(float &left, float &right) {
        if (control-- == 0) {
            updatePad();
            control = kControlFrames - 1;
        }
        float l = 0, r = 0;
        for (auto &voice : pad) {
            voice.gain += voice.gainStep;
            voice.support += voice.supportStep;
            if (voice.gain < .00001f && voice.support < .00001f)
                continue;
            const double phase = voice.hz * seconds + voice.phase;
            const float weight = voice.gain * (.87f + .13f * voice.drift);
            const float support = voice.support * (.015f * wave(sine, phase * .9991 + .18) +
                                                   .009f * wave(sine, phase * 1.0011 + .62));
            const float body = .013f * weight * wave(mist, phase + .027 * voice.drift);
            const double leftPhase = phase * .99782 + .16;
            const double rightPhase = phase * 1.00216 + .57;
            l +=
                support + body +
                weight * (.011f * wave(mist, leftPhase) + .0044f * wave(shimmer, rightPhase + .12));
            r +=
                support + body +
                weight * (.011f * wave(mist, rightPhase) + .0044f * wave(shimmer, leftPhase + .23));
        }
        const size_t mask = reflections[0].size() - 1;
        const float earlyL = reflections[1][(reflectionHead - 2737) & mask];
        const float earlyR = reflections[0][(reflectionHead - 3571) & mask];
        reflections[0][reflectionHead] = l;
        reflections[1][reflectionHead] = r;
        reflectionHead = (reflectionHead + 1) & mask;
        l += .14f * earlyL;
        r += .14f * earlyR;
        left = .92f * (l + .25f * lowBand.mix(0, l) + .14f * highBand.mix(0, l));
        right = .92f * (r + .25f * lowBand.mix(1, r) + .14f * highBand.mix(1, r));
    }

    void renderCues(float &left, float &right) {
        for (auto &voice : cues) {
            if (!voice.length || voice.age++ < 0)
                continue;
            if (voice.age >= voice.length) {
                voice.length = 0;
                continue;
            }
            const float time = float((voice.age - 1) * kStep);
            float sample = 0;
            switch (voice.type) {
            case Tone::Melodic:
                sample = wave(sine, voice.phase) + .22f * wave(shimmer, voice.phase + .08);
                break;
            case Tone::Chord:
                sample = .78f * wave(mist, voice.phase) + .12f * wave(shimmer, voice.phase + .04);
                break;
            case Tone::Snare: {
                noise ^= noise << 13;
                noise ^= noise >> 17;
                noise ^= noise << 5;
                const float raw = float(noise >> 8) * (1.f / 8388608.f) - 1.f;
                voice.low += .17f * (raw - voice.low);
                voice.high += .52f * (raw - voice.low - voice.high);
                sample = 1.5f * voice.high + .18f * wave(sine, voice.phase);
                break;
            }
            case Tone::Kick:
                sample = wave(sine, voice.phase) + .38f * wave(sine, 2 * voice.phase);
                break;
            }
            const double hz =
                voice.type == Tone::Kick ? voice.hz + 77.0 * std::exp(-time / .022) : voice.hz;
            voice.phase += hz * kStep;
            const float scaled = sample * voice.gain * smooth(time / voice.attack);
            voice.gain *= voice.decayStep;
            left += scaled * (.70f - .30f * voice.pan);
            right += scaled * (.70f + .30f * voice.pan);
        }
    }

    void clearTransient() {
        cues = {};
        reflections = {};
        reflectionHead = 0;
        lowBand = {};
        highBand = {};
        control = 0;
        arcade.reset();
        ambienceGain = cueGain = 0;
        lastTick = -1;
        repeatedTicks = 0;
    }
};

ShellAudio::ShellAudio() : state_(std::make_unique<State>()) {}
ShellAudio::~ShellAudio() = default;

void ShellAudio::preload() {
    state_->arcade.preload();
}

void ShellAudio::play(Cue cue) {
    if (mode_.load(std::memory_order_relaxed) == Mode::Off)
        return;
    const unsigned write = write_.load(std::memory_order_relaxed);
    const unsigned next = (write + 1) % queue_.size();
    if (next == read_.load(std::memory_order_acquire))
        return;
    queue_[write] = cue;
    write_.store(next, std::memory_order_release);
}

void ShellAudio::reset() {
    read_.store(write_.load(std::memory_order_acquire), std::memory_order_release);
    titleVisible_.store(false, std::memory_order_relaxed);
    auto &s = *state_;
    s.clearTransient();
    s.seconds = 0;
    s.padReveal = 0;
    s.moves = 0;
    s.suspended = false;
    for (auto &note : s.pad) {
        note.gain = note.gainStep = 0;
        note.support = note.supportStep = 0;
    }
}

void ShellAudio::suspend() {
    read_.store(write_.load(std::memory_order_acquire), std::memory_order_release);
    auto &s = *state_;
    if (!s.suspended) {
        s.clearTransient();
        s.suspended = true;
    }
}

void ShellAudio::startCue(Cue cue) {
    auto &s = *state_;
    const Harmony harmony = harmonyAt(s.seconds);
    const int root = kChords[harmony.blend < .5f ? harmony.from : harmony.to].root;
    const float cueLevel = cue == Cue::Enter ? 1.f : kMenuCueLevel;
    auto note = [&](int midi, float level, float pan, float attack, float decay, float delay = 0) {
        s.install(Tone::Melodic, midi, level * cueLevel, pan, attack, decay, delay);
    };
    switch (cue) {
    case Cue::Move: {
        constexpr int focus[][3]{
            {1, 2, 4}, {1, 3, 5}, {0, 2, 3}, {1, 3, 4}, {0, 2, 5}, {1, 2, 3},
        };
        const auto &voices = focus[s.moves++ % std::size(focus)];
        auto chord = [&](int index, float gain) {
            if (gain < .02f)
                return;
            for (int n = 0; n < 3; ++n)
                s.install(Tone::Chord, kChords[index].notes[voices[n]] + 12,
                          gain * cueLevel * (.044f - .005f * n), .27f * (n - 1), .027f, .074f);
        };
        chord(harmony.from, 1.f - harmony.blend);
        chord(harmony.to, harmony.blend);
        break;
    }
    case Cue::Confirm:
    case Cue::Launch:
        note(root, .056f, -.10f, .016f, .155f);
        note(root + 7, .060f, .16f, .018f, .151f, .027f);
        note(root + 12, .034f, -.18f, .023f, .168f, .068f);
        break;
    case Cue::Back:
        note(root + 12, .071f, .08f, .017f, .095f);
        note(root + 7, .062f, -.11f, .021f, .114f, .095f);
        break;
    case Cue::Tick: {
        s.repeatedTicks = s.seconds - s.lastTick < .22 ? std::min(s.repeatedTicks + 1, 3) : 0;
        s.lastTick = s.seconds;
        const float gain = .052f * (1.f - .11f * s.repeatedTicks);
        s.install(Tone::Snare, 50, gain * cueLevel, 0, .002f, .024f);
        break;
    }
    case Cue::Kick:
        s.install(Tone::Kick, 0, .131f * cueLevel, 0, .004f, .074f);
        break;
    case Cue::Enter:
        note(root, .060f, -.24f, .072f, .22f);
        note(root + 7, .052f, .19f, .088f, .27f, .08f);
        note(root + 14, .033f, .28f, .078f, .24f, .19f);
        break;
    }
}

void ShellAudio::mix(float *out, int frames) {
    const Scene scene = scene_.load(std::memory_order_relaxed);
    const Mode mode = mode_.load(std::memory_order_relaxed);
    const unsigned write = write_.load(std::memory_order_acquire);
    unsigned read = read_.load(std::memory_order_relaxed);
    while (read != write) {
        if (mode != Mode::Off)
            startCue(queue_[read]);
        read = (read + 1) % queue_.size();
    }
    read_.store(read, std::memory_order_release);

    auto &s = *state_;
    s.suspended = false;
    const bool ambience = mode == Mode::Full && scene != Scene::Leaving;
    // A quick dismissal can reach iOX before the title becomes fully visible.
    const bool revealPad = scene != Scene::Title || titleVisible_.load(std::memory_order_relaxed);
    for (int frame = 0; frame < frames; ++frame) {
        s.ambienceGain = approach(s.ambienceGain, ambience ? 1.f : 0.f, ambience ? 2.2f : .27f);
        if (revealPad)
            s.padReveal = approach(s.padReveal, 1.f, kPadFadeIn);
        s.cueGain = approach(s.cueGain, mode == Mode::Off ? 0.f : 1.f, .018f);
        float l = 0, r = 0;
        if (s.ambienceGain > 0) {
            float arcadeL, arcadeR;
            s.renderPad(l, r);
            s.arcade.mix(arcadeL, arcadeR, ambience);
            const float padLevel = kPadLevel * smooth(s.padReveal);
            l = (l * padLevel + arcadeL) * s.ambienceGain;
            r = (r * padLevel + arcadeR) * s.ambienceGain;
        }
        float cueL = 0, cueR = 0;
        s.renderCues(cueL, cueR);
        *out++ = std::clamp(kShellLevel * (l + cueL * s.cueGain), -1.f, 1.f);
        *out++ = std::clamp(kShellLevel * (r + cueR * s.cueGain), -1.f, 1.f);
        s.seconds += kStep;
    }
}

} // namespace mino
