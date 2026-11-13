#include "neon/particles.h"
#include "stage_runtime.h"
#include "campaign_actors.h"
#include "headbutt.h"
#include "terrain_tables.h"
#include "neon/entity_list.h"
#include "neon/entity_contact.h"
#include "neon/integer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace goatup2 {
namespace {
void assignChannels(neon::colour &colour, neon::PaletteChannels channels) {
    colour.r = channels.r;
    colour.g = channels.g;
    colour.b = channels.b;
    colour.a = channels.a;
}

class OrdinaryParticle final : public StageActor {
  public:
    OrdinaryParticle(StageRuntime &runtime, const StageParticleBirth &b)
        : StageActor(runtime, runtime.allocation, b.gravity, false), savedVelocity(b.savedVelocity),
          fade(b.fadePerUpdate), opacity(b.opacity), spin(b.spin), phase(b.phase),
          rate(b.phaseRate), ramp(b.colourRamp) {
        stagePosition = b.stagePosition;
        velocity = b.velocity;
        radius = b.radius;
        sprite = b.sprite;
        if (b.worldPosition)
            worldPosition = *b.worldPosition;
        if (b.drawScale)
            drawScale = *b.drawScale;
        savedDrawScale = b.savedDrawScale;
        tileInteractionMask = b.tileInteractionMask;
        policy.ledgeResponse = b.ledgeResponse;
        policy.solidCollisions = b.solidCollisionEnabled;
        resting = b.restingContact;
        attackPower = b.projectileContactGate;
    }
    void run() override {
        // Original goatPoop::run 0x5b7f0. Colour samples the OLD phase.
        if (ramp)
            assignChannels(colour, unpackParticleColour(sampleColourRamp(phase, *ramp)));
        const float nextPhase = phase + rate;
        phase = nextPhase < 1.f ? nextPhase : 0.f;
        moveHorizontal();
        fall();
        opacity = opacity - fade;
        heading = heading + spin;
        neon::particles::retireLife(opacity, dead, neon::particles::LifeEnd::AtOrBelow, .2);
        colour.a = double(opacity);
        spin = float(double(spin) * .995);
        if (landed) {
            landed = false;
            savedVelocity.y = savedVelocity.y * .95;
            velocity.y = savedVelocity.y;
        }
        if (stagePosition.x > double(owner().bounds.halfWidth) * 1.5 ||
            stagePosition.x < double(owner().bounds.halfWidth) * -1.5 ||
            stagePosition.y > double(owner().bounds.halfHeight) * 1.5 ||
            stagePosition.y < double(owner().bounds.halfHeight) * -1.5)
            dead = true;
    }
    neon::vec2d savedVelocity;
    float fade, opacity, spin, phase, rate;
    std::optional<ColourRampView> ramp;
};

class GroundParticle final : public StageActor {
  public:
    GroundParticle(StageRuntime &runtime, const GroundParticleBirth &b)
        : StageActor(runtime, runtime.allocation, b.gravity, false), kind(b.kind),
          savedVelocity(b.savedVelocity), fade(b.fadePerUpdate), opacity(b.opacity), spin(b.spin) {
        worldPosition = b.worldPosition;
        stagePosition = b.stagePosition;
        velocity = b.velocity;
        radius = b.radius;
        sprite = b.sprite;
        heading = b.angle;
        drawScale.x = b.horizontalDrawScale;
        savedDrawScale = b.savedDrawScale;
        tileInteractionMask = b.tileInteractionMask;
        policy.ledgeResponse = b.ledgeResponse;
        if (b.colourOverride)
            assignChannels(colour, *b.colourOverride);
    }
    void run() override {
        // grassParticle 0x61978 and nomParticle 0x61a40 are distinct updates.
        if (kind == GroundParticleBirth::Kind::Nom)
            colour = owner().rainbow;
        moveHorizontal();
        fall();
        opacity = opacity - fade;
        heading = heading + spin;
        neon::particles::retireLife(opacity, dead, neon::particles::LifeEnd::AtOrBelow, .2);
        colour.a = double(opacity);
        if (kind == GroundParticleBirth::Kind::Grass) {
            spin = float(double(spin) * .995);
            if (landed) {
                landed = false;
                savedVelocity.y = savedVelocity.y * .7;
                velocity.y = savedVelocity.y;
            }
        }
    }
    GroundParticleBirth::Kind kind;
    neon::vec2d savedVelocity;
    float fade, opacity, spin;
};

class Bubble final : public StageActor {
  public:
    Bubble(StageRuntime &runtime, const BubbleBirth &b)
        : StageActor(runtime, runtime.allocation, b.gravity, false) {
        worldPosition = b.worldPosition;
        stagePosition = b.stagePosition;
        radius = b.radius;
        mediumFlags = b.mediumFlags;
        changedMediumFlags = b.changedMediumFlags;
        sprite = b.sprite;
        resting = b.restingContact;
        bubbleThreshold = b.bubbleThreshold;
        savedDrawScale = b.savedDrawScale;
    }
    void run() override {
        fall(); // 0x623b4: retire on leaving water or opposite contact.
        if (!(mediumFlags & 1U) || oppositeContact25a)
            dead = true;
    }
};

// Original expanding flash 0x80e78/0x61ad0, separate from ordinary particles.
class ExplosionFlash final : public StageActor {
  public:
    explicit ExplosionFlash(StageActor &source)
        : ExplosionFlash(source.owner(), source.stagePosition) {}
    ExplosionFlash(StageRuntime &w, neon::vec2d p) : StageActor(w, w.allocation, 0, false) {
        stagePosition = p;
        sprite = 43;
        radius = float(owner().random.range(.02, .03));
        heading = float(owner().random.range(-0x1.921fb54524550p+1, 0x1.921fb54524550p+1));
        fade = float(owner().random.range(.04, .06));
    }
    void run() override {
        neon::particles::fadeLife(opacity, fade, dead);
        colour.a = double(opacity);
        radius = float(double(radius) * 1.005);
    }
    float opacity = 1, fade;
};

class DeathBanner final : public StageActor {
  public:
    explicit DeathBanner(StageRuntime &runtime)
        : StageActor(runtime, runtime.allocation, 0, false),
          amplitude(float(runtime.random.range(-.39269908175, .39269908175))) {
        category = 0;
        sprite = 110;
        radius = .1f;
        worldPosition = {0, -.2};
        directWorldDraw = true;
        drawScale = savedDrawScale = owner().spriteScale(110);
        heading = amplitude;
    }
    void run() override {
        if (age < 100)
            radius = float(double(radius) * 1.01);
        else
            colour.a -= .01;
        age = neon::integers::add32(age, 1);
        if (colour.a < 0)
            dead = true;
        const float argument = float(double(owner().sourceTime) * .1);
        heading = amplitude * std::sin(argument);
    }
    void preparePresentation() override {} // source base entity draws world coordinates directly
    float amplitude;
};
class DeathSlide final : public StageActor {
  public:
    explicit DeathSlide(StageRuntime &runtime) : StageActor(runtime, runtime.allocation, 0, false) {
        category = 8;
        sprite = 118;
        radius = .2f;
        worldPosition = {0, .68};
        directWorldDraw = true;
        drawScale = savedDrawScale = owner().spriteScale(118);
        drawScale.x *= 1.3;
    }
    void run() override {
        // 0x622d0's ITTT GT guards ALL three clamp instructions. The supplied
        // C incorrectly looks like it stores an actor address into progress.
        if (progress < 1.f)
            progress = std::min(1.f, progress + .08f);
        worldPosition = {double(neon::interpolateFloat(0, 0, progress)),
                         double(neon::interpolateFloat(float(.68), float(.32), progress))};
        if (progress == 1.f) {
            const int32_t remaining = neon::integers::subtract32(255, age);
            age = neon::integers::add32(age, 1);
            if (age > 255)
                dead = true;
            colour.a = double(float(remaining) * .00390625f);
        }
    }
    void preparePresentation() override {} // original base entity, not stage-position routing
    float progress = 0;
};
} // namespace

StageActor::StageActor(StageRuntime &runtime, StageAllocationInputs a, float g, bool isPlayer)
    : world_(&runtime), player(isPlayer), mediumFlags(a.mediumFlags),
      changedMediumFlags(a.changedMediumFlags), mediumEntryAge(a.mediumEntryAge), gravity(g),
      effectiveGravity(a.effectiveGravity), oppositeContact25a(a.oppositeContact25a) {}

MediumActorView StageActor::mediumView() {
    return {player,          mediumFlags,      changedMediumFlags, mediumEntryAge, velocity,
            verticalImpulse, hazard,           worldPosition,      stagePosition,  gravity,
            drawScale.y,     submergedDamping, bubbleThreshold};
}
HorizontalActorView StageActor::horizontalView() {
    return {stagePosition,
            velocity,
            horizontalImpulse,
            travelled.x,
            freezeTicks,
            hopCounter,
            hopPeriod,
            hopSpeedX,
            hopSpeedY,
            mediumFlags,
            gravity,
            effectiveGravity,
            radius,
            drawScale,
            policy,
            player,
            tileInteractionMask,
            resting,
            hazard,
            turnSignal,
            horizontalContact,
            ledgeGrace,
            supportColumn,
            supportRow};
}
VerticalActorView StageActor::verticalView() {
    return {mediumView(),        stagePosition,    drawScale,   radius,          buoyancy,
            elasticity,          effectiveGravity, travelled.y, policy,          motionFlags,
            tileInteractionMask, resting,          landed,      verticalContact, oppositeContact259,
            oppositeContact25a,  supportColumn,    supportRow,  forcedJumpTicks};
}
void StageActor::moveHorizontal() {
    goatup2::moveHorizontal(horizontalView(), owner().horizontalTerrain());
}
void StageActor::moveVertical() {
    goatup2::moveVertical(verticalView(), owner().verticalTerrain(), owner().medium, owner().random,
                          owner(), owner());
}
void StageActor::fall() {
    applyFalling(verticalView(), owner().verticalTerrain(), owner().medium, owner().random, owner(),
                 owner());
}
void StageActor::prepareWorldPosition() {
    worldPosition = stageToWorld(stagePosition, owner().gridOffset, owner().layout, owner().bounds);
}
void StageActor::preparePresentation() {
    // Mutating part of 0x56710, once per logical frame, never per host redraw.
    prepareWorldPosition();
    worldPosition.y += double(float((gravity < 0 ? -1.f : 1.f) * renderOffset));
    if (motionFlags & 2U)
        drawScale.y = gravity < 0 ? -savedDrawScale.y : savedDrawScale.y;
    if (motionFlags & 4U)
        drawScale.y = velocity.y < 0 ? -savedDrawScale.y : savedDrawScale.y;
    if ((motionFlags & 1U) && std::fabs(velocity.x) > .001)
        drawScale.x = velocity.x <= 0 ? savedDrawScale.x : -savedDrawScale.x;
    if (uint32_t(paletteIndex) < 32U)
        colour = owner().terrainPalette[size_t(paletteIndex)];
    readyForContacts = true;
}
void StageActor::onContact(StageActor *) {} // source entity's empty virtual slot 0x308cc
void StageActor::wasShot(StageActor *) {
    throw std::logic_error("base projectile hit-point path is not integrated");
}
void StageActor::hitBy(StageActor *other) {
    if (!other)
        return;
    other->onContact(this);
    if (other->category == 1)
        contactHealth = -1;
    freezeTicks = 30;
    contactHealth = contactHealth - other->attackPower;
    if (contactHealth <= 0) {
        dead = true;
        owner().explode(*this, 10);
    }
}

GoatActor::GoatActor(StageRuntime &runtime, StageAllocationInputs a, neon::vec2d spawn,
                     bool inverted, neon::vec2d spriteScale)
    : StageActor(runtime, a, inverted ? -.0002f : .0002f, true), checkpoints(spawn, gravity),
      attractFrequencyX(runtime.random.range(.1, .3) + .1),
      attractFrequencyY(runtime.random.range(.1, .3) + .1) {
    stagePosition = spawn;
    drawScale = spriteScale;
    savedDrawScale = spriteScale;
    sprite = 107;
    radius = .035f;
    policy.ledgeResponse = 0;
    tileInteractionMask = 0x440000U;
    category = 1;
    attackPower = 100;
    bubbleThreshold = 64;
    prepareWorldPosition();
    renderOffset = .008f;
    readyForContacts = true;
    // Source stage constructor initializes sentinel X and gravity/state arrays;
    // Y/speed are explicit host storage until a record is written.
    for (auto &s : followerHistory)
        s = {{-1, 0}, 0, 0, 0};
}
void GoatActor::preparePresentation() {
    if (state >= 3)
        return;
    // 0x5b428 calls vector subtraction 0x9054; follower 0x58d00 calls addition
    // 0x9c80. Keep these source signs, including the omitted scale product.
    const auto shifted = stagePosition - (owner().gridOffset * owner().layout.displayReciprocal);
    auto &sample = followerHistory[followerCursor & 1023U];
    sample.position = {shifted.x, shifted.y + double(sinkOffset)};
    sample.gravity = gravity;
    sample.state = state;
    sample.speed = std::min(1.f, float(velocity.length() / .02));
    followerCursor = (followerCursor + 1U) & 1023U;
    prepareWorldPosition();
    const float offset = sinkOffset + float((gravity < 0 ? -1.f : 1.f) * renderOffset);
    worldPosition.y += double(offset);
    drawScale.y = gravity < 0 ? -savedDrawScale.y : savedDrawScale.y;
}

KidActor::KidActor(StageRuntime &runtime, StageAllocationInputs a, neon::vec2d spawn, bool inverted,
                   neon::vec2d scale)
    : StageActor(runtime, a, inverted ? -.00035f : .00035f, false) {
    category = 2;
    sprite = 19;
    radius = .02f;
    originalRadius = owner().placementReceiverRadius;
    attackPower = 100;
    stagePosition = spawn;
    drawScale = savedDrawScale = scale;
    damping = {owner().random.range(.9, 1), owner().random.range(.8, 1)};
}
void KidActor::onContact(StageActor *) {
    if (headbuttState == 1 && radius <= originalRadius)
        radius = originalRadius * 10.f;
}
void StageActor::followGoat() {
    if (followDelay < targetDelay) {
        followDelay = neon::integers::add32(followDelay, 2);
        if (followDelay > targetDelay)
            followDelay = targetDelay;
    }
    if (!owner().goat)
        return;
    auto &goat = *owner().goat;
    const auto &sample =
        goat.followerHistory[(goat.followerCursor - uint32_t(followDelay)) & 1023U];
    stagePosition = sample.position + (owner().gridOffset * owner().layout.displayReciprocal);
    stagePosition.y += double((goat.radius - radius) * .5f);
    gravity = sample.gravity;
    followedState = sample.state;
    prepareWorldPosition();
}
void KidActor::run() {
    if (headbuttState == 2) {
        radius = float(double(radius) * 1.01);
        colour.r = owner().rainbow.r;
        colour.g = owner().rainbow.g;
        colour.b = owner().rainbow.b;
        colour.a -= .003;
        if (colour.a <= 0) {
            colour.a = 0;
            dead = true;
        }
        return;
    }
    if (headbuttState == 1) {
        const auto old = stagePosition;
        followGoat();
        owner().activeFollowers = neon::integers::add32(owner().activeFollowers, 1);
        velocity = stagePosition - old;
        drawScale.x = velocity.x < 0 ? 1. : -1.;
        heading = float((velocity.y * 29.) * drawScale.x);
        if (originalRadius < radius) {
            radius = float(double(radius) - .005);
            if (radius < originalRadius)
                radius = originalRadius;
        }
        if (followedState == 1) {
            dead = true;
            owner().explode(*this, 20);
        }
        if (followedState == 2) {
            owner().explode(*this, 20);
            headbuttState = 2;
            if (!owner().medium.splashSoundSuppressed)
                owner().playSample(48, uint32_t(owner().medium.sampleVolume) << 12, 0);
            // Preserve the unused source rotation and its arithmetic; no RNG.
            neon::vec2d unused{0, .005};
            neon::rotateClockwise(unused.x, unused.y, double(owner().sourceTime),
                                  neon::ZeroAngleRule::PreserveCoordinates);
            owner().exitOrdinal = neon::integers::add32(owner().exitOrdinal, 1);
            const int32_t amount = neon::integers::signedBits(uint32_t(owner().exitOrdinal) * 100U);
            owner().addScore(amount);
            owner().exitBonus = neon::integers::add32(owner().exitBonus, amount);
            owner().random.range(.0001, .0002); // source discarded draw still feeds shared stream
        }
        return;
    }
    if (headbuttState != 0)
        return;
    const bool wasResting = resting;
    fall();
    if (!resting) {
        if (wasResting && !resting)
            velocity.y = gravity > 0 ? -.006 : .006;
        sprite = 19;
    } else {
        const neon::vec2d weights{30, 0};
        updateMovementAnimation({freezeTicks, travelled, weights, 1, 19, savedDrawScale.x,
                                 stagePosition.x, owner().animationFrameFlags, drawScale.x, heading,
                                 sprite});
    }
    heading = float((velocity.y * 29.) * drawScale.x);
}
void KidActor::hitBy(StageActor *other) {
    if (headbuttState == 1 || !readyForContacts)
        return;
    other->onContact(this);
    if (other->category != 1 || owner().goat.get() != other || owner().goat->state != 0)
        return;
    auto &goat = *owner().goat;
    if ((goat.gravity < 0) != (gravity < 0) || !goat.readyForContacts)
        return;
    goat.followingCount = neon::integers::add32(goat.followingCount, 1);
    serial = goat.followingCount;
    followDelay = 0;
    targetDelay = neon::integers::signedBits(uint32_t(goat.followingCount) * 6U);
    goat.hunger = std::min(1.f, float(double(goat.hunger) + .05));
    headbuttState = 1;
    owner().multiplier = neon::integers::add32(owner().multiplier, 1);
    rewardValue = owner().multiplier;
    if (!owner().medium.splashSoundSuppressed)
        owner().playSample(24, uint32_t(owner().medium.sampleVolume) << 12, 0);
    auto label = std::make_unique<FloatingScoreActor>(*this, rewardValue);
    label->paletteIndex = 4;
    label->radius = .01f * 6.f;
    neon::entities::prepend<StageActor>(owner().foregroundEffects, *label);
    label.release();
}
void GoatActor::onContact(StageActor *other) {
    // Recovered directly from the small vtable callback at 0x5b3d8.
    if (other->category != 5)
        return;
    deathSoundSuppressed = false;
    if (state != 0 || invulnerability > 0)
        return;
    if (uint32_t(other->sprite) - 141U < 3U || other->sprite == 93 || other->sprite == 130)
        owner().hazardContactCount = neon::integers::add32(owner().hazardContactCount, 1);
    hazard = true;
}
void GoatActor::animate() {
    updateMovementAnimation({freezeTicks, travelled, animationWeights, animationMode, baseSprite,
                             savedDrawScale.x, stagePosition.x, owner().animationFrameFlags,
                             drawScale.x, heading, sprite});
}
void GoatActor::run() {
    // Original 0x59238 player state switch, including the separate camera demo.
    zoom = std::max(0.f, float(double(zoom) - .3));
    owner().playerVisible = true;
    if (bubbleThreshold > 64)
        --bubbleThreshold;
    if (ledgeGrace > 0)
        --ledgeGrace;
    if (owner().sharedGrace > 0) {
        --owner().sharedGrace;
        const double refill = double(hunger) + .01;
        hunger = refill > .98 ? .98f : float(refill);
    }
    if (state != 0) {
        switch (state) {
        case 1:
            if (owner().activeFollowers < 1) {
                const bool freeRestart = owner().mode == 4 && owner().modeDetail == 1;
                if (!freeRestart)
                    owner().lives = neon::integers::add32(owner().lives, -1);
                if (freeRestart || owner().lives >= 0) {
                    invulnerability = 50;
                    followingCount = 0;
                    state = 3;
                    // Pure re-read 0x49f44 followed by 0x46f70 is represented
                    // by the immutable prepared stage snapshot, not gameplay's
                    // mutable adjacent backup storage. No RNG is consumed.
                    owner().working = owner().initialWorking;
                    owner().backup = owner().initialBackup;
                } else {
                    state = 4;
                    owner().terminal = true;
                }
            }
            break;
        case 2:
            radius = float(double(radius) * 1.01);
            colour.r = owner().rainbow.r;
            colour.g = owner().rainbow.g;
            colour.b = owner().rainbow.b;
            colour.a -= .003;
            if (colour.a <= 0) {
                colour.a = 0;
                state = 4;
            }
            owner().displayFade = float(colour.a);
            owner().sceneFade = float(colour.a);
            owner().exitFadeTicks = neon::integers::add32(owner().exitFadeTicks, 1);
            break;
        case 3:
            invulnerability = neon::integers::add32(invulnerability, -1);
            if (invulnerability < 1)
                resetFromCheckpoint();
            break;
        case 4:
            owner().playerVisible = false;
            if (owner().sceneFade > 0) {
                owner().sceneFade = float(double(owner().sceneFade) - .008);
                if (owner().sceneFade < 0) {
                    neon::entities::destroyList(owner().enemies,
                                                [](StageActor *actor) { delete actor; });
                    owner().sceneFade = 0;
                }
                owner().displayFade = owner().sceneFade;
            }
            break;
        default:
            break;
        }
        return;
    }
    if (invulnerability > 0)
        --invulnerability;
    if (owner().stageTicks < 215999)
        ++owner().stageTicks;
    sprite = baseSprite = 107;
    if (owner().attractTicks > 0) {
        // 0x59238 state 0: a camera demonstration, not generated controls.
        if (attractCountdown < 0) {
            fall();
            if (++attractCountdown >= 0)
                attractCountdown = 1;
            attractCameraTarget = stagePosition;
        } else {
            if (--attractCountdown < 1)
                attractCountdown = 100;
            owner().random.next(); // discarded original draw still advances RNG
            attractCameraTarget = {
                double(std::sin(float(double(owner().sourceTime) * attractFrequencyX))),
                double(std::sin(float(double(owner().sourceTime) * attractFrequencyY)))};
        }
        return;
    }
    activeTicks = neon::integers::add32(activeTicks, 1);
    hunger = float(double(hunger) - .0005);
    if (mediumFlags & 1U)
        hunger = float(double(hunger) - .0007);
    if (hunger < 0) {
        hazard = true;
        hunger = 0;
        if (!owner().medium.splashSoundSuppressed)
            owner().playSample(16, uint32_t(owner().medium.sampleVolume) << 12, 0);
    }
    if (owner().jumpInput == 0 && jumpConsumed)
        jumpConsumed = false;
    if (activeTicks > 65535)
        activeTicks = 10;
    oppositeContact25a = false;
    const float requested = float(owner().movementIntent * .12);
    if (double(std::fabs(requested)) < owner().layout.displayReciprocal.x * .8)
        velocity.x = double(requested);
    owner().gravityDirection = gravity < 0 ? -1 : 1;
    if (!riding) {
        fall();
        if (!riding)
            interactWithGround({{worldPosition, stagePosition, drawScale, gravity, hunger},
                                stagePosition,
                                resting,
                                supportColumn,
                                supportRow,
                                zoom,
                                sinkOffset,
                                horizontalImpulse,
                                forcedJumpTicks,
                                forcedJumpSpeed,
                                checkpoints},
                               {owner().working, owner().backup, &owner().glyphLookup},
                               owner().layout, owner().clock, owner().exitContact, owner().medium,
                               owner().scoreView(), owner().random, owner());
    }
    const float savedHorizontal = float(velocity.x);
    moveHorizontal();
    velocity.x = double(savedHorizontal);
    if (std::fabs(savedHorizontal) > .0001)
        drawScale.x = savedHorizontal >= 0 ? -1. : 1.;
    if (riding) {
        auto *platform = riding;
        heading = 0;
        const auto ownVelocity = velocity;
        velocity = platform->velocity;
        moveVertical();
        moveHorizontal();
        velocity = ownVelocity;
        if (owner().jumpInput == 1 || platform->drawScale.x * double(platform->radius) * 1.5 <
                                          std::fabs(platform->stagePosition.x - stagePosition.x)) {
            if (owner().jumpInput == 1)
                resting = true;
            platform->riderAttached = false;
            riding = nullptr;
        }
    }
    updateJumpDecisions({mediumFlags,
                         gravity,
                         effectiveGravity,
                         stagePosition.y,
                         drawScale.x,
                         velocity.y,
                         heading,
                         bubbleThreshold,
                         resting,
                         landed,
                         oppositeContact259,
                         oppositeContact25a,
                         forcedJumpTicks,
                         forcedJumpSpeed,
                         heldJumpTicks,
                         heldJumpFinished,
                         jumpConsumed,
                         headbuttFlag,
                         sprite,
                         ledgeGrace},
                        owner().jumpInput, owner().bounds.halfHeight, owner().exitContact,
                        owner().headbuttEvent,
                        {owner().medium.sampleVolume, owner().medium.splashSoundSuppressed},
                        owner().random, owner());
    prepareWorldPosition();
    if (zoom == 0)
        owner().collide(*this, owner().enemies);
    if (zoom == 0)
        owner().collide(*this, owner().grassBonuses);
    if (activeTicks > 10 && !hazard)
        owner().collide(*this, owner().contactObjects);
    if ((effectiveGravity > 0 && stagePosition.y > double(owner().bounds.halfHeight) * 1.1) ||
        (effectiveGravity < 0 && stagePosition.y < double(owner().bounds.halfHeight) * -1.1))
        hazard = true;
    if (!owner().deathDisabled && hazard) {
        if (activeTicks > 10)
            owner().beginDeath(*this);
        hazard = false;
    }
}
void GoatActor::resetFromCheckpoint() {
    // State 3 reuses this object; it is NOT a constructor or whole-actor clear.
    owner().multiplier = 1;
    owner().exitContact = false;
    owner().inputFriction = .99f;
    owner().savedInputFriction = .02f;
    category = 1;
    deathSoundSuppressed = false;
    headbuttFlag = false;
    lastCheckpointDevice = -1;
    attractCountdown = -10;
    attractFrequencyX = owner().random.range(.1, .3) + .1;
    attractFrequencyY = owner().random.range(.1, .3) + .1;
    state = 0;
    forcedJumpTicks = 0;
    sprite = 107;
    invulnerability = 150;
    riding = nullptr;
    renderOffset = .008f;
    bubbleThreshold = 64;
    drawScale = owner().spriteScale(107);
    colour.r = colour.g = colour.b = colour.a = 1;
    zoom = 4;
    readyForContacts = true;
    heldJumpTicks = 0;
    heldJumpFinished = true;
    jumpConsumed = false;
    policy.airborneResponse = 2;
    followingCount = 0;
    emissionRemainder = 0;
    hunger = .5f;
    velocity = {0, 0};
    horizontalImpulse = verticalImpulse = 0;
    tileInteractionMask = 0x440000U;
    policy.ledgeResponse = 0;
    hazard = false;
    resting = true;
    followerCursor = 0;
    const auto checkpoint = checkpoints.latest();
    stagePosition = checkpoint.position;
    gravity = checkpoint.gravity;
    // Keep medium state, activeTicks, travel, radius, contacts, sink, history
    // entries and other fields which this source reset does not overwrite.
}

StageRuntime::StageRuntime(TerrainLayout l, TerrainBounds b, std::vector<uint32_t> w,
                           std::vector<uint32_t> saved, StageAllocationInputs a,
                           MediumSharedState shared, uint32_t seed)
    : layout(l), bounds(b), working(std::move(w)), backup(std::move(saved)),
      initialWorking(working), initialBackup(backup), medium(shared), allocation(a), random(seed) {
    if (working.size() != GridCells || backup.size() != GridCells)
        throw std::invalid_argument("stage runtime requires both physical grids");
    glyphLookup = terrain_data::GlyphLookup;
}
StageRuntime::~StageRuntime() {
    clearActors();
}
void StageRuntime::clearActors() {
    goat.reset();
    neon::entities::destroyList(foregroundEffects, [](StageActor *a) { delete a; });
    neon::entities::destroyList(ordinaryEffects, [](StageActor *a) { delete a; });
    for (auto **list :
         {&contactObjects, &enemies, &grassBonuses, &overlayEffects, &placementOverlays})
        neon::entities::destroyList(*list, [](StageActor *a) { delete a; });
}
void StageRuntime::installPreparedTerrain(std::vector<uint32_t> cells,
                                          std::vector<uint32_t> saved) {
    if (cells.size() != GridCells || saved.size() != GridCells)
        throw std::invalid_argument("prepared stage needs both physical grids");
    working = std::move(cells);
    backup = std::move(saved);
    initialWorking = working;
    initialBackup = backup;
}
void StageRuntime::spawnGoat(neon::vec2d p, bool inverted, neon::vec2d scale) {
    if (goat)
        throw std::logic_error("goat already constructed; restart must reuse it");
    goat = std::make_unique<GoatActor>(*this, allocation, p, inverted, scale);
}
void StageRuntime::spawnKid(neon::vec2d p, bool inverted) {
    auto child = std::make_unique<KidActor>(*this, allocation, p, inverted, spriteScale(19));
    neon::entities::prepend<StageActor>(contactObjects, *child);
    child.release();
}
void StageRuntime::advanceLocomotion(double intent, int32_t jump, uint32_t sourceClock,
                                     uint8_t frameFlags) {
    beginLogicalFrame();
    runLocomotion(intent, jump, sourceClock, frameFlags);
}
void StageRuntime::beginLogicalFrame() {
    headbuttEvent = false;
    sounds.clear();
    textRowsToClear.clear();
}
void StageRuntime::resetCurrents() {
    downwardCurrentPhase = upwardCurrentPhase = 0;
    medium.downwardRamp = medium.upwardRamp = .0625f;
}
void StageRuntime::advanceCurrents() {
    // 0x4311e/0x43140 compare the SUM, with GE wrapping once. Contact in
    // medium/ground handling accelerates these same ramps later in the tick.
    auto advance = [](float &phase, float &ramp) {
        phase = phase + ramp;
        if (phase >= 1.f)
            phase = phase - 1.f;
        if (ramp > .0625f)
            ramp = float(double(ramp) * .99);
    };
    advance(downwardCurrentPhase, medium.downwardRamp);
    advance(upwardCurrentPhase, medium.upwardRamp);
}
void StageRuntime::runLocomotion(double intent, int32_t jump, uint32_t sourceClock,
                                 uint8_t frameFlags) {
    if (!goat)
        throw std::logic_error("no goat");
    movementIntent = intent;
    jumpInput = jump;
    clock = sourceClock;
    animationFrameFlags = frameFlags;
    goat->run();
    advanceEffects();
    preparePresentation();
}
void StageRuntime::advanceEffects() {
    activeFollowers = 0; // 0x4567c clears +d08 AFTER the goat update
    neon::entities::runAndCull(foregroundEffects, [](StageActor *a) { delete a; });
    neon::entities::runAndCull(ordinaryEffects, [](StageActor *a) { delete a; });
    for (auto **list :
         {&enemies, &contactObjects, &grassBonuses, &overlayEffects, &placementOverlays})
        neon::entities::runAndCull(*list, [](StageActor *a) { delete a; });
}
void StageRuntime::preparePresentation() {
    if (goat)
        goat->preparePresentation();
    for (auto *list : {ordinaryEffects, foregroundEffects, enemies, contactObjects, grassBonuses,
                       overlayEffects, placementOverlays})
        for (auto *actor = list; actor; actor = actor->next)
            actor->preparePresentation();
}
void StageRuntime::retainEffectsFrom(StageRuntime &previous) {
    // 0x4ca24 clears goat/enemy/contact/grass/+3b4 lists, but leaves +3ac
    // and +3bc alive. The port replaces its per-stage container, whereas the
    // original game owner is stable. Rebind surviving effects to the new stage
    // so their terrain, sounds and RNG continue through that same owner.
    for (auto pair : {std::pair{&ordinaryEffects, &previous.ordinaryEffects},
                      std::pair{&foregroundEffects, &previous.foregroundEffects}}) {
        auto *&incoming = *pair.second;
        for (auto *a = incoming; a; a = a->next)
            a->world_ = this;
        auto **tail = pair.first;
        StageActor *last = nullptr;
        while (*tail) {
            last = *tail;
            tail = &last->next;
        }
        *tail = incoming;
        if (incoming)
            incoming->prev = last;
        incoming = nullptr;
    }
}
HorizontalTerrainView StageRuntime::horizontalTerrain() {
    return {{working, backup, &glyphLookup}, layout, bounds, clock, cursor};
}
VerticalTerrainView StageRuntime::verticalTerrain() {
    return {working, backup, layout, bounds, clock, cursor, terrainPalette, &glyphLookup};
}
GroundScoreView StageRuntime::scoreView() {
    return {score, stageTotal, highScore, recordCounter, newRecord, multiplier};
}
void StageRuntime::addScore(int32_t amount) {
    score = neon::integers::add32(score, amount);
    stageTotal = neon::integers::add32(stageTotal, amount);
    if (score > highScore) {
        const int32_t previous = highScore;
        highScore = score;
        if (!newRecord) {
            recordCounter = neon::integers::add32(recordCounter, 1);
            if (previous > 0 && !medium.splashSoundSuppressed)
                playSample(70, uint32_t(medium.sampleVolume) << 12, 0);
        }
        newRecord = true;
    }
}
neon::vec2d StageRuntime::spriteScale(int32_t index) const {
    if (index < 0 || size_t(index) >= spriteScales.size())
        throw std::logic_error("required source sprite metadata is not loaded");
    return spriteScales[size_t(index)];
}
int StageRuntime::collide(StageActor &actor, StageActor *targets) {
    // 0x9538 -> 0x91e0, bounce=false. The shared early traversal retains its
    // sticky same-group suppression and post-callback next reads. These source
    // delay/dead/active gates are supplied at the module boundary.
    if (actor.drawDelay != 0 || actor.dead)
        return 0;
    return neon::entities::collideEarly(
        &actor, targets, false, [](const StageActor &b) { return b.drawDelay != 0 || !b.active; });
}
void StageRuntime::emitRadialBurst(StageActor &source, int32_t count, float speed, float spread,
                                   float strength) {
    if (count <= 0)
        return;
    static constexpr uint32_t stops[] = {0xffffffffU, 0x0000ffffU, 0xff0000ffU};
    const float step = 1.f / float(count);
    const float maximum = (speed * spread) + speed;
    float progress = 0;
    for (int32_t i = 0; i < count; ++i) {
        StageParticleBirth b;
        b.stagePosition = source.stagePosition;
        b.radius = float(random.range(.01, .015));
        b.sprite = 28;
        b.fadePerUpdate = float(random.range(.02, .04));
        b.velocity = {0, random.range(double(speed), double(maximum))};
        b.gravity = float(random.range(double(strength) * .000038, double(strength) * .00004));
        if (gravityDirection <= -1)
            b.gravity = -b.gravity; // ITT LE at 0x56b10
        const double angle = double(progress) * 6.2831853072;
        neon::rotateClockwise(b.velocity.x, b.velocity.y, angle,
                              neon::ZeroAngleRule::PreserveCoordinates);
        b.spin = float(random.range(-.1, .1));
        b.phaseRate = float(random.range(.02, .0205));
        b.savedVelocity = b.velocity;
        b.colourRamp = ColourRampView{stops, 2};
        b.solidCollisionEnabled = true;
        b.projectileContactGate = .3f;
        if (alternateBurstSprite || cameraSettleFrames > 0) {
            b.sprite = 129;
            b.colourRamp.reset();
            b.drawScale = spriteScale(129);
            b.savedDrawScale = *b.drawScale;
        }
        auto child = std::make_unique<OrdinaryParticle>(*this, b);
        neon::entities::prepend<StageActor>(foregroundEffects, *child);
        child.release();
        progress = progress + step;
    }
}
void StageRuntime::emitCollectionBurst(StageActor &source) {
    // 0x5f7f8: one initial angle draw, then four draws for each admitted lobe
    // sample. The degenerate .02..02 phase-rate range STILL consumes a draw.
    static constexpr int32_t counts[] = {30, 40, 50, 60, 70, 80, 90, 100};
    static constexpr float speedFactors[] = {.0006f, .0002f, .0004f, .0003f,
                                             .0002f, .0005f, .0004f, .0002f};
    static constexpr float cutoffs[] = {.1f, .3f, .1f, .2f, .5f, .3f, .3f, .9f};
    constexpr double pi = 0x1.921fb54524550p+1;
    const float initialAngle = float(random.range(-pi, pi));
    const auto index = uint32_t(multiplier) & 7U;
    const int32_t count = counts[index];
    const float step = 1.f / float(count);
    const double lobes = double(float(neon::integers::add32(multiplier, 2)));
    const float cutoff = cutoffs[index], speed = speedFactors[index] * 40.f;
    const double remainder = 1. - double(cutoff);
    float progress = 0;
    for (int32_t i = 0; i < count; ++i) {
        const double angle = double(progress) * 6.2831853072;
        const float sine = std::sin(float(lobes * angle));
        const float height = float(double(sine) * .5 + .5);
        if (height > cutoff) {
            StageParticleBirth b;
            b.stagePosition = source.stagePosition;
            const float factor = float(double(height - cutoff) / remainder);
            b.radius = float(double(factor) * .01 + .005);
            b.sprite = 28;
            b.fadePerUpdate = float(random.range(.01, .04));
            b.velocity = {0, (double(height) - .4) * double(speed)};
            b.gravity = float(random.range(.00038, .0004));
            if (gravityDirection <= -1)
                b.gravity = -b.gravity;
            neon::rotateClockwise(b.velocity.x, b.velocity.y, double(initialAngle) + angle,
                                  neon::ZeroAngleRule::PreserveCoordinates);
            b.spin = float(random.range(-.1, .1));
            b.phaseRate = float(random.range(.02, .02));
            b.colourRamp = headbuttColourRamp();
            b.savedVelocity = b.velocity;
            b.solidCollisionEnabled = true;
            b.projectileContactGate = .3f;
            auto child = std::make_unique<OrdinaryParticle>(*this, b);
            neon::entities::prepend<StageActor>(foregroundEffects, *child);
            child.release();
        }
        progress = progress + step;
    }
}
void StageRuntime::editorRemovalFlash(neon::vec2d p) {
    auto flash = std::make_unique<ExplosionFlash>(*this, p);
    neon::entities::prepend<StageActor>(foregroundEffects, *flash);
    flash.release();
}
void StageRuntime::explode(StageActor &source, int32_t fragments) {
    // 0x4a718: flash, radial burst, then a separate source-sprite fragment.
    auto flash = std::make_unique<ExplosionFlash>(source);
    neon::entities::prepend<StageActor>(foregroundEffects, *flash);
    flash.release();
    emitRadialBurst(source, fragments, .01f, .1f, 5.f);
    StageParticleBirth b;
    b.stagePosition = source.stagePosition;
    b.radius = source.radius;
    b.sprite = source.sprite;
    b.drawScale = source.drawScale;
    b.savedDrawScale = source.drawScale;
    b.fadePerUpdate = float(random.range(.02, .04));
    b.velocity.y = random.range(-.015, -.009);
    b.velocity.x = random.range(-.01, .01);
    b.gravity = source.gravity;
    if (b.gravity < 0)
        b.velocity.y = -b.velocity.y;
    b.spin = float(random.range(-.1, .1));
    b.phaseRate = float(random.range(.02, .0205));
    b.savedVelocity = b.velocity;
    b.restingContact = false;
    b.projectileContactGate = .3f;
    auto fragment = std::make_unique<OrdinaryParticle>(*this, b);
    neon::entities::prepend<StageActor>(foregroundEffects, *fragment);
    fragment.release();
}
void StageRuntime::beginDeath(GoatActor &source) {
    explode(source, 200);
    source.hazard = false;
    if (!source.deathSoundSuppressed && !medium.splashSoundSuppressed) {
        playSample(42, uint32_t(medium.sampleVolume) << 12, 0);
        playSample(58, uint32_t(medium.sampleVolume) << 12, 0);
    }
    auto banner = std::make_unique<DeathBanner>(*this);
    neon::entities::prepend<StageActor>(overlayEffects, *banner);
    banner.release();
    auto slide = std::make_unique<DeathSlide>(*this);
    neon::entities::prepend<StageActor>(overlayEffects, *slide);
    slide.release();
    source.state = 1;
    sharedGrace = 0;
    sounds.push_back({63, 0, 0, RuntimeSound::Kind::Stop});
    textRowsToClear.push_back(6);
    textRowsToClear.push_back(8);
}
FloatingScoreActor::FloatingScoreActor(StageActor &target, int32_t number)
    : StageActor(target.owner(), target.owner().allocation, 0, false), value(number),
      glyphScale(target.owner().spriteScale(140)) {
    paletteIndex = 3;
    stagePosition = target.stagePosition;
    const double offset = (double(target.radius) * target.drawScale.y) * .5;
    if (target.gravity >= 0) {
        stagePosition.y -= offset;
        velocity.y = -.002;
    } else {
        stagePosition.y += offset;
        velocity.y = .005;
    }
    radius = .01f;
    if (number == -1)
        layoutScale = .2f;
    setNumber(number);
    radius = radius * 3.f;
}
void FloatingScoreActor::setNumber(int32_t number) {
    value = number;
    firstDigit = 14;
    digits[15] = 0; // source explicitly clears +7334
    if (number > 0) {
        int32_t index = 15;
        do {
            digits[size_t(index)] = number % 10;
            --index;
            const bool more = number > 9;
            number /= 10;
            if (!more)
                break;
        } while (index >= 0);
        firstDigit = index;
    }
}
FloatingScoreActor &StageRuntime::showScore(StageActor &target, int32_t number, float scale,
                                            int32_t palette, int32_t icon) {
    auto child = std::make_unique<FloatingScoreActor>(target, number);
    child->radius = .01f * scale;
    child->paletteIndex = palette;
    child->iconSprite = icon;
    auto *result = child.get();
    neon::entities::prepend<StageActor>(foregroundEffects, *child);
    child.release();
    return *result;
}
void FloatingScoreActor::run() {
    // Source floatingScore::run 0x4be48; no falling/medium calls or RNG.
    if (owner().originalPauseFlag)
        return;
    life = life - fade;
    if (screenSpace)
        worldPosition = worldPosition + velocity;
    else
        stagePosition = stagePosition + velocity;
    if (life < 0) {
        life = 0;
        dead = true;
    }
    radius = radius * growth;
    colour = owner().terrainPalette.at(size_t(paletteIndex));
    colour.a = double(life);
}
void FloatingScoreActor::preparePresentation() {
    if (!screenSpace)
        prepareWorldPosition(); // 0x4bf48 does not use generic stage drawing
}
void StageRuntime::explodeHeadbuttTarget(StageActor &actor, int32_t count) {
    explode(actor, count);
}
void StageRuntime::createHeadbuttMultiplierLabel(StageActor &target, int32_t number) {
    auto child = std::make_unique<FloatingScoreActor>(target, number);
    neon::entities::prepend<StageActor>(foregroundEffects, *child);
    child.release();
}
void StageRuntime::adoptBubble(const BubbleBirth &b) {
    auto child = std::make_unique<Bubble>(*this, b);
    neon::entities::prepend<StageActor>(foregroundEffects, *child);
    child.release();
}
void StageRuntime::adoptSplash(const SplashBirth &b) {
    auto child = std::make_unique<OrdinaryParticle>(*this, b);
    assignChannels(child->colour, b.colour);
    neon::entities::prepend<StageActor>(ordinaryEffects, *child);
    child.release();
}
void StageRuntime::adoptTileFragment(const TileFragmentBirth &b) {
    auto child = std::make_unique<OrdinaryParticle>(*this, b);
    child->colour = b.colour;
    neon::entities::prepend<StageActor>(ordinaryEffects, *child);
    child.release();
}
void StageRuntime::adoptImpact(const StageParticleBirth &b) {
    auto child = std::make_unique<OrdinaryParticle>(*this, b);
    neon::entities::prepend<StageActor>(ordinaryEffects, *child);
    child.release();
}
void StageRuntime::adoptGroundParticle(const GroundParticleBirth &b) {
    auto child = std::make_unique<GroundParticle>(*this, b);
    neon::entities::prepend<StageActor>(foregroundEffects, *child);
    child.release();
}
void StageRuntime::adoptHeadbuttParticle(const StageParticleBirth &b) {
    adoptImpact(b);
}
void StageRuntime::playSample(int32_t sample, uint32_t gain, int32_t option) {
    sounds.push_back({sample, gain, option});
}
void StageRuntime::updateMovementAnimation() {
    goat->animate();
}
bool StageRuntime::attemptHeadbutt() {
    return dispatchHeadbutt(contactObjects, headbuttDisabled, goat->ledgeGrace, sharedGrace,
                            multiplier, goat->followingCount, *this);
}
void StageRuntime::emitHeadbuttParticles(float emission, float strength, neon::DarwinRandom &rng) {
    goatup2::emitHeadbuttParticles(
        {{goat->stagePosition, goat->drawScale, goat->radius, goat->gravity},
         goat->worldPosition,
         goat->hunger,
         goat->hazard,
         goat->emissionRemainder},
        {medium.sampleVolume, medium.splashSoundSuppressed}, emission, strength, rng, *this);
}
} // namespace goatup2
