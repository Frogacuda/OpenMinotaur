#pragma once
#include "palette.h"
#include "neon/math.h"
#include "neon/source_random.h"
#include <array>
#include <optional>

namespace goatup2 {
// Common overrides written by the three factories of the same original class.
// Defaults describe those factories, NOT unreviewed base actor initialization.
struct StageParticleBirth {
    neon::vec2d stagePosition, velocity, savedVelocity;
    std::optional<neon::vec2d> worldPosition; // only factories which copy parent +8c
    std::optional<neon::vec2d> drawScale;     // otherwise retained constructor unit scale
    float fadePerUpdate, gravity, spin, phaseRate;
    int32_t sprite;
    float radius = .01f, opacity = 1, phase = 0;
    std::optional<ColourRampView> colourRamp;
    uint32_t tileInteractionMask = 0x440000U;
    int32_t ledgeResponse = 0;
    float projectileContactGate = 0;
    bool solidCollisionEnabled = false;
    bool restingContact = true;
    neon::vec2d savedDrawScale{1, 1}; // copied from source child constructor scale
};
struct TileFragmentBirth : StageParticleBirth {
    neon::colour colour; // whole source value; no implicit cycle reset
};
struct ParticleSourceView {
    const neon::vec2d &stagePosition;
    const neon::vec2d &drawScale;
    float radius, gravity;
};
struct ParticleSoundSettings {
    int32_t volume;
    bool suppressed;
};

class ContactParticleOutput {
  public:
    virtual ~ContactParticleOutput() = default;
    virtual void adoptTileFragment(const TileFragmentBirth &) = 0;
    // Impact leaves colour at its inherited value; its ramp is sampled on update.
    virtual void adoptImpact(const StageParticleBirth &) = 0;
    virtual void playSample(int32_t sample, uint32_t gain, int32_t sourceOption) = 0;
};

// Post-base creation sequences 0x57f78/0x582f8. No live no-op output adapter.
void emitTileFragments(ParticleSourceView actor, ParticleSoundSettings sound,
                       const std::array<neon::colour, 32> &palette, int32_t count,
                       uint32_t encodedSprite, neon::DarwinRandom &random,
                       ContactParticleOutput &output);
void emitImpactParticles(ParticleSourceView actor, ParticleSoundSettings sound, int32_t count,
                         neon::DarwinRandom &random, ContactParticleOutput &output);

struct HeadbuttEmissionView {
    ParticleSourceView source;
    const neon::vec2d &worldPosition;
    float &hunger;
    bool &hazard;
    float &remainder;
};
class HeadbuttParticleOutput {
  public:
    virtual ~HeadbuttParticleOutput() = default;
    virtual void adoptHeadbuttParticle(const StageParticleBirth &) = 0;
    virtual void playSample(int32_t sample, uint32_t gain, int32_t sourceOption) = 0;
};
// 0x5a6c0. Updates fractional emission even when strength<1 prevents births.
// Finite arguments with a representable signed emission count, as in callers.
void emitHeadbuttParticles(HeadbuttEmissionView actor, ParticleSoundSettings sound, float emission,
                           float strength, neon::DarwinRandom &random,
                           HeadbuttParticleOutput &output);
} // namespace goatup2
