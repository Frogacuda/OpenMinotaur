#pragma once
#include "particles.h"
#include <cstdint>

namespace goatup2 {
// A borrowed field projection, not an alternative actor or constructor. Every
// reference must bind to the actual actor's state when live integration exists.
struct MediumActorView {
    bool player;
    uint32_t &flags;
    uint32_t &changedFlags;
    int32_t &entryAge;
    neon::vec2d &velocity;
    double &verticalImpulse;
    bool &hazard;
    const neon::vec2d &worldPosition;
    const neon::vec2d &stagePosition;
    float gravity;
    double verticalDrawScale;
    float submergedDamping;
    int32_t bubbleThreshold;
};

struct MediumSharedState {
    // Source +0x3cf60/+0x3cf68: also the negative/positive horizontal conveyor
    // ramps. Keep the actual shared pair across the two kinds of current.
    float downwardRamp, upwardRamp;
    double cachedVerticalCellSize; // source template +0xc34; NOT a recomputed product
    int32_t sampleVolume;
    bool splashSoundSuppressed; // source byte +0x389; broader meaning still under review
};

// Post-base-constructor overrides only. These descriptions do not imply that
// inherited initialization, drawing or complete actor lifetime is implemented.
struct BubbleBirth {
    neon::vec2d worldPosition, stagePosition;
    float radius, gravity;
    uint32_t mediumFlags, changedMediumFlags;
    int32_t sprite = 151;
    bool restingContact = false; // +0x256, inferred from vertical contact paths
    int32_t bubbleThreshold = -1;
    neon::vec2d savedDrawScale{1, 1}; // source copies child's constructor scale
};

struct SplashBirth : StageParticleBirth {
    neon::PaletteChannels colour;
};

// Required synchronous output boundary, with no no-op/default gameplay path.
// A live adapter must perform source base construction and prepend bubbles to
// +0x3bc / splash particles to +0x3ac, without extra RNG draws or parent mutation.
class MediumOutput {
  public:
    virtual ~MediumOutput() = default;
    virtual void adoptBubble(const BubbleBirth &) = 0;
    virtual void adoptSplash(const SplashBirth &) = 0;
    virtual void playSample(int32_t sample, uint32_t gain, int32_t sourceOption) = 0;
};

// 0x57058: consumes shared colour randomness even for nonpositive counts.
void emitSplash(const MediumActorView &actor, const MediumSharedState &shared, int32_t count,
                uint32_t sprite, neon::DarwinRandom &random, MediumOutput &output);

// 0x56ce8: query word is narrowed to its low byte, as in the original callee.
// Finite numeric state required. No constructor default/seed is chosen here.
void updateMedium(MediumActorView actor, MediumSharedState &shared, uint32_t queryWord,
                  neon::DarwinRandom &random, MediumOutput &output);
} // namespace goatup2
