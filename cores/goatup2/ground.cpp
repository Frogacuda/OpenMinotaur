#include "ground.h"
#include "neon/integer.h"
#include <algorithm>
#include <stdexcept>

namespace goatup2 {
void collectGroundReward(GroundRewardActorView a, GroundScoreView score,
                         ParticleSoundSettings sound, neon::DarwinRandom &random,
                         GroundOutput &output) {
    a.hunger = std::min(1.f, float(double(a.hunger) + .04));
    score.score = neon::integers::add32(score.score, score.multiplier);
    score.stageTotal = neon::integers::add32(score.stageTotal, score.multiplier);
    const uint32_t gain = uint32_t(sound.volume) << 12;
    if (score.score > score.highScore) {
        const int32_t previousHigh = score.highScore;
        score.highScore = score.score;
        if (!score.newRecordLatch) {
            score.recordCounter = neon::integers::add32(score.recordCounter, 1);
            if (previousHigh > 0 && !sound.suppressed)
                output.playSample(70, gain, 0);
        }
        score.newRecordLatch = true;
    }
    const auto soundChoice = random.next(); // retained even when sound is suppressed
    if (!sound.suppressed)
        output.playSample(int32_t((soundChoice & 1U) | 22U), gain, 0);
    neon::vec2d fourthVelocity;
    for (int index = 0; index < 4; ++index) {
        GroundParticleBirth b;
        b.kind = GroundParticleBirth::Kind::Grass;
        b.velocity = {random.range(-.004, .004), random.range(-.009, -.004)};
        if (a.gravity < 0)
            b.velocity.y = -b.velocity.y;
        b.savedVelocity = b.velocity;
        fourthVelocity = b.velocity;
        b.gravity = a.gravity;
        b.sprite = 2;
        b.radius = float(random.range(.01, .02));
        const uint32_t green = 63U + (random.next() & 127U);
        b.colourOverride = unpackParticleColour((green << 16) | 255U);
        b.fadePerUpdate = float(random.range(.03, .05));
        constexpr double pi = 0x1.921fb54524550p+1;
        b.angle = float(random.range(-pi, pi));
        b.spin = float(random.range(-.1, .1));
        b.horizontalDrawScale = .3;
        b.worldPosition = a.worldPosition;
        b.stagePosition = a.stagePosition;
        b.savedDrawScale = a.drawScale;
        output.adoptGroundParticle(b);
    }
    GroundParticleBirth b;
    b.kind = GroundParticleBirth::Kind::Nom;
    b.velocity = {random.range(-.002, .002), random.range(-.003, -.001)};
    if (a.gravity < 0)
        b.velocity.y = -b.velocity.y;
    b.savedVelocity = fourthVelocity; // 0x5b22c loads the last grass actor's velocity
    b.gravity = 0;
    b.sprite = 26;
    b.radius = float(random.range(.01, .014));
    b.horizontalDrawScale = 3;
    b.fadePerUpdate = float(random.range(.01, .03));
    b.angle = float(random.range(-.1, .1));
    b.spin = 0;
    b.worldPosition = a.worldPosition;
    b.stagePosition = a.stagePosition;
    b.savedDrawScale = a.drawScale;
    output.adoptGroundParticle(b);
}

void interactWithGround(GroundActorView a, MutableTerrainGridView grid, const TerrainLayout &layout,
                        uint32_t clock, bool &exitContact, MediumSharedState &shared,
                        GroundScoreView score, neon::DarwinRandom &random, GroundOutput &output) {
    if (&a.stagePosition != &a.reward.stagePosition)
        throw std::invalid_argument("ground views must bind the same position");
    if (!a.restingContact) {
        a.sinkOffset = float(double(a.sinkOffset) * .8);
        return;
    }
    auto surface = resolveTerrainWord(physicalTerrainWord(grid, a.supportColumn, a.supportRow),
                                      clock, grid.glyphLookup);
    const int32_t neighbourRow = neon::integers::add32(a.supportRow, a.reward.gravity < 0 ? 1 : -1);
    auto &neighbour = physicalTerrainWord(grid, a.supportColumn, neighbourRow);
    const auto adjacent = resolveTerrainWord(neighbour, clock, grid.glyphLookup);
    bool checkpoint = false, collect = false;
    uint32_t changed = adjacent;
    switch (adjacent & 255U) {
    case 4:
        changed = (adjacent & 0xff7fff00U) | 0x800006U;
        collect = true;
        break;
    case 5:
        changed = (adjacent & 0xfe7fff00U) | 0x1800007U;
        collect = true;
        break;
    case 92:
        changed = (adjacent & 0xff7fff00U) | 0x80005eU;
        collect = true;
        checkpoint = true;
        break;
    case 93:
        changed = (adjacent & 0xff7fff00U) | 0x80005fU;
        collect = true;
        checkpoint = true;
        break;
    }
    const ParticleSoundSettings sound{shared.sampleVolume, shared.splashSoundSuppressed};
    if (collect) {
        neighbour = changed;
        collectGroundReward(a.reward, score, sound, random, output);
    }
    exitContact = false;
    const float sign = a.reward.gravity < 0 ? -1.f : 1.f;
    if ((surface & 255U) == 89U) {
        if (a.zoom == 0) {
            a.forcedJumpTicks = 10;
            a.forcedJumpSpeed = float(double(sign) * (-.0108));
            a.restingContact = false;
            if (!sound.suppressed)
                output.playSample(68, uint32_t(sound.volume) << 12, 0);
        }
    } else if ((surface & 255U) == 25U)
        exitContact = true;
    if ((surface & 0x40000U) && a.zoom == 0) {
        uint32_t counter = 0;
        if ((!(surface & 0x200000U) || a.reward.gravity >= 0) &&
            (!(surface & 0x400000U) || a.reward.gravity <= 0)) {
            const uint32_t limit = (surface & 0x600000U) ? 63U : 32U;
            counter = ((surface & 0x3fffffffU) >> 24) + 1U;
            if (counter > limit) {
                surface = 0;
                a.stagePosition.y = a.stagePosition.y + double(a.sinkOffset);
                a.sinkOffset = 0;
            } else
                surface = (surface & 0xffffffU) | (counter << 24);
        }
        physicalTerrainWord(grid, a.supportColumn, a.supportRow) = surface;
        // 0x5a572 forms reciprocal*scale before the counter product. The C
        // export reassociates this expression; retain the instruction grouping.
        const double cell = layout.displayReciprocal.y * layout.scale.y;
        const double scaled = (double(float(counter)) * .015625) * cell;
        a.sinkOffset = float(scaled * double(sign)); // also written after a collapse
    }
    if ((surface & 0x240000U) == 0x200000U && a.zoom == 0) {
        const auto id = surface & 255U;
        if (id == 13U) {
            const double old = double(shared.upwardRamp);
            a.horizontalImpulse = old * layout.cellSize.x;
            if (old < .9)
                shared.upwardRamp = float(old + .006);
        } else if (id == 12U) {
            const double old = double(shared.downwardRamp);
            a.horizontalImpulse = layout.cellSize.x * double(-shared.downwardRamp);
            if (old < .9)
                shared.downwardRamp = float(old + .006);
        }
    }
    if (checkpoint)
        a.checkpoints.record(a.stagePosition, a.reward.gravity);
}
} // namespace goatup2
