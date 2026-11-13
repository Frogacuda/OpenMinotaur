// Audio clock and sample playback contract for this game namespace.
#pragma once
#include "common/neon_audio_types.inl"

namespace NEON_GAME {
// Rescue stops the first matching looped voice when a saucer leaves.
void AudioSystemStopSample(int sample);
} // namespace NEON_GAME
