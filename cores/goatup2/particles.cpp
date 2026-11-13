#include "particles.h"

namespace goatup2 {
namespace {
void contactSound(ParticleSoundSettings sound, ContactParticleOutput &output) {
    if (!sound.suppressed)
        output.playSample(43, uint32_t(sound.volume) << 12, 0);
}
} // namespace
void emitTileFragments(ParticleSourceView actor, ParticleSoundSettings sound,
                       const std::array<neon::colour, 32> &palette, int32_t count,
                       uint32_t encodedSprite, neon::DarwinRandom &random,
                       ContactParticleOutput &output) {
    contactSound(sound, output);
    const neon::colour colour = palette[(encodedSprite & 0x1fffffU) >> 16];
    if (count <= 0)
        return;
    const float step = 1.f / float(count);
    float progress = step;
    for (int32_t index = 0; index < count; ++index) {
        TileFragmentBirth birth;
        birth.stagePosition = actor.stagePosition;
        birth.sprite = int32_t(encodedSprite & 0xffffU);
        birth.fadePerUpdate = float(random.range(.01, .04));
        birth.velocity = {0, actor.gravity > 0 ? -.014 : .014};
        const float angle = neon::interpolateFloat(-1.7f, 1.7f, progress);
        neon::rotateClockwise(birth.velocity.x, birth.velocity.y, double(angle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        birth.gravity = actor.gravity * 3.f;
        birth.spin = neon::interpolateFloat(-.3f, .3f, progress);
        birth.phaseRate = float(random.range(.02, .0205));
        birth.savedVelocity = birth.velocity;
        birth.colour = colour;
        output.adoptTileFragment(birth);
        progress = progress + step;
    }
}

void emitImpactParticles(ParticleSourceView actor, ParticleSoundSettings sound, int32_t count,
                         neon::DarwinRandom &random, ContactParticleOutput &output) {
    contactSound(sound, output);
    for (int32_t index = 0; index < count; ++index) {
        StageParticleBirth birth;
        birth.stagePosition = {actor.stagePosition.x - actor.drawScale.x * double(actor.radius),
                               actor.stagePosition.y - actor.drawScale.y * .005};
        birth.radius = float(random.range(.005, .01));
        birth.sprite = 51;
        birth.fadePerUpdate = float(random.range(.01, .04));
        birth.velocity = {random.range(.006, .008), 0};
        // Source literal is close to pi, but is not the host's pi constant.
        constexpr double sourcePi = 0x1.921fb54524550p+1;
        const double angle = random.range(-sourcePi, sourcePi);
        neon::rotateClockwise(birth.velocity.x, birth.velocity.y, angle,
                              neon::ZeroAngleRule::PreserveCoordinates);
        birth.gravity = 0;
        birth.spin = float(random.range(-.1, .1));
        birth.phaseRate = float(random.range(.02, .0205));
        birth.colourRamp = impactColourRamp();
        birth.savedVelocity = birth.velocity;
        birth.solidCollisionEnabled = true; // retained stage constructor setting
        output.adoptImpact(birth);
    }
}

void emitHeadbuttParticles(HeadbuttEmissionView actor, ParticleSoundSettings sound, float emission,
                           float strength, neon::DarwinRandom &random,
                           HeadbuttParticleOutput &output) {
    if (actor.hunger < 0) {
        actor.hazard = true;
        actor.hunger = 0;
        if (!sound.suppressed)
            output.playSample(16, uint32_t(sound.volume) << 12, 0);
    }
    // SIMD lower lane: every operation is binary32. Upper lane is unused.
    const float scaledEmission = emission * 100.f;
    const float requested = scaledEmission * strength;
    const float accumulated = requested + actor.remainder;
    const int32_t count = int32_t(accumulated);
    actor.remainder = accumulated - float(count);
    const float fraction = strength / 20.f;
    float angle = float(double(fraction) * -0x1.921fb54524550p-1);
    if (fraction > 1.f)
        angle = -0x1.921fb6p-1f;
    // Reverse the emission angle under negative gravity (IT MI at 0x5a7ce).
    if (actor.source.gravity < 0)
        angle = -angle;
    if (strength < 1.f || count < 1)
        return;
    const double maximumSpeed = double(strength) * .0001 + .008;
    for (int32_t index = 0; index < count; ++index) {
        StageParticleBirth birth;
        birth.worldPosition = actor.worldPosition;
        birth.stagePosition = {actor.source.stagePosition.x +
                                   actor.source.drawScale.x * double(actor.source.radius),
                               actor.source.stagePosition.y + actor.source.drawScale.y * .005};
        birth.radius = float(random.range(.01, .015));
        birth.sprite = int32_t((random.next() & 3U) + 51U);
        birth.fadePerUpdate = float(random.range(.01, .04));
        birth.velocity = {random.range(.0006, maximumSpeed), random.range(-.0006, .0006)};
        neon::rotateClockwise(birth.velocity.x, birth.velocity.y, double(angle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        if (actor.source.drawScale.x < 0)
            birth.velocity.x = -birth.velocity.x;
        birth.gravity = float(random.range(.000044, .0001));
        if (actor.source.gravity < 0)
            birth.gravity = -birth.gravity;
        birth.spin = float(random.range(-.1, .1));
        birth.phaseRate = float(random.range(.02, .0205));
        birth.colourRamp = headbuttColourRamp();
        birth.savedVelocity = birth.velocity;
        birth.solidCollisionEnabled = true; // no source override of inherited setting
        output.adoptHeadbuttParticle(birth);
    }
}
} // namespace goatup2
