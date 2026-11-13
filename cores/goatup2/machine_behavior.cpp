#include "campaign_actors.h"
#include "actor_tables.h"
#include "neon/entity_list.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace goatup2 {
namespace {
float sign(float value) {
    return value < 0 ? -1.f : 1.f;
}
void reflect(double position, double extent, double &speed) {
    if (position < -extent && speed < 0)
        speed = -speed;
    if (position > extent && speed > 0)
        speed = -speed;
}
void wrapHeading(float &value) {
    if (double(value) > 3.141592654)
        value = float(double(value) - 6.2831853072);
    if (double(value) < -3.141592654)
        value = float(double(value) + 6.2831853072);
}
} // namespace
float originalAimAngle(neon::vec2d from, neon::vec2d to) {
    const float x = float(to.x - from.x), y = -float(to.y - from.y);
    float angle = float(std::atan2(double(y), double(x)) - 1.570796327);
    if (double(angle) < -3.141592654)
        angle = float(double(angle) + 6.2831853072);
    return angle;
}
float originalSteeringStep(neon::vec2d from, neon::vec2d to, float step, float current) {
    // 0x9b28's lost IT predicates are recovered in steering-instructions.txt.
    const float target = originalAimAngle(from, to), distance = std::abs(current - target);
    float result = target <= current ? -step : step;
    if (double(distance) > 3.141592654)
        result = -result;
    if (distance < std::abs(step))
        result = 0;
    return result;
}
void EnemyActor::initializeMachine() {
    // 0x55aa8, shared by placed actors AND contact-born split children.
    policy.automaticVerticalHandling = behavior == 1;
    switch (behavior) {
    case 0:
    case 1:
    case 3:
    case 10:
    case 12:
        break;
    case 2:
        animationWeights = {0, 30};
        break;
    case 4:
    case 13:
        cycleState = 0;
        break;
    case 5:
    case 6:
        velocity.y = -velocity.y;
        spriteAngleOffset = -.7853982f;
        steeringRate = behavior == 5 ? .01f : .04f;
        if (behavior == 6)
            attackCooldown = fireInterval = 60;
        break;
    case 7:
        // 0x55baa: colour, narrowed angle, rotation, then independent spin draw.
        {
            const auto c =
                unpackParticleColour(actor_data::SplitColours[owner().random.next() & 3U]);
            colour = {c.r, c.g, c.b, c.a};
        }
        heading = float(owner().random.range(-0x1.921fb54524550p+1, 0x1.921fb54524550p+1));
        neon::rotateClockwise(velocity.x, velocity.y, double(heading),
                              neon::ZeroAngleRule::PreserveCoordinates);
        steeringRate = float(owner().random.range(-.01, .01));
        splitDepth = 2;
        break;
    case 8:
        contactHealth = 50;
        break;
    case 9:
        contactHealth = 500;
        break;
    case 11:
        gravity = float(double(gravity) * 1.3);
        resting = false;
        cycleState = 0;
        break;
    case 15:
        velocity.y *= -2.;
        steeringRate = .04f;
        if (sprite == 130 || sprite == 131)
            spriteAngleOffset = -1.5707964f;
        // The source float +1cc cache has no reader in this machine's path.
        // The initializer's doubling of +730c is overwritten by factory r13.
        break;
    case 16:
        attackCooldown = fireInterval = 200;
        break;
    case 14:
        velocity.y = std::abs(stagePosition.y + .5) * .01 + .003;
        contactHealth = 10;
        break;
    default:
        throw std::logic_error("machine behavior outside reconstructed campaigns");
    }
}
void EnemyActor::runMachine() {
    // 0x5c2e0. These timers are not the stage-entity freeze timer +1ec.
    if (attackCooldown > 0)
        --attackCooldown;
    if (launchCooldown > 0)
        --launchCooldown;
    auto *goat = owner().goat.get();
    switch (behavior) {
    case 88:
        heading = float(double(heading) * .95);
        steeringAngle = float(double(steeringAngle) * .95);
        for (auto *target = owner().contactObjects; target; target = target->next)
            if (target->category == 10) {
                const auto delta = target->stagePosition - stagePosition;
                if (delta.length() >= .01)
                    velocity = delta.normalize() * .002;
                else
                    stagePosition = target->stagePosition;
                stagePosition += velocity;
                break;
            }
        if (radius < returnRadius)
            radius = float(double(radius) + .0001);
        return;
    case 1: {
        fall();
        if (attackCooldown < 1 && goat) {
            auto child = std::make_unique<CannonballActor>(*this);
            neon::entities::prepend<StageActor>(owner().enemies, *child);
            child.release();
            attackCooldown = 500;
        }
        return;
    }
    case 4:
    case 11: {
        // Laser collision samples the interval before this tick's movement.
        if (behavior == 4 && goat && std::abs(stagePosition.x - goat->stagePosition.x) < .02) {
            const float live = float(stagePosition.y), origin = float(originalPosition.y);
            if (double(std::min(live, origin)) < goat->stagePosition.y &&
                goat->stagePosition.y < double(std::max(live, origin)))
                goat->hazard = true;
        }
        const auto trigger = [&](double reach) {
            return goat && std::abs(goat->stagePosition.x - stagePosition.x) < reach &&
                   sign(float(goat->stagePosition.y - stagePosition.y)) == sign(gravity);
        };
        if (cycleState == 2) {
            velocity.y = double(sign(gravity)) * (-.003);
            if (behavior == 11) {
                velocity.y *= 4.;
                moveHorizontal();
                reflect(stagePosition.x, double(owner().bounds.halfWidth), velocity.x);
            }
            stagePosition.y += velocity.y;
            if ((velocity.y < 0 && stagePosition.y < originalPosition.y) ||
                (velocity.y > 0 && stagePosition.y > originalPosition.y)) {
                velocity.y = 0;
                cycleState = 0;
            }
            if (behavior == 4 && trigger(.1))
                cycleState = 1;
        } else if (cycleState == 0) {
            if (behavior == 11) {
                moveHorizontal();
                reflect(stagePosition.x, double(owner().bounds.halfWidth), velocity.x);
            }
            if (trigger(double(behavior == 4 ? .1f : .08f)))
                cycleState = 1;
        } else if (cycleState == 1) {
            fall();
            if (verticalContact || stagePosition.y > double(owner().bounds.halfHeight)) {
                verticalContact = false;
                cycleState = 2;
                if (!owner().medium.splashSoundSuppressed)
                    owner().playSample(61, uint32_t(owner().medium.sampleVolume) << 9, 0);
            }
        }
        return;
    }
    case 5:
    case 6:
        if (!goat)
            return;
        steeringAngle = steeringAngle + originalSteeringStep(stagePosition, goat->stagePosition,
                                                             steeringRate, steeringAngle);
        velocity = originalVelocity;
        neon::rotateClockwise(velocity.x, velocity.y, double(steeringAngle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        fireMachine();
        moveVertical();
        moveHorizontal();
        if (verticalContact || horizontalContact) {
            verticalContact = horizontalContact = false;
            heading = heading + steeringRate * 8.f;
        }
        wrapHeading(steeringAngle);
        heading = steeringAngle + spriteAngleOffset;
        return;
    case 15: {
        if (!goat)
            return;
        steeringAngle = steeringAngle + originalSteeringStep(stagePosition, goat->stagePosition,
                                                             steeringRate, steeringAngle);
        const float distance = float((goat->stagePosition - stagePosition).length());
        const float limited = std::min(steeringRange, distance);
        const float factor = (steeringRange - limited) / steeringRange;
        velocity = originalVelocity * double(factor);
        neon::rotateClockwise(velocity.x, velocity.y, double(steeringAngle),
                              neon::ZeroAngleRule::PreserveCoordinates);
        fireMachine();
        moveVertical();
        moveHorizontal();
        if (verticalContact || horizontalContact)
            verticalContact = horizontalContact = false;
        wrapHeading(steeringAngle);
        if (sprite == 130 || sprite == 131 || sprite == 174)
            heading = steeringAngle + spriteAngleOffset;
        return;
    }
    case 9:
        if (launchCooldown > 0)
            --launchCooldown;
        moveHorizontal();
        reflect(stagePosition.x, double(owner().bounds.halfWidth), velocity.x);
        reflect(stagePosition.y, double(owner().bounds.halfHeight), velocity.y);
        return;
    case 10:
    case 16:
        fall();
        if (verticalContact) {
            velocity.y = 0;
            verticalContact = false;
        }
        fireMachine();
        return;
    case 12:
        fall();
        if (verticalContact) {
            if (std::abs(velocity.y) > .005 ||
                std::abs(stagePosition.y) > double(owner().bounds.halfHeight)) {
                dead = true;
                emitImpactParticles(
                    {stagePosition, drawScale, radius, gravity},
                    {owner().medium.sampleVolume, owner().medium.splashSoundSuppressed}, 30,
                    owner().random, owner());
            } else
                velocity.y = 0;
            verticalContact = false;
        }
        return;
    case 14:
        contactHealth = 10;
        stagePosition.y += velocity.y;
        if (stagePosition.y > double(owner().bounds.halfHeight) && velocity.y > 0)
            stagePosition.y = -double(owner().bounds.halfHeight);
        if (stagePosition.y < -double(owner().bounds.halfHeight) && velocity.y < 0)
            stagePosition.y = double(owner().bounds.halfHeight);
        return;
    case 7:
        heading = heading + steeringRate;
        // 0x5c6e0's ITTT GT is absent from the C export: stop draining at 1.
        if (contactHealth > 1.f)
            contactHealth = contactHealth - 1.f;
        [[fallthrough]];
    case 0:
    case 2:
    case 3:
    case 8:
    case 13:
        if (motionFlags & 8U)
            fall();
        else
            moveVertical();
        moveHorizontal();
        reflect(stagePosition.x, double(owner().bounds.halfWidth), velocity.x);
        reflect(stagePosition.y, double(owner().bounds.halfHeight), velocity.y);
        fireMachine();
        return;
    default:
        throw std::logic_error("untranslated machine update behavior");
    }
}
void EnemyActor::fireMachine() {
    // 0x5bb70. Modes without a firing branch deliberately produce nothing.
    if (behavior == 8) {
        if (owner().requiredFruit < 1) {
            paletteIndex = 2;
            animationMode = 0;
            velocity = {0, double(gravity * (-4.f))};
            originalVelocity = velocity;
            steeringRate = .04f;
            returnRadius = radius + radius;
            behavior = 88;
        }
    } else if (behavior == 2) {
        if (attackCooldown > 0)
            return;
        attackCooldown = 120;
        if (!owner().goat || (owner().goat->stagePosition - stagePosition).length() <= .1)
            return;
        auto child = std::make_unique<EnemyBombActor>(*this);
        neon::entities::prepend<StageActor>(owner().enemies, *child);
        child.release();
    } else if (behavior == 16) {
        if (attackCooldown > 0)
            return;
        attackCooldown = fireInterval;
        auto child = std::make_unique<EnemyShotActor>(*this, EnemyShotActor::Sideways{});
        neon::entities::prepend<StageActor>(owner().enemies, *child);
        child.release();
    } else if (behavior == 6) {
        if (attackCooldown > 0)
            return;
        attackCooldown = fireInterval;
        double difference = 0;
        if (owner().goat)
            difference = double(std::abs(
                steeringAngle - originalAimAngle(stagePosition, owner().goat->stagePosition)));
        if (std::abs(difference) >= .5)
            return;
        auto child = std::make_unique<EnemyShotActor>(*this, true);
        neon::entities::prepend<StageActor>(owner().enemies, *child);
        child.release();
    }
}
} // namespace goatup2
