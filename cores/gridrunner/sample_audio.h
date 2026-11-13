// Later-engine voice rules with float accumulation and output headroom.
#pragma once
#include "neon/sample_audio.h"
namespace gridrunner {
using Sample = neon::audio::Sample;
using Voice = neon::audio::Voice;
using neon::audio::parseWav;
class SampleMixer : public neon::audio::SampleMixer {
  public:
    SampleMixer()
        : neon::audio::SampleMixer({32, neon::audio::MixArithmetic::ForwardFloatToInteger,
                                    neon::audio::RetriggerRule::RewindFirst},
                                   44100, .5f, neon::audio::PlaybackMix::CleanFloat) {}
};
} // namespace gridrunner
