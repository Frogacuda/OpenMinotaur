#include "world.h"
#include <algorithm>
#include <stdexcept>
namespace fiveaday {
HomingMissile::HomingMissile(World &w, const Attacker &source, bool special) : Attacker(w) {
    reward = weight = 0;
    position = source.position;
    angle = special ? source.gunAngle : source.angle;
    init(w.currentLaunch);
    missileChance = 0;
    world.noise(76, 10);
    shotMode = -1;
    shotChance = 0;
    aimThreshold = 0;
    world.flash(position, 5, .3f);
    turnStep = float(double(turnStep) * world.random.range(.8, 1.2));
    speed = float(double(speed) * world.random.range(.8, 1.2));
    spinOffset = 0;
    // Caller customizes the spawned missile after init. Both original random
    // draws still occur even though these two values are subsequently replaced.
    heading = source.heading;
    speed = source.speed * 1.5f;
    orbitRadius = -.5f;
    frame = 107;
    radius = targetRadius = .02f;
    turnStep = source.turnStep * .5f;
    scale = {1, 2};
    health = 0;
    if (special) {
        health = 1;
        heading = source.missileHeading;
        speed = source.shotSpeed;
        turnStep = source.missileTurn;
        frame = source.missileFrame;
        radius = targetRadius = source.missileRadius;
        scale = source.missileScale;
    }
}
void HomingMissile::run() {
    Attacker::run();
    if (frame == 48) {
        colour = world.palette(4);
        spinOffset = 3.1415927f;
    } else if (frame == 84)
        spinOffset += .1f;
    else
        colour = {1, 1, 1, 1};
    ++age;
    if (culled && retireWhenCulled)
        dead = true;
    if (lifetime < age)
        retireWhenCulled = true;
    if (world.ship.state != Ship::State::Flying) {
        dead = true;
        world.explode(position, .02f, .02f, .002f);
    }
}
Boss::Boss(World &w, const Launch &launch, BossAllocationInputs allocation)
    : Attacker(w), reflectionLatch(allocation.reflectionLatch) {
    constexpr int frames[] = {20, 35, 41, 47, 56, 63};
    constexpr uint32_t colours[] = {0xffffffff, 0x80ffff7f, 0xffffffcf,
                                    0xffffffff, 0xffffffff, 0xffffffff};
    position = retained = launch.position;
    culled = true;
    cullMargin = .09f;
    type = 3;
    ++w.population;
    radius = targetRadius = .09f;
    reward = 1000;
    armed = false;
    suppressDeathBlast = true;
    frame = frames[int(w.month) % 6];
    colour = unpack(colours[int(w.month) % 6]);
    spinOffset = 0;
    heading = launch.heading;
    damageRange = .5f;
    missileChance = int(double(w.year) * 128.) + 128;
    missileInterval = int(float(w.month * -10.f + 100.f));
    health = int(double(w.week) * 10.) + (w.month < 1 ? 5 : int(double(w.month) * 10.));
    tailLength = 24;
    speed = .002f;
    orbitRadius = 0;
    orbitGain = 4;
    shotInterval = shotTicks = 10;
    shotChance = 256;
    shotMode = 1;
    turnStep = float(double(w.week) * .04 + .01);
    aimThreshold = 3.1415927f;
    shotSpeed = float((double(w.week) * 1.5 + 1.4) * -.003);
    switch (frame) {
    case 20:
        shotSpeed = float((double(w.week) * 3.5 + 1.4) * -.003);
        break;
    case 35:
        shotMode = 4;
        gunAngle = angle;
        shotSpeed = float((double(w.week) + double(w.week) + 1.) * -.001);
        shotCount = 3;
        shotChance = 4096;
        shotInterval = 10;
        shotSpread = .7853982f;
        aimThreshold = 1.5707964f;
        gunAngleStep = float(double(w.week) * .05 + .025);
        break;
    case 41:
        shotMode = 4;
        gunAngle = angle;
        shotCount = 2;
        shotChance = 4096;
        shotInterval = 20;
        shotSpread = 3.1415927f;
        aimThreshold = 6.2831855f;
        shotSpeed = float((double(w.week) + 1.) * -.0035);
        break;
    case 47:
        scale.y = .5;
        missileRadius = .02f;
        missileScale = {1, 1.8};
        missileFrame = 48;
        missileHeading = 3.1415927f;
        gunAngle = 3.1415927f;
        shotMode = 3;
        missileTurn = float(double(w.week) * .02 + .01);
        shotChance = int(double(w.week) * 64.) + 64;
        colour = {.4, .4, .4, 1};
        aimThreshold = 6.2831855f;
        shotSpeed = float(double(w.week) * .01 + .005);
        break;
    case 56:
        shotMode = 4;
        gunAngle = angle;
        shotChance = 4096;
        shotInterval = 10;
        shotCount = 4;
        shotSpread = 6.2831855f;
        aimThreshold = 6.2831855f;
        shotSpeed = float((double(w.week) + 1.) * -.0035);
        break;
    case 63:
        radius = targetRadius = .12f;
        missileScale = {1, 1};
        missileRadius = .03f;
        missileFrame = 84;
        missileHeading = 1.5707964f;
        gunAngle = 3.1415927f;
        shotMode = 3;
        missileTurn = float(double(w.week) * .04 + .01);
        shotInterval = 40 - int(double(w.week) * 5.);
        shotChance = int(double(w.week) * 64.) + 512;
        aimThreshold = 6.2831855f;
        shotSpeed = float(double(w.week) * .008 + .005);
        break;
    }
    maximumHealth = health;
}
void Boss::run() {
    world.bossPosition = position;
    healthFraction = float(health) / float(maximumHealth);
    if (healthFraction > displayHealth)
        displayHealth = std::min(healthFraction, float(double(displayHealth) + .02));
    else if (healthFraction < displayHealth)
        displayHealth = std::max(healthFraction, float(double(displayHealth) - .02));
    if (deathTicks > 0 && world.ship.state != Ship::State::Leaving) {
        world.ship.state = Ship::State::Leaving;
        world.ship.exitAcceleration = 0;
        world.noise(27);
        world.bossDefeated = true;
        if (world.cleanStage)
            world.achieveGoal(0);
        world.levelEndedTicks = 500;
        world.message(10, "LEVEL ENDED", 500);
    }
    Attacker::run();
    switch (frame) {
    case 35:
        colour = world.palette(0);
        colour.a = .5;
        spinOffset = float(double(spinOffset) - .13);
        gunAngle -= gunAngleStep;
        break;
    case 41:
        gunAngle = angle;
        spinOffset = float(double(spinOffset) + .05);
        break;
    case 47:
        angle = 0;
        colour = world.palette(1);
        break;
    case 56:
        angle = gunAngle = 0;
        break;
    case 63:
        colour = world.palette(1);
        missileHeading = (world.random.next() & 1) ? 1.5707964f : -1.5707964f;
        angle = 0;
        break;
    }
    world.touch(*this, world.fruits);
}
void Boss::hitBy(Entity &other) {
    if (deathTicks > 0)
        return;
    Attacker::hitBy(other);
    if (!wasHit)
        return;
    const bool allowed = world.playerCanFinishBoss();
    if (other.type == 600 && allowed)
        world.explode(other.position, .01f, .04f, .002f, 51);
    hitFlash = 1;
    if (health < 1) {
        if (!allowed) {
            ++health;
            deathTicks = 0;
            colour.a = 1;
        } else
            world.noise(25);
    } else
        world.noise(24);
    world.flash(position, 2, .6f);
}
void Boss::didHit(Entity &other) {
    if ((world.ticks & 7) != 0) {
        reflectionLatch = false;
        return;
    }
    if (reflectionLatch || deathTicks > 0)
        return;
    reflectionLatch = true;
    --health;
    hitFlash = 1;
    world.flash(position, 2, .6f);
    if (health < 1) {
        if (!world.playerCanFinishBoss())
            ++health;
        else {
            world.noise(25);
            world.explode(position, .02f, .02f, .006f);
            world.flash(position, 1, .3f);
            deathTicks = 2;
            colour.a = 0;
        }
    } else {
        world.noise(24);
        radius = targetRadius * 1.5f;
        if (world.playerCanFinishBoss())
            world.explode(other.position, .01f, .04f, .006f, 51);
    }
}
void Boss::prepare(std::vector<ActorDraw> &out) {
    hitFlash = float(double(hitFlash) * .92);
    if (!world.demo)
        out.push_back({{0, -.46},
                       {double(displayHealth) * .7, .01},
                       .5f,
                       0,
                       0,
                       {1, 0, 0, double(hitFlash) * .5 + .4}});
    Attacker::prepare(out);
}
void World::achieveGoal(int index) {
    if (demo)
        return;
    const uint32_t bit = uint32_t(1) << unsigned(index);
    if (achievements & bit)
        return;
    achievements |= bit;
    goal = index;
    awardMessages.push_back(index);
}
} // namespace fiveaday
