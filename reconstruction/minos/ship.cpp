// Minos player ship family. State and interfaces are in ship.h; translated from the
// decompilation; float/double boundaries follow the original instructions where checked
// (comments name the binary32 steps). Offsets are the original's.
#include "neon/particles.h"
#include "ship.h"
#include "neon/entity_list.h"
#include "neon/entity_contact.h"
#include <cmath>
#include <cstring>

namespace minos {
namespace {
#include "ship_data.inc"

using neon::vec2d;
inline vec2d V(Vec2 v) {
    return {v.x, v.y};
}
inline Vec2 W(vec2d v) {
    return {v.x, v.y};
}
inline int sgn(float x) {
    return x < 0.0f || std::isnan(x) ? -1 : 1;
} // Neon_Internals::sgn
inline Vec2 rotated(Vec2 v, double a) {
    vec2d r = V(v);
    r.rotateBy(a);
    return W(r);
}
inline double length(Vec2 v) {
    return std::sqrt(v.y * v.y + v.x * v.x);
}
// vcvt.s32.f32 / .f64 (round toward zero, saturating)
inline int32_t toInt(double v) {
    if (std::isnan(v))
        return 0;
    if (v >= 2147483647.0)
        return INT32_MAX;
    if (v <= -2147483648.0)
        return INT32_MIN;
    return int32_t(v);
}

// The visible tile at a screen position, as sensor::run / thrustParticle::run / shot::run
// index it: column (x + 0.5) * columns - fraction.x, line (y + 0.5) * rows - fraction.y,
// stride columns + 1. Out-of-buffer reads use an explicit host safety guard.
int32_t tileAt(const cavern::StripMap &m, int col, int row) {
    const long i = long(row) * (m.columns + 1) + col;
    if (i < 0 || i >= long(m.tiles.size()))
        return 0;
    return m.tiles[size_t(i)];
}
} // namespace

// ------------------------------------------------------------------ theGame parts
void PlayState::addEntity(Entity *&list, Entity *e) {
    neon::entities::prepend(list, *e);
}

void PlayState::deleteList(Entity *&list) {
    neon::entities::destroyList(list, [this](Entity *e) { dispose(e); });
}

void PlayState::purgeTouches() { // controlBlock::purgeTouches 0xddd8
    for (int i = 0; i < 4; i++) {
        touchPressed[i] = false;
        touchDown[i] = false;
    } // +0xf8.., +0x16c..
}

void PlayState::runDebris() {
    neon::entities::runAndCull(debris, [this](Entity *e) { dispose(e); });
}

// controlBlock::collide 0xedc8, translated here (instruction-checked) rather than through the
// shared engine's collideEarly: the original forms the opposite angle as
// float(double(ang) + 3.141592654) (binary64 pool constant; Minotron's original does the
// same), where the engine adds 3.1415927f in binary32 (Engine ownership contract).
// Every other step matches the engine's loop. The return value is not used by any caller.
int PlayState::collide(Entity &a, Entity *list, bool bounce) {
    if (a.drawDelay != 0 || a.dead || !list)
        return 0;
    const float ra = a.size, ma = a.mass;
    const double ax = a.pos.x, ay = a.pos.y; // read once, before the loop (d10/d11)
    int result = 0;
    bool reject = false; // r8: set by a same group or a rejected target
    for (Entity *b = list; b; b = b->next) {
        if (b == &a || b->dead)
            continue;
        const float sumR = ra + b->size;
        const double dx = ax - b->pos.x, dy = ay - b->pos.y;
        const float dist = float(std::sqrt(dy * dy + dx * dx));
        if (sumR <= dist) {
            a.touching = false;
            continue;
        }
        if (a.group > 0 && a.group == b->group)
            reject = true;
        if (a.touching || b->drawDelay != 0 || !b->collisionEnabled) {
            reject = true;
            continue;
        }
        if (reject)
            continue;
        a.hit = true;
        if (a.flags & 3)
            b->wasShot(&a);
        b->hitBy(&a);
        result = 1;
        if (bounce) {
            const float mt = ma + b->mass;
            const float overlap = sumR - dist;
            const float ang = float(std::atan2(b->pos.y - ay, b->pos.x - ax));
            const float pushA = overlap * (b->mass / mt);
            const float pushB = overlap * (ma / mt);
            const float opp = float(double(ang) + 3.141592654);
            a.pos.x = a.pos.x + double(std::cos(opp) * pushA);
            a.pos.y = a.pos.y + double(std::sin(opp) * pushA);
            b->pos.x = b->pos.x + double(std::cos(ang) * pushB);
            b->pos.y = b->pos.y + double(std::sin(ang) * pushB);
            const float c = std::cos(ang), sn = std::sin(ang);
            const double dc = double(c), ds = double(sn);
            const float va = float(ds * a.vel.y + a.vel.x * dc);
            const float vb = float(ds * b->vel.y + dc * b->vel.x);
            const float k = (va - vb + (va - vb)) / mt;
            const float ka = b->mass * k;
            a.vel.x = a.vel.x - double(c * ka * a.elasticity);
            a.vel.y = a.vel.y - double(sn * ka * a.elasticity);
            const float kb = k * ma;
            b->vel.x = b->vel.x + double(c * kb * b->elasticity);
            b->vel.y = b->vel.y + double(sn * kb * b->elasticity);
        }
        contactPos = b->pos; // +0x2c0, then the unit normal +0x2b0
        const double nx = b->pos.x - a.pos.x, ny = b->pos.y - a.pos.y;
        const double l = std::sqrt(ny * ny + nx * nx);
        bounceNormal = !(l > 0.0) ? Vec2{0, 0} : Vec2{nx / l, ny / l}; // ble: also unordered
    }
    return result;
}

void PlayState::runShots() {
    neon::entities::runAndCull(shots, [this](Entity *e) { dispose(e); });
}

void PlayState::spark(int n, Vec2 pos, Vec2 vel) { // theGame::spark 0x5f5cc
    for (int i = 0; i < n; i++) {
        const double s = rnd(0.004, 0.05);
        Vec2 v{vel.x * s, vel.y * s};
        v = rotated(v, rnd(-0.3, 0.3));
        auto *b = new Bspark;
        b->vel = v;
        b->ageRate = float(rnd(0.001, 0.006));
        b->exploChance = 0.3f; // 0x3e99999a
        b->pos = pos;
        b->rot = float(rnd(-kPi, kPi));
        b->spriteIndex = 3;
        b->g = this;
        b->size = 0.02f;
        b->colourIndex = 1;
        b->maxScale = 0.6f;
        addEntity(debris, b);
    }
}

void PlayState::checkKlaxons() { // theGame::checkKlaxons 0x5e05c
    float f;
    if (!fuelWarn) {
        if (fuel < 0.3) {
            fuelWarn = true;
            fuelTimer = 0x50;
            sample(0x2d);
        }
    } else {
        if (!fuelCritical) {
            f = fuel;
            if (f < 0.1) {
                fuelCritical = true;
                sample(0x2e);
                f = fuel;
            }
        } else {
            f = fuel;
            if (0.1 < f)
                fuelCritical = false;
        }
        if (0.3 < f)
            fuelWarn = false;
        else if (--fuelTimer < 1) {
            sample(fuelCritical ? 0x2f : 0x2a);
            fuelTimer = 0x50;
        }
    }
    if (shieldWarn) {
        if (0.3 < shield) {
            shieldWarn = false;
            return;
        }
        if (--shieldTimer > 0)
            return;
        sample(0x32);
        shieldTimer = 0x78;
        return;
    }
    if (0.3 <= shield)
        return;
    shieldWarn = true;
    shieldTimer = 0x78;
    sample(0x2c);
}

// ------------------------------------------------------------------ sensor
void Sensor::run() { // 0x5eec0
    rot = float(double(rot) + 0.1);
    rotated = minos::rotated(offset, double(ship->rot));
    pos = {ship->pos.x + rotated.x, ship->pos.y + rotated.y};
    if (ship->type == 3) {
        shipLanded = true;
        impact = 0;
        return;
    }
    const cavern::StripMap &m = *g->map;
    const float c = float((pos.x + 0.5) * double(float(m.columns)) - m.fraction.x);
    touching = false;
    hazard = false;
    bool collisionResult = false;
    if (c > -1.0f) {
        const float r = float((pos.y + 0.5) * double(float(m.rows)) - m.fraction.y);
        if (r > -1.0f) {
            const int ic = toInt(c), ir = toInt(r);
            cell[0] = ic;
            cell[1] = ir;
            const int32_t code = tileAt(m, ic, ir);
            if (code < 0)
                hazard = true;
            const uint32_t low = uint32_t(code) & 0xff;
            const int32_t blk = low < uint32_t(kTransBlockCount)
                                    ? kTransBlock[low]
                                    : 0; // Unmapped characters have no collision block.
            if (blk == -1)
                collisionResult = true;
            else if (blk > 0) {
                int ix = toInt(float((c - float(ic)) * 16.0f));
                if (kFlipX[low])
                    ix = 15 - ix;
                int iy = toInt(float((r - float(ir)) * 15.0f));
                if (kFlipY[low])
                    iy = 14 - iy;
                const int rowIndex = blk * 16 + iy - 16;
                if (rowIndex >= 0 && rowIndex < 16 && ix >= 0 && ix < 16 &&
                    kColBlockRows[rowIndex][ix] == '*')
                    collisionResult = true;
            }
        }
    }
    if (!collisionResult) {
        sparkDecay = 1.0f;
        return;
    }
    touching = true;
    if (!hazard) {
        const Vec2 n = minos::rotated(normal, double(ship->rot));
        Vec2 v = ship->vel;
        v.x = sgn(float(v.x)) == sgn(float(n.x)) ? 0.0 : v.x * std::fabs(n.x);
        v.y = sgn(float(v.y)) == sgn(float(n.y)) ? 0.0 : v.y * std::fabs(n.y);
        impact = toInt(length(v) * 1000.0 * double(weight));
    } else {
        impact = 10;
    }
    if (!shipLanded) {
        sparkDecay = float(double(sparkDecay) * 0.8);
        const int k = impact / 5;
        for (int i = 0; i < k + 1; i++) {
            const double s = g->rnd(0.004, 0.05);
            Vec2 v{normal.x * s, normal.y * s};
            v = minos::rotated(v, g->rnd(-0.3, 0.3));
            auto *b = new Bspark;
            b->vel = v;
            b->ageRate = float(g->rnd(0.001, 0.006));
            b->exploChance = 0;
            b->pos = pos;
            b->rot = float(g->rnd(-kPi, kPi));
            b->age = 0;
            b->spriteIndex = 3;
            b->colourIndex = 1;
            b->g = g;
            b->size = 0.02f;
            b->maxScale = 0.6f;
            g->addEntity(g->debris, b);
        }
    }
}

// ------------------------------------------------------------------ gnu, shot
void Gun::run() { // 0x59ad8
    if (ship->type == 3 || g->landed)
        return;
    rotated = offset;
    rotated.x = rotated.x * double(ship->gunFacing);
    rotated = minos::rotated(rotated, double(ship->rot));
    pos = {ship->pos.x + rotated.x, ship->pos.y + rotated.y};
    if (reloadLeft < 1) {
        if (left < burst) {
            if (wait > 0) {
                wait--;
                return;
            }
            fire();
            if (left < 0)
                reloadLeft = reload;
        } else if (!g->paused && (word1a4 < 0 || g->byte31e || !disabled || g->flag203808)) {
            fire();
        }
    } else if (--reloadLeft == 0) {
        wait = 0;
        left = burst;
    }
}

void Gun::fire() { // 0x598f0
    left--;
    wait = interval;
    flash = 1.0f;
    auto *s = new Shot;
    s->pos = pos;
    const float vx = spread * speed; // binary32
    s->vel.x = double(vx) + g->rnd(-double(jitter), double(jitter));
    s->vel.y = double(float(dirY * speed));
    s->vel = minos::rotated(s->vel, double(ship->rot));
    s->g = g;
    s->spriteIndex = 3;
    s->col.white(1.0);
    s->size = 0.009f; // 0x3c1374bc
    s->grow = 1;
    g->addEntity(g->shots, s);
}

int Gun::draw(EntityRenderer &r, Vec2 screen) { // 0x5405c
    if (0.1 < double(flash)) {
        const float s = flash * 3.0f;
        scale = {double(s), double(s)};
        r.drawEntity(*this, screen);
        flash = float(double(flash) * 0.7);
    }
    return 0;
}

void Shot::run() {                  // 0x71544
    g->touch(*this, g->bounceList); // controlBlock::touch(this, +0x2e8)
    grow = toInt(double(float(grow)) * 0.9);
    col = g->colours[1];
    if (double(float(grow)) < 0.1)
        grow++;
    const float sc = float(grow << 1);
    scale = {double(sc), double(sc)};
    pos.x = pos.x + double(g->shotDrift) * 0.03;
    pos = {pos.x + vel.x, pos.y + vel.y};
    if (pos.y < -0.6)
        dead = true;
    if (0.6 < pos.y)
        dead = true;
    if (pos.x < -0.6)
        dead = true;
    if (pos.x > 0.6)
        dead = true;
    cavern::StripMap &m = *g->map;
    const int particleColour =
        toInt((double(float(pos.x)) + 0.5) * double(float(m.columns)) - m.fraction.x);
    if (particleColour < 0 || particleColour > m.columns)
        return;
    const int row = toInt((double(float(pos.y)) + 0.5) * double(float(m.rows)) - m.fraction.y);
    if (row < 0 || row > m.rows || tileAt(m, particleColour, row) <= 0)
        return;
    auto *e = new Explo;
    e->from = 0.5f;
    e->to = 2.0f;
    e->t = 0;
    e->rate = 0.06f;
    e->spriteIndex = 4;
    e->pos = pos;
    e->rot = float(g->rnd(-kPi, kPi));
    e->baseScale = {1.0, 1.0};
    e->g = g;
    g->addEntity(g->debris, e);
    dead = true;
    const int mr = toInt(m.scroll.y + (double(float(pos.y)) + 0.5) * double(float(m.rows)));
    const int mc = toInt(m.scroll.x + (double(float(pos.x)) + 0.5) * double(float(m.columns)));
    const long ci = long(mr) * cavern::StripMap::kMapColumns + mc;
    uint32_t *cell = ci >= 0 && ci < long(m.cells.size()) ? &m.cells[size_t(ci)] : nullptr;
    if (cell && (*cell & 0x200)) {
        g->sample(0x34);
        *cell = (*cell & 0xfffffcffu) | 0x100;
    }
}

// ------------------------------------------------------------------ particles
void ThrustParticle::init(PlayState &playState) { // 0x56cd8
    g = &playState;
    spriteIndex = 4;
    spin = float(vel.y * 3.0);
    if (g->libcRand() & 1)
        spin = -spin;
    vel = rotated(vel, g->rnd(-0.2, 0.2));
    col.white(1.0);
    life = 1.0f;
    scale = {1.0, 1.0};
    lastTile = 0x3039;
    fall = 0;
    hot = false;
}

void ThrustParticle::run() { // 0x5a5ac
    if (g->paused)
        return;
    pos = {pos.x + vel.x, pos.y + vel.y};
    pos.y = pos.y - g->scrollDelta.y;
    vel.y = vel.y + double(fall);
    if (!hot)
        col.a = double(life) * 0.4;
    else
        col = g->colours[1];
    rot = rot + spin;
    if (scale.x < 4.0)
        scale = {scale.x * double(growth), scale.y * double(growth)};
    if (collides) {
        const cavern::StripMap &m = *g->map;
        const double y = pos.y;
        const int particleColour =
            toInt((double(float(pos.x)) + 0.5) * double(float(m.columns)) - m.fraction.x);
        int32_t word = -1;
        int row = -1;
        if (particleColour >= 0 && particleColour <= m.columns) {
            row = toInt((double(float(y)) + 0.5) * double(float(m.rows)) - m.fraction.y);
            if (row >= 0 && row <= m.rows)
                word = tileAt(m, particleColour, row);
        }
        if (int64_t(word) != lastTile) {
            lastTile = word;
            uint32_t idx = 0xff;
            if (particleColour >= 0 && particleColour <= m.columns && row >= 0 && row <= m.rows)
                idx = uint32_t(tileAt(m, particleColour, row)) & 0xff;
            // The original indexes its stack copy of stm::gang C.243 (31 words) with the tile's
            // low byte. Indices 31..46 read the adjacent stack copies of C.242 and C.241 (float
            // bits, deterministic); >= 47 read caller stack contents: not recoverable, taken
            // as negative (no bounce).
            int32_t slope = -1;
            if (idx < 31)
                slope = kGangSlope[idx];
            else if (idx < 39)
                std::memcpy(&slope, &kGangNormalB[idx - 31], 4);
            else if (idx < 47)
                std::memcpy(&slope, &kGangNormalA[idx - 39], 4);
            if (slope >= 0) {
                const int k = slope & 7;
                const float a = float(
                    std::atan2(double(-kGangNormalB[k]), double(kGangNormalA[k])) - kThrustHalfPi);
                if (a < 10.0f) {
                    const float b = float(std::atan2(-vel.y, vel.x) - kThrustHalfPi);
                    double turn = double(-(b - a));
                    turn = turn + g->rnd(-0.2, 0.2);
                    vel = rotated(vel, turn);
                    const double s = g->rnd(0.3, 0.9);
                    vel = {vel.x * s, vel.y * s};
                    scale = {scale.x * 1.1, scale.y * 1.1}; // 0x3ff199999999999a
                    spin = float(g->rnd(-0.1, 0.1));
                    if (fall <= 0.0f)
                        fall = 0.0005f;
                    if (!hot && (g->libcRand() & 0xff) < 2) {
                        hot = true;
                        vel = {vel.x * 2.0, vel.y * 2.0};
                        scale = {1.0, 1.0};
                        size = float(g->rnd(0.004, 0.0055));
                    }
                }
            }
        }
    }
    life = life - fade;
    if (life < 0.0f)
        dead = true;
}

void Bspark::run() { // 0x71194
    pos = {pos.x - g->scrollDelta.x, pos.y - g->scrollDelta.y};
    vel.y = vel.y + 0.0001;
    pos = {pos.x + vel.x, pos.y + vel.y};
    age = age + ageRate * 3.0f;
    neon::particles::retireProgress(age, dead, neon::particles::ProgressEnd::AboveAndClamp);
    const double s = g->rnd(double(maxScale) * 0.2, double(maxScale));
    scale = {s, s};
    col = g->colours[colourIndex & 7];
    col.a = double(1.0f - age);
    if (0.0f < exploChance && g->shipList && g->shipList->type == 3) {
        const double r = g->rnd(0.0, 1.0);
        if (r < double((1.0f - age) * exploChance)) {
            const float f = float(double(age) + 0.2);
            auto *e = new Explo;
            e->to = f + f;
            e->from = f;
            e->spriteIndex = 4;
            e->rate = float(double(f) * 0.06);
            e->pos = pos;
            e->rot = float(g->rnd(-kPi, kPi));
            e->t = 0;
            e->g = g;
            e->baseScale = {1.0, 1.0};
            g->addEntity(g->debris, e);
        }
    }
}

void Explo::run() { // 0x541b8
    if (g->paused)
        return;
    t = t + rate;
    const float span = to - from;
    pos = {pos.x - g->scrollDelta.x, pos.y - g->scrollDelta.y};
    float tt = t;
    neon::particles::retireProgress(tt, dead, neon::particles::ProgressEnd::AboveAndClamp);
    t = tt;
    const float s = from + span * tt;
    scale = {double(s) * baseScale.x, double(s) * baseScale.y};
    col.a = double(1.0f - tt);
}

// ------------------------------------------------------------------ ship
void Ship::initCommon(PlayState &playState) {
    g = &playState;
    type = 2;
    mapPos = {9.5, 10.0};
    dying = false;
    lastLanding = {-599.0, -599.0};
    sinceContact = 100;
    age = 0;
    deathTimer = 0;
    idleFuel = 0.0001f;
    gunFacing = 1.0f;
    hitDamage = kDamRate[g->shipKind & 3] * 0.0003125f;
    thrust = 0;
    shake.x = 0;
    isLanded = false;
    launchHold = false;
    for (auto &c : children)
        c = nullptr;
}

void Ship::addSensor(int i, Vec2 normal, float weight, Vec2 offset) {
    auto *s = new Sensor;
    s->g = g;
    s->ship = this;
    s->rated = true;
    s->normal = normal;
    s->weight = weight;
    s->offset = offset;
    s->spriteIndex = 0;
    s->size = 0.004f;
    s->touching = false;
    s->hazard = false;
    s->shipLanded = false;
    s->sparkDecay = 1.0f;
    children[i] = s;
    g->addEntity(g->debris, s);
}

Gun *Ship::addGun(Vec2 offset, float aimX, float aimY) {
    auto *gun = new Gun;
    gun->offset = offset;
    gun->spread = aimX;
    gun->dirY = aimY;
    gun->burst = 7;
    gun->speed = 0.01f;
    gun->interval = 4;
    gun->reload = 8;
    gun->left = 7;
    gun->wait = 0;
    gun->reloadLeft = 0;
    gun->flash = 0;
    gun->word1a4 = 0;
    gun->jitter = 0;
    gun->disabled = false;
    gun->ship = this;
    gun->g = g;
    gun->pos = {pos.x + offset.x, pos.y + offset.y};
    gun->spriteIndex = 4;
    gun->col.white(1.0);
    gun->size = 0.02f;
    g->addEntity(g->debris, gun);
    return gun;
}

void Ship::init(PlayState &playState) { // ship::init 0x62b70
    type = 2;
    g = &playState;
    mapPos = {9.5, 10.0};
    dying = false;
    spriteIndex = 1;
    size = 0.03f;
    scale.x = 2.0;
    drag = {0.92, 0.99};
    gravity = 0.003f;
    f164 = 0.006f;
    col.unpackRGBA(0x30c0ffff);
    thrust = 0;
    shake.x = 0;
    damage = 0.0003125f;
    isLanded = false;
    fuelPerThrust = 0.0004f;
    lastLanding = {-599.0, -599.0};
    sinceContact = 100;
    age = 0;
    deathTimer = 0;
    idleFuel = 0.0001f;
    gunFacing = 1.0f;
    launchHold = false;
    hitDamage = kDamRate[g->shipKind & 3] * 0.0003125f; // binary32 multiply
    for (auto &c : children)
        c = nullptr;

    struct S {
        double nx, ny;
        float weight;
        double ox, oy;
    };
    const double sx = double(size) * scale.x, sy = double(size) * scale.y;
    // (size * scale) * factor with the factor a binary32 literal widened (e.g. -0.8f)
    const S sensors[5] = {
        {0.09, -0.72, 0.2f, sx * double(-0.8f), sy * double(0.9f)},
        {-0.09, -0.72, 0.2f, sx * double(0.8f), sy * double(0.9f)},
        {0.72, 0.09, 3.0f, sx * double(-0.9f), sy * 0.0},
        {-0.72, 0.09, 3.0f, sx * double(0.9f), sy * 0.0},
        {0.0, 0.9, 3.0f, sx * 0.0, -sy},
    };
    // exact doubles of the constructor words (0x3fb70a3d76666667 etc.)
    auto bits = [](uint64_t u) {
        double d;
        std::memcpy(&d, &u, 8);
        return d;
    };
    const double n09 = bits(0x3fb70a3d76666667ull), n72 = bits(0x3fe70a3d76666667ull);
    const double nfix[5][2] = {
        {n09, -n72}, {-n09, -n72}, {n72, n09}, {-n72, n09}, {0.0, bits(0x3feccccccccccccdull)}};
    for (int i = 0; i < 5; i++) {
        auto *s = new Sensor;
        s->g = g;
        s->ship = this;
        s->rated = true;
        s->normal = {nfix[i][0], nfix[i][1]};
        s->weight = sensors[i].weight;
        s->offset = {sensors[i].ox, sensors[i].oy};
        s->spriteIndex = 0;
        s->size = 0.004f; // 0x3b83126f
        s->touching = false;
        s->hazard = false;
        s->shipLanded = false;
        s->sparkDecay = 1.0f;
        children[i] = s;
        g->addEntity(g->debris, s);
    }
    const double gx[3] = {sx * double(-0.9f), sx * double(0.9f), sx * 0.0};
    const double gy[3] = {sy * double(0.9f), sy * double(0.9f), -sy};
    for (int i = 0; i < 3; i++) {
        auto *gun = new Gun;
        gun->offset = {gx[i], gy[i]};
        gun->spread = 0;
        gun->dirY = i == 2 ? -1.0f : 1.0f;
        gun->burst = 7;
        gun->speed = 0.01f;
        gun->interval = 4;
        gun->reload = 8;
        gun->left = 7;
        gun->wait = 0;
        gun->reloadLeft = 0;
        gun->flash = 0;
        gun->word1a4 = 0;
        gun->jitter = 0;
        gun->disabled = false;
        gun->ship = this;
        gun->g = g;
        gun->pos = {pos.x + gun->offset.x, pos.y + gun->offset.y};
        gun->spriteIndex = 4;
        gun->col.white(1.0);
        gun->size = 0.02f;
        g->addEntity(g->debris, gun);
    }
}

void Ship::doControls() { // shentity::doControls 0x65134
    PlayState &G = *g;
    if (dying) {
        if (deathTimer > 0 && --deathTimer == 0) {
            G.lives--;
            if (G.lives < 1) {
                deathTimer = 50000;
                G.gameOver = true;
                return;
            }
            auto *s = static_cast<Ship *>(G.shipList);
            G.word478 = 0;
            G.byte442 = false;
            G.respawnedThisCavern = true;
            s->vel = {0, 0};
            s->mapPos = G.respawnPos;
            s->pos.x = 400.0;
            s->isLanded = G.respawnLanded;
            s->rot = G.respawnRot;
            G.fuel = 1.0f;
            G.shield = 1.0f;
            s->launchHold = true;
            s->dying = false;
            s->touchedDown = false;
            s->type = 2;
            s->landingTimer = 15;
            s->thrustTarget = 0;
            s->thrust = 0;
            G.scrollDelta = {0, 0};
            G.minosSaved = 0;
            G.landed = G.respawnLanded;
            G.rescueBonus = 0;
            for (Entity *c : s->children) {
                if (!c)
                    continue;
                auto *sn = static_cast<Sensor *>(c);
                sn->hazard = false;
                if (!G.respawnLanded)
                    sn->touching = false;
            }
            G.reset310 = true;
        }
        return;
    }
    if (0.0f < G.fuel) {
        G.fuel = G.fuel - idleFuel;
        if (G.fuel < 0.0f) {
            G.fuel = 0;
            G.sample(0x44);
        }
    }
    G.checkKlaxons();
    if (!isLanded)
        airborne++;
    else
        airborne = 0;
    secondTouch = -1;
    thrustSlot = -1;
    rotateSlot = -1;
    touches = 0;
    thrustHeld = 0;
    rotateHeld = 0;
    age++;
    const bool phone = !G.iPad;
    auto rotateSide = [&](int i) { // true: this touch rotates
        if (phone)
            return 0.0 < G.touchPos[i][1];
        if ((G.prefsControl & 1) == 0)
            return G.touchPos[i][0] < double(0.2f);
        return -0.2 < G.touchPos[i][0];
    };
    if (G.touchDown[0] && (!G.updatedMenus || G.touchGameplayEnabled)) {
        secondTouch = 0;
        touches = 1;
        if (rotateSide(0)) {
            rotateSlot = 0;
            rotateHeld++;
        } else {
            thrustSlot = 0;
            thrustHeld = phone ? 1 : thrustHeld + 1;
        }
    }
    if (G.touchDown[1] && (!G.updatedMenus || G.touchGameplayEnabled)) {
        if (touches < 1)
            secondTouch = 1;
        touches++;
        if (rotateSide(1)) {
            if (rotateHeld < 1)
                rotateSlot = 1;
            rotateHeld++;
        } else {
            if (thrustHeld < 1)
                thrustSlot = 1;
            thrustHeld++;
        }
    }
    analogDrive = false;
    if (G.prefsControl == 0x16 && G.physicalTiltEnabled()) {
        const float j = float(G.lateralTilt * 0.5);
        if (0.01f < std::fabs(j)) {
            rotateSlot = 3;
            rotateHeld = 1;
            G.touchStartX[3] = double(j);
            G.touchNowX[3] = 0;
            G.touchFlag[3] = false;
            analogDrive = true;
        }
    }
    if (0.01 < std::fabs(G.analog)) {
        rotateSlot = 3;
        rotateHeld = 1;
        G.touchStartX[3] = G.shipKind == 3 ? G.analog * -0.2 : G.analog * 0.1;
        G.touchNowX[3] = 0;
        G.touchFlag[3] = false;
    }
    if (G.prefsButtons & 0xff0) {
        thrustSlot = 3;
        thrustHeld = 1;
    }
    if (G.fuel == 0.0f) {
        G.shield = float(double(G.shield) - 0.005);
        if (G.shield < 0.0f)
            G.shield = 0;
        thrustHeld = 0;
    }
    if (G.shield <= 0.0f) {
        dying = true;
        G.reset310 = true;
        type = 3;
        deathTimer = 100;
        G.scrollDelta = {0, 0};
        auto *e = new Explo;
        e->from = 2.0f;
        e->to = 40.0f;
        e->rate = 0.026f; // 0x40000000, 0x42200000, 0x3cd4fdf4
        e->spriteIndex = 4;
        e->pos = pos;
        e->rot = float(G.rnd(-kPi, kPi));
        e->t = 0;
        e->baseScale = {1.0, 1.0};
        e->g = g;
        G.addEntity(G.debris, e);
        G.sample(0x1e);
        G.cleanRun = false;
        G.sample(0x1f);
        G.purgeTouches();
        G.clearAll = 100;
        G.deleteList(G.list2ec);
        if ((G.libcRand() & 3) == 0)
            G.sample(0x30);
        for (int i = 0; i < 50; i++) {
            auto *b = new Bspark;
            b->vel.y = G.rnd(-0.02, -0.005);
            b->vel.x = G.rnd(-0.01, 0.01);
            b->ageRate = float(G.rnd(0.001, 0.016));
            b->exploChance = 0.5f;
            b->pos = pos;
            b->rot = float(G.rnd(-kPi, kPi));
            b->age = 0;
            b->spriteIndex = 3;
            b->maxScale = 1.0f;
            b->colourIndex = 1;
            b->g = g;
            b->size = 0.02f;
            G.addEntity(G.debris, b);
        }
    }
    if (0 < thrustHeld && !isLanded && (G.libcRand() & 7) == 0 && !G.muted)
        G.playSample(0x16, G.volume * 0x640);
}

int Ship::rateLanding() { // shentity::rateLanding 0x5e374
    PlayState &G = *g;
    G.bonusPending = false;
    const Vec2 d{lastLanding.x - mapPos.x, lastLanding.y - mapPos.y};
    lastLanding = mapPos;
    G.sample(0x2b);
    G.respawnPos = mapPos;
    G.respawnLanded = true;
    G.respawnRot = rot;
    G.landingBonus = 0;
    const int q = impact / 10;
    if (q < 5 && 29 < age) {
        if (length(d) < 3.0)
            return 1;
        G.bonusPos = pos;
        G.landingBonus = 5 - q;
        G.bonusPending = true;
        return 0;
    }
    G.cleanRun = false;
    if (!G.muted) {
        G.playSample(0x20, G.volume << 12);
        G.sample(0x34);
    }
    G.perfectLandingStreak = 0;
    return 1;
}

int Ship::draw(EntityRenderer &r, Vec2 screen) { // shentity::draw 0x55194
    if (dying)
        return 0;
    const Vec2 saved = pos;
    const float sh = g->shield;
    if (double(sh) < 0.1) {
        const float k = sh * -10.0f + 1.0f; // binary32
        pos = {pos.x + shake.x * double(k), pos.y + shake.y * double(k)};
    }
    r.drawEntity(*this, screen);
    if ((g->frameCount & 1) == 0) {
        shake.x = g->rnd(-0.005, 0.005);
        shake.y = g->rnd(-0.005, 0.005);
    }
    pos = saved;
    return 0;
}

void Ship::run() { // ship::run 0x68118
    PlayState &G = *g;
    touchedDown = false;
    rot = float(double(rot) * 0.9);
    doControls();
    if (dying)
        return;
    Sensor *s0 = sensor(0);
    Sensor *s1 = sensor(1);
    if (!(s0->touching && s1->touching)) {
        if (isLanded && vel.y < 0.0) {
            if (--landingTimer < 1) {
                isLanded = false;
                G.reset310 = false;
                G.landed = isLanded;
            }
        }
    }
    if (s0->touching && s1->touching) {
        if (!s0->hazard && !s1->hazard) {
            if (!isLanded && double(std::fabs(rot)) < 0.001) {
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
            if (thrustHeld < 1 || launchHold)
                vel.y = 0;
        }
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
        if (!(ay <= ax) || ay == ax) { // !(ay < ax) incl. unordered
            if (sgn(float(vel.y)) != sgn(float(n.y))) {
                const double vy = vel.y;
                mapPos.y = prevMapPos.y;
                vel.y = -(vy * ay);
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
    if (touchedDown)
        impact = impact * rateLanding();
    if (sinceContact == 0 && !isLanded) {
        if (!G.muted)
            G.playSample(0x17, G.volume << 13);
        G.cleanRun = false;
        age = 0;
    }
    if (0 < impact && !(0.0f < G.fuel))
        G.shield = 0;
    G.shield = G.shield - float(impact) * damage;
    if (G.shield < 0.0f)
        G.shield = 0;
    if (reflect) {
        double d = 0;
        if (s0->touching) {
            if (!sensor(1)->touching)
                d = 1.0;
        } else if (sensor(1)->touching)
            d = -1.0;
        rot = float(double(rot) + d * vel.y);
    }
    if (launchHold) {
        if (thrustHeld < 1)
            launchHold = false;
        else
            thrustHeld = 0;
    }
    if (isLanded)
        rotateHeld = 0;
    thrustTarget = thrustHeld == 0 ? 0.0f : gravity + gravity;
    const float v = float(std::fabs(vel.y) * 1.3);
    float step = v * v;
    if (step < 0.008f)
        step = 0.008f;
    if (isLanded || sinceContact < 10)
        step = 0.05f;
    float t = thrust;
    if (t < thrustTarget) {
        t = float(double(t) + double(step) * 0.005);
        thrust = t;
    }
    if (thrustTarget < t) {
        t = float(double(t) - 5.555555555555556e-05);
        thrust = t;
    } // binary64 1/18000
    if (t < 0.0f)
        thrust = 0;
    if (thrustHeld != 0) {
        vel.y = vel.y - double(thrust);
        G.fuel = G.fuel - fuelPerThrust;
    }
    if (G.fuel < 0.0f) {
        G.fuel = 0;
        G.sample(0x44);
    }
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
        vel.x = vel.x + double(f) * -0.1;
    }
    vel.x = vel.x * drag.x;
    prevMapPos = mapPos;
    mapPos.x = mapPos.x + vel.x * 5.0;
    if (!(2.0 <= mapPos.x))
        mapPos.x = 2.0;
    else if (18.0 < mapPos.x)
        mapPos.x = 18.0;
    if (!reflect && !isLanded)
        vel.y = vel.y + double(gravity);
    vel.y = vel.y * drag.y;
    mapPos.y = mapPos.y + vel.y;
    if (0.01 < vel.y)
        G.scrollSpeed = -0.25;
    if (vel.y < -0.01 || G.flag203808)
        G.scrollSpeed = 0.15; // 0x3fc3333333333333
    pos.x = mapPos.x / 20.0 - 0.5;
    pos.y = G.screenY;
    const double prevCamera = G.cameraY;
    G.cameraY = (mapPos.y - 12.5) + G.screenY * -25.0;
    if (G.cameraY < 0.0) {
        pos.y = pos.y + G.cameraY / 25.0;
        G.cameraY = 0;
        if (pos.y < -0.48) {
            pos.y = -0.48; // 0xbfdeb851eb851eb8
            if (vel.y < 0.0)
                vel.y = 0;
            thrustTarget = 0;
            thrust = 0;
        }
        if (G.flag203808) { // "Dock with the" / "MOTHERSHEEP!"
            G.setMessage(1, 300, 0xe, 2, kGoMsg[4]);
            G.setMessage(3, 0x140, 0x10, 2, kGoMsg[5]);
        }
    }
    if (G.scrollLimit < G.cameraY) {
        pos.y = pos.y + (G.cameraY - G.scrollLimit) / 25.0;
        G.cameraY = G.scrollLimit;
    }
    G.cameraStep = G.cameraY - double(float(prevCamera)); // original narrows the old camera
    G.scrollDelta.y = G.cameraStep / 25.0;
    const float tp = thrustTarget * 100.0f;
    if (0.0f < tp && (G.frameCount & 1)) {
        auto *p = new ThrustParticle;
        p->pos = pos;
        p->vel.x = 0;
        const double lo =
            double(float(double(tp + 1.0f) * 0.004)); // f32 sum, f64 product, narrowed
        p->vel.y = G.rnd(lo, lo * 1.4);
        p->size = 0.02f;
        p->growth = float(G.rnd(1.01, 1.04));
        p->collides = true;
        p->fade = float(G.rnd(0.02, 0.03));
        p->init(G);
        G.addEntity(G.debris, p);
    }
    G.touch(*this, G.touchList);
    const Vec2 savedPos = pos;
    const Vec2 savedVel = vel;
    G.bounceOff(*this, G.bounceList);
    if (dead) { // bounceOff marks the ship
        dead = false;
        const Vec2 d{pos.x - savedPos.x, pos.y - savedPos.y};
        if (float(length(d)) < size) {
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

} // namespace minos
