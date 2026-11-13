#include "jump.h"
#include "neon/integer.h"

namespace goatup2 {
namespace {
// Original words at 0x5a228..0x5a257; preserve their binary64 precision.
constexpr double normalSpeed = 0x1.61e4f80000000p-8;
constexpr double waterSpeed = 0x1.3e8145999999ap-8;
constexpr double headbuttSpeed = 0x1.cc100f3333333p-8;
} // namespace
void updateJumpDecisions(JumpActorView a, int32_t &jumpInput, float halfHeight, bool &exitContact,
                         bool &headbuttEvent, ParticleSoundSettings sound,
                         neon::DarwinRandom &random, JumpActions &actions) {
    const uint32_t fullGain = uint32_t(sound.volume) << 12;
    if ((a.mediumFlags & 1U) && jumpInput == 1) {
        a.verticalVelocity = a.effectiveGravity < 0 ? waterSpeed : -waterSpeed;
        if (!sound.suppressed)
            actions.playSample(62, uint32_t(sound.volume) << 9, 0);
        a.bubbleThreshold = 256;
    }
    auto restartHold = [&]() {
        a.jumpConsumed = true;
        a.heldJumpTicks = 1;
        a.sprite = 107;
        a.heldJumpFinished = false;
    };
    if (a.forcedJumpTicks >= 1) {
        --a.forcedJumpTicks;
        const double sign = a.forcedJumpSpeed < 0 ? -1. : 1.;
        a.heading = float(double(a.heading) + sign * .6);
        jumpInput = 0;
        a.verticalVelocity = double(a.forcedJumpSpeed);
        a.restingContact = false;
        a.heldJumpTicks = 1;
        a.heldJumpFinished = false;
    } else if (a.restingContact) {
        if (a.landed)
            a.landed = false;
        a.heading = 0;
        actions.updateMovementAnimation();
        if (jumpInput == 1 && !a.oppositeContact25a) {
            a.verticalVelocity = a.gravity > 0 ? -normalSpeed : normalSpeed;
            if (!sound.suppressed)
                actions.playSample(15, fullGain, 0);
            restartHold();
            a.restingContact = false;
        }
        return;
    }
    exitContact = false;
    if (a.forcedJumpTicks < 1)
        a.heading = float((a.verticalVelocity * 29.) * a.drawScaleX);
    if (a.oppositeContact259 || a.oppositeContact25a) {
        a.heldJumpTicks = 30;
        a.oppositeContact259 = false;
        a.oppositeContact25a = false;
    } else if (!a.heldJumpFinished) {
        a.heldJumpTicks = neon::integers::add32(a.heldJumpTicks, 1);
        if (jumpInput == 0)
            a.heldJumpTicks = 30;
        else if (a.heldJumpTicks < 30) {
            a.verticalVelocity = a.effectiveGravity > 0 ? -normalSpeed : normalSpeed;
            return;
        }
        a.heldJumpFinished = true;
    }
    if (jumpInput != 1 || !(a.localY > -(double(halfHeight) + .1)))
        return;
    if (!actions.attemptHeadbutt())
        return;
    a.headbuttFlag = true;
    a.verticalVelocity = a.effectiveGravity > 0 ? -headbuttSpeed : headbuttSpeed;
    if (a.ledgeGrace > 0) {
        if (!sound.suppressed)
            actions.playSample(15, fullGain, 0);
        restartHold();
    } else {
        actions.emitHeadbuttParticles(.04f, 5.f, random);
        headbuttEvent = true;
        const auto choice = random.next();
        if (!sound.suppressed)
            actions.playSample(int32_t((choice & 3U) + 11U), fullGain, 0);
    }
}
} // namespace goatup2
