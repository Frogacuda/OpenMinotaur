// Background effects; state and public interfaces are in bge.h.
#include "bge.h"
#include <cmath>
#include <cstring>

namespace minos {

namespace {
constexpr double kPi =
    3.141592654; // the pool word pair 0x400921fb54524550 (oxRNG(-pi, pi), galag's turn)
inline Vec2 rotated(Vec2 v, double a) {
    neon::vec2d r{v.x, v.y};
    r.rotateBy(a);
    return {r.x, r.y};
}
inline Vec2 minus(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
inline Vec2 plus(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

// The ship's damage on contact, as the hitBy functions of this family write it:
// shield -= hitDamage * 800 (vmls.f32), floored at 0, +0x31c cleared, sample 0x20.
void shipDamage(PlayState &G, Entity *a) {
    if (a->type != 2)
        return;
    auto *ship = static_cast<Ship *>(a);
    G.shield = G.shield - ship->hitDamage * 800.0f;
    if (G.shield < 0.0f)
        G.shield = 0;
    G.cleanRun = false;
    G.sample(0x20);
}

// The common tail of every run here: dead outside [lo, hi] in y, then an explo (0.5, 2,
// 0.06) and death while clearAll is set.
template <class E> void edgeAndClear(E &e, double hi, double lo) {
    if (hi < e.pos.y || e.pos.y < lo)
        e.dead = true;
    if (e.g->clearAll != 0) {
        addExplo(*e.g, e.pos, 0.5f, 2.0f, 0.06f);
        e.dead = true;
    }
}
} // namespace

// ------------------------------------------------------------------ asteroid
void Asteroid::init(PlayState &G) { // 0x559cc
    pos.y = -0.6;
    g = &G;
    pos.x = G.rnd(-0.3, 0.3);
    const double r = G.rnd(0.002, 0.005);
    vel = {r, r}; // +0xa4 = r, +0x9c = r
    vel = rotated(vel, G.rnd(-0.5, 0.5));
    if (G.libcRand() & 1)
        vel.x = -vel.x;
    spin = float(G.rnd(-0.1, 0.1));
    col.unpackRGBA(kRcols[int32_t(G.rnd(0.0, 10.0)) + 1]);
    const double s = G.rnd(0.04, 0.09);
    spriteIndex = 0x1e;
    generation = 1;
    size = float(s);
}

void Asteroid::run() { // 0x6f804
    PlayState &G = *g;
    pos = minus(pos, G.scrollDelta);
    pos = plus(pos, vel);
    pos.x = pos.x + double(G.shotDrift) * 0.03;
    rot = rot + spin; // binary32
    if (0.6 < pos.y || pos.y < -0.7)
        dead = true;
    if (vel.x < 0.0) {
        if (pos.x < double(size * 0.5f) - 0.5)
            vel.x = -vel.x;
    } else if (double(size * -0.5f) + 0.5 < pos.x)
        vel.x = -vel.x;
    if (G.clearAll != 0) {
        addExplo(G, pos, 0.5f, 2.0f, 0.06f);
        dead = true;
    }
}

void Asteroid::hitBy(Entity *a) { // 0x6f044
    PlayState &G = *g;
    shipDamage(G, a);
    if (a->dead)
        return;
    a->dead = true;
    static const int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21}; // _booms
    const uint32_t r = G.libcRand();
    G.sample(kBooms[(r & 0x7fff) % 7]);
    addExplo(G, pos, 2.0f, 8.0f, 0.016f);
    G.score(0x32, pos, {1.0, 1.0});
    for (int i = 0; i < 10; i++)
        addSpark(G, pos, -0.02, -0.005, -0.01, 0.01, 0.001, 0.006);
    dead = true;
    if (0 < generation) { // split in two
        generation--;
        for (int k = 0; k < 2; k++) {
            auto *c = new Asteroid;
            c->init(G);
            c->pos = pos;
            c->generation = generation;
            c->vel = {vel.x * 0.8, vel.y * 0.8}; // literal 0x3fe999999999999a (0x6f494)
            c->vel = rotated(c->vel, G.rnd(-1.5, 1.5));
            c->size = float(double(size) * 0.6);
            c->pos = plus(c->pos, {c->vel.x * 3.0, c->vel.y * 3.0}); // 0x40080000 (0x6f528)
            G.addEntity(G.bounceList, c);
        }
    }
}

// ------------------------------------------------------------------ invader
void Invader::run() { // 0x6fb70
    PlayState &G = *g;
    const int t = --timer;
    if (dying == 0) {
        if (t < 1) {
            pos.x = pos.x + double(stepX);
            timer = score; // +0x16c
            if (pos.x < -0.6 || 0.6 < pos.x)
                dead = true;
            frames++;
            spriteIndex = int32_t(uint32_t(frames) & 1) + spriteBase;
        }
        if (int32_t(G.libcRand() & 0xfff) < fireChance) {
            auto *b = new InvBull;
            b->pos = pos;
            b->saved = col;
            b->g = &G;
            b->size = 0.02f;
            b->vel = {0, 0};
            b->aimX = 0;
            b->noDrift = false;
            b->spriteIndex = 0x2b;
            uint64_t u = 0x3f2e2584f4c6e6daull;
            std::memcpy(&b->fall, &u, 8); // +0x174
            b->scale.y = b->scale.y * 1.5;
            b->pos.y = b->pos.y + 0.029999999329447746;
            G.addEntity(G.bounceList, b);
        }
    } else if (dying == 1 && t < 1) {
        dead = true;
    }
    pos = minus(pos, G.scrollDelta);
    edgeAndClear(*this, 0.6, -1.0);
}

void Invader::hitBy(Entity *a) { // 0x5f86c
    PlayState &G = *g;
    shipDamage(G, a);
    if (a->dead)
        return;
    a->dead = true;
    G.sample(0x33);
    G.score(0x32, pos, {1.0, 1.0});
    for (int i = 0; i < 10; i++)
        addSpark(G, pos, -0.02, -0.005, -0.01, 0.01, 0.001, 0.006);
    timer = 0x1e;
    spriteIndex = 0x2a;
    collisionEnabled = false;
    dying = 1;
    type = 3;
    for (Entity *e = G.bounceList; e; e = e->next) { // the wave speeds up and fires more
        if (e->type != 4)
            continue;
        auto *o = dynamic_cast<Invader *>(e);
        if (!o || o->group != group)
            continue;
        if (3 < o->score) {
            o->score -= 2;
            o->fireChance += 10;
        }
    }
}

void InvBull::run() { // 0x70718
    EnemyBull::run();
    col = saved;
    const int32_t v = int32_t(pos.y * 50.0);
    scale.x = (v & 1) == 0 ? -1.0 : 1.0;
}

// ------------------------------------------------------------------ galag
void Galag::start(PlayState &G) { // 0x59768
    g = &G;
    base = G.bgeOrigin;
    wobble = {0, 0};
    pos = base;
    spriteIndex = G.bgeSide + 0x2e;
    angle = G.bgeAngle;
    angleRate = G.bgeDx;
    const float s = std::sin(G.bgeAngle); // sinf(+0x203834)
    size = 0.05f;
    score = 0x1e;
    damage = 100;
    sound = 0x35;
    vel.y = 0.01;
    vel.x = double(s) * 0.005;
    rot = float(std::atan2(-0.01, vel.x) - 1.570796327);
    phase = 0;
    phaseRate = float(G.rnd(0.003, 0.007));
    fall = G.bgeSpin;   // +0x203838
    speed = G.bgeSpeed; // +0x20383c
}

void Galag::run() { // 0x6e554
    PlayState &G = *g;
    const Vec2 old = pos;
    angle = angle + angleRate;
    phase = phase + phaseRate;
    vel.x = double(std::sin(angle) * speed); // binary32 product
    if (pos.x < -0.45 && vel.x < 0.0)
        angle = float(double(angle) + kPi);
    if (0.45 < pos.x && 0.0 < vel.x)
        angle = float(double(angle) - kPi);
    vel.y = vel.y + double(fall);
    base = plus(base, vel);
    base.x = base.x + double(G.shotDrift) * 0.03;
    base = minus(base, G.scrollDelta);
    wobble.x = double(std::sin(float(double(phase) * 0.3))) * 0.2;
    pos = plus(base, wobble);
    const Vec2 d = minus(pos, old);
    rot = float(std::atan2(-d.y, d.x) - 1.570796327);
    edgeAndClear(*this, 0.8, -1.5);
}

// ------------------------------------------------------------------ grunt
void Grunt::run() { // 0x6ffcc
    PlayState &G = *g;
    if (--timer < 1) {
        timer = period;
        const neon::vec2d d =
            neon::vec2d{G.shipList->pos.x - pos.x, G.shipList->pos.y - pos.y}.normalize();
        vel = {d.x * double(speed), d.y * double(speed)};
        pos = plus(pos, vel);
        if (pos.x < -0.6 || 0.6 < pos.x)
            dead = true;
        frames++;
        scale.x = (uint32_t(frames) & 1) ? -1.0 : 1.0;
    }
    pos = minus(pos, G.scrollDelta);
    edgeAndClear(*this, 0.6, -1.0);
}

void Grunt::hitBy(Entity *a) { // 0x6efd4
    Enemy::hitBy(a);           // xentity::hitBy
    type = 3;
    for (Entity *e = g->bounceList; e; e = e->next) { // the wave moves more often
        if (e->type != 5)
            continue;
        auto *o = dynamic_cast<Grunt *>(e);
        if (!o || o->group != group)
            continue;
        if (5 < o->period)
            o->period--;
    }
}

// ------------------------------------------------------------------ slab, ball
void Slab::run() { // 0x6b38c
    PlayState &G = *g;
    pos = minus(pos, G.scrollDelta);
    pos = plus(pos, vel);
    spriteIndex = int32_t((uint32_t(G.frameCount) >> 2) & 7) + 0x4c;
    edgeAndClear(*this, 0.6, -0.7);
}

void Slab::hitBy(Entity *a) {
    shipDamage(*g, a);
} // 0x5dfe0: indestructible

void Ball::run() { // 0x6b0d8
    PlayState &G = *g;
    pos = minus(pos, G.scrollDelta);
    pos = plus(pos, vel);
    spriteIndex = int32_t((uint32_t(G.frameCount) >> 4) & 1) + 0x54;
    edgeAndClear(*this, 0.6, -1.0);
}

// ------------------------------------------------------------------ tet
void Tet::start(PlayState &G) { // 0x54a24
    type = 6;
    score = 10;
    damage = 800;
    sound = -1;
    g = &G;
    const double vy = G.rnd(0.002, 0.01);
    pos.y = -0.6;
    vel.y = vy;
    pos.x = G.rnd(-0.45, 0.45);
    spriteIndex = int32_t(G.rnd(0.0, 4.9)) + 0x56;
    scale = {1.0, G.getScale(spriteIndex).y}; // getSize(frame + 2): h / w
    size = 0.05f;
    hits = int32_t(G.rnd(0.0, 4.0)) + 1;
    spinStep = (G.libcRand() & 1) == 0 ? -0.39269909f : 0.39269909f; // 0xbec90fdb / 0x3ec90fdb
}

void Tet::run() { // 0x6ae3c
    PlayState &G = *g;
    pos = minus(pos, G.scrollDelta);
    pos = plus(pos, vel);
    edgeAndClear(*this, 0.6, -1.0);
}

void Tet::hitBy(Entity *a) { // 0x6ef00: knocked up a little per hit
    if (a->dead)
        return;
    if (a->type != 2)
        a->dead = true;
    pos.y = pos.y - 0.03;
    g->sample(0x1a);
    if (dead)
        return;
    vel.y = vel.y * 1.05;
    hits--;
    rot = rot + spinStep;
    if (hits < 1) {
        a->dead = false;
        Enemy::hitBy(a);
    }
}

// ------------------------------------------------------------------ launchBGEs
void launchBGEs(PlayState &G) { // 0x5c884
    if (0 < G.approachTimer || G.paused || G.clearAll != 0)
        return;
    if (G.bossPresent) {
        G.bossPresent = false;
        return;
    }
    if (G.bgeKind == 1)
        G.bgeOrigin = minus(G.bgeOrigin, G.scrollDelta);
    if (G.bgeLeft < 1) {
        if (0 < --G.bgeGap)
            return;
        G.bgeIntervalSetup();
        G.bgeGap = G.bgeGapReload;
        G.bgeTimer = 1;
        G.bgeLeft = G.bgeWaves;
    }
    if (0 < --G.bgeTimer)
        return;
    G.bgeTimer = G.bgePeriod;
    G.bgeLeft--;
    switch (G.bgeKind) {
    case 0: {
        auto *e = new Asteroid;
        e->init(G);
        G.addEntity(G.bounceList, e);
        break;
    }
    case 1: {
        auto *e = new Invader;
        e->g = &G;
        e->size = 0.04f;
        e->pos = G.bgeOrigin;
        const uint32_t r = G.libcRand();
        e->score = 10;
        e->timer = 0;
        e->spriteBase = int32_t(((r & 0xf) % 3) >> 1) + 0x24;
        e->stepX = e->pos.x < 0.0 ? 0.03f : -0.03f; // 0x3cf5c28f / 0xbcf5c28f
        e->col.unpackRGBA(G.bgeColour);             // +0x2078ac
        e->spriteIndex = e->spriteBase;
        e->dying = 0;
        e->type = 4;
        e->group = G.bgeIntervals; // +0x2078b0
        e->frames = G.bgeIntervals;
        e->fireChance = 0xf;
        G.addEntity(G.bounceList, e);
        break;
    }
    case 2: {
        auto *e = new Galag;
        e->start(G);
        G.addEntity(G.bounceList, e);
        break;
    }
    case 3: {
        static const int32_t kSounds[8] = {0x3f, 0x3f, 0x3f, 0x40, 0x40, 0x30, 0x3e, 0x3d};
        auto *e = new Grunt;
        e->g = &G;
        e->size = 0.06f;
        e->pos.x = G.rnd(-0.4, 0.4);
        e->pos.y = -0.6;
        e->period = 0x14;
        e->timer = 0;
        e->speed = 0.01f;
        e->spriteIndex = 0x33;
        e->type = 5;
        e->group = G.bgeIntervals;
        e->frames = G.bgeIntervals;
        e->sound = kSounds[G.libcRand() & 7];
        e->score = 10;
        e->damage = 400;
        G.addEntity(G.bounceList, e);
        break;
    }
    case 4: {
        auto *e = new Slab;
        e->mass = 10.0f;
        e->pos.y = -0.6;
        e->g = &G;
        e->pos.x = G.rnd(-0.3, 0.3);
        e->vel.y = 0.003; // 0x3f689374bc6a7efa
        const double s = G.rnd(0.04, 0.06);
        e->scale.y = 0.6;
        e->spriteIndex = 0x4c;
        e->size = float(s);
        G.addEntity(G.bounceList, e);
        break;
    }
    case 5: {
        auto *e = new Ball;
        e->pos.x = G.bgeOrigin.x;
        e->type = 6;
        e->score = 10;
        e->g = &G;
        e->damage = 100;
        e->sound = 0x34;
        const double v = G.rnd(0.003, 0.007);
        e->vel.y = v;
        e->pos.y = G.rnd(-0.9, -0.3);
        e->vel.x = e->pos.x < 0.0 ? v : -v;
        e->spriteIndex = 0x54;
        e->size = 0.02f;
        G.addEntity(G.bounceList, e);
        break;
    }
    case 6: {
        auto *e = new Tet;
        e->start(G);
        G.addEntity(G.bounceList, e);
        break;
    }
    default:
        break;
    }
}

} // namespace minos
