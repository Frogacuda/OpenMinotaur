#include "shell_arcade.h"

#include "bundled_assets.h"
#include "core_api.h"
#include "neon/sample_audio.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

namespace mino {
namespace {
constexpr float kStep = 1.f / kAudioRate;
constexpr float kSaucerSeconds = 3.25f;
enum Clip {
    Ting, Jelly, Jump, Pop, Saucer, SaucerShot, MinotronShot, Warp,
    Zap, Spinner, Powerup, EnemyFire, Bonus, DeflexCSharp, DeflexG, DeflexBleep,
    Thrust, Landing, ClipCount
};
struct Source {
    const char *game, *file;
};
constexpr std::array<Source, ClipCount> kSources{{
    {"fiveaday", "ting2.wav"}, {"fiveaday", "jellyping.wav"},
    {"goatup", "jump.wav"}, {"goatup", "pop.wav"},
    {"minorescue", "bigsaucerloop.wav"}, {"minorescue", "saucershot.wav"},
    {"minotron", "ltshot.wav"}, {"minotron", "warpoutstart.wav"},
    {"gridrunner", "zap2.wav"}, {"gridrunner", "spinner_hit.wav"},
    {"gridrunner", "powerupget.wav"}, {"superoxwars", "eshot.wav"},
    {"superoxwars", "bonuspip.wav"}, {"deflex", "tone13.wav"},
    {"deflex", "tone19.wav"}, {"deflex", "bleep.wav"},
    {"minos", "thrust.wav"}, {"minos", "landingping.wav"},
}};
struct Event {
    Clip clip = ClipCount;
    float offsetSeconds = 0, gain = 0, speed = 1;
};
struct Phrase {
    std::array<Event, 4> events;
    float level;
    bool alwaysMoves = false;
};
// Each row combines actions from one game. The shell schedules the phrases
// independently, so their dry sounds and reverb tails can overlap.
constexpr Phrase kPhrases[]{
    {{{{Jelly, 0, .56f, .94f}, {Ting, 1.30f, .81f, .88f},
       {Jelly, 2.49f, .60f, .94f}}}, .91f},
    {{{{Jump, 0, .55f, .91f}, {Pop, .41f, .63f, .96f},
       {Jump, 1.04f, .77f, .90f}, {Pop, 1.83f, .50f, .96f}}}, .89f},
    {{{{Saucer, 0, .82f}, {SaucerShot, 1.31f, .24f, .93f},
       {SaucerShot, 1.72f, .31f, .93f}}}, .91f, true},
    {{{{MinotronShot, 0, .51f, .96f}, {MinotronShot, .24f, .57f, .96f},
       {MinotronShot, .88f, .46f, .95f}, {Warp, 2.10f, 1.f}}}, 1.04f},
    {{{{Zap, 0, .62f, .91f}, {Spinner, .37f, .65f, .93f},
       {Powerup, 1.54f, .60f, .91f}}}, .77f},
    {{{{EnemyFire, 0, .52f, .94f}, {EnemyFire, .28f, .65f, .94f},
       {Bonus, 1.11f, .75f, .91f}, {Bonus, 1.34f, .48f, .91f}}}, .68f},
    {{{{DeflexCSharp, 0, .65f, .997f}, {DeflexG, .68f, .63f, .999f},
       {DeflexBleep, 1.56f, .46f, .93f}, {DeflexCSharp, 2.81f, .46f, .997f}}}, .78f},
    {{{{Thrust, 0, .50f, .95f}, {Thrust, .52f, .33f, .95f},
       {Landing, 1.72f, .62f}}}, .65f},
};
float smooth(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

// The four delays share feedback through an orthogonal mixing matrix.
// Unequal lengths and damped returns give a diffuse stereo tail.
class Room {
  public:
    Room() : lines_{std::vector<float>(2351), std::vector<float>(2969),
                    std::vector<float>(3529), std::vector<float>(4093)} {}

    void mix(float inputL, float inputR, float &left, float &right) {
        std::array<float, 4> taps{};
        for (size_t i = 0; i < lines_.size(); ++i) {
            const float delayed = lines_[i][heads_[i]];
            damped_[i] += .31f * (delayed - damped_[i]);
            taps[i] = damped_[i];
        }
        const auto [a, b, c, d] = taps;
        const std::array<float, 4> feedback{a + b + c + d, a - b + c - d,
                                           a + b - c - d, a - b - c + d};
        const std::array<float, 4> input{inputL, inputR, -.72f * inputL, -.72f * inputR};
        for (size_t i = 0; i < lines_.size(); ++i) {
            lines_[i][heads_[i]] = .34f * input[i] + .418f * feedback[i];
            if (++heads_[i] == lines_[i].size())
                heads_[i] = 0;
        }
        left = .63f * (a + .73f * c - .38f * b);
        right = .63f * (b + .73f * d - .38f * c);
    }

    void reset() {
        for (auto &line : lines_)
            std::fill(line.begin(), line.end(), 0.f);
        heads_ = {};
        damped_ = {};
    }

  private:
    std::array<std::vector<float>, 4> lines_;
    std::array<size_t, 4> heads_{};
    std::array<float, 4> damped_{};
};
} // namespace

struct ShellArcade::State {
    struct Sample {
        neon::audio::Sample audio;
        float scale = 0;
    };
    struct Passing {
        int phrase = -1, age = 0;
        float end = 0, pan = 0, drift = 0, gain = 0;
        float low = 0, dark = 0, dc = 0;
    };
    std::array<Sample, ClipCount> samples;
    std::array<bool, std::size(kPhrases)> available{};
    std::array<Passing, 3> passing{};
    Room room;
    uint32_t random = 0x6146a3b9u;
    int untilNext = 0, last = -1;
    bool loaded = false;

    float unit() {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return float(random >> 8) * (1.f / 16777216.f);
    }

    void start() {
        std::array<int, std::size(kPhrases)> choices{};
        size_t count = 0;
        for (size_t i = 0; i < available.size(); ++i)
            if (available[i] && int(i) != last)
                choices[count++] = int(i);
        if (count == 0 && last >= 0)
            choices[count++] = last;
        auto slot = std::find_if(passing.begin(), passing.end(), [](const Passing &voice) {
            return voice.phrase < 0;
        });
        if (count == 0 || slot == passing.end()) {
            untilNext = kAudioRate;
            return;
        }
        last = choices[size_t(unit() * count)];
        const float side = unit() < .5f ? -1.f : 1.f;
        const auto &phrase = kPhrases[last];
        const bool moves = phrase.alwaysMoves || unit() < .47f;
        const float drift = moves ? -side * (.43f + .53f * unit()) : 0.f;
        *slot = {last, 0, duration(last), side * (.31f + .28f * unit()), drift,
                 phrase.level * (.79f + .33f * unit())};
        // Short gaps let phrases overlap. Longer gaps give the pad room.
        const float chance = unit();
        const float seconds = chance < .29f ? 1.3f + 1.1f * unit()
                            : chance < .89f ? 2.5f + 2.f * unit()
                                            : 4.6f + 1.4f * unit();
        untilNext = int(seconds * kAudioRate);
    }

    float duration(int index) const {
        float end = 0;
        for (const Event &event : kPhrases[index].events) {
            if (event.clip == ClipCount || samples[event.clip].audio.pcm.empty())
                continue;
            const auto &audio = samples[event.clip].audio;
            const float length = event.clip == Saucer
                                     ? kSaucerSeconds
                                     : audio.pcm.size() / (audio.rate * event.speed);
            end = std::max(end, event.offsetSeconds + length);
        }
        return end;
    }

    float sample(const Event &event, float time) const {
        if (event.clip == ClipCount || time < 0)
            return 0;
        const auto &source = samples[event.clip];
        const auto &pcm = source.audio.pcm;
        if (pcm.size() < 2)
            return 0;
        float position = time * (source.audio.rate * event.speed);
        const bool loop = event.clip == Saucer;
        if (loop) {
            if (time >= kSaucerSeconds)
                return 0;
            // Slow slightly during the pass, keeping the sample phase continuous.
            position *= 1.025f - .009f * time;
            position = std::fmod(position, float(pcm.size()));
        } else if (position >= pcm.size() - 1) {
            return 0;
        }
        const size_t index = size_t(position);
        const size_t next = index + 1 == pcm.size() ? 0 : index + 1;
        const float fade = loop ? smooth(time / 1.25f) * smooth((kSaucerSeconds - time) / 1.35f)
                                : smooth(time / .008f) *
                                      smooth((pcm.size() - 1 - position) / (.066f * source.audio.rate));
        const float interpolated = pcm[index] + (pcm[next] - pcm[index]) * (position - index);
        return interpolated * (source.scale * fade * event.gain);
    }
};

ShellArcade::ShellArcade() : state_(std::make_unique<State>()) {}
ShellArcade::~ShellArcade() = default;

void ShellArcade::preload() {
    auto &s = *state_;
    if (s.loaded)
        return;
    s.loaded = true;
    // Vary the opening activity across cold starts as well as within a session.
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    s.random = uint32_t(now) | 1u;
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < kSources.size(); ++i) {
        const auto &source = kSources[i];
        auto &target = s.samples[i];
        if (!readBundledAsset(source.game, source.file, bytes) ||
            !neon::audio::parseWav(bytes, target.audio))
            continue;
        int peak = 1;
        for (int16_t value : target.audio.pcm)
            peak = std::max(peak, std::abs(int(value)));
        target.scale = 1.f / peak;
    }
    for (size_t i = 0; i < s.available.size(); ++i)
        s.available[i] = s.duration(int(i)) > 0;
}

void ShellArcade::reset() {
    auto &s = *state_;
    s.passing = {};
    s.room.reset();
    s.untilNext = int((1.5f + 1.2f * s.unit()) * kAudioRate);
}

void ShellArcade::mix(float &left, float &right, bool active) {
    auto &s = *state_;
    if (active && --s.untilNext <= 0)
        s.start();
    float directL = 0, directR = 0;
    for (auto &voice : s.passing) {
        if (voice.phrase < 0)
            continue;
        const float time = voice.age++ * kStep;
        if (time >= voice.end + .045f) {
            voice.phrase = -1;
            continue;
        }
        float mono = 0;
        for (const auto &event : kPhrases[voice.phrase].events)
            mono += s.sample(event, time - event.offsetSeconds);
        voice.dc += .019f * (mono - voice.dc);
        voice.low += .21f * (mono - voice.dc - voice.low);
        voice.dark += .24f * (voice.low - voice.dark);
        const float position = voice.pan + voice.drift * smooth(time / voice.end);
        const float level = voice.dark * voice.gain * .085f;
        directL += level * (.53f - .38f * position);
        directR += level * (.53f + .38f * position);
    }
    float wetL, wetR;
    s.room.mix(directL, directR, wetL, wetR);
    left = .80f * directL + wetL;
    right = .80f * directR + wetR;
}

} // namespace mino
