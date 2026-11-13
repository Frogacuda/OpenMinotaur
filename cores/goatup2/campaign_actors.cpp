#include "campaign_actors.h"
#include "actor_tables.h"
#include "neon/entity_list.h"
#include "neon/integer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace goatup2 {
namespace {
float sign(float value) {
    return value < 0 ? -1.f : 1.f;
}
void sound(StageRuntime &w, int sample) {
    if (!w.medium.splashSoundSuppressed)
        w.playSample(sample, uint32_t(w.medium.sampleVolume) << 12, 0);
}
neon::colour packed(uint32_t value) {
    auto c = unpackParticleColour(value);
    return {c.r, c.g, c.b, c.a};
}
const std::array<int32_t, 16> &deviceRecord(int variant) {
    using namespace actor_data;
    const std::array<int32_t, 16> *records[] = {&Device0, &Device1, &Device2,
                                                &Device3, &Device4, &Device5};
    if (unsigned(variant) >= 6)
        throw std::invalid_argument("untranslated device variant");
    return *records[variant];
}
const std::array<int32_t, 16> &enemyRecord(int kind, int variant) {
    if (auto *record = actor_data::find(kind, variant))
        return *record;
    throw std::invalid_argument("untranslated enemy record");
}
// Base entity with its own update (0x623e0), not a falling stage particle.
class LaunchFlash final : public StageActor {
  public:
    explicit LaunchFlash(StageActor &target)
        : StageActor(target.owner(), target.owner().allocation, 0, false) {
        directWorldDraw = true;
        worldPosition = target.worldPosition;
        sprite = 162;
        radius = float(owner().random.range(.02, .04));
        heading = float(owner().random.range(-0x1.921fb54524550p+1, 0x1.921fb54524550p+1));
        colour = {1, 1, 1, 1};
    }
    void run() override {
        radius = float(double(radius) * 1.04);
        opacity = float(double(opacity) - .08);
        colour.a = double(opacity);
        if (opacity < 0)
            dead = true;
    }
    void preparePresentation() override {}
    float opacity = 1;
};
void launchFlash(StageActor &a) {
    auto flash = std::make_unique<LaunchFlash>(a);
    neon::entities::prepend<StageActor>(a.owner().foregroundEffects, *flash);
    flash.release();
}
void enemyReward(StageActor &a) {
    auto &w = a.owner();
    sound(w, 25);
    auto fruit = std::make_unique<DroppedFruitActor>(a);
    neon::entities::prepend<StageActor>(w.contactObjects, *fruit);
    fruit.release();
    a.rewardValue = neon::integers::signedBits(uint32_t(w.multiplier) * 1000U);
    w.addScore(a.rewardValue);
    w.showScore(a, a.rewardValue, 3.f, 3, 0);
    a.dead = true;
}
} // namespace
DeviceActor::DeviceActor(StageRuntime &w, neon::vec2d p, int v, int slot)
    : StageActor(w, w.allocation, 0, false), variant(v), placementSlot(slot) {
    const auto &record = deviceRecord(v);
    sprite = std::abs(record[0]);
    drawScale = w.spriteScale(sprite);
    if (v >= 4)
        drawScale = drawScale * .7;
    savedDrawScale = drawScale;
    if (v == 5)
        drawScale.y = -drawScale.y;
    gravity = float(double(sign(float(drawScale.y))) * .00035);
    category = 4;
    stagePosition = p;
    radius = .055f;
    colour = packed(uint32_t(record[2]));
}
void DeviceActor::run() {
    // 0x61b30: f32 storage, but .98 multiplication is binary64.
    if (stageDrawScale > 1)
        stageDrawScale = float(double(stageDrawScale) * .98);
    if (stageDrawScale < 1)
        stageDrawScale = 1;
    if (headbuttState == 0 && variant < 2) {
        colour = owner().terrainPalette[0];
        gravity = variant == 0 ? 1.f : -1.f;
    } else if (headbuttState == -1) {
        if (!contactLatch)
            headbuttState = 0;
        contactLatch = false;
    }
}
void DeviceActor::hitBy(StageActor *other) {
    if (!other)
        return;
    contactLatch = true;
    if (headbuttState != 0)
        return;
    if (stageDrawScale <= 1) {
        if (variant == 2 || variant == 3) {
            stageDrawScale = 2.5f;
            headbuttState = -1;
            if (other->category != 1)
                return;
            DeviceActor *nearest = nullptr;
            float distance = 999.f;
            for (auto *a = owner().contactObjects; a; a = a->next) {
                auto *candidate = dynamic_cast<DeviceActor *>(a);
                if (!candidate || candidate->variant != (variant == 2 ? 3 : 2))
                    continue;
                const float d = float((candidate->worldPosition - worldPosition).length());
                if (d < distance) {
                    distance = d;
                    nearest = candidate;
                }
            }
            if (!nearest)
                return;
            nearest->headbuttState = -1;
            nearest->contactLatch = true;
            nearest->stageDrawScale = 2.5f;
            owner().cameraSettleFrames = 10;
            owner().goat->invulnerability = 30;
            owner().emitRadialBurst(*nearest, 100, .004f, .2f, 1.f);
            sound(owner(), 59);
            other->stagePosition = nearest->stagePosition;
            other->worldPosition = nearest->worldPosition;
            return;
        }
        if (variant >= 4) {
            if (other->category != 1 || sign(other->gravity) != sign(gravity))
                return;
            auto &goat = *owner().goat;
            if (goat.lastCheckpointDevice == placementSlot)
                return;
            goat.lastCheckpointDevice = placementSlot;
            sound(owner(), 78);
            goat.checkpoints.record(stagePosition, goat.gravity);
            return;
        }
        if (other->category != 1)
            return;
        if (variant == 0) {
            if (other->gravity != 0 && other->gravity >= 0)
                return;
        } else if (other->gravity < 0)
            return;
        other->gravity = -other->gravity;
        owner().gravityDirection = -owner().gravityDirection;
        sound(owner(), 41);
        owner().emitRadialBurst(*this, 100, .004f, .2f, 1.f);
        stageDrawScale = 2.5f;
    }
    headbuttState = -1;
}
EnemyActor::EnemyActor(StageRuntime &w, neon::vec2d p, int kind, uint32_t flags)
    : StageActor(w, w.allocation, .00035f, false), patroller(kind == 4), variant(int(flags & 255U)),
      kind(kind) {
    const auto &r = enemyRecord(kind, variant);
    sprite = baseSprite = r[0];
    behavior = patroller ? variant : r[7];
    animationMode = r[1];
    animationWeights = {80, patroller ? w.allocation.enemyAnimationVertical : 120.};
    stagePosition = p;
    drawScale = w.spriteScale(sprite);
    if (kind == 5 || kind == 6)
        resting = false;
    velocity = {patroller ? (r[3] ? -1. / double(float(r[3])) : 0.)
                : r[3]    ? double(float(r[3])) / 256. * -.005
                          : 0.,
                r[4] ? double(float(r[4])) / 256. * .005 : 0.};
    maximumSpeed = {std::abs(velocity.x), std::abs(velocity.y)};
    category = 5;
    policy.ledgeResponse = policy.groundedWallResponse = policy.airborneResponse = 1;
    if (patroller) {
        velocity.x *= w.random.range(.9, 1.1);
        velocity.y *= w.random.range(.9, 1.1);
        maximumSpeed.x *= w.random.range(.9, 1.1);
        maximumSpeed.y *= w.random.range(.9, 1.1);
        gravity = float(w.random.range(.8, 1.) * double(gravity));
        if (velocity.y != 0) {
            verticalRebound = float(velocity.y);
            velocity.y = 0;
            policy.ledgeResponse = policy.groundedWallResponse = 3;
            gravity = gravity * .4f;
        }
        if (uint32_t(r[2]) > 31U)
            colour = packed(uint32_t(r[2]));
        else
            paletteIndex = r[2];
        if (variant < 14 && (0x2310U & (1U << variant))) {
            policy.ledgeResponse = policy.groundedWallResponse = 2;
            contactHealth = 10;
        } else if (variant == 7)
            contactHealth = 2;
    } else {
        // 0x55aa8: these modes consume no constructor random values.
        if (uint32_t(r[2]) < 32U)
            paletteIndex = r[2];
        else
            colour = packed(uint32_t(r[2]));
        initializeMachine();
    }
    originalPosition = stagePosition;
    originalVelocity = velocity;
    savedDrawScale = drawScale;
    const unsigned transforms = flags >> 8;
    placementTransform = transforms & 255U;
    if (!patroller && (uint32_t(r[6]) & 64U))
        steeringAngle = float(float(transforms & 3U) * 1.5707964f + steeringAngle);
    else {
        if (transforms & 1U) {
            velocity.x = -velocity.x;
            drawScale.x = -drawScale.x;
        }
        if (transforms & 2U) {
            gravity = -gravity;
            drawScale.y = -drawScale.y;
            if (!patroller)
                velocity.y = -velocity.y;
        }
    }
    hopPeriod = r[9] - 1;
    if (r[9] > 0)
        hopCounter = 0;
    buoyancy = float(r[8]) / 1000.f;
    submergedDamping = float(r[10]) / 1000.f;
    const float scale = float(r[5]) * (1.f / 256.f);
    stretch = (float(r[11]) * (1.f / 256.f)) / scale;
    radius = .03f * scale;
    animationMode = r[1];
    motionFlags = uint32_t(r[6]);
    steeringRange = float(r[13]) / 1000.f;
    if (r[12] > 0)
        contactHealth = float(r[12]); // original ITTTT GE guards a positive-health store
}
EnemyActor::EnemyActor(EnemyActor &parent, SplitChild)
    : StageActor(parent.owner(), parent.owner().allocation,
                 parent.owner().allocation.splitChildGravity, false),
      patroller(false), variant(parent.variant), behavior(7), animationMode(0),
      baseSprite(parent.sprite), kind(parent.kind), animationWeights{0, 0} {
    // 0x5d090: scale velocity BEFORE the initializer rotates it independently.
    // Do not copy the parent's medium, gravity, movement flags or table policies.
    stagePosition = parent.stagePosition;
    sprite = parent.sprite;
    velocity = parent.velocity * 1.2;
    drawScale = owner().spriteScale(sprite);
    initializeMachine();
    originalPosition = stagePosition;
    originalVelocity = velocity;
    radius = float(double(parent.radius) * .6);
    savedDrawScale = parent.savedDrawScale;
    splitDepth = parent.splitDepth - 1;
    contactHealth = 100;
    policy.ledgeResponse = policy.groundedWallResponse = policy.airborneResponse = 1;
}
void EnemyActor::run() {
    updateMovementAnimation({freezeTicks, travelled, animationWeights, animationMode, baseSprite,
                             savedDrawScale.x, stagePosition.x, owner().animationFrameFlags,
                             drawScale.x, heading, sprite});
    if (!patroller) {
        runMachine();
        return;
    }
    const float dx = owner().goat ? float(owner().goat->stagePosition.x - stagePosition.x) : 1.f;
    const float dy = owner().goat ? float(owner().goat->stagePosition.y - stagePosition.y) : 1.f;
    const bool sameDirection = sign(dy) == sign(gravity);
    if (variant < 14 && (0x2310U & (1U << variant))) {
        const bool previous = charging;
        contactHealth = 10;
        if (double(std::abs(dy)) >= .04) {
            charging = false;
            velocity.x *= .95;
        } else {
            charging = true;
            if (dx > 0 && velocity.x < maximumSpeed.x)
                velocity.x += .001;
            if (dx < 0 && velocity.x > -maximumSpeed.x)
                velocity.x -= .001;
            if (!previous)
                sound(owner(), 60);
        }
    } else if (variant == 5) {
        if (dx > 0 && velocity.x < maximumSpeed.x)
            velocity.x += .0005;
        if (dx < 0 && velocity.x > -maximumSpeed.x)
            velocity.x -= .0005;
    } else if (variant != 12)
        heading = float((velocity.y * 29.) * drawScale.x);
    if (variant == 6 && (owner().animationFrameFlags & 7U) == 7 && owner().goat &&
        std::abs(owner().goat->stagePosition.x - stagePosition.x) < .1 && !sameDirection) {
        auto child = std::make_unique<EnemyShotActor>(*this);
        neon::entities::prepend<StageActor>(owner().enemies, *child);
        child.release();
    }
    fall();
    if (variant != 12 || (mediumFlags & 1U))
        moveHorizontal();
    if (hazard) {
        const double y = stagePosition.y;
        if (y > -double(owner().bounds.halfHeight) && y < double(owner().bounds.halfHeight) &&
            !(motionFlags & 32U)) {
            contactHealth = 0;
            hitBy(nullptr);
        }
        hazard = false;
    }
    if (turnSignal) {
        turnSignal = false;
        velocity.y = double((sameDirection ? 0.f : 1.f) * (-verticalRebound));
        resting = false;
    }
    if ((stagePosition.x > double(owner().bounds.halfWidth) * 1.1 && velocity.x > 0) ||
        (stagePosition.x < double(owner().bounds.halfWidth) * -1.1 && velocity.x < 0))
        velocity.x = -velocity.x;
    if (stagePosition.y > double(owner().bounds.halfHeight) * 1.1 && velocity.y >= 0) {
        if (!(motionFlags & 16U))
            dead = true;
        else {
            stagePosition.y = double(owner().bounds.halfHeight) * -1.01;
            velocity.y = 0;
        }
    }
    if (stagePosition.y < double(owner().bounds.halfHeight) * -1.1 && velocity.y <= 0) {
        if (!(motionFlags & 16U))
            dead = true;
        else {
            stagePosition.y = double(owner().bounds.halfHeight) * 1.1;
            velocity.y = 0;
        }
    }
}
void EnemyActor::hitBy(StageActor *other) {
    if (!patroller) {
        if (!other || (other->stagePosition - stagePosition).length() > double(radius * .8f))
            return;
        if (behavior == 13) {
            if (other->category != 1)
                return;
            auto &goat = *owner().goat;
            const double distance = std::abs(goat.worldPosition.y - worldPosition.y);
            if (riderAttached) {
                if (distance < .01)
                    goat.stagePosition.y = stagePosition.y + double(sign(goat.gravity)) * (-.04);
            } else if (distance < .08 && !goat.riding && goat.resting) {
                goat.riding = this;
                goat.velocity.y = 0;
                goat.stagePosition.y = stagePosition.y + double(sign(goat.gravity)) * (-.04);
                riderAttached = true;
            }
            return;
        }
        if (behavior == 9 || behavior == 11) {
            if (other->category != 1)
                return;
            auto &goat = *owner().goat;
            goat.forcedJumpTicks = 30;
            goat.forcedJumpSpeed = float(double(sign(gravity)) * (-.0108));
            if (behavior == 11) {
                goat.forcedJumpTicks = 10;
                goat.forcedJumpSpeed = float(double(sign(gravity)) * (-.0108) * (-1.2));
            }
            if (launchCooldown > 0)
                return;
            launchCooldown = 40;
            sound(owner(), 64);
            launchFlash(*this);
            launchFlash(*other);
            return;
        }
    } else if (headbuttState > 0)
        return;
    if (other) {
        other->onContact(this);
        if (other->category == 1)
            contactHealth = -1;
        freezeTicks = 30;
        contactHealth = contactHealth - other->attackPower;
        if (contactHealth <= 0) {
            dead = true;
            owner().explode(*this, 10);
        }
        if (other->category == 1)
            contactHealth = -1;
    }
    if (contactHealth <= 0) {
        enemyReward(*this);
        if (!patroller && behavior == 7 && splitDepth > 0) {
            // Both constructors run before either child gets an update. Prepend
            // each in source order: the second child becomes the new list head.
            for (int i = 0; i < 2; ++i) {
                auto child = std::make_unique<EnemyActor>(*this, SplitChild{});
                neon::entities::prepend<StageActor>(owner().enemies, *child);
                child.release();
            }
        }
    }
}
void EnemyActor::preparePresentation() {
    // 0x5dbd8 mode 9 changes its drawn position only; live local position is
    // restored afterwards. Contacts retain the resulting prepared world position.
    const auto saved = stagePosition;
    if (!patroller && behavior == 9) {
        const int tick = launchCooldown > 20 ? 40 - launchCooldown : launchCooldown;
        stagePosition.y += double(float(tick) / -20.f) * .1 * double(sign(gravity));
    }
    StageActor::preparePresentation();
    stagePosition = saved;
}
DroppedFruitActor::DroppedFruitActor(StageActor &target)
    : StageActor(target.owner(), target.owner().allocation, target.gravity, false) {
    sprite = owner().dropFruitSequence % 8 + 63;
    owner().dropFruitSequence = neon::integers::add32(owner().dropFruitSequence, 1);
    stagePosition = target.stagePosition;
    radius = .04f;
    category = 6;
    resting = false;
    velocity.y = gravity < 0 ? .01 : -.01;
    // Source asks for sprite metadata into a temporary, but does not copy it
    // to this object's unit draw scale. Preserve that separate original path.
}
void DroppedFruitActor::run() {
    if (headbuttState == 2) {
        radius = float(double(radius) * 1.02);
        colour.a -= .01;
        fall();
        if (colour.a < .01)
            dead = true;
    } else if (headbuttState == 0) {
        fall();
        if (heading < .7853982f)
            heading = heading + .02f;
        if (resting)
            headbuttState = 1;
    }
}
void DroppedFruitActor::hitBy(StageActor *other) {
    if (!other || headbuttState > 1)
        return;
    other->category = 1;
    headbuttState = 2;
    velocity.y = double(gravity * (-30.f));
    gravity = gravity * 1.5f;
    sound(owner(), 27);
    resting = false;
    policy.solidCollisions = false;
    auto &goat = *owner().goat;
    goat.hunger = std::min(.98f, goat.hunger + .1f);
    rewardValue = neon::integers::signedBits(uint32_t(owner().multiplier) * 500U);
    owner().addScore(rewardValue);
    owner().showScore(*this, rewardValue, 5.f, 3, 0);
}
EnemyShotActor::EnemyShotActor(StageActor &source)
    : StageActor(source.owner(), source.owner().allocation, 0, false) {
    // 0x5dfe0 case 6: the sound selector consumes a draw even when muted.
    const int sample = int(owner().random.next() & 3U) + 11;
    sound(owner(), sample);
    stagePosition = source.stagePosition;
    mediumFlags = source.mediumFlags;
    changedMediumFlags = source.changedMediumFlags;
    sprite = 0;
    category = 5;
    drawScale = savedDrawScale = owner().spriteScale(0);
    radius = .01f;
    motionFlags &= ~1U;
    velocity = source.velocity * 2.;
    colour = source.colour;
    paletteIndex = source.paletteIndex; // overwritten below, as in the source
    velocity.x = owner().random.range(-.001, .001);
    spin = float(owner().random.range(-.08, .08));
    velocity.y = double(source.gravity * (-20.f));
    paletteIndex = -1;
    colour = packed(0xffa030ffU);
    gravity = float(double(source.gravity) * .2); // 0x5e2aa multiplies in binary64
}
void EnemyShotActor::run() {
    // 0x61f58: no base integration, damping or extra enemy-contact sweep.
    heading = heading + spin;
    fall();
    moveHorizontal();
    lifetime = neon::integers::add32(lifetime, -1);
    if (lifetime < 0)
        dead = true;
    if (landed || verticalContact || horizontalContact ||
        stagePosition.x > double(owner().bounds.halfWidth) ||
        stagePosition.x < -double(owner().bounds.halfWidth) ||
        stagePosition.y > double(owner().bounds.halfHeight) ||
        stagePosition.y < -double(owner().bounds.halfHeight))
        dead = true;
}
EnemyShotActor::EnemyShotActor(EnemyActor &source, bool /*aimed*/)
    : StageActor(source.owner(), source.owner().allocation, 0, false), spin(0) {
    stagePosition = source.stagePosition;
    mediumFlags = source.mediumFlags;
    changedMediumFlags = source.changedMediumFlags;
    sprite = 0;
    category = 5;
    drawScale = owner().spriteScale(0);
    savedDrawScale = drawScale * .5;
    radius = .01f;
    motionFlags &= ~1U;
    velocity = source.velocity * 2.;
    colour = source.colour;
    paletteIndex = source.paletteIndex;
    policy.automaticVerticalHandling = false;
}
EnemyShotActor::EnemyShotActor(EnemyActor &source, Sideways)
    : StageActor(source.owner(), source.owner().allocation, 0, false), spin(0) {
    // 0x5bb70 mode 16: separate horizontal shot, not the aimed half-scale
    // variant. Direction is chosen from the narrowed local X difference.
    stagePosition = source.stagePosition;
    mediumFlags = source.mediumFlags;
    changedMediumFlags = source.changedMediumFlags;
    sprite = 129;
    category = 5;
    drawScale = savedDrawScale = owner().spriteScale(129);
    radius = .04f;
    motionFlags &= ~1U;
    colour = source.colour;
    paletteIndex = source.paletteIndex;
    policy.automaticVerticalHandling = false;
    velocity.x = owner().goat
                     ? double(sign(float(owner().goat->stagePosition.x - stagePosition.x))) * .01
                     : double(.01f);
    heading = float(owner().random.range(-0x1.921fb54524550p+1, 0x1.921fb54524550p+1));
    sound(owner(), 19);
}
EnemyBombActor::EnemyBombActor(StageActor &source)
    : StageActor(source.owner(), source.owner().allocation, float(double(source.gravity) * .4),
                 false) {
    stagePosition = source.stagePosition;
    stagePosition.y += double(sign(gravity)) * .02;
    mediumFlags = source.mediumFlags;
    changedMediumFlags = source.changedMediumFlags;
    sprite = 97;
    category = 5;
    drawScale = savedDrawScale = owner().spriteScale(97);
    radius = .015f;
    motionFlags &= ~1U;
    colour = source.colour;
    // 0x5bb70 assigns a gravity-derived velocity, then explicitly zeroes it.
    velocity = {0, 0};
}
void EnemyBombActor::run() {
    const neon::vec2d weights{80, 80};
    updateMovementAnimation({freezeTicks, travelled, weights, 6, 97, savedDrawScale.x,
                             stagePosition.x, owner().animationFrameFlags, drawScale.x, heading,
                             sprite});
    fall();
    if (landed || stagePosition.y > double(owner().bounds.halfHeight) ||
        stagePosition.y < -double(owner().bounds.halfHeight) ||
        double(gravity) > std::abs(velocity.y))
        dead = true;
    lifetime = neon::integers::add32(lifetime, -1);
    if (lifetime < 0)
        dead = true;
}
CannonballActor::CannonballActor(StageActor &source)
    : StageActor(source.owner(), source.owner().allocation, float(double(source.gravity) * .2),
                 false),
      spin(source.owner().allocation.cannonballSpin) {
    // 0x5ced8: separate randomized X/Y speeds and a third direction draw.
    category = 5;
    stagePosition = source.stagePosition;
    buoyancy = -.4f;
    submergedDamping = .99f;
    sprite = 96;
    radius = .03f;
    elasticity = .6f;
    velocity.x = owner().random.range(.003, .005);
    velocity.y = owner().random.range(.0005, .001);
    if (owner().random.next() & 1U)
        velocity.x = -velocity.x;
    if (source.gravity < 0)
        velocity.y = -velocity.y;
    policy.ledgeResponse = 0;
    policy.airborneResponse = 1;
    policy.automaticVerticalHandling = false;
    mediumFlags = source.mediumFlags;
    previousVerticalSpeed = float(velocity.y);
    // Metadata is not queried here: the original saves the unit inherited scale.
}
void CannonballActor::run() {
    // 0x62018: retain the PRE-fall narrowed speed for the special bounce path.
    previousVerticalSpeed = float(velocity.y);
    const neon::vec2d weights{80, 80};
    updateMovementAnimation({freezeTicks, travelled, weights, 9, 96, savedDrawScale.x,
                             stagePosition.x, owner().animationFrameFlags, drawScale.x, heading,
                             sprite});
    fall();
    moveHorizontal();
    if (!(mediumFlags & 1U))
        hasLeftWater = true;
    else if (hasLeftWater) {
        lifetime = neon::integers::add32(lifetime, -1);
        if (lifetime < 0)
            dead = true;
    }
    heading = heading + spin;
    if (landed) {
        landed = false;
        stagePosition.y -= double(previousVerticalSpeed);
        previousVerticalSpeed = previousVerticalSpeed * (-.5f);
        if (std::abs(previousVerticalSpeed) > .001f || horizontalContact) {
            if (std::abs(previousVerticalSpeed) > .001f)
                horizontalContact = true;
            sound(owner(), 49);
            bounces = neon::integers::add32(bounces, -1);
            if (bounces < 1) {
                owner().explode(*this, 20);
                dead = true;
            }
        }
        velocity.y = double(previousVerticalSpeed);
        resting = false;
    }
    if (stagePosition.x > double(owner().bounds.halfWidth) * 1.5 ||
        stagePosition.x < double(owner().bounds.halfWidth) * -1.5 ||
        stagePosition.y > double(owner().bounds.halfHeight) * 1.5 ||
        stagePosition.y < double(owner().bounds.halfHeight) * -1.5)
        dead = true;
}
void CannonballActor::hitBy(StageActor *other) {
    sound(owner(), 80);
    StageActor::hitBy(other);
}
} // namespace goatup2
