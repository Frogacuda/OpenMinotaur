// ent_enemies2.cpp - the enemies spawned by theGame::genLevel (part 2:
// recognisers, sheepipedes, astro-smash cannons, the trail-leaving bosses).
#include "game.h"
#include "game_entities.h"
#include "game_tables.h"
#include "neon_audio.h"

#include <cmath>

namespace NEON_GAME {
namespace Neon_Internals {

namespace {
constexpr double kPi = 3.141592654;
constexpr double kHalfPi = 1.570796327;

void moveEdge(entity *e, int edge) {
    vec2d np = e->pos + e->vel;
    double sz = double(e->size);
    double vx = e->vel.x;
    if ((vx > 0.0 && 0.44999998807907104 < sz + np.x) ||
        (vx < 0.0 && np.x - sz < -0.46000000834465027)) {
        if (edge == 1)
            e->dead = true;
        else if (edge == 2)
            e->vel.x = -(vx * double(e->elasticity));
        else if (edge == 0)
            e->vel.x = 0;
    } else {
        e->pos.x = np.x;
    }
    double vy = e->vel.y;
    if ((vy > 0.0 && 0.44999998807907104 < sz + np.y) ||
        (vy < 0.0 && np.y - sz < -0.3799999952316284)) {
        if (edge == 1)
            e->dead = true;
        else if (edge == 2)
            e->vel.y = -(vy * double(e->elasticity));
        else if (edge == 0)
            e->vel.y = 0;
    } else {
        e->pos.y = np.y;
    }
}

void killIfPlayer(entity *by) {
    if (by && by->type == 3)
        static_cast<playerMinotaur *>(by)->kill();
}

double pauseAlpha(theGame *g, float a) {
    return (g->paused || g->demo) ? 0.30000001192092896 : double(a);
}

// Draws a trail newest-first, fading each point by `fade` per frame and
// shrinking the sprite by 0.98 per point. Returns false when the newest
// point has already faded out.
bool drawTrail(sprite *s, vec2d sz, trailPoint *tr, unsigned index, double a, double fade,
               bool purple, const float *rots) {
    unsigned i = (index - 1) & 0x3ff;
    if (tr[i].alpha <= 0.0)
        return false;
    do {
        colour to;
        if (purple) {
            s->col.purple(a);
            to.red(a);
        } else {
            s->col.yellow(1.0);
            to.red(1.0);
        }
        s->col = s->col.lerp(to, float(tr[i].alpha));
        s->col.a = tr[i].alpha * 0.7 * a;
        s->pos = tr[i].pos;
        double na = tr[i].alpha * fade;
        tr[i].alpha = na;
        if (na < 0.01)
            tr[i].alpha = 0;
        s->pos = s->pos * sz;
        if (rots)
            s->angle = double(rots[i]);
        s->setupChecking();
        s->width *= 0.98;
        s->height *= 0.98;
        i = (i - 1) & 0x3ff;
    } while (0.0 < tr[i].alpha);
    return true;
}

// Adds a trail point every `step` of distance moved.
void addTrail(trailPoint *tr, unsigned &index, float &dist, vec2d pos, vec2d old, float step) {
    float d = float(double(dist) + (pos - old).length());
    dist = d;
    if (step <= d) {
        dist = 0;
        tr[index].pos = pos;
        tr[index].alpha = 1.0;
        index = (index + 1) & 0x3ff;
    }
}

// A gruntle dropped at p (recogniser, sheepipede).
void dropGruntle(theGame *g, vec2d p) {
    gruntle *gr = new gruntle();
    gr->game = g;
    gr->setup(g);
    gr->pos = p;
    g->rez(gr, 1, 0.4f, 0x1e);
    g->addEntity(gr, &g->enemies);
}
} // namespace

// =========================================================== recogniser ====

void recogniser::init(theGame *g) {
    spriteIndex = 0x94;
    game = g;
    size = 0.08f;
    hp = 30;
    pos.y = oxRNG::rand(-0.30000001192092896, 0.3700000047683716);
    seekSpeed = 0.0015f;
    dropReload = 50;
    dropTimer = 50;
    if ((neonRand() & 1) == 0) {
        pos.x = double(size + size + 0.45f);
        double sp = double(seekSpeed);
        vel = vec2d(-oxRNG::rand(sp, sp * 1.3), 0);
    } else {
        pos.x = double(size * -2.0f - 0.46f);
        double sp = double(seekSpeed);
        vel = vec2d(oxRNG::rand(sp, sp * 1.3), 0);
    }
    turnReload = 120;
    unsigned r = neonRand();
    entered = false;
    type = 0x20;
    turnTimer = int(r & 0x7f) + 120;
}

void recogniser::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    moveEdge(this, 2);
    entity *p = g->players;
    vec2d t = p ? p->pos : vec2d(0, 0);
    if (!entered) {
        double s = double(size);
        if (-0.46000000834465027 < pos.x - s && pos.x + s < 0.44999998807907104)
            entered = true;
    } else {
        if (--turnTimer < 1) {
            int r = turnReload;
            turnTimer = int(neonRand() & 0x3f) + r;
            if (vel.x == 0.0) {
                vel.x = pos.x <= t.x ? double(seekSpeed) : double(-seekSpeed);
                vel.y = 0;
            } else {
                vel.y = pos.y <= t.y ? double(seekSpeed) : double(-seekSpeed);
                vel.x = 0;
            }
        }
        if (--dropTimer < 1) {
            dropTimer = dropReload;
            dropGruntle(g, pos);
        }
    }
    if (0.08 < double(size))
        size = float(double(size) * 0.95);
}

void recogniser::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 500, 1);
    }
    if (hp < 1) {
        bool odd = neonRand() & 1;
        if (!g->demo)
            g->playSample(odd ? 0x30 : 0x2f);
    }
    size = 0.1f;
}

// ============================================================== hydrant ====

void hydrant::init(theGame *g) {
    type = 7;
    game = g;
    turnReload = 120;
    turnTimer = 0;
    shotReload = 90;
    mode = 0;
    int r = oxRNG::irand(90.0);
    age = 0;
    speed = 0.004f;
    spriteIndex = 0x5b;
    size = 0.001f;
    shotTimer = r + 90;
}

void hydrant::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    float sz = size;
    age++;
    if (sz < 0.025f) { // grows in first
        size = sz + 0.001f;
        return;
    }
    entity *p = g->players;
    spriteIndex = ((age >> 2) & 1) + 0x5b;
    if (--turnTimer < 0) {
        mode ^= 1;
        double v = oxRNG::rand(double(speed) * 0.8, double(speed) * 1.2);
        vel = vec2d(0, -v);
        float ang = 0;
        if (mode == 0) {
            if (p) {
                vec2d t(p->pos.x + oxRNG::rand(-0.3, 0.3), 0);
                t.y = p->pos.y + oxRNG::rand(-0.3, 0.3);
                vec2d d = t - pos;
                ang = float(std::atan2(-d.y, d.x) - kHalfPi);
            }
        } else {
            ang = float(oxRNG::rand(-kPi, kPi));
        }
        vel.rotateBy(double(ang));
        turnTimer = oxRNG::irand(double(float(turnReload / 2))) + turnReload / 2;
    }
    vec2d np = pos + vel;
    if (np.x <= -0.46000000834465027 || 0.44999998807907104 <= np.x)
        vel.x = -vel.x;
    else
        pos.x = np.x;
    if (np.y <= -0.3799999952316284 || 0.44999998807907104 <= np.y)
        vel.y = -vel.y;
    else
        pos.y = np.y;
    if (p && --shotTimer < 1) {
        if (!g->demo)
            g->playSample(0x15);
        shotTimer = shotReload;
        vec2d t(p->pos.x + oxRNG::rand(-0.2, 0.2), 0);
        t.y = p->pos.y + oxRNG::rand(-0.2, 0.2);
        vec2d d = t - pos;
        double h = std::atan2(-d.y, d.x);
        vec2d v(0, -0.008);
        v.rotateBy(double(float(h - kHalfPi)));
        shootableEnemyBullet *b = new shootableEnemyBullet();
        b->init(g);
        b->pos = pos;
        b->vel = v;
        g->addEntity(b, &g->enemyShots);
    }
}

void hydrant::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    g->damage(by, this, 1, 100, 1);
    if (hp < 1 && !g->demo)
        g->playSample(0x2b);
}

// =========================================================== sheepipede ====

void sheepipede::init(theGame *g) {
    game = g;
    spriteIndex = 0x95;
    pos = g->newPosition(0.3f);
    segments = 12;
    hitDelay = 0;
    hp = 5;
    segHp = 5;
    for (auto &t : trail)
        t = pos;
    trailIndex = 0;
    dropReload = 40;
    dropTimer = 40;
    size = 0.06f;
    double sp = oxRNG::rand(0.01, 0.012);
    seekAccelX = 1.2e-4;
    seekAccelY = 1.2e-4;
    drag = vec2d(0.98, 0.98);
    speedLimitY = sp;
    speedLimitX = sp;
    g->rez(this, 2, 1.0f, 0x28);
    type = 0x21;
}

void sheepipede::run() {
    theGame *g = game;
    if (hitDelay > 0)
        hitDelay--;
    g->liveEnemyCount++;
    g->inertialSeek(this, g->players, 2);
    trail[trailIndex] = pos;
    wobble += 0.1f;
    rot = std::sin(wobble) * 0.2f;
    trailIndex = (trailIndex + 1) & 0x1ff;
    if (g->players && --dropTimer < 1) {
        dropTimer = dropReload;
        dropGruntle(g, pos);
    }
}

// Body segments (woolly sheep) follow the head along its trail.
int sheepipede::draw(sprite *s, vec2d sz) {
    theGame *g = game;
    unsigned ti = trailIndex;
    int n = segments;
    float a0 = float(col.a);
    double a = pauseAlpha(g, a0);
    s->angle = 0;
    s->frame = spriteIndex + kLitBase;
    s->col.white(a);
    if (n > 0) {
        unsigned k = ti - unsigned(n) * 4;
        float t = float(ti & 0x1ff) * 0.001953125f * 12.0f;
        for (int i = 0; i < n; i++) {
            float sn = std::sin(t);
            unsigned idx = k & 0x1ff;
            k += 4;
            float m = float(double(sn) * 0.2 + 1.4);
            t = float(double(t) + 0.4);
            s->width = double(m * size) * sz.x;
            s->height = double(m * size) * sz.y;
            s->pos = trail[idx] * sz;
            s->setupChecking();
        }
    }
    col.a = a;
    int r = entity::draw(s, sz);
    col.a = double(a0);
    return r;
}

void sheepipede::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        if (hitDelay == 0) {
            g->damage(by, this, 1, 100, 0);
            vel = vel + by->vel * 0.4;
            double vx = vel.x;
            vel.x = vx * oxRNG::rand(0.8, 1.2);
            double vy = vel.y;
            double r = oxRNG::rand(0.8, 1.2);
            hitDelay = 1;
            vel.y = vy * r;
        }
    }
    if (hp < 1) {
        // The tail sheep breaks off as a beastie to rescue.
        beast *b = new beast();
        b->pos = trail[(trailIndex - unsigned(segments) * 4) & 0x1ff];
        b->initRaw(g);
        g->addEntity(b, &g->beasts);
        if (segments > 0)
            segments--;
        hp = segHp;
        if (segments < 1) {
            g->derez(this, 2, 0.5f, 0x78);
            bool odd = neonRand() & 1;
            if (!g->demo)
                g->playSample(odd ? 0x30 : 0x2f);
        }
    } else if (!g->demo) {
        g->playSample(0x23);
    }
}

// ===================================================== astroSmashCannon ====

void astroSmashCannon::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (0.03 < double(size))
        size = float(double(size) * 0.9);
    if (g->spawnCount > 0)
        return;
    entity *p = g->players;
    if (!p)
        return;
    double px = p->pos.x;
    if (lockX == 0.0) {
        if (pos.y <= p->pos.y)
            pos.y += double(seekSpeed);
        else
            pos.y -= double(seekSpeed);
    }
    if (lockY == 0.0) {
        if (pos.x <= px)
            pos.x += double(seekSpeed);
        else
            pos.x -= double(seekSpeed);
    }
    if (--shotTimer < 1) {
        shotTimer = shotReload;
        genericEnemyBull *b = new genericEnemyBull();
        b->game = g;
        b->hittingShotSurvives = false;
        b->pos = pos;
        b->col.unpackRGBA(uint32_t(rockCols[neonRand() & 3]));
        b->vel = shotVel;
        b->hittingShotSurvives = true;
        b->spriteIndex = shotSprite;
        b->hp = 1;
        g->addEntity(b, &g->enemyShots);
    }
    int reload = rockReload;
    if (reload > 0 && --rockTimer < 1) {
        rockTimer = reload;
        astroSmashRock *r = new astroSmashRock();
        r->initFrom(this);
        g->addEntity(r, &g->enemyShots);
    }
}

void astroSmashCannon::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 150, 0);
    }
    seekSpeed = float(double(seekSpeed) * 1.3);
    if (!g->demo)
        g->playSample(0x38);
    if (hp < 1)
        g->derez(this, 2, 2.5f, 0x3c);
    size = 0.04f;
}

void astroSmashRock::initFrom(astroSmashCannon *c) {
    game = c->game;
    seekTurnRate = float(oxRNG::rand(-0.1, 0.1));
    seekSpeed = float(oxRNG::rand(0.001, 0.003));
    col.unpackRGBA(uint32_t(rockCols[neonRand() & 3]));
    size = 0.04f;
    spriteIndex = 0x8b;
    generations = 2;
    pieces = 2;
    life = 600;
    if (c->f1ac == 0) {
        pos.y = -0.35;
        pos.x = oxRNG::rand(-0.46000000834465027, 0.44999998807907104);
        target.x = oxRNG::rand(-0.46000000834465027, 0.44999998807907104);
        target.y = 0.45;
    }
    vel = (target - pos).normalize() * double(seekSpeed);
}

void astroSmashRock::run() {
    game->liveEnemyCount++;
    rot += seekTurnRate;
    moveEdge(this, 2);
    if (--life < 1)
        dead = true;
}

void astroSmashRock::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 5, 1);
    }
    if (!g->demo)
        g->playSample(0x3b);
    if (hp < 1 && generations > 0 && pieces > 0) {
        for (int k = 0; k < pieces; k++) {
            astroSmashRock *r = new astroSmashRock();
            r->game = g;
            r->col = col;
            r->seekSpeed = seekSpeed;
            r->seekTurnRate = float(oxRNG::rand(-0.1, 0.1));
            r->spriteIndex = 0x8b;
            r->generations = generations - 1;
            r->pieces = pieces + 1;
            r->size = size * 0.6f;
            r->pos = pos;
            r->vel = vel;
            r->life = 600;
            r->vel.rotateBy(oxRNG::rand(-0.7853981635, 0.7853981635));
            g->addEntity(r, &g->enemies);
        }
    }
}

// ================================================================== fhg ====

void fhg::init(theGame *g) {
    game = g;
    pos = g->newPosition(0.3f);
    heading = float(oxRNG::rand(-kPi, kPi));
    if (type == 6) {
        spriteIndex = 0x58;
        turn = float(oxRNG::rand(-0.01, 0.01));
        drops = 6;
        speed = 0.005f;
        turnReload = 100;
        turnTimer = 100;
        dropTimer = 300;
        dropReload = 0x78;
    } else {
        spriteIndex = 0xa8;
        turn = float(oxRNG::rand(-0.02, 0.02));
        speed = 0.008f;
        turnReload = 0x32;
        turnTimer = 0x32;
        dropTimer = 100;
        dropReload = 0x50;
        drops = 8;
    }
    baseSprite = spriteIndex;
    age = 0;
    g->pixelShatter(nullptr, this, vec2d(2, 2), -20, 0, 1, 0, 2);
    dying = false;
    drawDelay = 20;
    dead = false;
    for (auto &t : trail) {
        t.pos = pos;
        t.alpha = 0;
    }
    trailDist = 0;
    trailIndex = 0;
}

void fhg::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (drawDelay != 0 || dying)
        return;
    vec2d old = pos;
    age++;
    int f = (age >> 1) & 3;
    if (f == 3)
        f = 1;
    spriteIndex = f + baseSprite;
    if (g->spawnCount > 0)
        return;
    vel = vec2d(0, double(speed));
    vel.rotateBy(double(heading));
    heading += turn;
    if (--turnTimer < 1) {
        turnTimer = turnReload;
        turn = float(oxRNG::rand(-0.03, 0.03));
    }
    vec2d np = pos + vel;
    if (np.x <= -0.46000000834465027 || 0.44999998807907104 <= np.x)
        heading += turn + turn;
    else
        pos.x = np.x;
    if (np.y <= -0.3799999952316284 || 0.44999998807907104 <= np.y)
        heading += turn + turn;
    else
        pos.y = np.y;
    if (drops < 1)
        return;
    if (--dropTimer < 1) {
        drops--;
        dropTimer = dropReload;
        hydrant *h = new hydrant();
        h->init(g);
        h->pos = pos;
        g->addEntity(h, &g->enemies);
        if (!g->demo)
            g->playSample(0x27);
    }
    float d = float(double(trailDist) + (pos - old).length());
    trailDist = d;
    if (0.001953125f <= d) {
        trailDist = 0;
        trail[trailIndex].pos = pos;
        trail[trailIndex].alpha = 1.0;
        float r = rot;
        trailRot[trailIndex] = r;
        trailIndex = (trailIndex + 1) & 0x3ff;
        rot = r + 0.1f;
    }
}

int fhg::draw(sprite *s, vec2d sz) {
    s->frame = 0x61;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    double fade = (game->levelFlags & 0x40) ? double(0.5f) : double(0.93f);
    float a0 = float(col.a);
    double a = pauseAlpha(game, a0);
    if (!drawTrail(s, sz, trail, trailIndex, a, fade, true, trailRot)) {
        if (dying)
            dead = true;
    }
    if (!dying)
        entity::draw(s, sz);
    col.a = double(a0);
    return 6;
}

void fhg::hitBy(entity *by) {
    if (dying)
        return;
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 1000, 1);
    }
    if (!g->demo)
        g->playSample(0x16);
    dead = false;
    dying = true;
    littleBonus *lb = new littleBonus();
    lb->spriteIndex = 0x48;
    lb->game = g;
    g->addScore(1000);
    lb->growthPerFrame = 1.0f;
    lb->framesRemaining = 300;
    lb->paletteIndex = 0;
    lb->size = 0.02f;
    lb->scaleY = 1.1;
    lb->vel.y = -0.0005;
    lb->pos = pos;
    g->addEntity(lb, &g->particles);
}

// =========================================================== falseTeeth ====

void falseTeeth::init(theGame *g) {
    game = g;
    if (type == 0x24) {
        r1 = 0.3f;
        a4 = 0;
        a3 = 0;
        da2 = 0.02f;
        da1 = 0.02f;
        r2 = 0;
        da3 = 0.06f;
        a1 = 0;
        a2 = 0;
        da4 = 0.07f;
    } else {
        a4 = 0;
        a3 = 0;
        a1 = 0;
        a2 = 0;
        da2 = -0.03f;
        da1 = -0.03f;
        r2 = 0.1f;
        da3 = -0.04f;
        r1 = 0.1f;
        da4 = 0.05f;
    }
    pos.x = double(std::sin(a1) * r1);
    spriteIndex = 0x99;
    size = 0.04f;
    pos.y = double(std::cos(a2) * r1);
    g->rez(this, 2, 1.0f, 0x1e);
    trailDist = 0;
    trailIndex = 0;
    dying = false;
    hp = 30;
    shotReload = 30;
    shotTimer = 30;
    for (auto &t : trail) {
        t.pos = pos;
        t.alpha = 0;
    }
}

void falseTeeth::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (dying)
        return;
    a1 += da1;
    a2 += da2;
    a3 += da3;
    a4 += da4;
    vec2d off(double(std::sin(a3) * r2), double(r2 * std::cos(a4)));
    vec2d old = pos;
    pos.x = double(std::sin(a1) * r1);
    pos.y = double(std::cos(a2) * r1);
    rot = a1 + a1;
    pos = pos + off;
    if (0.04f < size)
        size *= 0.9f;
    addTrail(trail, trailIndex, trailDist, pos, old, 0.0078125f);
    if (--shotTimer < 1) {
        if (!g->demo)
            g->playSample(0x38);
        shotTimer = shotReload;
        size = 0.05f;
        entity *p = g->players;
        int c = rockCols[neonRand() & 3];
        vec2d at = p ? p->pos : vec2d(0, 0);
        g->shootFrom(this, at - pos, type == 0x24 ? 0 : 5, 0x9a, 2, 0.02f, uint32_t(c));
    }
}

int falseTeeth::draw(sprite *s, vec2d sz) {
    s->frame = 0x32;
    s->width = double(size) * sz.x;
    s->height = double(size) * sz.y;
    float a0 = float(col.a);
    double a = pauseAlpha(game, a0);
    if (!drawTrail(s, sz, trail, trailIndex, a, 0.93, false, nullptr)) {
        if (dying)
            dead = true;
    }
    col.a = a;
    if (!dying)
        entity::draw(s, sz);
    col.a = double(a0);
    return 6;
}

void falseTeeth::hitBy(entity *by) {
    if (dying)
        return;
    theGame *g = game;
    bool killed = false;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 250, 1);
        if (hp < 1) {
            dead = false;
            dying = true;
            killed = true;
            bool odd = neonRand() & 1;
            if (!g->demo)
                g->playSample(odd ? 0x30 : 0x2f);
        }
    }
    if (!killed && !g->demo)
        g->playSample(0x14);
    size = 0.06f;
    if (type == 0x24) {
        da1 *= 1.02f;
        da2 *= 1.03f;
        da3 *= 0.98f;
        da4 *= 1.04f;
        r2 = float(double(r2) + 0.001);
    } else {
        da1 *= 1.05f;
        da2 *= 1.02f;
        da3 *= 1.03f;
        da4 *= 1.04f;
        r2 = float(double(r2) + 0.0015);
    }
    if (10 < shotReload)
        shotReload--;
}

// ============================================================ kevinToms ====

void kevinToms::init(theGame *g) {
    r1 = 0.3f;
    a5 = 0;
    a4 = 0;
    a3 = 0;
    a2 = 0;
    game = g;
    a1 = 0;
    da2 = 0.02f;
    da1 = 0.02f;
    da5 = 0.23f;
    r2 = 0;
    da3 = 0.06f;
    da4 = 0.07f;
    pos.x = double(std::sin(0.0f) * r1);
    baseSprite = 0xad;
    spriteIndex = 0xad;
    seekTurnRate = 0.02f;
    size = 0.04f;
    pos.y = double(std::cos(0.0f) * r1);
    g->rez(this, 2, 1.0f, 0x1e);
    shotReload = 0x78;
    burst = 5;
    shotTimer = 0x78;
    trailDist = 0;
    trailIndex = 0;
    hp = 30;
    phase = 0;
    type = 0x33;
    dying = false;
    for (auto &t : trail) {
        t.pos = pos;
        t.alpha = 0;
    }
}

void kevinToms::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (dying)
        return;
    a1 += da1;
    a2 += da2;
    a3 += da3;
    a4 += da4;
    a5 += da5;
    vec2d old = pos;
    int base = baseSprite;
    if (--shotTimer < 0)
        shotTimer = shotReload;
    spriteIndex = base;
    if (shotTimer < burst) {
        // A sweeping spray of shots.
        spriteIndex = base + 1;
        vec2d v(0, 0.002);
        v.rotateBy(double(std::sin(a5)) * kPi);
        g->shootFrom(this, v, 7, 0xaf, 0, 0.03f, 0xffffffffu);
    }
    if (phase == 0) {
        vec2d off(double(std::sin(a3) * r2), double(r2 * std::cos(a4)));
        pos.x = double(std::sin(a1) * r1);
        pos.y = double(std::cos(a2) * r1);
        pos = pos + off;
        rot = std::sin(a1 * 3.0f) * 0.3f;
    } else if (phase == 1) {
        if (stun == 0) {
            g->vectorSeek(this, g->players, 0);
        } else {
            stun--;
            moveEdge(this, 0);
        }
    }
    addTrail(trail, trailIndex, trailDist, pos, old, 0.0078125f);
}

int kevinToms::draw(sprite *s, vec2d sz) {
    s->frame = 0x32;
    s->width = sz.x * 0.02;
    s->height = sz.y * 0.02;
    float a0 = float(col.a);
    double a = pauseAlpha(game, a0);
    if (!drawTrail(s, sz, trail, trailIndex, a, 0.93, false, nullptr)) {
        if (dying)
            dead = true;
    }
    col.a = a;
    if (!dying)
        entity::draw(s, sz);
    col.a = double(a0);
    return 6;
}

// Two lives: the first death frees it to chase the minotaur.
void kevinToms::hitBy(entity *by) {
    if (dying)
        return;
    theGame *g = game;
    bool hurt = false; // the shot counted (no "clang" sample)
    if (by) {
        killIfPlayer(by);
        if (stun < 1) {
            g->damage(by, this, 1, 5000, 1);
            if (hp < 1) {
                dead = false;
                int ph = phase;
                if (ph == 0) {
                    seekSpeed = 0.01f;
                    seekHeading = float(oxRNG::rand(-kPi, kPi));
                    da1 = 0.04f;
                    if (!g->demo)
                        g->playSample(0xe);
                    stun = 0;
                    hp = 0x32;
                    phase = 1;
                } else if (ph == 1) {
                    dying = true;
                    hurt = true;
                    bool odd = neonRand() & 1;
                    if (!g->demo) {
                        g->playSample(odd ? 0x30 : 0x2f);
                        if (g->goalAllowed(8))
                            g->goal(3, 8, 7);
                        hurt = false;
                    }
                }
            }
        }
    }
    if (!hurt && !g->demo)
        g->playSample(0x14);
    if (stun < 1) {
        float sz;
        if (phase == 0) {
            burst += 2;
            if (!g->demo)
                g->playSample(0x39);
            sz = size;
        } else {
            seekTurnRate = float(double(seekTurnRate) + 0.002);
            if (by) {
                seekHeading = float(oxRNG::rand(-kPi, kPi));
                vel = vel + by->vel * 0.1;
            }
            unsigned r = neonRand();
            da5 = float(double(da5) - 0.02);
            stun = int(r & 1) + 3;
            if (!g->demo)
                g->playSample(0xd);
            sz = float(double(size) + 0.002);
            size = sz;
        }
        size = sz + 0.002f;
        da1 = float(double(da1) * 1.04);
        da2 = float(double(da2) * 1.05);
        da3 = float(double(da3) * 0.97);
        da4 = float(double(da4) * 1.05);
        r1 = float(double(r1) * 1.001);
        r2 = float(double(r2) + 0.0015);
    }
}

// ============================================================ brainBull ====

void brainBull::init(theGame *g) {
    game = g;
    seekSpeed = 0.005f;
    if ((neonRand() & 1) == 0) {
        unsigned r = neonRand();
        axis = 1;
        vel.y = double((r & 1) == 0 ? -seekSpeed : seekSpeed);
    } else {
        unsigned r = neonRand();
        axis = 0;
        vel.x = double((r & 1) == 0 ? -seekSpeed : seekSpeed);
    }
    spriteIndex = 0x7c;
    baseSprite = 0x7c;
    size = 0.02f;
    turnReload = 0x1e;
    int t = oxRNG::irand(30.0);
    type = 0x18;
    dying = false;
    dead = false;
    scaleY = 1.4;
    scaleX = 1.4;
    turnTimer = t + 0x1e;
    for (auto &tp : trail) {
        tp.pos = pos;
        tp.alpha = 0;
    }
    trailDist = 0;
    trailIndex = 0;
}

void brainBull::run() {
    if (dying)
        return;
    theGame *g = game;
    g->liveEnemyCount++;
    vec2d old = pos;
    entity *p = g->players;
    double px = p ? p->pos.x : 0.0;
    double py = p ? p->pos.y : 0.0;
    moveEdge(this, 2);
    turnTimer--;
    rot += 0.1f;
    float sn = std::sin(rot);
    age++;
    spriteIndex = baseSprite + int((unsigned(age) & 0xf) >> 2);
    double sc = (double(sn) * 0.5 + 0.5) * 0.5 + 1.3;
    scaleX = sc;
    scaleY = sc;
    if (turnTimer < 1) {
        int r = turnReload;
        int t = oxRNG::irand(double(float(r)));
        axis ^= 1;
        turnTimer = t + r;
        if (axis == 0) {
            vel.y = 0;
            vel.x = double(px < pos.x ? -seekSpeed : seekSpeed);
        } else {
            vel.x = 0;
            vel.y = double(py < pos.y ? -seekSpeed : seekSpeed);
        }
    }
    addTrail(trail, trailIndex, trailDist, pos, old, 0.0078125f);
}

int brainBull::draw(sprite *s, vec2d sz) {
    s->frame = 0x32;
    float a0 = float(col.a);
    double a = pauseAlpha(game, a0);
    s->width = double(size) * sz.x;
    s->height = double(size) * sz.y;
    if (!drawTrail(s, sz, trail, trailIndex, a, 0.93, false, nullptr)) {
        if (dying)
            dead = true;
    }
    col.a = a;
    if (!dying)
        entity::draw(s, sz);
    col.a = double(a0);
    return 6;
}

void brainBull::hitBy(entity *by) {
    if (dying)
        return;
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 50, 0);
    }
    if (hp < 1) {
        g->derez(this, 0, 3.0f, 0x3c);
        if (!g->demo)
            g->playSample(0x2d);
        dead = false;
        dying = true;
    }
}

} // namespace Neon_Internals
} // namespace NEON_GAME
