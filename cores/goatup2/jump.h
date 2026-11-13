#pragma once
#include "particles.h"

namespace goatup2 {
// The player fields at entry to its post-movement jump section. Actual actor
// construction and the earlier input-release clear belong to the outer update.
struct JumpActorView {
    const uint32_t &mediumFlags;
    const float &gravity;
    const float &effectiveGravity;
    const double &localY;
    const double &drawScaleX;
    double &verticalVelocity;
    float &heading;
    int32_t &bubbleThreshold;
    bool &restingContact;
    bool &landed;
    bool &oppositeContact259;
    bool &oppositeContact25a;
    int32_t &forcedJumpTicks;
    const float &forcedJumpSpeed;
    int32_t &heldJumpTicks;
    bool &heldJumpFinished;
    bool &jumpConsumed;
    bool &headbuttFlag;
    int32_t &sprite;
    const int32_t &ledgeGrace;
};
class JumpActions {
  public:
    virtual ~JumpActions() = default;
    virtual void updateMovementAnimation() = 0;
    virtual bool attemptHeadbutt() = 0;
    // Original 0x5a6c0, including state changes and its own shared RNG draws.
    virtual void emitHeadbuttParticles(float emission, float strength, neon::DarwinRandom &) = 0;
    virtual void playSample(int32_t sample, uint32_t gain, int32_t sourceOption) = 0;
};

void updateJumpDecisions(JumpActorView actor, int32_t &jumpInput, float halfHeight,
                         bool &exitContact, bool &headbuttEvent, ParticleSoundSettings sound,
                         neon::DarwinRandom &random, JumpActions &actions);
} // namespace goatup2
