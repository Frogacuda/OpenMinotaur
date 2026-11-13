// Distinct original ship classes. Shared shentity controls, drawing, sensors,
// weapons and contact services live in ship.cpp; flight rules stay here by class.
#include "ship.h"
#include <cmath>
#include <cstring>

namespace minos {
namespace {
#include "ship_variant_data.inc"
double bitsD(uint64_t u) {
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}
int sgn(float x) {
    return x < 0.0f || std::isnan(x) ? -1 : 1;
}
} // namespace

void VicLander::init(PlayState &playState) { // 0x61614
    initCommon(playState);
    spriteIndex = 10;
    size = 0.04f;
    scale.x = 1.3;
    drag = {0.96, 0.9999};
    gravity = 0.0005f;
    col.unpackRGBA(0x30ffffff);
    idleFuel = 0.00005f;
    fuelPerThrust = 0.0006f;
    damage = 0.002f;
    const double sx = double(size) * scale.x, sy = double(size) * scale.y;
    const double nx = bitsD(0x3faeb851f3333333ull);
    const double sideY = bitsD(0xbf689374b3333333ull);
    addSensor(0, {nx, -0.3}, 1.0f, {sx * double(-0.9f), sy * double(0.9f)});
    addSensor(1, {-nx, -0.3}, 1.0f, {sx * double(0.9f), sy * double(0.9f)});
    addSensor(2, {0.3, sideY}, 2.0f, {-sx, sy * double(0.9f)});
    addSensor(3, {-0.3, sideY}, 2.0f, {sx, sy * double(0.9f)});
    addSensor(4, {0.0, 0.3}, 6.0f, {sx * 0.0, -sy});
    addGun({sx * double(-0.9f), sy * double(0.9f)}, 0, 1);
    addGun({sx * double(0.9f), sy * double(0.9f)}, 0, 1);
    addGun({sx * 0.0, -sy}, 0, -1);
    // Original also writes +0x248=0.001f and clears +0x24c/+0x250.
    // No reader in this class or shentity uses those trailing fields.
}

void VicLander::run() { // 0x65f08
    PlayState &G = *g;
    touchedDown = false;
    rot = float(double(rot) * 0.9);
    doControls();
    if (dying)
        return;
    Sensor *s0 = sensor(0);
    Sensor *s1 = sensor(1);
    if (!(s0->touching && s1->touching) && isLanded && vel.y < 0.0) {
        if (--landingTimer < 1) {
            isLanded = false;
            G.reset310 = false;
            G.landed = false;
        }
    }
    if (s0->touching && s1->touching && !s0->hazard && !s1->hazard && !isLanded &&
        double(std::fabs(rot)) < 0.001) {
        isLanded = true;
        launchHold = true;
        touchedDown = true;
        landingTimer = 15;
        G.landed = true;
        G.padCell[0] = s0->cell[0];
        G.padCell[1] = s0->cell[1];
        vel = {0, 0};
        thrustTarget = 0;
        thrust = 0;
    }
    sinceContact++;
    impact = 0;
    bool reflect = false;
    for (Entity *c : children) {
        if (!c)
            continue;
        auto *s = static_cast<Sensor *>(c);
        s->shipLanded = isLanded;
        if (!(s->rated && s->touching))
            continue;
        sinceContact = 0;
        impact += s->impact;
        s->impact = 0;
        const Vec2 n = s->normal;
        if (isLanded || s->hazard)
            continue;
        const double ay = std::fabs(n.y), ax = std::fabs(n.x);
        if (!(ay <= ax) || ay == ax) {
            if (sgn(float(vel.y)) != sgn(float(n.y))) {
                mapPos.y = prevMapPos.y;
                vel.y = -(vel.y * ay);
                if (vel.y < 0.0)
                    reflect = true;
            }
            if (!isLanded) {
                vel.x = vel.x + n.x * 0.01;
                vel.y = vel.y + n.y * 0.01;
            }
        }
        if (ay < ax) {
            if (sgn(float(vel.x)) != sgn(float(n.x))) {
                vel.x = -(vel.x * ax);
                mapPos.x = prevMapPos.x;
            }
            if (!isLanded) {
                vel.x = vel.x + n.x * 0.01;
                vel.y = vel.y + n.y * 0.01;
            }
        }
    }
    if (sinceContact == 0 && !isLanded) {
        if (!G.muted)
            G.playSample(0x18, G.volume << 13);
        G.cleanRun = false;
        age = 0;
    }
    if (touchedDown)
        impact = impact * rateLanding();
    if (0 < impact && !(0.0f < G.fuel))
        G.shield = 0;
    G.shield = G.shield - float(impact) * damage;
    if (G.shield < 0.0f)
        G.shield = 0;
    if (reflect) {
        double d = 0;
        if (s0->touching) {
            if (!s1->touching)
                d = 1.0;
        } else if (s1->touching)
            d = -1.0;
        rot = float(double(rot) + d * vel.y * 10.0);
    }
    if (launchHold) {
        if (thrustHeld < 1)
            launchHold = false;
        else
            thrustHeld = 0;
    }
    if (isLanded)
        rotateHeld = 0;
    thrustTarget = thrustHeld == 0 ? 0.0f : gravity * 3.0f;
    const float speed = float(std::fabs(vel.y) + std::fabs(vel.y));
    float step = speed * speed;
    if (double(step) < 0.008)
        step = 0.008f;
    if (isLanded || sinceContact < 10)
        step = 0.05f;
    float t = thrust;
    if (t < thrustTarget) {
        t = float(double(t) + double(step) * 0.02);
        thrust = t;
    }
    if (thrustTarget < t) {
        t = float(double(t) - kThrustFall);
        thrust = t;
    } // 0x66880: vsub.f64, then narrow
    if (t < 0.0f)
        thrust = 0;
    if (0 < thrustHeld) {
        vel.y = vel.y - double(thrust);
        G.fuel = G.fuel - fuelPerThrust;
    }
    if (G.fuel < 0.0f) {
        G.fuel = 0;
        G.sample(0x44);
    }
    if (vel.y < -0.2)
        vel.y = -0.2;
    if (rotateHeld != 0) {
        const int i = rotateSlot & 7;
        float f = float((G.touchNowX[i] - G.touchStartX[i]) * 0.19);
        if (G.touchFlag[i] && sgn(float(G.touchDeltaX[i])) != sgn(f) &&
            0.006 < std::fabs(G.touchDeltaX[i])) {
            G.touchNowX[i] = G.touchStartX[i];
            f = -f;
        }
        if (!analogDrive && sgn(f) != sgn(float(vel.x)))
            f = f + f;
        // 0x664b8/c0: binary64 product narrowed before adding to velocity.
        vel.x = vel.x + double(float(double(-f) * 0.03));
        if (0.08 < vel.x)
            vel.x = 0.08;
        else if (vel.x < -0.08)
            vel.x = -0.08;
    }
    vel.x = vel.x * drag.x;
    prevMapPos = mapPos;
    mapPos.x = mapPos.x + vel.x * 5.0;
    if (!reflect && !isLanded)
        vel.y = vel.y + double(gravity);
    vel.y = vel.y * drag.y;
    if (thrustHeld == 0 && vel.y < 0.0)
        vel.y = vel.y * drag.y * 0.98;
    mapPos.y = mapPos.y + vel.y;
    if (0.01 < vel.y)
        G.scrollSpeed = -0.25;
    if (vel.y < -0.01 || G.flag203808)
        G.scrollSpeed = 0.15;
    pos.x = mapPos.x / 20.0 - 0.5;
    pos.y = G.screenY;
    const double oldCamera = G.cameraY;
    G.cameraY = (mapPos.y - 12.5) + G.screenY * -25.0;
    if (G.cameraY < 0.0) {
        pos.y = pos.y + G.cameraY / 25.0;
        G.cameraY = 0;
        if (pos.y < -0.48) {
            pos.y = -0.48;
            if (vel.y < 0.0)
                vel.y = 0;
            thrustTarget = 0;
            thrust = 0;
        }
        if (G.flag203808) {
            G.setMessage(1, 300, 0xe, 2, kGoMsg[4]);
            G.setMessage(3, 0x140, 0x10, 2, kGoMsg[5]);
        }
    }
    if (G.scrollLimit < G.cameraY) {
        pos.y = pos.y + (G.cameraY - G.scrollLimit) / 25.0;
        G.cameraY = G.scrollLimit;
    }
    G.cameraStep = G.cameraY - double(float(oldCamera));
    G.scrollDelta.y = G.cameraStep / 25.0;
    if (0.0f < thrustTarget && (G.frameCount & 1)) {
        auto *p = new ThrustParticle;
        p->pos = pos;
        p->vel.x = 0;
        const double lo = double(float(double(thrustTarget + 1.0f) * 0.004));
        p->vel.y = G.rnd(lo, lo * 1.4);
        p->size = 0.02f;
        p->growth = float(G.rnd(1.03, 1.05));
        p->collides = true;
        p->fade = float(G.rnd(0.02, 0.03));
        p->init(G);
        G.addEntity(G.debris, p);
    }
    G.touch(*this, G.touchList);
    const Vec2 savedPos = pos, savedVel = vel;
    G.bounceOff(*this, G.bounceList);
    if (dead) {
        dead = false;
        const Vec2 delta{pos.x - savedPos.x, pos.y - savedPos.y};
        if (float(std::sqrt(delta.y * delta.y + delta.x * delta.x)) < size) {
            const double dx = vel.x - savedVel.x;
            vel.x = savedVel.x + (dx + dx) * 0.5;
            vel.y = savedVel.y + (vel.y - savedVel.y) * 2.5 * 0.5;
            mapPos.x = mapPos.x + (pos.x - savedPos.x) * 20.0;
            mapPos.y = mapPos.y + (pos.y - savedPos.y) * 25.0;
        }
        pos = savedPos;
        G.spark(0x14, pos, {-G.bounceNormal.x, -G.bounceNormal.y});
    }
}
void Taxi::init(PlayState &playState) { // 0x62184
    initCommon(playState);
    spriteIndex = 0x4b;
    size = 0.04f;
    scale = {1.6, 0.8};
    drag = {0.985, 1.0};
    gravity = 0.0014f;
    col.white(1.0);
    idleFuel = 0.00015f;
    fuelPerThrust = 0.0003f;
    damage = 0.005f;
    const double sx = double(size) * scale.x, sy = double(size) * scale.y;
    const double nx = bitsD(0x3f9eb851f3333333ull), sideY = bitsD(0xbf689374b3333333ull);
    addSensor(0, {nx, -0.3}, 1.0f, {sx * double(-0.8f), sy * double(0.9f)});
    addSensor(1, {-nx, -0.3}, 1.0f, {sx * double(0.8f), sy * double(0.9f)});
    addSensor(2, {0.3, sideY}, 0.2f, {sx * double(-0.9f), sy * 0.0});
    addSensor(3, {-0.3, sideY}, 0.2f, {sx * double(0.9f), sy * double(0.2f)});
    addSensor(4, {0.0, 0.3}, 6.0f, {sx * 0.0, -sy});
    for (int i = 0; i < 4; ++i) {
        auto *gun = addGun({sx * double(kTaxiPositionX[i]), sy * double(kTaxiPositionY[i])},
                           kTaxiAimX[i], kTaxiAimY[i]);
        gun->disabled = kTaxiDisabled[i];
        if (i == 0)
            gun->jitter = 0.001f;
    }
    // Unread trailing fields: +0x248=0.0028f, +0x24c/+0x250=0.
}

void Taxi::run() { // 0x66fa8
    PlayState &G = *g;
    touchedDown = false;
    rot = float(double(rot) * 0.9);
    doControls();
    if (dying)
        return;
    Sensor *s0 = sensor(0);
    Sensor *s1 = sensor(1);
    if (!(s0->touching && s1->touching) && isLanded && vel.y < 0.0) {
        if (--landingTimer < 1) {
            isLanded = false;
            G.reset310 = false;
            G.landed = false;
        }
    }
    if (s0->touching && s1->touching && !s0->hazard && !s1->hazard && !isLanded &&
        double(std::fabs(rot)) < 0.001) {
        isLanded = true;
        launchHold = true;
        touchedDown = true;
        landingTimer = 15;
        G.landed = true;
        G.padCell[0] = s0->cell[0];
        G.padCell[1] = s0->cell[1];
        vel = {0, 0};
        thrustTarget = 0;
        thrust = 0;
    }
    sinceContact++;
    impact = 0;
    bool reflect = false;
    for (Entity *c : children) {
        if (!c)
            continue;
        auto *s = static_cast<Sensor *>(c);
        s->shipLanded = isLanded;
        if (!(s->rated && s->touching))
            continue;
        sinceContact = 0;
        impact += s->impact;
        s->impact = 0;
        const Vec2 n = s->normal;
        if (isLanded || s->hazard)
            continue;
        const double ay = std::fabs(n.y), ax = std::fabs(n.x);
        if (!(ay <= ax) || ay == ax) {
            if (sgn(float(vel.y)) != sgn(float(n.y))) {
                mapPos.y = prevMapPos.y;
                vel.y = -(vel.y * ay);
                if (vel.y < 0.0)
                    reflect = true;
            }
            if (!isLanded) {
                vel.x = vel.x + n.x * 0.01;
                vel.y = vel.y + n.y * 0.01;
            }
        }
        if (ay < ax) {
            if (sgn(float(vel.x)) != sgn(float(n.x))) {
                vel.x = -(vel.x * ax);
                mapPos.x = prevMapPos.x;
            }
            if (!isLanded) {
                vel.x = vel.x + n.x * 0.01;
                vel.y = vel.y + n.y * 0.01;
            }
        }
    }
    if (sinceContact == 0 && !isLanded) {
        if (!G.muted)
            G.playSample(0x49, G.volume << 12);
        G.cleanRun = false;
        age = 0;
    }
    if (touchedDown)
        impact = impact * rateLanding();
    if (0 < impact && !(0.0f < G.fuel))
        G.shield = 0;
    G.shield = G.shield - float(impact) * damage;
    if (G.shield < 0.0f)
        G.shield = 0;
    if (reflect) {
        double d = 0;
        if (s0->touching) {
            if (!s1->touching)
                d = 1.0;
        } else if (s1->touching)
            d = -1.0;
        rot = float(double(rot) + d * vel.y * 10.0);
    }
    if (launchHold) {
        if (thrustHeld < 1)
            launchHold = false;
        else
            thrustHeld = 0;
    }
    if (isLanded)
        rotateHeld = 0;
    thrustTarget = thrustHeld == 0 ? 0.0f : gravity * 3.0f;
    const float speed = float(std::fabs(vel.y) * 2.1);
    float step = speed * speed;
    if (double(step) < 0.008)
        step = 0.008f;
    if (isLanded || sinceContact < 10)
        step = 0.05f;
    float t = thrust;
    if (t < thrustTarget) {
        t = float(double(t) + double(step) * 0.05);
        thrust = t;
    }
    if (thrustTarget < t) {
        t = float(double(t) - 0.00025);
        thrust = t;
    } // 0x67a98: vsub.f64, then narrow
    if (t < 0.0f)
        thrust = 0;
    if (0 < thrustHeld) {
        vel.y = vel.y - double(thrust);
        G.fuel = G.fuel - fuelPerThrust;
    }
    if (G.fuel < 0.0f) {
        G.fuel = 0;
        G.sample(0x44);
    }
    if (vel.y < -0.3)
        vel.y = -0.3;
    else if (0.3 < vel.y)
        vel.y = 0.3;
    float steering = 0;
    if (rotateHeld != 0) {
        const int i = rotateSlot & 7;
        float f = float((G.touchNowX[i] - G.touchStartX[i]) * 0.19);
        if (G.touchFlag[i] && sgn(float(G.touchDeltaX[i])) != sgn(f) &&
            0.006 < std::fabs(G.touchDeltaX[i])) {
            G.touchNowX[i] = G.touchStartX[i];
            f = -f;
        }
        if (!analogDrive && sgn(f) != sgn(float(vel.x)))
            f = f + f;
        // The steering increment is stored as binary32 before the velocity add.
        steering = float(double(-f) * 0.045);
        vel.x = vel.x + double(steering);
        if (0.08 < vel.x)
            vel.x = 0.08;
        else if (vel.x < -0.08)
            vel.x = -0.08;
    }
    vel.x = vel.x * drag.x;
    prevMapPos = mapPos;
    mapPos.x = mapPos.x + vel.x * 5.0;
    if (!reflect && !isLanded)
        vel.y = vel.y + double(gravity);
    vel.y = vel.y * drag.y;
    if (thrustHeld == 0 && vel.y < 0.0)
        vel.y = vel.y * drag.y * 0.98;
    mapPos.y = mapPos.y + vel.y;
    if (0.01 < vel.y)
        G.scrollSpeed = -0.25;
    if (vel.y < -0.01 || G.flag203808)
        G.scrollSpeed = 0.15;
    pos.x = mapPos.x / 20.0 - 0.5;
    pos.y = G.screenY;
    const double oldCamera = G.cameraY;
    G.cameraY = (mapPos.y - 12.5) + G.screenY * -25.0;
    if (G.cameraY < 0.0) {
        pos.y = pos.y + G.cameraY / 25.0;
        G.cameraY = 0;
        if (pos.y < -0.48) {
            pos.y = -0.48;
            if (vel.y < 0.0)
                vel.y = 0;
            thrustTarget = 0;
            thrust = 0;
        }
        if (G.flag203808) {
            G.setMessage(1, 300, 0xe, 2, kGoMsg[4]);
            G.setMessage(3, 0x140, 0x10, 2, kGoMsg[5]);
        }
    }
    if (G.scrollLimit < G.cameraY) {
        pos.y = pos.y + (G.cameraY - G.scrollLimit) / 25.0;
        G.cameraY = G.scrollLimit;
    }
    G.cameraStep = G.cameraY - double(float(oldCamera));
    G.scrollDelta.y = G.cameraStep / 25.0;
    if (0.0f < thrustTarget && (G.frameCount & 1)) {
        auto *p = new ThrustParticle;
        p->pos = {pos.x, pos.y + 0.02};
        p->vel.x = 0;
        const double lo = double(float(double(thrustTarget + 1.0f) * 0.004));
        p->vel.y = G.rnd(lo, lo * 1.4);
        p->size = 0.02f;
        p->growth = float(G.rnd(1.03, 1.05));
        p->collides = true;
        p->fade = float(G.rnd(0.02, 0.03));
        p->init(G);
        G.addEntity(G.debris, p);
    }
    if (0.0 < double(std::fabs(steering))) {
        scale.x = steering < 0.0f ? -1.6 : 1.6;
        gunFacing = steering < 0.0f ? -1.0f : 1.0f;
    }
    G.touch(*this, G.touchList);
    const Vec2 savedPos = pos, savedVel = vel;
    G.bounceOff(*this, G.bounceList);
    if (dead) {
        dead = false;
        const Vec2 delta{pos.x - savedPos.x, pos.y - savedPos.y};
        if (float(std::sqrt(delta.y * delta.y + delta.x * delta.x)) < size) {
            const double dx = vel.x - savedVel.x;
            vel.x = savedVel.x + (dx + dx) * 0.5;
            vel.y = savedVel.y + (vel.y - savedVel.y) * 2.5 * 0.5;
            mapPos.x = mapPos.x + (pos.x - savedPos.x) * 20.0;
            mapPos.y = mapPos.y + (pos.y - savedPos.y) * 25.0;
        }
        pos = savedPos;
        G.spark(0x14, pos, {-G.bounceNormal.x, -G.bounceNormal.y});
    }
}
void AsteroidsSpaceship::init(PlayState &playState) { // 0x60cf8
    initCommon(playState);
    spriteIndex = 11;
    size = 0.02f;
    scale = {1.3, 1.7};
    vel = {0, 0};
    drag = {0.99, 0.999};
    gravity = 0.0002f;
    idleFuel = 0.0002f;
    fuelPerThrust = 0.0002f;
    damage = 0.0033333334f;
    col.unpackRGBA(0xc0ffffff);
    const double sx = double(size) * scale.x, sy = double(size) * scale.y;
    const double nx = bitsD(0x3fb70a3d80000000ull), sideY = bitsD(0xbf689374b3333333ull);
    addSensor(0, {nx, -0.3}, 4.0f, {-sx, sy});
    addSensor(1, {-nx, -0.3}, 4.0f, {sx, sy});
    addSensor(2, {0.3, sideY}, 2.0f, {-sx, sy * double(0.9f)});
    addSensor(3, {-0.3, sideY}, 2.0f, {sx, sy * double(0.9f)});
    addSensor(4, {0.0, 0.3}, 6.0f, {sx * 0.0, -sy});
    for (int i = 0; i < 5; ++i) {
        auto *gun =
            addGun({sx * double(kAsteroidsPositionX[i]), sy * double(kAsteroidsPositionY[i])},
                   kAsteroidsAimX[i], kAsteroidsAimY[i]);
        gun->burst = 10;
        gun->left = 10;
        gun->reload = 5;
        gun->word1a4 = 1;
        gun->disabled = kAsteroidsDisabled[i];
    }
    // Unread trailing fields: +0x248=0.0039999997f, +0x24c/+0x250=0.
}

void AsteroidsSpaceship::run() { // 0x75b64
    PlayState &G = *g;
    touchedDown = false;
    doControls();
    if (dying)
        return;
    Sensor *s0 = sensor(0);
    Sensor *s1 = sensor(1);
    if (!(s0->touching && s1->touching) && isLanded) {
        if (--landingTimer < 1) {
            isLanded = false;
            G.reset310 = false;
            G.landed = false;
        }
    }
    if (s0->touching && s1->touching && !s0->hazard && !s1->hazard) {
        if (!isLanded) {
            isLanded = true;
            touchedDown = true;
            launchHold = true;
            landingTimer = 15;
            G.landed = true;
            G.padCell[0] = s0->cell[0];
            G.padCell[1] = s0->cell[1];
            if (2.8 < double(std::fabs(rot)) && !G.muted && G.awardEligibilityGate < 2 &&
                !(G.goalBits & 1))
                G.achieve(1, 4, "'BATTY'");
            thrustTarget = 0;
            thrust = 0;
            vel = {0, 0};
        }
        if (thrustHeld < 1)
            vel.y = 0;
    }
    if (double(std::fabs(rot)) < 0.2 && 0.0 < vel.y && vel.y < 0.5)
        rot = float(double(rot) * 0.9);
    Vec2 contactPush{0, 0};
    impact = 0;
    sinceContact++;
    for (Entity *c : children) {
        if (!c)
            continue;
        auto *s = static_cast<Sensor *>(c);
        s->shipLanded = isLanded;
        if (!(s->rated && s->touching))
            continue;
        sinceContact = 0;
        impact += s->impact;
        s->impact = 0;
        if (isLanded || s->hazard)
            continue;
        // The last safe sensor replaces the impulse; it is not accumulated.
        neon::vec2d n{s->normal.x * kAsteroidsContactPush, s->normal.y * kAsteroidsContactPush};
        n.rotateBy(double(rot));
        contactPush = {n.x, n.y};
        mapPos.x = mapPos.x + contactPush.x;
        mapPos.y = mapPos.y + contactPush.y;
        if (std::sqrt(vel.y * vel.y + vel.x * vel.x) < 0.05 && double(std::fabs(rot)) < 0.8)
            rot = float(double(rot) * 0.7);
        vel = {0, 0};
    }
    if (!isLanded) {
        vel.x = vel.x + contactPush.x;
        vel.y = vel.y + contactPush.y;
    }
    if (touchedDown)
        impact = impact * rateLanding();
    if (0 < impact && !(0.0f < G.fuel))
        G.shield = 0;
    G.shield = G.shield - float(impact) * damage;
    if (G.shield < 0.0f)
        G.shield = 0;
    if (sinceContact == 0 && !isLanded) {
        if (!G.muted)
            G.playSample(0x19, G.volume << 11);
        G.cleanRun = false;
        age = 0;
    }
    if (launchHold) {
        if (thrustHeld < 1)
            launchHold = false;
        else
            thrustHeld = 0;
    }
    if (isLanded)
        rotateHeld = 0;
    thrustTarget = thrustHeld == 0 ? 0.0f : gravity * 3.0f;
    float t = thrust;
    if (t < thrustTarget) {
        t = float(double(t) + kAsteroidsThrustRise);
        thrust = t;
    }
    if (thrustTarget < t) {
        t = float(double(t) - kThrustFall);
        thrust = t;
    }
    if (t < 0.0f)
        thrust = 0;
    neon::vec2d exhaust{0, 0};
    if (0 < thrustHeld) {
        exhaust.y = double(thrust);
        G.fuel = G.fuel - fuelPerThrust;
    }
    if (G.fuel < 0.0f) {
        G.fuel = 0;
        G.sample(0x44);
    }
    exhaust.rotateBy(double(rot));
    vel.y = vel.y - exhaust.y;
    vel.x = vel.x - exhaust.x;
    if (rotateHeld != 0) {
        const int i = rotateSlot & 7;
        float f = float((G.touchNowX[i] - G.touchStartX[i]) * 0.19);
        if (G.touchFlag[i] && sgn(float(G.touchDeltaX[i])) != sgn(f) &&
            0.006 < std::fabs(G.touchDeltaX[i])) {
            G.touchNowX[i] = G.touchStartX[i];
            f = -f;
        }
        if (!analogDrive && sgn(f) != sgn(float(vel.x)))
            f = f + f;
        float turn = double(std::fabs(f)) < 0.003 ? -0.0f : f < 0.0f ? 0.015f : -0.015f;
        if (!G.iPad) {
            turn = turn + turn;
            rot = rot + turn;
        } else
            rot = rot + turn * 5.0f;
        if (rot < -3.1415927f)
            rot = rot + 6.2831855f;
        if (3.1415927f < rot)
            rot = rot - 6.2831855f;
    }
    prevMapPos = mapPos;
    vel.x = vel.x * drag.x;
    mapPos.x = mapPos.x + vel.x * 3.0;
    if (!isLanded)
        vel.y = vel.y + double(gravity);
    vel.y = vel.y * drag.y;
    mapPos.y = mapPos.y + vel.y * 3.0 * 1.25;
    pos.x = mapPos.x / 20.0 - 0.5;
    pos.y = G.screenY;
    const double oldCamera = G.cameraY;
    G.cameraY = (mapPos.y - 12.5) + G.screenY * -25.0;
    if (G.cameraY < 0.0) {
        pos.y = pos.y + G.cameraY / 25.0;
        G.cameraY = 0;
        if (pos.y < -0.48) {
            pos.y = -0.48;
            if (vel.y < 0.0)
                vel.y = 0;
            thrustTarget = 0;
            thrust = 0;
        }
        if (G.flag203808) {
            G.setMessage(1, 300, 0xe, 2, kGoMsg[4]);
            G.setMessage(3, 0x140, 0x10, 2, kGoMsg[5]);
        }
    }
    if (G.scrollLimit < G.cameraY) {
        pos.y = pos.y + (G.cameraY - G.scrollLimit) / 25.0;
        G.cameraY = G.scrollLimit;
    }
    G.cameraStep = G.cameraY - double(float(oldCamera));
    G.scrollDelta.y = G.cameraStep / 25.0;
    if (0.0f < thrustTarget * 100.0f && (G.frameCount & 1)) {
        auto *p = new ThrustParticle;
        p->pos = pos;
        p->vel = {exhaust.x * 5.0, exhaust.y * 5.0};
        p->size = 0.02f;
        p->growth = float(G.rnd(1.03, 1.05));
        p->collides = true;
        p->fade = float(G.rnd(0.02, 0.03));
        p->init(G);
        G.addEntity(G.debris, p);
    }
    G.touch(*this, G.touchList);
    const Vec2 savedPos = pos, savedVel = vel;
    G.bounceOff(*this, G.bounceList);
    // Unlike the landers, this path always applies the short-displacement response
    // and clears dead; it does not emit a collision spark.
    dead = false;
    const Vec2 delta{pos.x - savedPos.x, pos.y - savedPos.y};
    if (float(std::sqrt(delta.y * delta.y + delta.x * delta.x)) < size) {
        const double dx = vel.x - savedVel.x;
        vel.x = savedVel.x + (dx + dx) * 0.5;
        vel.y = savedVel.y + (vel.y - savedVel.y) * 2.5 * 0.5;
        mapPos.x = mapPos.x + (pos.x - savedPos.x) * 20.0;
        mapPos.y = mapPos.y + (pos.y - savedPos.y) * 25.0;
    }
    pos = savedPos;
}
} // namespace minos
