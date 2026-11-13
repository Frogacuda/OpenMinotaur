#pragma once
#include "neon/foundation_rules.h"
#include <cstdint>

namespace goatup2 {
// Borrowed packed RGBA stops. Storage has segments+1 entries and must outlive
// every particle retaining the view. This is not a guest pointer or address.
struct ColourRampView {
    const uint32_t *stops;
    int32_t segments;
};

// 0xa7a8, finite phase 0..1 and positive segment count.
uint32_t sampleColourRamp(float phase, ColourRampView ramp);
// 0x9920, RGBA prefix only; preserves a caller's separate cycle state.
neon::PaletteChannels unpackParticleColour(uint32_t packed);
ColourRampView impactColourRamp();
ColourRampView headbuttColourRamp();
} // namespace goatup2
