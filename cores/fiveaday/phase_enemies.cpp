#include "world.h"
#include <algorithm>
namespace fiveaday {
namespace {
float bearing(Vec2 v) {
    float a = float(std::atan2(-v.y, v.x) - 1.570796327);
    if (double(a) < -Pi)
        a = float(double(a) + Tau);
    return a;
}
bool patternBit(uint32_t pattern, unsigned shift) {
    shift &= 255;
    return shift < 32 ? ((int32_t(pattern) >> shift) & 1) != 0 : int32_t(pattern) < 0;
}
} // namespace
Jellyfish::Jellyfish(World &w, const Launch &p) : Wrapped(w) {
    position = retained = p.position;
    heading = p.heading;
    type = 7;
    frame = 70;
    radius = targetRadius = .08f;
    cullMargin = .08f;
    culled = true;
    health = 0;
    world.noise(68);
}
void Jellyfish::run() {
    --phaseTicks;
    velocity = {0, double(-speed)};
    if (double(speed) > .0003)
        speed = float(double(speed) * .998);
    heading = float(double(heading) + double(float(targetHeading - heading)) * .04);
    velocity.rotate(double(heading));
    Wrapped::run();
    travel = float(double(travel) + velocity.length());
    scale.x = baseWidth + double(std::sin(float(travel * 30.f))) * .2;
    if (phase == 0) {
        if (phaseTicks < 1) {
            targetHeading = bearing(position * -1.);
            speed = topSpeed;
            phaseTicks = diveTicks;
            phase = 1;
        }
        if (colour.a < 1)
            colour.a *= 1.05;
        if (colour.a > 1)
            colour.a = 1;
        if (--shotTicks < 1) {
            shotTicks = shotInterval;
            world.radialShoot(*this, shotCount, heading, 3.1415927f, .004f);
            world.noise(33);
        }
    } else if (phase == 1) {
        if (phaseTicks < 1) {
            speed = 0;
            phaseTicks = restTicks;
            phase = 0;
        }
        if (colour.a > .2)
            colour.a *= .994;
    }
    world.touch(*this, world.fruits);
    if (world.bossDefeated) {
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 1, .3f);
        dead = true;
    }
}
void Jellyfish::hitBy(Entity &other) {
    if (&other == this || colour.a < .8)
        return;
    other.didHit(*this);
    if (health < 1) {
        world.explode(position, .03f, .04f, .005f);
        dead = true;
        world.radialShoot(*this, 20, heading, 6.2831855f, .003f);
        world.noise(69);
        world.addScore(int(float(100.f * world.scoreMultiplier)));
    } else {
        --health;
        radius = targetRadius * 1.5f;
        world.explode(other.position, .01f, .04f, .002f, 51);
        world.noise(59);
    }
}
void Jellyfish::didHit(Entity &other) {
    if (phase == 1) {
        --phaseTicks;
        return;
    }
    if (world.ticks & 7) {
        reflectionLatch = false;
        return;
    }
    if (reflectionLatch)
        return;
    reflectionLatch = true;
    --health;
    world.flash(position, 2, .6f);
    if (health < 1) {
        world.noise(69);
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 1, .3f);
        world.radialShoot(*this, 20, heading, 6.2831855f, .003f);
        dead = true;
        colour.a = 0;
        world.addScore(int(float(500.f * world.scoreMultiplier)));
    } else {
        world.noise(24);
        radius = targetRadius * 1.5f;
        world.explode(other.position, .01f, .04f, .006f, 51);
    }
}
PinkPlane::PinkPlane(World &w, const Launch &p) : Wrapped(w) {
    position = retained = p.position;
    heading = p.heading;
    frame = 81;
    radius = targetRadius = .05f;
    cullMargin = .05f;
    culled = true;
    health = 0;
    shotPattern = w.random.next();
    soundPattern = w.random.next();
    shotPattern |= (w.random.next() & 1) ? 1u : 2u;
    soundPattern |= (w.random.next() & 1) ? 0x5555u : 0xaaaau;
    pulses = pulseCount = int(float(w.week * 18.f + 6.f));
    diveTicks = 100 - int(double(w.week) * 10.);
}
void PinkPlane::run() {
    --phaseTicks;
    velocity = {0, double(-speed)};
    heading = float(double(heading) + double(float(targetHeading - heading)) * .08);
    velocity.rotate(double(heading));
    Wrapped::run();
    if (phase == 0) {
        if (pulses < 1) {
            targetHeading = bearing(position * -1.);
            phaseTicks = diveTicks;
            pulses = pulseCount;
            world.noise(74);
            phase = 1;
            patternCursor = 0;
        }
        if (--beatTicks < 1) {
            beatTicks = beatInterval;
            if (patternBit(soundPattern, patternCursor))
                world.noise(75);
            if (patternBit(shotPattern, patternCursor)) {
                world.noise(73);
                radius = targetRadius * 1.4f;
                world.radialShoot(*this, shotCount, bearing(position), 0, .002f);
            }
            ++patternCursor;
            --pulses;
        }
    } else if (phase == 1) {
        if (--beatTicks < 1) {
            beatTicks = beatInterval;
            if (patternBit(soundPattern, patternCursor)) {
                world.noise(75);
                radius = targetRadius * 1.2f;
            }
            patternCursor = (patternCursor + 1) & 15;
        }
        if (phaseTicks < 1) {
            phaseTicks = restTicks;
            phase = 0;
        }
    }
    if (world.bossDefeated) {
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 1, .3f);
        dead = true;
    }
}
void PinkPlane::hitBy(Entity &other) {
    if (&other == this)
        return;
    other.didHit(*this);
    if (health < 1) {
        world.explode(position, .03f, .04f, .005f);
        dead = true;
        if (++world.lifetimePlanes > 100)
            world.achieveGoal(6);
        world.radialShoot(*this, int(double(world.week) * 4.) + 8, heading,
                          float(double(world.week) * Pi + 1.5707963268), .003f);
        world.noise(75);
        world.addScore(int(float(100.f * world.scoreMultiplier)));
    } else {
        --health;
        radius = targetRadius * 1.5f;
        world.explode(other.position, .01f, .04f, .002f, 51);
        world.noise(59);
    }
}
DecoyCloud::DecoyCloud(World &w, const Launch &p) : Wrapped(w) {
    position = retained = p.position;
    heading = p.heading;
    frame = 58;
    radius = targetRadius = .03f;
    scale.x = 3;
    cullMargin = float(double(.03f) * 3.);
    culled = true;
    health = 2;
    burstCount = bursts = int(float(w.week * 10.f + 2.f));
    diveTicks = 500 - int(double(w.week) * 10.);
}
void DecoyCloud::run() {
    --phaseTicks;
    velocity = {0, double(-speed)};
    heading = float(double(heading) + double(float(targetHeading - heading)) * .008);
    velocity.rotate(double(heading));
    Wrapped::run();
    angle = 0;
    if (phase == 0) {
        speed = float(double(speed) * .998);
        if (bursts < 1) {
            targetHeading = bearing(position * -1.);
            phaseTicks = diveTicks;
            bursts = burstCount;
            phase = 1;
        }
        if (--shotTicks < 1) {
            shotTicks = shotInterval;
            if (bursts > 0) {
                --bursts;
                world.radialShoot(*this, 10, bearing(position), 6.2831855f, .002f);
            }
        }
    } else if (phase == 1 && phaseTicks < 1) {
        phaseTicks = restTicks;
        phase = 0;
    }
    if (world.bossDefeated) {
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 1, .3f);
        dead = true;
    }
}
void DecoyCloud::hitBy(Entity &other) {
    if (&other == this)
        return;
    other.didHit(*this);
    if (health < 1) {
        world.explode(position, .03f, .04f, .005f);
        dead = true;
        world.radialShoot(*this, 20, heading, 6.2831855f, .003f);
        world.noise(72);
        world.addScore(int(float(100.f * world.scoreMultiplier)));
    } else {
        --health;
        radius = targetRadius * 1.5f;
        world.explode(other.position, .01f, .04f, .002f, 51);
        world.noise(59);
    }
}
} // namespace fiveaday
