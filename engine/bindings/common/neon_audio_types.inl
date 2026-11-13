// neon_audio.h - the game's view of the audio system.
//
// The original drives an AudioUnit mixer (AudioSystem.c) and a procedural
// "wave" clock (WaveGen) whose beat counter some shaders read. The platform
// layer supplies the implementation; the engine only calls these.
// Included once per game namespace by its guarded binding header.

namespace NEON_GAME {
namespace Neon_Internals {

// Procedural audio clock state read by the renderer (Waves->beat).
struct WaveState {
    int beat = 0;
};
extern WaveState *Waves;

} // namespace Neon_Internals

// AudioSystemPlaySample(sample, volume, flags): plays sample number `sample`
// (index into the <Sample> list of NeonResources.xml) at fixed-point volume
// (the game passes level << 12, level 0..7). Nonzero loop arguments
// remain enabled until stopped; the shared native mixer owns their semantics.
void AudioSystemPlaySample(int sample, int volume, int flags);

} // namespace NEON_GAME
