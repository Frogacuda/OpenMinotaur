#include "native_head.h"
#include <cmath>
#include <stdexcept>
namespace gr::reconstruction::ios {
namespace {
constexpr float pi = 3.1415927f, halfPi = 1.5707964f, quarterPi = .7853982f,
                threeQuarterPi = 2.3561945f;
}
void HeadWorld::checkSweep(Head &h) {
    h.checkSweep();
}
void Head::initialize(const HeadSpawn &s, Head *parentHead, uint32_t flags) {
    if (s.turnTicks < 1 || s.separation < 0 || (flags != 0 && flags != 4 && flags != 0x104))
        throw std::invalid_argument("Invalid source Head initialization parameters");
    parent = parentHead;
    type = parentHead ? 7 : 2;
    armour = originalArmour = parentHead ? s.bodyArmour : s.cycle + s.headArmour;
    tint = parentHead ? ac.bodyTint : ac.headTint;
    bump = false;
    age = 0;
    headCooldown = podCooldown = deathCountdown = 0;
    frame = 0;
    sprite = baseSprite = 20;
    position = {double(s.x), double(s.y)};
    speed = (float(s.cycle) * .3f + 1.f) * s.baseSpeed;
    separation = s.separation;
    turnTicks = s.turnTicks;
    // init writes position/opacity/mode histories only. Do not invent a
    // history full of valid movement states; untouched fields retain storage.
    positions.fill(position);
    opacities.fill(0);
    modes.fill(-1);
    opacity = 0;
    if ((flags & 255) == 4) {
        mode = 4;
        movementAngle = (flags >> 8) == 1 ? (s.x < 0 ? -threeQuarterPi : threeQuarterPi)
                                          : (s.randomBit & 1 ? quarterPi : -quarterPi);
    } else {
        mode = 0;
        movementAngle = s.x < 0 ? halfPi : -halfPi;
    }
    radius = baseRadius = .03f;
    damageRadius = .08f;
}
void Head::startTurn(float increment, float destination) {
    turnStep = increment;
    target = destination;
    turning = turnTicks;
    returnMode = mode;
    mode = 1;
}
void Head::normalTurn() {
    bump = false;
    if (!world.shipPosition)
        throw std::logic_error("Original normalTurn requires a ship");
    const bool downward =
        (ac.deathRepel == 0 || position.y <= -.32) && position.y < world.shipPosition->y;
    if (velocity.x > 0)
        startTurn((downward ? -pi : pi) / float(turnTicks), -halfPi);
    else if (velocity.x < 0)
        startTurn((downward ? pi : -pi) / float(turnTicks), halfPi);
}
void Head::diagonalBoundary() {
    const float step = halfPi / float(turnTicks);
    if (velocity.x > 0 && position.x > .41) {
        if (velocity.y < 0)
            startTurn(step, -threeQuarterPi);
        else
            startTurn(-step, -quarterPi);
    } else if (velocity.x < 0 && position.x < -.39) {
        if (velocity.y < 0)
            startTurn(-step, threeQuarterPi);
        else
            startTurn(step, quarterPi);
    } else if (velocity.y < 0 && position.y < -.322) {
        if (velocity.x < 0)
            startTurn(step, -quarterPi);
        else
            startTurn(-step, quarterPi);
    } else if (velocity.y > 0 && position.y > .418) {
        if (velocity.x > 0)
            startTurn(step, threeQuarterPi);
        else
            startTurn(-step, -threeQuarterPi);
    }
}
void Head::writeHistory() {
    const uint32_t slot = frame & 31;
    positions[slot] = position;
    opacities[slot] = opacity;
    modes[slot] = mode;
    returnModes[slot] = returnMode;
    targets[slot] = target;
    turnSteps[slot] = turnStep;
    turnCounts[slot] = turning;
    angles[slot] = movementAngle;
    ++frame;
}
HeadSample Head::historyAt(uint32_t slot) const {
    slot &= 31;
    return {positions[slot], opacities[slot], modes[slot],      returnModes[slot],
            targets[slot],   turnSteps[slot], turnCounts[slot], angles[slot]};
}
void Head::hitBy(Entity &other) {
    // iOS 0x4f518. Attacker notification precedes dying/type/dead filters.
    if (age < 30 || opacity < 1)
        return;
    other.didHit(*this);
    if (mode == 2 || type == 6 || dead)
        return;
    const int32_t particles = ac.powers[1] ? 2 : 10;
    if (other.type == 2 || other.type == 7 || &other == this ||
        (other.type != 5 && other.type != 8))
        return;
    if (other.type == 5) {
        if (!world.ship)
            throw std::logic_error("Original bullet hit requires Ship record");
        // 0x4f5ea..616: compare with 10; add 2 only when below. NEON
        // immediate efc0 0f10 expands to 0x40000000, not Ghidra's printed 0.
        if (world.ship->fireBoost < 10)
            world.ship->fireBoost += 2.f;
        ac.hitFlash = 1;
    }
    if (armour < 1) {
        if (other.type == 5 && ac.powers[1] == 0)
            world.makePod(position, false);
    } else if (other.type == 5) {
        armour = add32(armour, -1);
        world.sound(62);
        radius = damageRadius;
        return;
    }
    if (type == 2) {
        world.score(30);
        world.sound(36);
        world.explode({position, 2, .002f, .01f, 2, particles * 2, .01f});
        world.bonus({position, 25, .02f, 4});
        const int32_t draw = int32_t(world.globalRandom() & 1023);
        if (add32(draw, int32_t(uint32_t(ac.level) * 8)) < ac.dropChance) {
            if (!world.survival || ac.level < 6) {
                int32_t kind;
                if (ac.level == 0) {
                    const auto first = world.globalRandom() & 3;
                    kind = int32_t(first + (world.globalRandom() & 3) + 1);
                    if (kind == 6) {
                        if (ac.warpUsed)
                            kind = 7;
                        else if (ac.timeScale < 1)
                            kind = 4;
                    }
                } else
                    kind = int32_t(world.globalRandom() & 3);
                world.sound((kind & 7) | 40);
                world.pickup(position, kind);
            }
            ac.dropChance = ac.dropReset;
            Vec2 v{};
            v.rotateBy(world.headRandomRange(*this, -3.141592654, 3.141592654));
            world.outline({position, v, 0, .06f, 1.1f});
        } else
            ac.dropChance = add32(ac.dropChance, ac.dropIncrement);
    } else {
        world.score(10);
        world.sound(37);
        world.explode({position, 2, .002f, .01f, 0x00ffffff, particles, .01f});
        world.bonus({position, 24, .02f, 5});
        const float rotation = float(world.headRandomRange(*this, -3.141592654, 3.141592654));
        world.outline({position, {}, rotation, .03f, 1.03f});
    }
    mode = 2;
    deathCountdown = 5;
    type = 6;
    tint.a = 0;
}
void Head::didHit(Entity &other) {
    // iOS 0x4fbd8 recovered from the binary; matches Android 0x86510.
    if ((mode != 0 && mode != 4) || dead || &other == this)
        return;
    if (other.type != 1 && other.type != 2 && other.type != 3 && other.type != 4)
        return;
    bump = true;
    if (other.type == 2) {
        if (headCooldown) {
            bump = false;
            return;
        }
        headCooldown = 30;
    }
    if (other.type == 1) {
        if (podCooldown)
            bump = false;
        else
            podCooldown = 10;
    }
}
void Head::checkSweep() {
    if (age < 30 || opacity < 1 || ac.powers[7] <= 0)
        return;
    world.score(30);
    world.sound(36);
    world.explode({position, 2, .002f, .01f, 2, 20, .01f});
    world.bonus({position, 25, .02f, 4});
    mode = 2;
    deathCountdown = 5;
    type = 6;
    tint.a = 0;
    Vec2 v{};
    v.rotateBy(world.headRandomRange(*this, -3.141592654, 3.141592654));
    world.outline({position, v, 0, .06f, 1.2f, 33, 1, .05f});
}
void Head::run() {
    // iOS 0x4e9e8 performs this movement even for followers, before replacing
    // their displayed state with a parent's delayed sample.
    const double shipY = world.shipPosition ? world.shipPosition->y : 0;
    age = add32(age, 1);
    sprite = (int32_t((position.x + .5) * 40) & 3) + baseSprite;
    velocity = {0, double(speed)};
    velocity.rotateBy(double(movementAngle));
    position.x += velocity.x * double(ac.timeScale);
    position.y += velocity.y * double(ac.timeScale);
    if (radius > baseRadius)
        radius = float(double(radius) * .95); // source double literal at 0x4ed78
    int32_t branch = mode;
    if (type == 2)
        tint = ac.headTint;
    else if (type == 7) {
        tint = ac.bodyTint;
        branch = 3;
    }
    if (deathCountdown > 0) {
        deathCountdown = add32(deathCountdown, -1);
        if (deathCountdown < 1)
            dead = true;
    }
    if (branch == 0) {
        if (opacity < 1)
            opacity += .05f;
        ac.liveThreats = add32(ac.liveThreats, 1);
        if (headCooldown)
            headCooldown = add32(headCooldown, -1);
        if (podCooldown)
            podCooldown = add32(podCooldown, -1);
        if (ac.timeScale < 1) {
            headCooldown = podCooldown = 5;
            bump = false;
        }
        const bool needsTurn =
            bump || (ac.deathRepel > 0 && position.y > -.2) ||
            (velocity.x > 0 && position.x > .41) || (velocity.x < 0 && position.x < -.37) ||
            (ac.laserFrozen && ac.timeScale >= 1 && std::abs(double(ac.laserX) - position.x) < .03);
        if (opacity < 1 || !needsTurn)
            bump = false;
        else {
            bump = true;
            normalTurn();
        }
        if (mode == 0 && position.y > shipY) {
            movementAngle = velocity.x < 0 ? -threeQuarterPi : threeQuarterPi;
            mode = 4;
        }
    } else if (branch == 1) {
        ac.liveThreats = add32(ac.liveThreats, 1);
        movementAngle += turnStep;
        turning = add32(turning, -1);
        if (turning < 1) {
            mode = returnMode;
            movementAngle = target;
        }
    } else if (branch == 3) {
        if (!parent)
            throw std::logic_error("Original follower requires its parent record");
        const float delay = ((1.f - ac.timeScale) * 2.f) * float(separation);
        const uint32_t slot =
            (parent->frame - uint32_t(separation) - uint32_t(int32_t(delay))) & 31;
        if (parent->type == 6) {
            type = 2;
            armour = parent->originalArmour;
        }
        const auto s = parent->historyAt(slot);
        position = s.position;
        opacity = s.opacity;
        mode = s.mode;
        returnMode = s.returnMode;
        movementAngle = s.angle;
        target = s.target;
        turnStep = s.turnStep;
        turning = s.turning;
        ac.liveThreats = add32(ac.liveThreats, 1);
    } else if (branch == 4) {
        if (opacity < 1)
            opacity += .05f;
        ac.liveThreats = add32(ac.liveThreats, 1);
        if (headCooldown)
            headCooldown = add32(headCooldown, -1);
        if (ac.timeScale < 1)
            headCooldown = 5;
        if (podCooldown)
            podCooldown = add32(podCooldown, -1);
        if (bump) {
            const float step = halfPi / float(turnTicks);
            if (velocity.x > 0) {
                if (velocity.y >= 0)
                    startTurn(-step, -quarterPi);
                else
                    startTurn(step, -threeQuarterPi);
            } else if (velocity.x < 0) {
                if (velocity.y >= 0)
                    startTurn(step, quarterPi);
                else
                    startTurn(-step, threeQuarterPi);
            }
        }
        bump = false;
        if (mode == 4)
            diagonalBoundary();
    }
    if (branch == 0 || branch == 1 || branch == 4)
        world.checkSweep(*this);
    if (ac.timeScale >= 1)
        tint.a = double(opacity);
    else {
        const float phase = (float(world.clock & 31) / 31.f) * 6.2831855f;
        const float wave = std::sin(phase);
        tint.a = double(opacity) * (double(wave) * .5 + .5);
    }
    writeHistory();
    world.touchEnemyList(*this);
    if ((std::abs(position.x) > .6 || std::abs(position.y) > .6) && type == 2) {
        mode = 2;
        deathCountdown = 5;
        type = 6;
    }
}
} // namespace gr::reconstruction::ios
