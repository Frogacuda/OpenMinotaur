#include "world.h"
#include <algorithm>
#include <stdexcept>

namespace fiveaday {
Bomber::Bomber(World &w, const Launch &p, BomberGunAllocationInputs allocation) : Attacker(w) {
    constexpr int frames[] = {22, 37, 42, 47, 53, 60};
    constexpr uint32_t colours[] = {0xffffffff, 0xffffff7f, 0xffffffcf,
                                    0xffffffff, 0xffffffff, 0xffffffff};
    position = retained = p.position;
    heading = p.heading;
    spinOffset = 0;
    type = 3;
    frame = frames[int(w.month) % 6];
    colour = unpack(colours[int(w.month) % 6]);
    radius = targetRadius = .06f;
    cullMargin = .06f;
    culled = true;
    ++w.population;
    armed = false;
    reward = 50;
    health = std::min(15, int(double(w.month) * 2.) + int(double(std::min(w.year, 4.f)) * 4.) + 2);
    tailLength = 24;
    turnStep = .005f;
    speed = float(double(w.week) * .002 + .003 + double(w.year) * .002);
    orbitRadius = .3f;
    orbitGain = 1;
    missileChance = 0;
    shotMode = -1;
    // The source leaves these fields unwritten. Zero aim closes the inherited
    // RNG gate; mode -1 alone only prevents projectile births. See world.h.
    aimThreshold = allocation.aimThreshold;
    shotTicks = allocation.shotTicks;
    shotInterval = allocation.shotInterval;
    shotChance = allocation.shotChance;
    bombTicks = bombInterval = int(std::min(w.year, 4.f)) * -10 + 60;
    world.noise(40);
    if (frame == 53)
        scale.x = 1.3;
    else if (frame == 60)
        scale.y = 2;
    else if (frame == 47) {
        scale.y = .7;
        colour = {.7, .5, .9, .75};
        health = int(double(w.week) + double(w.week)) + 1;
    }
}
void Bomber::run() {
    Attacker::run();
    if (frame == 42)
        spinOffset += .2f;
    else if (frame == 47)
        angle = 0;
    else if (frame == 37)
        spinOffset += .1f;
    if (world.gameOver)
        return;
    world.touch(*this, world.fruits);
    if (bombTicks > 0)
        --bombTicks;
    if (std::abs(position.x) < .5 && std::abs(position.y) < .5 && bombTicks < 1 &&
        (world.random.next() & 255) < 8) {
        world.add(world.enemies, new Bomb(world, *this));
        bombTicks = bombInterval;
    }
    if (soundTicks < 1) {
        if (distance <= .5f) {
            soundTicks = soundInterval;
            world.noiseGain(32, 8 * int(double(distance) * 8192.));
        }
    } else
        --soundTicks;
}
void Bomber::hitBy(Entity &other) {
    Attacker::hitBy(other);
    if (!wasHit)
        return;
    if (health < 1)
        world.noise(33);
    world.noise(int(world.random.next() & 3) + 41, 10);
    world.flash(position, 2, .6f);
}
void Bomber::didHit(Entity &) {
    if (world.ticks & 15)
        return;
    --health;
    world.flash(position, 3, .6f);
    world.noise(int(world.random.next() & 3) + 41, 11);
    if (health < 1) {
        world.noise(33);
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 4, .3f);
        killedNow = true;
        deathTicks = 2;
        colour.a = 0;
    } else {
        radius = targetRadius * 1.5f;
        world.explode(position, .01f, .04f, .002f, 51);
    }
}
Bomb::Bomb(World &w, const Bomber &parent) : Entity(w) {
    position = parent.position;
    velocity = parent.velocity * -1.5;
    velocity.rotate(w.random.range(-.5, .5));
    angle = float(w.random.range(-Pi, Pi));
    radius = .03f;
    frame = 23;
    triggerRadius = float(w.random.range(.1, .2));
    spin = float(w.random.range(-.3, .3));
    shotCount = int(w.month) + 5;
    constexpr int frames[] = {23, 38, 43, 49, 54, 62};
    constexpr uint32_t colours[] = {0xffffffff, 0xff80ff7f, 0xffffffcf,
                                    0xffffffff, 0x4733c4ff, 0xffffffff};
    frame = frames[int(w.month) % 6];
    colour = unpack(colours[int(w.month) % 6]);
    if (parent.frame == 47) {
        velocity = {-.006, .006};
        radius = .025f;
    }
    world.noise(39);
}
void Bomb::hitBy(Entity &other) {
    other.didHit(*this);
    explode();
}
void Bomb::explode() {
    world.noise(38);
    world.noise(20);
    world.explode(position, .02f, .02f, .006f);
    world.flash(position, 3, .2f);
    world.radialShoot(*this, shotCount, 0, 6.2831855f, .003f);
    dead = true;
}
void Bomb::run() {
    position += world.camera;
    const Vec2 delta = world.ship.position - position;
    world.touch(*this, world.fruits);
    if (state == 0) {
        position += velocity;
        velocity = velocity * .97;
        if (delta.length() < double(triggerRadius) || velocity.length() < .0001) {
            world.noise(37);
            fuse = 240;
            state = 1;
        }
    } else if (state == 1) {
        const double length = delta.length();
        if (length > 0)
            velocity += delta * (.0001 / length);
        // controlBlock::attract: bomb mass 10, ship default mass 1, strength 10000.
        const Vec2 attraction = world.attractShip(position, 10.f, 10000.f);
        if (attraction.length() < double(.01f))
            velocity += attraction;
        angle += spin;
        colour = world.palette(3);
        position += velocity;
        velocity = velocity * .97;
        if (--fuse < 1)
            explode();
    }
}
MinotaurKiller::MinotaurKiller(World &w, const Launch &p) : Attacker(w) {
    constexpr int frames[] = {33, 36, 40, 50, 55, 61};
    constexpr uint32_t colours[] = {0xffffffff, 0xffff807f, 0xffffffcf,
                                    0xffffffff, 0xffffffff, 0xffffffff};
    position = retained = p.position;
    heading = p.heading;
    spinOffset = 0;
    type = 3;
    frame = frames[int(w.month) % 6];
    colour = unpack(colours[int(w.month) % 6]);
    radius = targetRadius = .04f;
    cullMargin = .04f;
    culled = true;
    ++w.population;
    reward = 100;
    carrier = true;
    formationLocked = true;
    tailLength = 120;
    tailDecay = float(double(w.week) * .2 + .01);
    formationColour = 1;
    health = int(w.month) + int(float(w.week * 3.f)) + 1;
    turnStep = .02f;
    speed = float(double(w.week) * .006 + .003);
    orbitRadius = 0;
    orbitGain = 4;
    missileChance = 0;
    shotInterval = shotTicks = 10;
    shotChance = (int(w.week) + 1) * 128;
    shotMode = 0;
    shotSpeed = float(double(w.week) * .5 + 1.8);
    aimThreshold = float(double(w.week) + .1 + double(w.year));
    if (frame == 36 || frame == 40 || frame == 55 || frame == 61)
        radius = targetRadius = .06f;
    if (frame == 50) {
        scale.y = 1.8;
        colour.r = .5;
        colour.g = .1;
        aimThreshold = 3.1415927f;
        shotMode = 2;
        shotSpeed = float(double(w.week) * .004 + .004);
    }
}
void MinotaurKiller::run() {
    Attacker::run();
    if (wasHit) {
        world.noise(53);
        if (killedNow)
            world.noise(47);
        world.flash(position, 2, .6f);
        wasHit = false;
    }
}
} // namespace fiveaday
