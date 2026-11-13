#include "ground.h"
#include "effects.h"
#include "projectiles.h"
#include "original_tables.h"
#include "pickups.h"
#include <algorithm>
namespace superoxwars {
GroundEnemy::GroundEnemy(World &w, int tile, int kind)
    : GameEntity(w), cell(tile), archetype(kind) {
    position = {-20, -20};
    retained = position;
    type = 0x3000;
    frame = 19;
    radius = .04f;
    angle = 3.1415927f;
    const float rate = float(double(float(w.level)) * .005);
    switch (kind) {
    case 0:
        spread = .31415927f;
        baseColour = unpack(0x7070c0ff);
        frame = 16;
        armour = 1;
        warmup = .1f;
        cooldown = .05f;
        burstCount = 3;
        gunMode = 1;
        gunChance = w.level * 60 + 128;
        angle = 0;
        break;
    case 1:
        frame = 18;
        radius = .03f;
        armour = 2;
        layerOffset = {.05, .03};
        warmup = .02f;
        cooldown = .04f;
        burstCount = 1;
        gunChance = w.level * 20 + 64;
        spin = rate * 4.f;
        shotCount = 8;
        spread = 1.5707964f;
        break;
    case 2:
        turnRate = rate;
        gunChance = w.level * 30 + 128;
        break;
    case 3:
    case 4:
        frame = kind == 3 ? 49 : 50;
        bias = kind == 3 ? -1 : 1;
        radius = .07f;
        armour = 2;
        layerFrame = 0;
        gunChance = 48;
        warmup = cooldown = .4f;
        burstCount = 10;
        spread = .3926991f;
        deathSound = 36;
        emissionOffset = {double(radius) * .3, 0};
        emissionMultiplier = {-1, 0};
        baseAngle = 3.1415927f;
        turnRate = w.level < 2 ? 0 : float(rate + rate * w.weaponThreat);
        break;
    case 5:
        frame = 24;
        radius = .06f;
        scale.x = 1.3;
        armour = 30;
        burstCount = 4;
        gunMode = 0;
        gunChance = 255;
        turnRate = .04f;
        angle = 0;
        deathSound = 34;
        break;
    case 6:
        frame = 26;
        radius = .07f;
        armour = 2;
        layerFrame = 0;
        gunChance = 48;
        warmup = .4f;
        cooldown = .1f;
        burstCount = 1;
        turnRate = rate + rate * w.weaponThreat;
        shotCount = 3;
        spread = .7853982f;
        deathSound = 36;
        break;
    case 7:
        frame = 27;
        radius = .06f;
        scale.x = 1.3;
        armour = 20;
        layerFrame = 0;
        layerPalette = 1;
        layerOffset.y = .01;
        warmup = cooldown = .03f;
        burstCount = 3;
        shotCount = 5;
        spread = .7853982f;
        turnRate = .003f;
        gunChance = 255;
        deathSound = 33;
        break;
    case 9:
        frame = 62;
        radius = .03f;
        armour = 10;
        layerFrame = 0;
        warmup = .3f;
        cooldown = .03f;
        burstCount = 3;
        shotCount = 2;
        spread = .19634955f;
        turnRate = 0;
        gunChance = 255;
        baseAngle = 3.1415927f;
        deathSound = 32;
        break;
    default:
        w.unsupported("Ground turret program " + std::to_string(kind));
        return;
    }
    if (kind != 3 && kind != 4)
        gunChance = gunChance * 255 >> 8;
    colour = baseColour;
}
void GroundEnemy::run() {
    if (!active)
        return;
    if (pulse > 1)
        pulse = std::max(1.f, float(double(pulse) * .98));
    angle += spin;
    if (position.y > double(radius) * scale.y + .6) {
        departed = true;
        dead = true;
        return;
    }
    aimAndFire();
}
void GroundEnemy::aimAndFire() {
    if (turnRate > 0 && world.player) {
        const float target = heading(world.player->position - position), delta = angle - target;
        float change = target > angle ? turnRate : -turnRate;
        if (std::abs(double(delta)) > 3.141592654)
            change = -change;
        if (std::abs(delta) < std::abs(turnRate))
            change = 0;
        angle += change;
    }
    if (double(angle) > 3.141592654)
        angle = float(double(angle) - 6.2831853072);
    if (double(angle) < -3.141592654)
        angle = float(double(angle) + 6.2831853072);
    colour = baseColour;
    int chance = 1023 - int(std::abs(double(world.polarity)) * 200.);
    if (bias < 0 && world.polarity < 0)
        chance += int(double(world.polarity) * -5120.);
    if (bias > 0 && world.polarity > 0)
        chance += int(double(world.polarity) * 5120.);
    const int originalState = gunState;
    if (gunState == 2) {
        gunClock -= interval;
        if (gunClock < 0) {
            gunClock = 1;
            if (emitEnemyShots(world, *this, gunMode,
                               float(double(world.weaponThreat) * .0001 + .003), shotCount, spread))
                pulse = 1.4f;
            if (--burstLeft < 1)
                gunState = 1;
        }
    } else if (gunState == 1) {
        gunClock += cooldown * -.5f;
        if (gunClock <= 0) {
            gunState = 0;
            gunClock = 0;
        }
    } else if (gunState == 0) {
        gunClock += warmup;
        // This surprising -1 threshold is present in the iOS instructions,
        // 52704/52718, and is retained rather than tidied into a different timer.
        if (gunClock > -1) {
            gunClock = 1;
            gunState = 2;
            burstLeft = burstCount;
            if (gunChance < int((world.random.next() & 32767) % uint32_t(chance)))
                gunState = 1;
        }
    }
    if (archetype == 0 && originalState == 2)
        colour = sequenceColour(1.f - gunClock, original::WarmupColours.data());
}
void GroundEnemy::prepare(std::vector<ActorDraw> &out) {
    position = world.tiles.positions[size_t(cell)];
    active = true;
    const auto saved = position;
    const auto savedScale = scale;
    const auto savedColour = colour;
    const int savedFrame = frame;
    Vec2 spacing = position * layerOffset;
    spacing.x += world.layerTilt * .012;
    float factor = 1;
    if (armour > 0 && layerFrame > 0) {
        colour = layerPalette < 16 ? world.palette[size_t(layerPalette)]
                                   : unpack(uint32_t(layerPalette));
        colour.a = double(layerAlpha);
        scale = scale * double(layerScale);
        frame = layerFrame;
        for (int n = 0; n < armour; ++n) {
            Entity::prepare(out);
            position += spacing;
            scale = scale * 1.05;
            factor = float(double(factor) * 1.05);
        }
        colour = savedColour;
    }
    muzzle = position;
    frame = savedFrame;
    scale = savedScale * double(pulse) * double(factor);
    const float savedAngle = angle;
    angle += baseAngle;
    Entity::prepare(out);
    angle = savedAngle;
    position = saved;
    scale = savedScale;
    colour = savedColour;
}
void GroundEnemy::hitBy(Entity &other) {
    other.didHit(*this);
    wasHit = true;
    if (armour > 0) {
        --armour;
        world.add(Domain::Decor, new Explosion(world, other.position, .03f, .12f, 1.07f));
        world.weaponPower = std::min(1.f, world.weaponPower + .35f);
        pulse = 1.4f;
        world.noise(26);
        return;
    }
    world.add(Domain::Decor, new Explosion(world, position, .08f, .1f, 1.08f));
    world.weaponPower = std::min(1.f, world.weaponPower + .5f);
    dead = true;
    if (other.type == 0x2000) {
        if (other.frame == 28) {
            if (bias < 0)
                world.award(reward * 2, -1, position);
            else
                world.award(reward, bias > 0 ? 1 : -1, position);
        } else if (other.frame == 29) {
            if (bias > 0)
                world.award(reward * 2, 1, position);
            else
                world.award(reward, bias < 0 ? -1 : 1, position);
        }
    }
    if (frame == 26 || frame == 49 || frame == 50)
        scatter(world, position, 10,
                frame == 49          ? 3
                : frame == 50        ? 4
                : world.polarity < 0 ? 3
                : world.polarity > 0 ? 4
                                     : 0);
    world.noise(deathSound);
    world.add(Domain::LaunchPods, new Crater(world, cell, radius * .6f));
    if (archetype == 3 || archetype == 4)
        world.add(Domain::Ground, new WeaponPickup(world, cell, archetype == 3 ? 28 : 29));
    if (archetype == 6 && world.polarity != 0) {
        const double roll = world.random.range(0, 1);
        if (roll < std::abs(double(world.polarity)) && world.lifeDropBudget > 0) {
            world.add(Domain::Ground, new LifePickup(world, cell, world.polarity < 0 ? 45 : 46));
            --world.lifeDropBudget;
        }
    }
}
Crater::Crater(World &w, int tile, float size) : GroundEnemy(w, tile, 0) {
    frame = 22;
    radius = size;
    armour = 0;
    gunState = -1;
    spin = turnRate = 0;
    layerFrame = 0;
    angle = 0;
    colour = baseColour = {};
    cycle = original::CraterColours.data();
}
void Eyeball::run() {
    GroundEnemy::run();
    world.bossPresent = true;
}
void Eyeball::prepare(std::vector<ActorDraw> &out) {
    position = world.tiles.positions[size_t(cell)];
    active = true;
    colour = {};
    const auto saved = position, savedScale = scale;
    scale = scale * double(pulse);
    Entity::prepare(out);
    ++frame;
    Vec2 offset{0, -.02};
    offset.rotateBy(double(angle));
    position += offset;
    muzzle = position;
    scale = scale * .6;
    colour = world.palette[pulse <= 1 ? 0 : 1];
    Entity::prepare(out);
    --frame;
    position = saved;
    scale = savedScale;
}
void Eyeball::hitBy(Entity &other) {
    GroundEnemy::hitBy(other);
    pulse = 1.4f;
    emitEnemyShots(world, *this, 0, float(double(world.weaponThreat) * .0001 + .003), shotCount,
                   spread);
    if (dead) {
        if (!world.demo)
            world.noise(34, 0x2000);
        scatter(world, position, 40, world.polarity < 0 ? 3 : world.polarity > 0 ? 4 : 0);
    }
}
void Crater::run() {
    GroundEnemy::run();
    position += positionDelta;
    phase += phaseStep;
    if (phase > 2)
        phase -= 2;
    if (cycle)
        colour = sequenceColour(phase * .5f, cycle);
    baseColour = colour;
}
} // namespace superoxwars
