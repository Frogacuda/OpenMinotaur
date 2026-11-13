#include "medium.h"
#include <cmath>
#include <stdexcept>

namespace goatup2 {
namespace {
void applyCurrent(float &ramp, double cellSize, double direction, double &impulse) {
    const double oldRamp = double(ramp);
    const double factor = oldRamp * cellSize;
    const double candidate = factor * direction;
    const double oldImpulse = impulse;
    impulse = candidate;
    if (oldRamp < .2)
        ramp = float(oldRamp + .006); // no clamp, and double .006
    if (std::fabs(candidate) > .001)
        impulse = double(float(oldImpulse));
}
} // namespace

void emitSplash(const MediumActorView &actor, const MediumSharedState &shared, int32_t count,
                uint32_t sprite, neon::DarwinRandom &random, MediumOutput &output) {
    if (!shared.splashSoundSuppressed)
        output.playSample(25, uint32_t(shared.sampleVolume) << 12, 0);
    const neon::PaletteChannels colour{0, 0, random.range(.8, 1), 1};
    if (count <= 0)
        return;
    const float step = 1.f / float(count);
    float progress = step; // source starts at 1/count, not zero
    for (int32_t index = 0; index < count; ++index) {
        SplashBirth birth;
        birth.stagePosition = actor.stagePosition;
        birth.sprite = int32_t(sprite & 0xffffU);
        birth.fadePerUpdate = float(random.range(.01, .04));
        birth.velocity = {0, actor.gravity > 0.f ? -.014 : .014};
        const float angle = neon::interpolateFloat(-1.7f, 1.7f, progress);
        neon::rotateClockwise(birth.velocity.x, birth.velocity.y, double(angle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        birth.velocity.x = random.range(.01, .2) * birth.velocity.x; // X only
        birth.gravity = actor.gravity * 3.f;
        birth.spin = neon::interpolateFloat(-.3f, .3f, progress);
        birth.phaseRate = float(random.range(.02, .0205));
        birth.savedVelocity = birth.velocity;
        birth.colour = colour;
        output.adoptSplash(birth);
        progress = progress + step; // retain per-child float accumulation
    }
}

void updateMedium(MediumActorView actor, MediumSharedState &shared, uint32_t queryWord,
                  neon::DarwinRandom &random, MediumOutput &output) {
    if (!std::isfinite(actor.velocity.x) || !std::isfinite(actor.velocity.y) ||
        !std::isfinite(actor.verticalImpulse) || !std::isfinite(actor.gravity) ||
        !std::isfinite(actor.verticalDrawScale) || !std::isfinite(actor.submergedDamping) ||
        !std::isfinite(shared.downwardRamp) || !std::isfinite(shared.upwardRamp) ||
        !std::isfinite(shared.cachedVerticalCellSize))
        throw std::invalid_argument("unsupported medium state");
    if (actor.entryAge < 400)
        ++actor.entryAge;
    const uint32_t oldFlags = actor.flags;
    const uint8_t medium = uint8_t(queryWord);
    if (medium == uint8_t('+')) {
        actor.flags = oldFlags | 1U;
        actor.velocity = actor.velocity * double(actor.submergedDamping);
    } else {
        actor.flags = oldFlags & ~1U;
        if (medium == uint8_t(',') && actor.player)
            actor.hazard = true;
    }
    if (actor.player) {
        if (medium == uint8_t('/'))
            applyCurrent(shared.upwardRamp, shared.cachedVerticalCellSize, .1,
                         actor.verticalImpulse);
        else if (medium == uint8_t('.'))
            applyCurrent(shared.downwardRamp, shared.cachedVerticalCellSize, -.1,
                         actor.verticalImpulse);
    }
    actor.changedFlags = actor.flags ^ oldFlags;
    if (actor.flags & 1U) {
        const int32_t chance = int32_t(random.next() & 4095U);
        if (chance < actor.bubbleThreshold) {
            BubbleBirth birth;
            birth.worldPosition = actor.worldPosition;
            birth.stagePosition = actor.stagePosition;
            birth.radius = float(random.range(.01, .02));
            birth.mediumFlags = actor.flags;
            birth.changedMediumFlags = actor.changedFlags;
            birth.gravity = actor.gravity + actor.gravity;
            if (birth.gravity == 0.f) {
                birth.gravity =
                    actor.verticalDrawScale >= 0 ? .007f : float(double(birth.gravity) + (-.007));
            }
            output.adoptBubble(birth);
        }
    }
    if ((actor.flags & 1U) && (actor.changedFlags & 1U)) {
        if (actor.entryAge > 80 && actor.player)
            emitSplash(actor, shared, 50, 2, random, output);
        actor.entryAge = 0;
    }
}
} // namespace goatup2
