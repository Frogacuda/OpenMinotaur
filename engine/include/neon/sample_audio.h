#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace neon::audio {
constexpr int NativeRate = 11025;
struct Sample {
    std::vector<int16_t> pcm;
    int rate = NativeRate, maxInstances = 1, priority = 1, group = 0, active = 0;
    bool restart = false;
};
struct Voice {
    int sample = -1, volume = 0, loop = 0;
    std::size_t position = 0;
};
enum class MixArithmetic { ReverseQ16Saturating, ForwardFloatToInteger };
// CleanFloat preserves the voice machine but defers clipping until host gain.
enum class PlaybackMix { OriginalInteger, CleanFloat };

enum class RetriggerRule { RewindFirst, RequireSampleRestart };
struct MixerRules {
    std::size_t voiceLimit = 32;
    MixArithmetic arithmetic = MixArithmetic::ReverseQ16Saturating;
    RetriggerRule retrigger = RetriggerRule::RewindFirst;
};

// Original sample/voice machine. Integer mono frames at the original 11025 Hz;
// no SDL, host output format, per-voice resampler or game identity lives here.
class NativeSampleMixer {
  public:
    explicit NativeSampleMixer(MixerRules rules = {});
    void setSamples(std::vector<Sample> samples);
    void play(int sample, int volume, int loop);
    void stop(int sample); // original: first matching slot and sample counter reset
    void stopAll();
    void setGroup(int group);
    void mixMono(int16_t *output, int frames);
    void mixFloat(float *output, int frames); // unclipped, normalized native stream
    const std::vector<Voice> &voices() const {
        return voices_;
    }
    const std::vector<Sample> &samples() const {
        return samples_;
    }

  private:
    void mixFrames(int16_t *original, float *clean, int frames);
    void start(std::size_t slot, int sample, int volume, int loop);
    MixerRules rules_;
    std::vector<Voice> voices_;
    std::vector<Sample> samples_;
    int group_ = 0;
};

// Host adapter: serialize commands with the callback, convert the already-mixed
// native stream to stereo float. CleanFloat applies gain before final clipping. Causal linear
// reconstruction adds one native frame of delay and never interpolates/clips individual voices in
// host space.
class SampleMixer {
  public:
    explicit SampleMixer(MixerRules rules = {}, int outputRate = 44100, float outputGain = 1.f,
                         PlaybackMix playback = PlaybackMix::OriginalInteger);
    void setSamples(std::vector<Sample> samples);
    void play(int sample, int volume, int loop);
    void stop(int sample);
    void stopAll();
    void setGroup(int group);
    void mix(float *output, int frames);

  private:
    std::mutex mutex_;
    NativeSampleMixer native_;
    int outputRate_;
    float outputGain_;
    PlaybackMix playback_;
    float previousFloat_ = 0, currentFloat_ = 0;
    int64_t phase_ = 0;
    int16_t previous_ = 0, current_ = 0;
    bool started_ = false;
};

// Host PCM decoder. Native assets are mono signed 16-bit/11025 Hz. Other PCM
// inputs are converted on load, outside the callback; lengths round down to
// eight native frames as in each original getFileInfo. Failed loads leave out
// unchanged, including its module-supplied sample metadata.
bool parseWav(const std::vector<uint8_t> &bytes, Sample &out);
} // namespace neon::audio
