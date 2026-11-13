// Concrete bindings to the maintained engine, compiled in GoatUp 2's namespace.
// Only the original inert Mixer dispatch differs from the shared binding.
#include "neon_modules.h"
#include "neon_audio.h"
#include "neon/source_random.h"
#include "neon/sample_audio.h"
#include <stdexcept>

namespace goatup2 {
namespace Neon_Internals {
thread_local neon::DarwinRandom *boundRandom = nullptr;
thread_local neon::audio::SampleMixer *boundMixer = nullptr;
WaveState *Waves = nullptr;
void neonSrand(uint32_t seed) {
    if (!boundRandom)
        throw std::logic_error("unbound GoatUp RNG");
    boundRandom->seed(seed);
}
int neonRand() {
    if (!boundRandom)
        throw std::logic_error("unbound GoatUp RNG");
    return int(boundRandom->next());
}
double oxRNG::rand(double a, double b) {
    if (!boundRandom)
        throw std::logic_error("unbound GoatUp RNG");
    return boundRandom->range(a, b);
}
double oxRNG::rand(double b) {
    return rand(0, b);
}
int oxRNG::irand(double b) {
    return int(rand(0, b));
}
colour getDynamicColour(int type, float alpha, int frame) {
    const auto c = neon::paletteColour(type, alpha, uint32_t(frame),
                                       {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
class InertMixer final : public Mixer {
  public:
    int Run_Module() override {
        return -1;
    } // iOS slot 0x255a0: effect-stack render binding
};
NeonModule_Base *createModule(int type) {
    if (type == 200 || type == 300 || (type >= 400 && type <= 406) || type == 499)
        return nullptr;
    NeonModule_Base *m = type == 500   ? static_cast<NeonModule_Base *>(new showJoy)
                         : type == 600 ? static_cast<NeonModule_Base *>(new cellGrid)
                                       : new InertMixer;
    m->typeId = type;
    return m;
}
} // namespace Neon_Internals
void AudioSystemPlaySample(int sample, int volume, int flags) {
    if (!Neon_Internals::boundMixer)
        throw std::logic_error("unbound GoatUp audio");
    Neon_Internals::boundMixer->play(sample, volume, flags);
}
} // namespace goatup2
