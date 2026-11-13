#include "air_enemy.h"
#include "effects.h"
#include "original_tables.h"
#include "projectiles.h"
#include <algorithm>
namespace superoxwars {
AirEnemy::AirEnemy(World &w, const LaunchPod &pod) : GameEntity(w) {
    const auto &p = pod.launch;
    archetype = p.archetype;
    groupIndex = p.group;
    position = {pod.retained.x * 1.5, pod.position.y};
    retained = position;
    type = 0x4003;
    frame = original::EnemyFrames[size_t(archetype)];
    radius = .06f;
    angle = 3.1415927f;
    if (w.level % 24 == 10)
        frame = 89;
    if (w.level % 24 == 11)
        frame = 90;
    // Common setup stores these response coefficients even in actors whose
    // selected program does not call them. Retain those random draws.
    w.random.range(1, 1.001);
    w.random.range(1, 1.001);
    scale = pod.scale * 2.5;
    w.random.range(1, 1.001);
    originScale = scale;
    const int ordinal = p.waveOrdinal;
    const float areaFraction = float(w.level & 7) / 7.f;
    auto &a = phases[0];
    auto &b = phases[1];
    switch (archetype) {
    case 0:
    case 2:
        a.gates = 2;
        a.cruise.y = double(float(ordinal)) * .0005 + .006;
        a.acceleration.y = .001;
        if (archetype == 2) {
            a.towards.x = double(float(ordinal)) * .002 + .005;
            a.acceleration.x = double(p.fraction) * .0003 + .0001;
        }
        if (ordinal > 0 || w.level > 0) {
            a.gunSpeed = 1.4f;
            a.gunType = ordinal % 3 + 1;
            a.gunChance = ordinal * 16 + 64;
            a.gunRate = .04f;
            shotCount = (w.level & 3) + w.level / 10 + 1;
        }
        break;
    case 1:
        a.wobble = .1f;
        a.towards.x = .001;
        a.cruise.y = .006;
        a.acceleration = {.0001, .001};
        a.targetY = float(double(float(ordinal)) * .1 + .2);
        a.gates = 0x40020;
        a.gunSpeed = areaFraction * .8f + .7f;
        a.gunType = 3;
        a.gunChance = w.level * 4 + 100;
        a.eventChance = ordinal * 100 + 100;
        a.gunRate = .03f;
        b.cruise.y = -.02;
        b.acceleration = {.0001, .0002};
        b.towards.x = double(float(ordinal)) * .005 + .01;
        b.gates = 6;
        b.gunType = 1;
        b.gunChance = int(float(ordinal) * 100.f);
        b.gunSpeed = 1.3f;
        wobblePhase = 0;
        wobbleStep = float(double(p.secondStep) * (double(float(ordinal + 1)) * .1 + 1.2));
        break;
    case 3:
        a.cruise.y = double(p.fraction) * .01 + .006;
        a.acceleration.y = .001;
        a.gates = 0x80;
        a.nearY = float(double(p.fraction) * .15 + .05);
        a.gunType = 1;
        a.gunChance = ordinal * 64 + 64 + w.level * 4;
        a.gunRate = .03f;
        a.gunSpeed = 1.6f;
        b.gates = 0x10007;
        b.cruise.y = double(p.fraction) * .0005 - .002;
        b.acceleration = {.0003, .0002};
        b.towards.x = .01;
        b.gunType = 1;
        b.gunChance = ordinal * 64 + 64;
        b.gunRate = .1f;
        b.gunSpeed = 1.3f;
        break;
    case 4:
        a.towards.x = .003;
        a.acceleration.x = .0001;
        a.cruise.y = double(p.fraction) * .002 + .006;
        a.acceleration.y = .0003;
        a.clock = float(double(p.fraction) * .2 + .6);
        b.clock = .9f;
        b.cruise.y = -.008;
        b.acceleration.y = .0003;
        shotCount = 3;
        shotSpread = .3926991f;
        a.gunSpeed = 1.6f;
        a.gates = 0x40008;
        a.gunType = 1;
        a.gunChance = w.level * 20;
        a.eventChance = 1000;
        b.towards.x = .01;
        b.acceleration.x = .0003;
        b.gates = 0x10004;
        break;
    case 5:
        a.gates = 0x180080;
        a.target = {(double(p.fraction) - .5) * .2, .1};
        a.acceleration = {.0002, .0002};
        a.towards = {.01, double(float(ordinal)) * .003 + .01};
        a.gunType = 2;
        a.gunChance = 40;
        a.gunRate = .03f;
        b.gates = 0x200200;
        b.signClock = 1;
        b.damping = {.9, .8};
        b.spin = float(w.random.range(.1, .2));
        b.signStep = .008333334f;
        b.gunType = 1;
        b.gunChance = w.level * 30 + ordinal * 20 + 100;
        b.gunRate = float(double(float(ordinal)) * .01 + .03);
        phases[2].gates = 0x180004;
        phases[2].target = {(double(p.fraction) - .5) * -2.3, -.9};
        phases[2].acceleration = {.0002, .0004};
        phases[2].towards = {.003, .02};
        break;
    case 6:
        for (int n = 0; n < 4; ++n) {
            auto &m = phases[size_t(n)];
            m.gates = n < 3 ? 0x180100 : 2;
            m.target = {w.random.range(n < 3 ? -.2 : -.4, n < 3 ? .2 : .4),
                        w.random.range(n < 3 ? -.2 : -.4, n < 3 ? .2 : .4)};
            m.acceleration = n < 3 ? Vec2{.0002, .0002} : Vec2{.0004, .0004};
            m.towards = n < 3 ? Vec2{.01, .01} : Vec2{.02, .02};
            m.gunType = 3;
        }
        a.gunChance = w.level * 20 + 40;
        a.gunRate = .03f;
        b.gunChance = w.level * 30 + 60;
        b.gunRate = .06f;
        phases[2].gunChance = w.level * 4000;
        phases[2].gunRate = .1f;
        phases[3].gunChance = ordinal * 30 + 170;
        phases[3].gunRate = .3f;
        break;
    case 19:
        a.gates = 0x200;
        a.signClock = 1;
        a.signStep = float(1. / (double(float(p.fraction * 10.f)) + 60.));
        a.towards = {.005, .02};
        a.acceleration = {.0001, .0005};
        b.gates = 0x4000;
        b.gunSpeed = 1.6f;
        deathShotCount = 8;
        deathShotSpread = 1.5707964f;
        deathShotType = 1;
        break;
    case 8:
    case 9:
        a.gates = 1;
        a.cruise.x = archetype == 8 ? .01 : -.01;
        a.acceleration.x = .001;
        a.towards.y = double(float(ordinal)) * .003;
        a.acceleration.y = a.towards.y * .01;
        a.gunType = 1;
        a.gunChance = ordinal * 10 + 40;
        a.gunRate = .07f;
        a.gunSpeed = float(double(float(ordinal)) * .1 + 1.2);
        break;
    case 10:
        a.gates = 0x180100;
        a.target = {};
        a.acceleration = {.0002, .0002};
        a.towards = {.01, .01};
        if (w.level > 3) {
            armour = ordinal + 1;
            b.gunType = 3;
            b.gunChance = w.level * 4000;
            phases[2].gunRate = .02f;
        }
        b.gates = 0x600200;
        b.forwardSpeed = .004f;
        b.signClock = 1;
        b.signStep = .01f;
        b.spin = float(double(1.f - p.fraction) * .1 + .01);
        b.spinDamping = .98f;
        phases[2].gates = 7;
        phases[2].acceleration = {.0004, .0004};
        phases[2].towards = {.02, .02};
        break;
    case 11:
    case 12:
        velocity.y = .02;
        a.damping.y = w.random.range(.97, .98);
        a.acceleration.y = 0;
        a.gates = archetype == 11 ? 0x40400 : 0x41000;
        if (archetype == 12) {
            a.damping.x = .98;
            a.towards.x = w.random.range(.005, .006);
        }
        shotSpread = archetype == 11 ? .7853982f : 1.5707964f;
        shotCount = archetype == 11 ? 2 : 3;
        a.gunType = 1;
        a.eventChance = 500;
        b.cruise.y = -.02;
        b.acceleration.y = double(p.fraction) * .0005 + .0003;
        b.gates = 4;
        break;
    case 13:
        a.damping = {.98, .98};
        a.towards = {.015, .015};
        a.gunSpeed = 2;
        a.gates = 0x2000;
        a.gunType = 1;
        a.gunRate = float(double(areaFraction) * .04 + .01);
        a.gunChance = 1000;
        break;
    case 14:
        a.target.x = double(p.fraction) - .5;
        a.gates = 0x80200;
        velocity.y = .02;
        a.towards = {.01, .01};
        a.signClock = 1;
        a.signStep = float(double(p.fraction) * .06 + .03);
        b.gates = 0x41000;
        b.acceleration.y = 0;
        b.damping.y = .98;
        shotCount = (w.level & 7) | 8;
        b.gunType = 1;
        b.eventChance = 1000;
        b.gunChance = 0;
        shotSpread = 6.2831855f;
        phases[2].damping.y = 1.04;
        phases[2].towards = {.01, .01};
        phases[2].gates = 7;
        break;
    case 15:
        a.target = {(double(p.fraction) - .5) * .3, double(p.fraction) * .2 - .4};
        a.gates = 0x180100;
        a.towards = {.016, .015};
        b.gates = 0x200200;
        armour = ordinal;
        b.gunType = 3;
        b.gunRate = .53333336f;
        b.gunChance = 1000;
        b.signClock = 1;
        b.signStep = float(1. / (double(float(float(ordinal) * 30.f)) + 60.));
        b.spin = (p.ordinal & 1) ? -.10471976f : .10471976f;
        phases[2].gates = 7;
        phases[2].towards = {.01, .01};
        break;
    case 16:
        a.eventChance = 1000;
        a.cruise.y = .003;
        a.gunChance = 0;
        a.gunType = 1;
        a.gunSpeed = 2;
        a.target.x = (double(p.fraction) - .5) * .3;
        a.towards.x = .0005;
        a.acceleration.x = .000002;
        a.gates = 0x880002;
        armour = 3;
        shotCount = ordinal * 3 + 3;
        shotSpread = float(double(p.fraction) * 1.570796327 + .39269908175);
        break;
    case 17:
        a.target.y = w.random.range(-.4, 0);
        a.towards.y = .02;
        a.acceleration.y = double(float(ordinal)) * .00003 + .0001;
        a.gates = 0x140080;
        shotCount = 3;
        a.gunType = 1;
        a.eventChance = 1000;
        a.gunChance = 0;
        armour = ordinal / 2;
        shotSpread = float(double(float(ordinal)) * .2 + .7853981635);
        b.target.x = -.9;
        if (w.random.next() & 1)
            b.target.x = -b.target.x;
        b.acceleration.y = 0;
        b.damping.y = .97;
        b.towards.x = .02;
        b.gates = 0x80001;
        break;
    case 18:
        a.target.y = w.random.range(-.4, 0);
        a.towards.y = .02;
        a.gates = 0x100080;
        b.gunType = 3;
        b.gunChance = 1000;
        b.gunRate = .03f;
        shotSpread = .7853982f;
        b.towards = {.01, .001};
        b.acceleration.y = .00001;
        b.damping.y = .97;
        b.gates = 7;
        break;
    case 20:
        a.cruise = {0, .002};
        a.acceleration.y = .001;
        a.gates = 0x8002;
        armour = 4;
        shotCount = 2;
        shotSpread = 3.1415927f;
        for (int n = 0; n < 4; ++n) {
            auto &m = phases[size_t(n)];
            m.gates = 0x8002;
            m.gunType = 2;
        }
        a.gunSpeed = 1.4f;
        a.gunChance = ordinal * 16 + 64;
        a.gunRate = .04f;
        b.cruise.y = .004;
        b.gunSpeed = 1.6f;
        b.gunChance = ordinal * 20 + 128;
        b.gunRate = .06f;
        phases[2].cruise.y = .006;
        phases[2].gunSpeed = 1.8f;
        phases[2].gunChance = ordinal * 40 + 160;
        phases[2].gunRate = .08f;
        phases[3].cruise.y = .012;
        phases[3].gunSpeed = 2;
        phases[3].gunChance = ordinal * 66 + 256;
        phases[3].gunRate = .1f;
        break;
    case 21:
        a.cruise.y = w.random.range(.002, .004);
        a.acceleration.y = .0001;
        a.towards.x = w.random.range(.002, .005);
        a.acceleration.x = w.random.range(.00002, .00004);
        a.gates = 0x8002;
        armour = 4;
        b.cruise.y = w.random.range(.006, .01);
        b.acceleration.y = .0001;
        b.towards.x = w.random.range(.004, .008);
        b.acceleration.x = w.random.range(.00004, .00008);
        b.gates = 0x8002;
        phases[2].cruise.y = w.random.range(.008, .012);
        phases[2].acceleration.y = .0001;
        phases[2].towards.x = w.random.range(.008, .02);
        phases[2].acceleration.x = w.random.range(.00006, .00012);
        phases[2].gates = 0x8002;
        phases[3].cruise.y = -.01;
        phases[3].acceleration.y = .0001;
        phases[3].towards.x = w.random.range(.006, .009);
        phases[3].acceleration.x = w.random.range(.00006, .00016);
        phases[3].gates = 7;
        phases[3].gunSpeed = 2;
        phases[3].gunType = 2;
        phases[3].gunChance = ordinal * 66 + 256;
        phases[3].gunRate = .1f;
        break;
    case 22:
        a.wobble = .1f;
        a.towards.x = .001;
        a.cruise.y = .006;
        a.acceleration = {.0001, .001};
        a.targetY = float(double(float(ordinal)) * .1 + .2);
        radius = .05f;
        scale.x = originScale.x = 1.3;
        armour = ordinal > 2 ? 0 : ordinal;
        a.gates = 0x8002;
        a.gunSpeed = areaFraction * .8f + .7f;
        a.gunType = 3;
        a.gunChance = w.level * 4 + 100;
        a.eventChance = ordinal * 100 + 100;
        a.gunRate = .03f;
        b.cruise.y = -.002;
        b.acceleration = {.0001, .0002};
        b.wobble = .2f;
        b.towards.x = double(float(ordinal)) * .005 + .01;
        b.gates = 6;
        b.gunType = 1;
        b.gunRate = .1f;
        shotCount = 4;
        shotSpread = 3.1415927f;
        b.gunChance = 1000;
        b.gunSpeed = 1.3f;
        wobbleStep = float(double(p.secondStep) * (double(float(ordinal + 1)) * .1 + 1.2));
        break;
    case 23:
        radius = .08f;
        a.cruise.y = .003;
        a.acceleration.y = .001;
        armour = (ordinal & 3) + 2;
        a.towards.x = double(p.fraction) * .02;
        a.gates = 0x8002;
        b.cruise.y = .006;
        b.gates = 0x8002;
        phases[2].cruise.y = .01;
        phases[2].gates = 0x8002;
        phases[3].cruise.y = .03;
        phases[3].gates = 0x8007;
        paletteSlot = 10;
        deathShotCount = 16;
        deathShotSpread = 6.2831855f;
        deathShotType = 1;
        a.eventChance = 500;
        break;
    case 24:
        a.cruise.y = .006;
        a.acceleration.y = .001;
        radius = .02f;
        a.towards.x = w.random.range(.002, .004);
        armour = ordinal > 1 ? 0 : ordinal;
        scale.y = originScale.y = 2;
        a.gates = 0x8002;
        b.cruise.y = -.008;
        b.towards.x = w.random.range(.005, .007);
        b.gunChance = 100;
        b.gunType = 3;
        b.gunRate = phases[2].gunRate = phases[3].gunRate = .1f;
        b.gunSpeed = 1.2f;
        b.gates = 4;
        phases[2].cruise.y = .015;
        phases[2].gates = 0x8002;
        phases[2].towards.x = w.random.range(.01, .02);
        phases[2].gunChance = 400;
        phases[2].gunType = 3;
        phases[2].gunSpeed = 1.4f;
        phases[3].cruise.y = .03;
        phases[3].gates = 0x8007;
        break;
    case 25:
    case 26:
    case 27:
    case 28:
        externalAcceleration.y = .0002;
        if (archetype == 25) {
            a.gates = 2;
            a.towards.x = .01;
            a.acceleration.x = .0001;
            a.cruise.y = .02;
            a.acceleration.y = a.towards.y / 100.;
            a.eventChance = 500;
            deathShotCount = ordinal + 3;
            deathShotSpread = 6.2831855f;
            deathShotType = 1;
        } else if (archetype == 26) {
            a.gates = 0x1000002;
            armour = ordinal + 1;
            a.towards.x = .005;
            a.acceleration.x = .00008333333333333333;
            a.cruise.y = .02;
            a.acceleration.y = a.towards.y / 100.;
        } else {
            a.gates = 0x1000003;
            armour = 2;
            a.cruise.x = archetype == 27 ? .01 : -.01;
            a.acceleration.x = .00016666666666666666;
            a.cruise.y = .02;
            if (archetype == 27)
                scale.x = originScale.x = -1;
            a.acceleration.y = a.towards.y / 100.;
            velocity.y = -(double(p.fraction) * .005 + .005);
            a.gunType = 1;
            a.gunChance = ordinal * 10 + 40;
            a.gunRate = .07f;
            a.gunSpeed = float(double(float(ordinal)) * .1 + 1.2);
        }
        break;
    case 7:
        a.gates = 0x200;
        a.acceleration = {.0002, .002};
        a.towards = {.002, double(float(ordinal)) * .002 + .008};
        a.signClock = 1;
        a.signStep = .04f;
        b.gates = 2;
        b.towards = {.003, .009};
        b.acceleration = {.0001, .0001};
        b.gunType = 3;
        b.gunChance = ordinal * 10 + 40;
        b.gunRate = .07f;
        b.gunSpeed = 1.6f;
        deathShotCount = (w.level & 7) * 2 + 8;
        deathShotSpread = 6.2831855f;
        deathShotType = 1;
        break;
    default:
        w.unsupported("Air enemy program " + std::to_string(archetype));
        return;
    }
    // 58194 tail: 12 draws per phase, in the source's interleaved order.
    // Unused coefficients still consume their draws.
    for (auto &nextA : phases) {
        nextA.gunRate = float(double(nextA.gunRate) * w.random.range(.95, 1.05));
        nextA.cruise.x *= w.random.range(.95, 1.05);
        nextA.cruise.y *= w.random.range(.95, 1.05);
        nextA.towards.x *= w.random.range(.95, 1.05);
        nextA.towards.y *= w.random.range(.95, 1.05);
        nextA.acceleration.x *= w.random.range(.95, 1.05);
        nextA.acceleration.y *= w.random.range(.95, 1.05);
        nextA.clock = float(double(nextA.clock) * w.random.range(.95, 1.05));
        nextA.wobble = float(double(nextA.wobble) * w.random.range(.95, 1.05));
        nextA.nearX = float(double(nextA.nearX) * w.random.range(.95, 1.05));
        nextA.nearY = float(double(nextA.nearY) * w.random.range(.95, 1.05));
        nextA.signStep = float(double(nextA.signStep) * w.random.range(.95, 1.05));
    }
}
void AirEnemy::fire(const AirPhase &a) {
    const int mode = a.gunType == 1 ? 0 : a.gunType == 2 ? 4 : 2;
    const float speed = a.gunSpeed * (a.gunType == 1 ? .003f : a.gunType == 2 ? .005f : .006f);
    if (a.gunType >= 1 && a.gunType <= 3 &&
        emitEnemyShots(world, *this, mode, speed, shotCount, shotSpread))
        displayScale = 1.4f;
}
void AirEnemy::run() {
    position.x += world.scrollDelta.x;
    retained.x += world.scrollDelta.x;
    const Vec2 before = position;
    if (paletteSlot >= 0)
        colour = world.palette[size_t(paletteSlot)];
    if (displayScale > 1) {
        displayScale = std::max(1.f, float(double(displayScale) * .95));
        scale = originScale * double(displayScale);
    }
    if (phase < 0) {
        previousStep = position - before;
        lastTravel = float(previousStep.length());
        ++lifeTicks;
        return;
    }
    auto &a = phases[size_t(phase)];
    const auto flags = a.gates;
    if ((flags & 0xffff) == 0) {
        dead = true;
        world.resetChain();
        return;
    }
    Vec2 target = world.player ? world.player->position : Vec2{};
    if (flags & 0x80000)
        target.x = a.target.x;
    if (flags & 0x100000)
        target.y = a.target.y;
    const auto delta = target - position;
    const auto sign = [](float v) { return v > 0 ? 1 : v < 0 ? -1 : 0; };
    const float sx = float(sign(float(delta.x))), sy = float(sign(float(delta.y)));
    bool triggered = false;
    if ((flags & 0x200) && a.signClock < 0)
        triggered = true;
    const float limitX = float(double(radius) * scale.x + .8),
                limitY = float(double(radius) * scale.y + .8);
    if ((flags & 1) && ((position.x < -double(limitX) && velocity.x < 0) ||
                        (position.x > double(limitX) && velocity.x > 0)))
        triggered = true;
    if ((flags & 2) && position.y > double(limitY))
        triggered = true;
    if ((flags & 4) && position.y < -double(limitY))
        triggered = true;
    if (flags & 8) {
        a.clock -= lastTravel;
        if (a.clock < 0)
            triggered = true;
    }
    if ((flags & 0x10) && std::abs(position.x - double(a.targetX)) <= velocity.x)
        triggered = true;
    if ((flags & 0x20) && std::abs(position.y - double(a.targetY)) <= velocity.y)
        triggered = true;
    if ((flags & 0x40) && std::abs(delta.x) < double(a.nearX))
        triggered = true;
    if ((flags & 0x80) && std::abs(delta.y) < double(a.nearY))
        triggered = true;
    if ((flags & 0x100) && delta.length() < double(a.nearX))
        triggered = true;
    if ((flags & 0x400) && velocity.length() < .005)
        triggered = true;
    if ((flags & 0x800) && velocity.x < .005)
        triggered = true;
    if ((flags & 0x1000) && velocity.y < .005)
        triggered = true;
    if ((flags & 0x4000) || ((flags & 0x8000) && wasHit))
        triggered = true;
    if (triggered) {
        if (flags & 0x4000)
            destroy();
        if ((flags & 0x40000) && int(world.random.next() & 255) < a.eventChance)
            fire(a);
        if (++phase > 3) {
            dead = true;
            world.resetChain();
            return;
        }
    }
    auto &m = phases[size_t(phase)];
    if ((flags & 0x800000) && wasHit && int(world.random.next() & 255) < m.eventChance)
        fire(m);
    if ((flags & 0x1000000) && wasHit && velocity.y > 0)
        velocity.y *= -.9;
    wasHit = false;
    if (flags & 0x400000) {
        velocity = {0, -double(m.forwardSpeed)};
        velocity.rotateBy(double(angle));
    } else {
        double vx = m.cruise.x, vy = m.cruise.y;
        if (m.towards.x > 0) {
            vx = double(sx) * m.towards.x;
            if (flags & 0x10000) {
                m.cruise.x = vx;
                m.towards.x = 0;
            }
        }
        if (m.towards.y > 0) {
            vy = double(sy) * m.towards.y;
            if (flags & 0x20000) {
                m.cruise.y = vy;
                m.towards.y = 0;
            }
        }
        const auto approach = [](double target, double current, double limit) {
            const double d = target - current;
            return limit <= std::abs(d) ? (d <= 0 ? current - limit : current + limit) : target;
        };
        velocity = {approach(vx, velocity.x, m.acceleration.x),
                    approach(vy, velocity.y, m.acceleration.y)};
    }
    retained.x += velocity.x;
    position.y += velocity.y;
    velocity = velocity * m.damping;
    velocity += externalAcceleration;
    wobble += float(float(m.wobble - wobble) * .1f);
    position.x = retained.x;
    if (wobble > 0) {
        position.x += double(float(std::sin(wobblePhase) * wobble));
        wobblePhase += wobbleStep;
    }
    if (m.gunType > 0) {
        gunClock -= m.gunRate;
        if (gunClock < 0) {
            gunClock += 1;
            if (int(world.random.next() & 511) < m.gunChance)
                fire(m);
        }
    }
    m.signClock -= m.signStep;
    if (flags & 0x200000) {
        angle += m.spin;
        if (double(angle) > 3.141592654)
            angle = float(double(angle) - 6.2831853072);
        if (double(angle) < -3.141592654)
            angle = float(double(angle) + 6.2831853072);
        m.spin *= m.spinDamping;
    } else
        angle = heading(previousStep);
    previousStep = position - before;
    lastTravel = float(previousStep.length());
    ++lifeTicks;
}
void AirEnemy::destroy() {
    dead = true;
    auto &gun = phases[size_t(phase)];
    gun.gunType = deathShotType;
    if (deathShotType > 0) {
        shotCount = deathShotCount;
        shotSpread = deathShotSpread;
        const int mode = deathShotType == 1 ? 0 : deathShotType == 2 ? 4 : 2;
        const float factor = deathShotType == 1 ? .003f : deathShotType == 2 ? .005f : .006f;
        if (emitEnemyShots(world, *this, mode, gun.gunSpeed * factor, shotCount, shotSpread, true))
            displayScale = 1.4f;
        world.noise(35);
    }
    world.add(Domain::Decor, new Explosion(world, position, .06f, .1f, 1.06f));
    world.noise(8);
}
void AirEnemy::hitBy(Entity &other) {
    if (position.y < -.45)
        return;
    other.didHit(*this);
    if (armour > 0) {
        --armour;
        wasHit = true;
        world.add(Domain::Decor, new Explosion(world, other.position, .03f, .12f, 1.07f));
        world.weaponPower = std::min(1.f, world.weaponPower + .35f);
        displayScale = 1.4f;
        world.noise(26);
        return;
    }
    world.weaponPower = std::min(1.f, world.weaponPower + .5f);
    ++world.groupKills[size_t(groupIndex)];
    if (other.type == 0x2000) {
        if (other.frame == 28)
            world.award(reward, -1, position);
        else if (other.frame == 29)
            world.award(reward, 1, position);
    }
    if (world.polarity != 0) {
        if (world.groupKills[size_t(groupIndex)] == world.groupSize[size_t(groupIndex)]) {
            ++world.chain;
            if (world.chain > 12) {
                world.noise(int(world.random.next() & 1) ? 12 : 13);
                world.achieveGoal(3);
            } else
                world.noise(world.chain - 1);
            const int points = (int(std::abs(double(world.polarity)) * 999.) + 1) * world.chain;
            world.award(points, world.polarity > 0 ? 1 : -1, position);
            if (world.scoreNotice)
                world.scoreNotice(
                    points, position, world.polarity > 0 ? 1 : -1,
                    {double(float(position.x * -.03)), double(float(position.y * -.02))}, .06f);
            if (world.groupReward)
                world.groupReward(position, world.polarity > 0 ? 4 : 3);
        }
    }
    destroy();
}
} // namespace superoxwars
