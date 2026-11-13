// ent_world.cpp - the world: planets and the sun, saucers with their shots
// and mines, minotaurs to rescue, and the particle effects.
#include "game.h"
#include "game_entities.h"
#include "neon_modules.h"

#include <cmath>

namespace NEON_GAME {
namespace Neon_Internals {

namespace {
constexpr double kPi = 3.141592654;
constexpr double kTwoPi = 6.2831853072;
constexpr double kHalfPi = 1.570796327;
} // namespace

// =============================================================== saucer ====

void saucer::run() {
    spaceWars *g = game;
    entity::run();
    if (kind == 3)
        rot = float(std::atan2(-vel.y, vel.x) - kHalfPi);
    if (pos.length() < 0.4)
        g->bounceOff(this, g->planets);
    g->bounceOff(this, g->players);
    col = getDynamicColour(colourType, 1.0f, g->frameCount);
    col.a = g->paused ? 0.3 : 1.0;

    entity *target = nullptr;
    for (entity *e = g->players; e; e = e->next)
        if (float((e->pos - pos).length()) < 1000.0f)
            target = e;
    vec2d d;
    if (target)
        d = target->pos - pos;

    // Chase: accelerate towards the player up to maxVel.
    unsigned f = moveFlags;
    if ((f & 3) && target) {
        if (f & 1) {
            double nv =
                d.x <= 0.0 ? vel.x - maxVel.x * double(accel) : vel.x + maxVel.x * double(accel);
            if (std::fabs(nv) < maxVel.x)
                vel.x = nv;
        }
        if (f & 2) {
            double nv =
                d.y <= 0.0 ? vel.y - maxVel.y * double(accel) : vel.y + maxVel.y * double(accel);
            if (std::fabs(nv) < maxVel.y)
                vel.y = nv;
        }
    }
    // Course changes.
    if (f & 4) {
        if (--moveTimer < 1) {
            if (!(f & 0x10))
                vel.y = -vel.y;
            else if (target)
                vel.y = d.y <= 0.0 ? -maxVel.y : maxVel.y;
            if ((f & 8) && target)
                vel.x = d.x <= 0.0 ? -maxVel.x : maxVel.x;
            moveTimer = oxRNG::irand(30.0) + 0x32;
        }
    }
    // Saucers that only drift across leave at the far side.
    if ((f & 9) == 0) {
        bool gone = vel.x <= 0.0 ? pos.x < -0.6 : 0.6 < pos.x;
        if (gone) {
            dead = true;
            g->releaseSaucerSound(big);
            g->saucerCount--;
        }
    }

    // Inside the central circle they lay mines and shoot.
    if (pos.length() <= 0.4) {
        int lv = g->level;
        int k = (lv >> 2) & 3;
        int diff = (lv & 3) + (lv >> 3);
        if (k != 0 && oxRNG::rand(100.0) < double((float(diff) / 7.0f) * 1.6f + 1.2f) &&
            pos.length() < 0.4) {
            if (!g->demo)
                g->playSample(10);
            mine *m = new mine();
            m->spriteIndex = 0xa6;
            m->fuse = 1.0f;
            m->count = 0;
            m->drag = vec2d(0.5, 0.5);
            m->type = 5;
            m->pos = pos;
            m->size = 0.01f;
            m->game = g;
            m->kind = kind;
            m->fuseRate = 0.002f;
            g->addEntity(m, &g->saucers);
            if (k == 2)
                m->mass = float(-(((lv & 3) + 1) * 2000) - (lv >> 4) * 3000);
            else if (k == 3)
                m->mass = float(20000 + (lv >> 4) * 100000 + (lv & 3) * 20000);
        }
        if (--shotTimer < 1) {
            float speed = float(double(float(diff)) * 0.001 + 0.003);
            int n;
            shotReload = diff * -4 + 100;
            if (big == 0) {
                n = 3;
            } else {
                shotReload >>= 1;
                n = 1;
                speed *= 1.5f;
            }
            if (shotReload < 0xf)
                shotReload = 0xf;
            if (burst == 0) {
                aimedCount++;
            } else {
                shotReload = 3 - big;
                if (--burst == 0) {
                    shotReload = diff * -0x10 + 100;
                    if (shotReload < 0x14)
                        shotReload = 0x14;
                }
            }
            shotTimer = shotReload;
            if (!g->demo)
                g->playSample(10);
            saucerShot *s = new saucerShot();
            g->addEntity(s, &g->enemyShots);
            s->type = 6;
            s->drag = vec2d(1.0, 1.0);
            s->game = g;
            s->pos = pos;
            s->size = 0.005f;
            s->spriteIndex = 0x8c;
            s->col.white(1.0);
            if (burst == 0) {
                int players = 0;
                for (entity *e = g->players; e; e = e->next)
                    players++;
                if (players == 0)
                    aimedCount = -1;
                if (aimedCount < n) {
                    s->vel = vec2d(0, 0.005);
                    s->vel.rotateBy(oxRNG::rand(-kPi, kPi));
                } else {
                    // Aimed (roughly) at a random player.
                    aimedCount = 0;
                    entity *e = g->players;
                    for (int i = oxRNG::irand(double(players)); i != 0; i--)
                        e = e->next;
                    vec2d t(e->pos.x + (e->vel.x / double(s->fade)) * 0.0,
                            e->pos.y + (e->vel.y / double(s->fade)) * 0.0);
                    t.x += oxRNG::rand(-0.02, 0.02);
                    t.y += oxRNG::rand(-0.02, 0.02);
                    if (t.x < -0.5)
                        t.x += 1.0;
                    if (0.5 < t.x)
                        t.x -= 1.0;
                    if (t.y < -0.5)
                        t.y += 1.0;
                    if (0.5 < t.y)
                        t.y -= 1.0;
                    s->vel = normalized(t - pos) * double(speed);
                    burstVel = s->vel * 1.1;
                }
            } else {
                s->vel = burstVel;
                burstVel = burstVel * 1.1;
            }
        }
    }
}

void saucer::wasShot(entity *by) {
    spaceWars *g = game;
    size = size + size;
    if (!g->demo)
        g->playSample(9);
    g->releaseSaucerSound(big);
    g->saucerCount--;
    int points = big == 0 ? 500 : 1000;
    scoreExplosion *x = new scoreExplosion();
    x->count = 0x32;
    x->from = col;
    x->to = col;
    x->from.a = 1.0;
    x->to.a = 0.5;
    x->rot = rot;
    x->seed = unsigned(neonRand());
    x->pos = pos;
    x->vel = vel * 0.1;
    x->size = size;
    x->game = g;
    x->setScore(points, pos);
    g->addEntity(x, &g->particles);
    dead = true;
    g->score(points, by->group - 1);
}

void saucerShot::run() {
    spaceWars *g = game;
    elasticity = 0.5f;
    if (g->bounceOff(this, g->bullets)) {
        makeSpark(g, pos);
        if (!g->demo)
            g->playSample(0x11);
    }
    elasticity = 1.0f;
    enemyBull::run();
    double s = oxRNG::rand(0.9, 2.0);
    scaleX = s;
    scaleY = s;
    col.r = oxRNG::rand(0.5, 1.0);
    col.b = oxRNG::rand(0.5, 1.0);
    col.a = g->paused ? 0.3 : 1.0;
}

// ================================================================= mine ====

// Pulses as its fuse burns; kind-2 mines are pushed away from the ships.
void mine::run() {
    spaceWars *g = game;
    static const int kFrames[16] = {0xa6, 0xa7, 0xa8, 0xa7, 0xa6, 0xa7, 0xa8, 0xa7,
                                    0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4};
    count++;
    fuse -= fuseRate;
    spriteIndex = kFrames[(kind * 4 + ((unsigned(count) & 7) >> 1)) & 15];
    if (0.6 < pos.length())
        dead = true;
    float t = float((count >> 1) & 3);
    size = float(double(t * 0.25f * (1.0f - fuse)) * 0.01 + 0.01);
    if (kind == 2) {
        int lv = g->level;
        float s = float(((lv & 3) + 1) * 2000 + ((lv >> 4) & 3) * 8000);
        vel = vel - g->attract(this, g->players, s) * 0.008;
    }
    pos = pos + vel;
    col = getDynamicColour(0, 1.0f, g->frameCount);
    col.a = g->paused ? 0.3 : 1.0;
    if (fuse < 0.0f)
        detonate(0);
}

void mine::wasShot(entity *by) {
    detonate(by->group);
}

// A ship (not invulnerable) that touches a mine is destroyed with it.
void mine::hitBy(entity *by) {
    if (by->type != 1)
        return;
    if (by->fc == 0)
        by->hp = -1;
    explosion *x = new explosion();
    x->count = 0x40;
    x->from.white(1.0);
    x->to.yellow(0.2);
    x->rot = rot;
    x->seed = unsigned(neonRand());
    x->pos = pos;
    x->vel = vel * 0.1;
    x->size = size * 12.0f;
    x->game = game;
    x->sizeStart = 0.2f;
    x->sizeEnd = 0.3f;
    game->addEntity(x, &game->particles);
    dead = true;
}

// Fuse out (byGroup 0): just blows up. Shot by a player: bonus points
// (100 x mines shot this level) shown in a burst. Kind-3 mines throw a ring
// of (level - 10) shots either way.
void mine::detonate(int byGroup) {
    spaceWars *g = game;
    if (byGroup == 0) {
        size = 0.03f;
        blowUp(unsigned(neonRand()), 0x40);
    } else {
        if (!g->demo)
            g->playSample(0xf);
        int n = g->minesHit++;
        if (g->minesHit == 0x14)
            g->achieveGoal(1);
        scoreExplosion *x = new scoreExplosion();
        x->count = 0x40;
        x->from.yellow(1.0);
        x->to.red(0.2);
        x->rot = rot;
        x->seed = unsigned(neonRand());
        x->pos = pos;
        x->vel = vel * 0.1;
        x->size = size * 4.0f;
        x->sizeStart = 0.5f;
        x->game = g;
        x->sizeEnd = 0.2f;
        x->setScore(n * 100, pos);
        x->colourType = 0;
        x->digitScale = 0.4f;
        g->addEntity(x, &g->particles);
        dead = true;
    }
    if (kind == 3) {
        int shots = g->level - 10;
        float step = 6.2831855f / float(shots);
        float a = 0;
        for (int i = 0; i < shots; i++) {
            saucerShot *b = new saucerShot();
            g->addEntity(b, &g->enemyShots);
            b->type = 6;
            b->game = g;
            b->drag = vec2d(0.98, 0.98);
            b->pos = pos;
            b->size = 0.005f;
            b->spriteIndex = 0x8c;
            b->col.yellow(1.0);
            b->vel = vec2d(0, 0.004);
            b->vel.rotateBy(double(a));
            a += step;
        }
    }
}

// ========================================================== collectible ====

void collectible::init(entity *from) {
    spriteIndex = 0x97;
    type = 4;
    scaleY = 1.6;
    game = from->game;
    pos = from->pos;
    float out = float(0.3 - from->pos.length());
    double k = out < 0.0f ? 0.0 : double(out);
    vel = (normalized(from->pos) * k) * 0.03;
    mass = 200.0f;
    size = 0.03f;
    spin = float(oxRNG::rand(-0.01, 0.01));
}

int collectible::draw(sprite *s, vec2d sz) {
    s->col = col;
    s->angle = double(rot);
    s->frame = spriteIndex + 1;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// Drawn by the planets (capped speed) and towards the ships.
void collectible::run() {
    spaceWars *g = game;
    vec2d nv = vel + g->feelTheForce(this, g->planets) * 0.01;
    if (nv.length() < 0.02)
        vel = nv;
    vel = vel + g->attract(this, g->players, 1000.0f);
    rot += spin;
    entity::run();
    col.a = g->paused ? 0.3 : 1.0;
    g->bounceOff(this, g->planets);
}

// Rescued: points (more the closer to the edge), a heart, a grunt.
void collectible::hitBy(entity *by) {
    spaceWars *g = game;
    float d = float(by->pos.length());
    float edge = d <= 0.5f ? (0.51f - d) + (0.51f - d) : 0.02f;
    float pts = float(double(float(g->level + 1)) * 1.1 * 1000.0);
    if (!g->sunOn)
        pts = pts / 10.0f;
    else
        pts = pts * edge;
    int points = int(pts);
    if (10000 < points)
        g->achieveGoal(2);
    g->levelsNoRescue = 0;
    g->score(points, by->group - 1);
    static_cast<spaceCraft *>(by)->weapon++;
    dead = true;
    minotaurLoveHeart *h = new minotaurLoveHeart();
    h->from.white(1.0);
    h->game = g;
    h->digitPtr = extractScore(h->digits, 8, points, -1);
    h->to.red(0.0);
    h->digitPos = pos;
    h->size = 0.03f;
    h->fade = 0.02f;
    h->pos = pos;
    h->group = by->group;
    h->spriteIndex = 0x98;
    h->sizeStart = 1.0f;
    h->sizeEnd = 2.0f;
    h->vel.y = double(-size) * 0.2;
    g->addEntity(h, &g->particles);
    g->rescued++;
    g->rescuedThisLevel++;
    if (g->mode == 0) {
        if (g->rescuedThisLevel == 0x14)
            g->achieveGoal(4);
        if (g->rescued == 100)
            g->achieveGoal(0);
    }
    static const int kGrunts[5] = {1, 4, 5, 6, 7};
    int gi = oxRNG::irand(5.0);
    if (!g->demo)
        g->playSample(kGrunts[gi]);
}

// ========================================================= tankMinotaur ====

int tankMinotaur::draw(sprite *s, vec2d sz) {
    if (countdown >= 1)
        return 0;
    s->col = col;
    s->angle = 0;
    s->frame = spriteIndex + 1;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// Counts down on the grid (digits), then appears and is drawn to the tanks.
void tankMinotaur::run() {
    spaceWars *g = game;
    col.a = g->paused ? 0.3 : 1.0;
    if (countdown < 1) {
        vec2d v = vel + g->feelTheForce(this, g->planets) * 0.01;
        vel = v + g->attract(this, g->players, 1000.0f);
        entity::run();
    } else {
        countdown--;
        if (g->grid)
            g->grid->poke(cellX, cellY, countdown == 0 ? ' ' : char((countdown >> 6) + '0'));
    }
}

// Collected by a tank: rapid fire, a power level, points.
void tankMinotaur::hitBy(entity *by) {
    spaceWars *g = game;
    if (countdown >= 1 || g->demo)
        return;
    tank *t = static_cast<tank *>(by);
    static_cast<tankGun *>(t->gun)->burst = 30;
    int p = t->powerLevel++;
    g->lives[by->group - 1] += p + 2;
    dead = true;
    _particle *h = new _particle();
    h->from.white(1.0);
    h->game = g;
    h->rot = 0;
    h->to.red(0.0);
    h->size = 0.03f;
    h->fade = 0.02f;
    h->pos = pos;
    h->spriteIndex = 0x98;
    h->sizeStart = 1.0f;
    h->sizeEnd = 2.0f;
    h->vel.y = double(-size) * 0.2;
    g->addEntity(h, &g->particles);
    if (!g->demo)
        g->playSample(0x1a);
}

// ============================================================ thePlanet ====

int thePlanet::draw(sprite *s, vec2d sz) {
    if (0.9 < pos.length())
        return 0;
    s->col = col;
    age2++;
    if (0.0f < flashT) {
        s->col = col.lerp(flashColour, flashT);
        flashT -= flashDecay;
    }
    s->frame = 0x8c;
    s->angle = double(rot);
    s->width = double(size + size) * sz.x;
    s->height = double(size + size) * sz.y;
    s->pos = pos * sz;
    s->col.a = game->paused ? 0.3 : 1.0;
    s->setupChecking();
    return 6;
}

// Orbits under the other planets' gravity (stronger as the level ends,
// when the planets fall away), pulled back from the edges, and attracted to
// the ships when there is no sun.
void thePlanet::run() {
    spaceWars *g = game;
    float len = float(pos.length());
    if (fc != 0)
        fc--;
    double k;
    if (!g->levelEnding) {
        k = double(0.01f);
    } else {
        if (0.6f < len)
            dead = true;
        int t = g->frameCount;
        if (t > 0xfe)
            t = 0xff;
        k = double((float(t) / 255.0f) * 0.04f + 0.01f);
    }
    vec2d nv = vel + g->feelTheForce(this, g->planets) * k;
    if (0.8 < double(len)) {
        float m = float(g->level % 13);
        nv = nv + pos * -(double(m / 7.0f) * 0.0008 + 0.0005);
    }
    if (!g->sunOn)
        nv = nv + g->attract(this, g->players, 500.0f);
    if (nv.length() < 0.01)
        vel = nv;
    else
        vel = vel * 0.99;
    rot += spin;
    entity::run();
    g->bounceOff(this, g->planets);
}

// A new planet drops in from outside the edge on an orbit (or a straight
// fall with no sun). Counts towards the level's quota of planets.
void thePlanet::reSpawn() {
    static const uint32_t kColours[6] = {0xc512c6ff, 0xcf593aff, 0x5a18ffff,
                                         0xf232f2ff, 0xc04a2cff, 0x1f8112ff};
    spaceWars *g = game;
    int lv = g->level;
    int quota = ((lv & 0xf) + (lv >> 3)) * 10 + 0x30;
    if (!g->demo && g->survival)
        quota = 0x18;
    if (g->blackHole)
        dead = true; // (cleared again below)
    orbitDir = g->planetsSpawned & 1;
    flashColour.red(1.0);
    flashT = 0;
    flashDecay = 0.4f;
    age2 = 0;
    pos = vec2d(0, oxRNG::rand(0.1) + 0.7);
    col.unpackRGBA(kColours[oxRNG::irand(6.0)]);
    spin = float(oxRNG::rand(-0.01, 0.01));
    pos.rotateBy(oxRNG::rand() * kTwoPi);
    int d = g->survival ? lv : (lv & 7) + (lv >> 2);
    if (!g->sunOn) {
        double r = -(oxRNG::rand(0.0008) + 0.00039999998989515007);
        vel = pos * r;
        mass = size * 2000.0f * g->massScale;
    } else {
        double r = oxRNG::rand(double(float(d) / 7.0f) * 0.002);
        vel = pos * -(r + 0.0007999999797903001);
        mass = size * 1000.0f; // (in survival a massScale version is computed and overwritten)
    }
    size = float(oxRNG::rand(0.05, 0.07));
    drag = vec2d(1.0, 1.0);
    dead = false;
    hp = 1;
    if (!g->demo) {
        if (generation == 2) {
            if (quota < g->planetsSpawned) {
                dead = true;
            } else if (quota < ++g->planetsSpawned) {
                if (!g->survival) {
                    g->frameCount = 0;
                    g->levelEnding = true;
                } else {
                    g->planetsSpawned = 0;
                    g->level++;
                    if (!g->demo)
                        g->playSample(0x10);
                }
            }
        }
        g->massScale = g->massScale * g->massGrowth;
    }
}

// Breaks off a smaller planet (one generation down) flying apart.
void thePlanet::split(vec2d v) {
    if (generation <= 0)
        return;
    thePlanet *p = new thePlanet();
    p->type = 2;
    p->generation = 2;
    p->mass = 20000.0f;
    p->hp = 0xf;
    p->wrapWidth = 1.6;
    p->wrapHeight = 1.6;
    p->elasticity = 0.3f;
    p->next = nullptr;
    p->prev = nullptr;
    p->game = game;
    p->reSpawn();
    p->pos = pos;
    p->mass = mass * 0.5f;
    p->size = size * 0.5f;
    p->fc = 10;
    p->generation = generation - 1;
    p->hp = 1;
    p->spin = float(oxRNG::rand(double(spin * -2.0f), double(spin + spin)));
    p->vel.x = oxRNG::rand(vel.x * -2.0, vel.x + vel.x);
    p->vel.y = oxRNG::rand(vel.y * -2.0, vel.y + vel.y);
    p->vel = p->vel + v * -0.3;
    game->addEntity(p, &game->planets);
}

// Shot: grows a little; out of hit points it splits (maybe releasing a
// minotaur), scores and respawns (whole planets only).
void thePlanet::wasShot(entity *by) {
    if (fc != 0)
        return;
    spaceWars *g = game;
    bull *b = static_cast<bull *>(by);
    if (size < 0.08f)
        size = size * 1.04f;
    hp -= b->damage;
    int bySaucer = b->type == 6;
    if (hp < 1) {
        if (g->minotaursOn && oxRNG::rand() < double(bySaucer) + 0.06) {
            collectible *c = new collectible();
            c->init(this);
            g->addEntity(c, &g->minotaurs);
        }
        split(b->vel);
        size = 0.045f;
        int points = (3 - generation) * g->level * b->power + 10;
        scoreExplosion *x = new scoreExplosion();
        x->count = (generation + 10) * 8;
        x->from = col;
        x->to = col;
        x->from.a = 1.0;
        x->to.a = 0.0;
        x->rot = rot;
        x->digitScale = 0.4f;
        x->colourType = b->group;
        x->seed = unsigned(neonRand());
        x->pos = pos;
        x->vel = vel * 0.1;
        x->size = size;
        x->game = g;
        x->setScore(points, pos);
        g->addEntity(x, &g->particles);
        dead = true;
        g->pulse = 1.0f;
        if (1 < generation)
            reSpawn();
        if (!bySaucer)
            g->score(points, b->group - 1);
    } else {
        flashT = 1.0f;
        if (!bySaucer)
            g->score(5, b->group - 1);
    }
    b->touching = true;
    b->life = 0;
    int smp = (neonRand() & 1) == 0 ? 8 : 0;
    if (!g->demo)
        g->playSample(smp);
}

void thePlanet::hitBy(entity *by) {
    if (by->type == 6 || by->type == 3)
        makeSpark(game, by->pos);
}

// =============================================================== theSun ====

int theSun::draw(sprite *s, vec2d sz) {
    if (collapsed)
        return 0;
    spaceWars *g = game;
    if (!g->paused) {
        for (int i : {0, 2, 1, 3}) {
            pulse[i] += pulseSpeed[i];
            if (1.0f < pulse[i])
                pulse[i] -= 1.0f;
        }
    }
    s->col.yellow(1.0);
    s->angle = 0;
    s->frame = 0x8d;
    s->width = double(size + size) * sz.x * (double(pulse[0]) * 0.5 + 0.1);
    s->height = double(size + size) * sz.y * (double(pulse[0]) * 0.5 + 0.1);
    s->pos = pos * sz;
    if (g->paused)
        s->col.a = 0.1;
    s->setupChecking();
    s->col.red(1.0);
    s->width = double(size + size) * sz.x * (double(pulse[1]) * 1.1 + 0.2);
    s->height = double(size + size) * sz.y * (double(pulse[1]) * 1.1 + 0.2);
    s->col.a = g->paused ? 0.05 : 0.5;
    s->setupChecking();
    return 12;
}

// Stays at the centre; recovers after a level.
void theSun::run() {
    spaceWars *g = game;
    pos = vec2d(0, 0);
    hp = 100;
    if (!g->levelDone)
        return;
    float s = size;
    if (0.02f < s) {
        s = s * 0.98f;
        size = s;
    }
    if (s < 0.02f)
        size = s + 0.001f;
    collapsed = false;
    mass = 150000.0f;
    if (g->blackHole)
        g->achieveGoal(7);
    g->blackHole = false;
}

// Swallows planets, minotaurs and ships. Each planet or minotaur it eats
// makes it heavier and bigger; past 0.18 it collapses into a black hole.
void theSun::hitBy(entity *by) {
    spaceWars *g = game;
    int lv = g->level;
    int t = by->type;
    if (t == 4 || t == 2) {
        by->blowUp(unsigned(neonRand()), 0x40);
        if (by->type == 2) {
            thePlanet *p = static_cast<thePlanet *>(by);
            if (1 < p->generation)
                p->reSpawn();
        }
        if (by->type == 4)
            g->levelsCleared = 0;
        solarFlare *f = new solarFlare();
        f->pos = pos;
        f->game = g;
        f->from.yellow(1.0);
        f->to.purple(0.1);
        f->fade = 0.003f;
        g->addEntity(f, &g->particles);
        if (!g->demo)
            g->playSample(0x15);
        if (!collapsed) {
            if (!g->demo) {
                float k = float(lv) * 0.0625f;
                mass = mass * (k * 0.03f + 1.03f);
                size = size * (k * 0.1f + 1.1f);
                if (0.18f < size) {
                    size = 0.0001f;
                    collapsed = true;
                    g->playSample(0x16);
                    mass = 1000000.0f;
                    g->blackHole = true;
                }
            }
        } else if (!g->demo) {
            mass = mass * 1.1f;
            if (2e6f < mass)
                mass = 2000000.0f;
        }
        t = by->type;
    }
    if (t == 1) {
        if (!g->demo)
            g->playSample(0xb);
        spaceCraft *c = static_cast<spaceCraft *>(by);
        c->size = 0.04f;
        c->blowUp(unsigned(neonRand()), 0x100);
        c->size = 0.02f;
        if (!g->demo)
            g->lives[c->joy]--;
        if (g->lives[c->joy] > 0) {
            c->reSpawn();
            c->fc = 120;
        }
        t = by->type;
    }
    if (t == 4) {
        static const uint32_t kCols[7] = {0xff200080, 0xff800080, 0xffff0080, 0x40ff0080,
                                          0x0080ff80, 0x2000ff80, 0x6000c080};
        for (int i = 0; i < 0x40; i++) {
            _particle *p = new _particle();
            p->vel = by->vel * -(oxRNG::rand(-0.5, 0.5) + 0.7);
            float r = float(oxRNG::rand(-0.3, 0.3));
            p->vel.rotateBy(double(r * 3.0f));
            p->pos = by->pos;
            int ci = int((double(r) + 0.3) * 10.0);
            p->from.unpackRGBA(kCols[ci < 0 ? 0 : (ci > 6 ? 6 : ci)]);
            p->to = p->from;
            p->to.a = 0.0;
            p->sizeEnd = 0.5f;
            p->size = 0.01f;
            p->game = g;
            g->addEntity(p, &g->particles);
        }
        by->blowUp(unsigned(neonRand()), 0x10);
    }
}

// ============================================================== effects ====

void gravParticle::run() {
    vel = vel + game->feelTheForce(this, game->planets);
    mass = life * 5.0f;
    _particle::run();
    if (wrapped)
        dead = true;
}

int gravParticle::draw(sprite *s, vec2d sz) {
    s->angle = 0;
    s->frame = 0x8d;
    float k = lerp(sizeStart, sizeEnd, 1.0f - life);
    s->width = double(size) * sz.x * double(k);
    s->height = double(k) * double(size) * sz.y;
    s->col = from;
    s->col.a = double(life);
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// A widening fan of dots, accelerating its fade.
int solarFlare::draw(sprite *s, vec2d sz) {
    fade = float(double(fade) * 1.08);
    s->angle = 0;
    s->frame = 0x8d;
    s->width = double(size) * (double(1.0f - life) + 0.2) * sz.x * 5.0;
    s->col = from.lerp(to, 1.0f - life);
    s->height = s->width;
    double d = double(-(1.0f - life)) * 0.5;
    vec2d a(d, d), b(-d, d);
    auto pair = [&](const vec2d &off) {
        s->pos = pos + off;
        s->setupChecking();
        s->pos = pos - off;
        s->setupChecking();
    };
    vec2d off = vlerp(a, b, 0.0f) * sz;
    pair(off);
    for (int k = 1; k < 9;) {
        off = vlerp(a, b, float(k) * 0.125f) * sz;
        pair(off);
        if (unsigned(k - 1) > 6u) {
            k++;
            continue;
        }
        std::swap(off.x, off.y);
        k++;
        pair(off);
    }
    return 0;
}

// Flies outwards (perspective) and fades in as `life` runs down.
int starFieldStar::draw(sprite *s, vec2d sz) {
    s->frame = spriteIndex + 1;
    s->angle = 0;
    s->col = col;
    double z = double(life * 4.0f + 1.0f);
    double px = sz.x * pos.x, py = sz.y * pos.y;
    s->pos.x = (px + px) / z;
    s->pos.y = (py + py) / z;
    double w = double(size) + double(size * life) * 0.01;
    s->width = w * sz.x;
    s->height = w * sz.y;
    s->col.a = double(1.0f - life);
    s->setupChecking();
    return 0;
}

int minotaurLoveHeart::draw(sprite *s, vec2d sz) {
    _particle::draw(s, sz);
    s->width *= 0.6;
    s->height *= 0.6;
    s->pos.y += double(1.0f - life) * 0.05 * sz.y;
    s->pos.x = sz.x * digitPos.x;
    digitPos.x *= 0.998;
    s->col = getDynamicColour(group, life, game->frameCount);
    int n = 0;
    while (digitPtr[n] != -1)
        n++;
    double step = s->width * 1.1;
    s->pos.x = double(float(float(step * double(n)) * -0.5f)) + s->pos.x + s->width * 0.5;
    for (int i = 0; i < n; i++) {
        s->frame = digitPtr[i] + 0x8e;
        s->setupChecking();
        s->pos.x += s->width * 1.1;
    }
    return 0;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
