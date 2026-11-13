// ent_enemies.cpp - the enemies spawned by theGame::genLevel (part 1:
// gruntles, mandies, robots, lasers, blobbies, skiers and friends) and
// the enemy bullets.
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

// pos += vel with the arena edges (x -0.46..0.45, y -0.38..0.45).
// edge: 0 stop, 1 die, 2 bounce by elasticity.
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

float headingOf(vec2d v) {
    return float(std::atan2(-v.y, v.x) - kHalfPi);
}
} // namespace

// ============================================================== gruntle ====

void gruntle::setup(theGame *g) {
    type = 4;
    game = g;
    timer = oxRNG::irand(double(float(g->gruntDelay)));
    gfx = gruntBaseGraphic[g->level];
    spriteIndex = gfx;
    animCount = 0;
    animType = gruntAnimationType[g->level];
    speed = g->gruntSpeed;
    size = 0.025f;
    scaleY = 1.3;
    int idx = -1;
    if (gfx == 0xb9) {
        idx = 0;
    } else {
        for (int i = 1; kGruntScaleTypes[i] > -1; i++) {
            if (kGruntScaleTypes[i] == gfx) {
                if (gfx == 0xd8)
                    scaleX = -1.3;
                idx = i;
                break;
            }
        }
    }
    if (idx >= 0)
        scaleY = double(kGruntScaleY[idx]);
    hp = (g->levelFlags & 0x20) ? 3 : 1;
    baseScX = scaleX;
    baseScY = scaleY;
    timerReload = g->gruntDelay;
    blob = false;
}

void gruntle::init(theGame *g) {
    setup(g);
    pos = g->newPosition(0.2f);
    g->pixelShatter(nullptr, this, vec2d(2, 2), -20, 0, 1, 0, 2);
    drawDelay = 20;
    dead = false;
}

void gruntle::run() {
    theGame *g = game;
    if (!blob)
        g->liveEnemyCount++;
    if (drawDelay != 0)
        return;
    if (g->spawnCount > 0)
        return;
    if (blob) {
        if (g->levelEnding)
            g->derez(this, 0, 3.0f, 0x3c);
        col = g->gcol[1];
        rot = float(double(rot) + 0.1);
        moveEdge(this, 2);
        return;
    }
    if (--timer > 0)
        return;
    if (type == 4) {
        timer = g->gruntDelay;
        speed = g->gruntSpeed;
    } else {
        timer = timerReload;
    }
    bool face = false;
    switch (animType) {
    case 0:
        scaleX = -scaleX;
        break;
    case 1:
        animCount++;
        spriteIndex = gfx + int(animCount & 1);
        break;
    case 2:
        animCount++;
        spriteIndex = gfx + int(animCount & 1);
        face = true;
        break;
    case 3:
        animCount++;
        rot = (animCount & 1) ? -0.1f : 0.1f;
        break;
    case 4:
        animCount++;
        size = (animCount & 1) ? 0.0325f : 0.025f;
        break;
    case 5:
        animCount++;
        size = (animCount & 1) ? 0.0325f : 0.025f;
        face = true;
        break;
    case 6:
        animCount++;
        rot = (animCount & 1) ? -0.1f : 0.1f;
        face = true;
        break;
    }
    if (face)
        scaleX = vel.x < 0.0 ? baseScX : -baseScX;
    entity *p = g->players;
    if (p && p->drawDelay == 0) {
        vel = (p->pos - pos).normalize() * double(speed);
        pos = pos + vel;
        if (type == 4) {
            g->gruntSpeed += g->stepSpeedAdd;
            if (g->maxGruntSpeed < g->gruntSpeed)
                g->gruntSpeed = g->maxGruntSpeed;
        }
    }
}

void gruntle::hitBy(entity *by) {
    theGame *g = game;
    bool split = true;
    if (by) {
        if (by->type == 0x31)
            split = false;
        else if (by->type == 3)
            static_cast<playerMinotaur *>(by)->kill();
        if (!blob)
            g->damage(by, this, 1, 20, 1);
        if ((g->levelFlags & 0x80) && hp < 1) {
            skateboard *s = new skateboard();
            s->launch(g, by->vel);
            s->pos = by->pos;
            g->addEntity(s, &g->enemyShots);
        }
    }
    if (!g->demo)
        g->playSample(6);
    if (hp > 0)
        split = false;
    if (split) {
        unsigned f = g->levelFlags;
        if (f & 1) {
            g->spinySplit(this);
            f = g->levelFlags;
        }
        if (f & 2) {
            g->zombeastSplit(this);
            f = g->levelFlags;
        }
        if (f & 0x200) {
            g->swarmerSplit(this);
            f = g->levelFlags;
        }
        if (f & 0x100) {
            beachball *b = new beachball();
            b->stun = 0;
            b->spriteIndex = 0xa6;
            b->baseSprite = 0xa6;
            b->game = g;
            double sp = oxRNG::rand(0.016, 0.022);
            b->speedLimitY = sp;
            b->speedLimitX = sp;
            b->seekAccelX = 2.5e-05;
            b->seekAccelY = 2.7e-05;
            b->pos = pos;
            b->type = 0x2a;
            b->moveType = 3;
            b->size = 0.03f;
            b->baseSize = 0.03f;
            b->stunDrag = 0.917f;
            b->hp = 2;
            g->addEntity(b, &g->enemyShots);
        }
    }
    if (blob)
        return;
    if (hp < 1) {
        g->gruntSpeed *= g->gruntSpeedMul;
        if (g->maxGruntSpeed < g->gruntSpeed)
            g->gruntSpeed = g->maxGruntSpeed;
        if (minGruntDelay[g->level] < g->gruntDelay)
            g->gruntDelay--;
    }
    if ((g->levelFlags & 4) && g->hotShotFrames < 1 && by) {
        // A "blob" level: the shot gruntle turns into a bouncing blob.
        dead = false;
        auto r = []() { return (neonRand() & 1) ? -0.003 : 0.003; };
        if (by->vel.x == 0.0) {
            vel.y = 0;
            vel.x = r();
        }
        if (by->vel.y == 0.0) {
            vel.y = r();
            vel.x = 0;
        }
        if (by->vel.x != 0.0 && by->vel.y != 0.0) {
            if ((neonRand() & 1) == 0) {
                vel.x = 0;
                vel.y = r();
            } else {
                vel.x = r();
                vel.y = 0;
            }
        }
        spriteIndex = 0x89;
        size = 0.02f;
        scaleX = 1.0;
        scaleY = 1.0;
        blob = true;
    }
}

// ================================================================ spiny ====

void spiny::init(theGame *g) {
    type = 9;
    game = g;
    f1a0 = 0.005f;
    int t = oxRNG::irand(10.0);
    timerReload = 10;
    gfx = 0x5e;
    spriteIndex = 0x5e;
    speed = 0.01f;
    animType = 2;
    animCount = 0;
    timer = t;
    pos = g->newPosition(0.2f);
    size = 0.025f;
    scaleY = 1.1;
    g->pixelShatter(nullptr, this, vec2d(2, 2), -20, 0, 1, 0, 2);
    drawDelay = 20;
    dead = false;
    hp = 2;
    blob = false;
    baseScX = scaleX;
    baseScY = scaleY;
}

void spiny::hitBy(entity *by) {
    if (by) {
        killIfPlayer(by);
        game->damage(by, this, 1, 50, 1);
    }
    if (hp < 1)
        game->spinySplit(this);
}

// ================================================================ mandy ====

void mandy::init(theGame *g) {
    type = 10;
    game = g;
    pos = g->newPosition(0.3f, 0.0f, 0.09f);
    spriteIndex = 0x62;
    size = 0.08f;
    facingLocked = 0;
    speedLimitX = oxRNG::rand(0.0005) + 0.004;
    double r = oxRNG::rand(0.0005);
    accelX = 2e-05;
    accelY = 2e-05;
    drag = vec2d(1.0, 1.0);
    hp = 20;
    baseScaleX = scaleX;
    baseScaleY = scaleY;
    speedLimitY = r + 0.004;
    g->rez(this, 3, 2.0f, 0x1e);
}

void mandy::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    float sz = size;
    if (drawDelay != 0)
        drawDelay--;
    entity *p = g->players;
    age++;
    if (0.08 < double(sz))
        size = float(double(sz) * 0.95);
    if (g->spawnCount > 0)
        return;
    if (facingLocked == 0) {
        vec2d d = p ? p->pos - pos : pos * -1.0;
        scaleX = d.x < 0.0 ? baseScaleX : -baseScaleX;
    }
    vec2d d = p ? p->pos - pos : vec2d(0, 0) - pos;
    float vx = float(vel.x);
    double nx = d.x < 0.0 ? double(vx) - accelX : double(vx) + accelX;
    if (double(std::fabs(float(nx))) < speedLimitX)
        vel.x = double(float(nx));
    float vy = float(vel.y);
    double ny = d.y < 0.0 ? double(vy) - accelY : double(vy) + accelY;
    if (double(std::fabs(float(ny))) < speedLimitY)
        vel.y = double(float(ny));
    pos = pos + vel;
    double s = double(size);
    if (pos.x - s < -0.46000000834465027 || 0.44999998807907104 < pos.x + s) {
        vel.x = -vel.x;
        pos.x += vel.x;
    }
    if (pos.y - s < -0.3799999952316284 || 0.44999998807907104 < pos.y + s) {
        vel.y = -vel.y;
        pos.y += vel.y;
    }
    vel = vel * drag;
}

// Two layers: the body (frame +2) and the face (frame +3), both coloured.
int mandy::draw(sprite *s, vec2d sz) {
    if (drawDelay != 0)
        return 0;
    float a0 = float(col.a);
    double a = (game->paused || game->demo) ? 0.30000001192092896 : double(a0);
    int ct;
    if (flashFrames == 0) {
        ct = 4;
    } else {
        flashFrames--;
        ct = 10;
    }
    s->col = getDynamicColour(ct, 1.0f, age);
    s->col.a = a;
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    s->col = getDynamicColour(flashFrames == 0 ? 0 : 0xd, 1.0f, age);
    s->col.a = a;
    s->pos = pos * sz;
    s->frame += 1;
    s->setupChecking();
    col.a = double(a0);
    return 12;
}

void mandy::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    g->damage(by, this, 1, 150, 0);
    if (hp < 1) {
        g->derez(this, 3, 3.0f, 0x1e);
        if (!g->demo)
            g->playSample(0xe);
    } else {
        size = 0.1f;
        if (!g->demo)
            g->playSample(0xd);
        vel = vel + by->vel * 0.15;
    }
}

void shootingMandy::run() {
    mandy::run();
    theGame *g = game;
    if (drawDelay != 0 || g->spawnCount > 0 || !g->players)
        return;
    if (shotReload > -1 && --shotTimer < 1) {
        // A spinning bolt straight ahead.
        double sx = double(shotSpeed);
        shotTimer = shotReload;
        if (0.0 < scaleX)
            sx = -sx;
        shootableEnemyBullet *b = new shootableEnemyBullet();
        b->init(g);
        b->size = 0.01f;
        b->spriteIndex = 0x60;
        b->baseSprite = 0x60;
        double h = std::atan2(-0.0, sx);
        b->spinPerFrame = 0;
        b->scaleY = 2.0;
        b->edgeBehavior = 1;
        b->killingShotSurvives = true;
        b->rot = float(h - kHalfPi);
        b->flightMode = 0;
        b->pos = pos;
        b->vel = vec2d(sx, 0);
        g->addEntity(b, &g->enemyShots);
    }
    if (rockReload > -1 && --rockTimer < 1) {
        rockTimer = rockReload;
        double sx = oxRNG::rand(0.004, 0.006);
        if (scaleX < 0.0)
            sx = -sx;
        int c = rockCols[neonRand() & 3];
        g->shootFrom(this, vec2d(sx, 0), 4, 0xa0, 1, 0.015f, uint32_t(c));
    }
}

// ========================================================== sixteenTons ====
// The weight: tracks the minotaur along the top, drops when above it.

void sixteenTons::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || drawDelay != 0)
        return;
    entity *p = g->players;
    if (!p || p->drawDelay != 0)
        return;
    double px = p->pos.x;
    double x = pos.x;
    if (px <= x)
        pos.x = x - double(trackSpeed);
    else
        pos.x = x + double(trackSpeed);
    if (!falling) {
        double y = pos.y;
        if (std::fabs(float(px - x)) < 0.08f && y < p->pos.y) {
            falling = true;
            vel.y = 0;
        }
        if (-0.28 < y)
            pos.y = y - double(riseSpeed);
    } else {
        vel.y += double(fallAccel);
        pos.y += vel.y;
        if (0.36 < pos.y) {
            falling = false;
            g->shakeFrames = 10;
            if (!g->demo)
                g->playSample(0x1d);
        }
    }
    if (g->levelEnding)
        g->derez(this, 2, 4.0f, 0x1e);
}

void sixteenTons::hitBy(entity *by) {
    killIfPlayer(by);
}

// =============================================================== brolly ====

void brolly::init(theGame *g) {
    closed = false;
    type = 0xd;
    spriteIndex = 0x65;
    game = g;
    vec2d v(0, 0.005);
    v.rotateBy(oxRNG::rand(6.2831853072));
    vel = v;
    pos = g->newPosition(0.3f);
    size = 0.04f;
    scaleX = 0.2;
    rainReload = 30;
    rainTimer = oxRNG::irand(30.0) + 30;
    g->rez(this, 0, 1.0f, 0x1e);
}

void brolly::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || !g->players)
        return;
    if (closed) {
        if (g->levelEnding)
            g->derez(this, 0, 1.0f, 0x1e);
        return;
    }
    g->liveEnemyCount++;
    pos = pos + vel;
    if (pos.x < -0.46000000834465027 || 0.44999998807907104 < pos.x) {
        vel.x = -vel.x;
        pos.x += vel.x;
    }
    if (pos.y < -0.3799999952316284 || 0.44999998807907104 < pos.y) {
        vel.y = -vel.y;
        pos.y += vel.y;
    }
    if (--rainTimer < 1) {
        rainTimer = rainReload;
        raindrop *r = new raindrop();
        r->type = 0xe;
        r->pos.y = -0.36;
        r->game = g;
        r->pos.x = oxRNG::rand(-0.3600000083446503, 0.34999998807907107);
        r->spriteIndex = 0x67;
        r->rot = g->driftAngle;
        r->size = 0.01f;
        r->scaleY = -2.0;
        g->addEntity(r, &g->enemyShots);
    }
}

// Touched by the minotaur, the brolly folds up (and stops raining).
void brolly::hitBy(entity *by) {
    if (!by || by->type != 3)
        return;
    if (!closed) {
        closed = true;
        spriteIndex++;
        scaleX = 1.0;
    }
}

// =============================================================== lasers ====

void hlaser::run() {
    theGame *g = game;
    if (g->spawnCount > 0)
        return;
    playerMinotaur *p = static_cast<playerMinotaur *>(g->players);
    if (!p)
        return;
    if (g->levelEnding) {
        if (rot <= 0.01f)
            g->derez(this, 2, 1.0f, 0x1e);
        else
            g->derez(this, 1, 1.0f, 0x1e);
    }
    vec2d d = p->pos - pos;
    float vx = float(vel.x);
    double nx = d.x < 0.0 ? double(vx) - accelX : double(vx) + accelX;
    if (double(std::fabs(float(nx))) < speedLimitX)
        vel.x = double(float(nx));
    float vy = float(vel.y);
    double ny = d.y < 0.0 ? double(vy) - accelY : double(vy) + accelY;
    if (double(std::fabs(float(ny))) < speedLimitY)
        vel.y = double(float(ny));
    vec2d np = pos + vel;
    double s = double(size);
    if (np.x - s <= -0.46000000834465027 || 0.44999998807907104 <= np.x + s)
        vel.x = 0;
    else
        pos.x = np.x;
    if (np.y - s <= -0.3799999952316284 || 0.44999998807907104 <= np.y + s)
        vel.y = 0;
    else
        pos.y = np.y;
    vel = vel * drag;
    // beamCycleFrames counts down to beamFireFrames: the beams fire.
    if (g->beamCycleFrames <= g->beamFireFrames) {
        if (g->beamCycleFrames == g->beamFireFrames) {
            g->fb90[12] = 1.0f;
            if (!g->demo)
                g->playSample(0x13);
        }
        double dd = 0.01f < rot ? p->pos.x - pos.x : p->pos.y - pos.y;
        if (std::fabs(float(dd)) < 0.03f)
            p->kill();
    }
}

void hlaser::hitBy(entity *by) {
    theGame *g = game;
    vec2d push(0, 0);
    if (!by) {
        if (g->players)
            push = (g->players->pos - pos).normalize() * 0.02;
    } else if (by->type == 0x31 || by->type == 0xf) {
        push = by->vel * 0.09;
    }
    flashFrames = 0x1e;
    vec2d nv = vel + push;
    if (std::fabs(nv.x) < speedLimitX)
        vel.x = nv.x;
    if (std::fabs(nv.y) < speedLimitY)
        vel.y = nv.y;
    if (!g->demo)
        g->playSample(0x22);
}

// The emitter, then (in the last 64 frames before firing) a beam on each
// side: a thin blue warning line, then the flickering laser itself.
int hlaser::draw(sprite *s, vec2d sz) {
    if (drawDelay != 0)
        return 0;
    theGame *g = game;
    age++;
    float a0 = float(col.a);
    double a = (g->paused || g->demo) ? 0.30000001192092896 : double(a0);
    if (flashFrames == 0) {
        s->col.white(1.0);
    } else {
        flashFrames--;
        s->col = getDynamicColour(10, 1.0f, age);
    }
    s->col.a = a;
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    col.a = double(a0);
    int cd = g->beamCycleFrames - g->beamFireFrames;
    if (cd > 0x40)
        return 6;
    double x1 = pos.x + 0.5;
    float w = float(x1 - (double(size) + 0.01));
    s->pos.x = (double(w * 0.5f) - 0.495) * sz.x;
    s->width = double(w) * sz.x;
    if (cd < 1) {
        s->col = getDynamicColour(0xd, 1.0f, age);
        s->height *= (age & 2) ? 0.3 : 0.1;
    } else {
        double k = double(1.0f - float(cd) * 0.015625f);
        s->col.blue(k * 0.8);
        s->height *= k * 0.2 + 0.05;
    }
    s->col.a = a;
    s->frame += 2;
    s->setupChecking();
    double sd = double(size);
    float w2 = float((1.0 - (pos.x + 0.5)) - (sd + 0.02));
    s->pos.x = (double(w2 * 0.5f) + sd + pos.x + 0.005) * sz.x;
    s->width = double(-w2) * sz.x;
    s->setupChecking();
    return 12;
}

int vlaser::draw(sprite *s, vec2d sz) {
    if (drawDelay != 0)
        return 0;
    theGame *g = game;
    age++;
    double a0 = double(float(col.a));
    double a = (g->paused || g->demo) ? 0.30000001192092896 : a0;
    if (flashFrames == 0) {
        s->col.white(1.0);
    } else {
        flashFrames--;
        s->col = getDynamicColour(10, 1.0f, age);
    }
    s->col.a = a;
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    col.a = a0;
    int cd = g->beamCycleFrames - g->beamFireFrames;
    if (cd > 0x40)
        return 6;
    s->angle = 0;
    double y1 = pos.y + 0.5;
    float h = float(y1 - (double(size) + 0.01 + 0.1));
    s->pos.y = ((double(h * 0.5f) - 0.5) + 0.10500000000000001) * sz.y;
    s->height = double(-h) * sz.y;
    if (cd < 1) {
        s->col = getDynamicColour(0xd, 1.0f, age);
        s->width *= (age & 2) ? 0.3 : 0.1;
    } else {
        double k = double(1.0f - float(cd) * 0.015625f);
        s->col.blue(k * 0.8);
        s->width *= k * 0.2 + 0.05;
    }
    s->col.a = a0;
    s->frame += 1;
    s->setupChecking();
    float h2 = float((1.0 - (pos.y + 0.5)) - (double(size) + 0.02));
    s->pos.y = (double(h2 * 0.5f) + double(size) + pos.y + 0.005) * sz.y;
    s->height = double(h2) * sz.y;
    s->setupChecking();
    return 12;
}

// ============================================================= blobbies ====

void bigBlobby::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || !g->players)
        return;
    g->liveEnemyCount++;
    g->vectorSeek(this, g->players, 0);
    age++;
    if (type == 0x12)
        spriteIndex = (age >> 4) % 3 + baseSprite;
    else
        spriteIndex = baseSprite + ((age >> 3) & 1);
}

void bigBlobby::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 50, 1);
    }
    if (!g->demo)
        g->playSample(0x1a);
    if (hp < 1 && type == 0x12) {
        int n = numBlobs[g->level / 10];
        float fn = float(n);
        float a = -3.1415927f;
        for (int k = 0; k < n; k++) {
            smallBlobby *b = new smallBlobby();
            b->game = g;
            b->type = 0x13;
            b->spriteIndex = 0x6e;
            b->baseSprite = 0x6e;
            b->size = 0.02f;
            b->scaleY = 1.3;
            oxRNG::rand(-kPi, kPi);
            b->seekTurnRate = 0.04f;
            double m = double(float(k) / fn) * 0.4 + 1.0;
            b->seekSpeed = float(m * 0.0018000000854954123);
            b->seekHeading = a;
            b->pos = pos;
            g->addEntity(b, &g->enemies);
            a += 6.2831855f / fn;
        }
    }
}

// =============================================================== spinner ===

void spinner::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || !g->players)
        return;
    g->liveEnemyCount++;
    g->inertialSeek(this, g->players, 2);
    float l = float(vel.length());
    rot += l * (vel.x <= 0.0 ? -10.0f : 10.0f);
}

void spinner::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    g->damage(by, this, 1, 50, 1);
    if (hp < 1 && !g->demo)
        g->playSample(0x3b);
    vel = vel + by->vel * 0.6;
}

// ========================================================== pointyArrow ====

void pointyArrow::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (g->spawnCount > 0 || drawDelay != 0)
        return;
    entity *p = g->players;
    if (!p || p->drawDelay != 0)
        return;
    if (mode == 1) {
        vel = vel * drag;
        moveEdge(this, 2);
        if (--timer < 1)
            mode = 0;
        rot = headingOf(vel);
    } else if (mode == 2) {
        float r = rot;
        if (--timer < 1)
            mode = 0;
        rot = float(double(r) + 0.4);
    } else if (mode == 0) {
        g->moveTowards(this, p, 0);
        mode = 1;
        timer = 100;
        rot = headingOf(vel);
    }
}

void pointyArrow::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        if (mode < 2)
            g->damage(by, this, 1, 50, 1);
    }
    if (!g->demo)
        g->playSample(0x39);
    if (hp > 0 && mode != 2) {
        flashFrames = 0x3c;
        mode = 2;
        timer = 0x3c;
    }
}

// ======================================================== killerRobot ======

void killerRobot::init(theGame *g) {
    game = g;
    prey = nullptr;
    int base;
    double sp = double(runningManSpeeds[g->level / 10]);
    if (type == 0x15) {
        sp = oxRNG::rand(sp * 0.2, sp * 0.4);
        base = 0x71;
        spriteIndex = 0x71;
        seekSpeed = float(sp);
    } else {
        sp = oxRNG::rand(sp * 0.8, sp * 2.4);
        spriteIndex = 0xab;
        hp = 8;
        base = spriteIndex;
        seekSpeed = float(sp);
    }
    baseSprite = base;
    size = 0.02f;
    pos = g->newPosition(0.3f, 0.4f, 0.03f);
    scaleY = 1.5;
    brainChance = 0;
    baseScaleX = scaleX;
    baseScaleY = scaleY;
    g->rez(this, 2, 1.0f, 0x1e);
}

void runningMan::init(theGame *g) {
    hp = 1;
    game = g;
    type = 0x17;
    prey = nullptr;
    int i = g->level / 10;
    float sp = runningManSpeeds[i];
    hp = runningManShields[i];
    seekSpeed = float(oxRNG::rand(double(sp), double(sp + sp)));
    spriteIndex = 0x74;
    baseSprite = 0x74;
    size = 0.025f;
    pos = g->newPosition(0.4f, 0.3f, 0.03f);
    scaleY = 1.5;
    brainChance = g->level / 4 + 10;
    baseScaleX = scaleX;
    baseScaleY = scaleY;
    g->rez(this, 1, 2.0f, 0x1e);
}

void killerRam::init(theGame *g) {
    game = g;
    prey = nullptr;
    type = 0x26;
    double sp = double(runningManSpeeds[g->level / 10]);
    seekSpeed = float(oxRNG::rand(sp * 2.2, sp * 1.4));
    spriteIndex = 0x9e;
    baseSprite = 0x9e;
    size = 0.04f;
    pos = g->newPosition(0.3f, 0.4f, 0.03f);
    brainChance = 0;
    scaleX = -1.0;
    baseScaleX = scaleX;
    baseScaleY = scaleY;
    hp = 10;
    g->rez(this, 2, 1.0f, 0x1e);
}

// Chases the nearest free beastie (or the minotaur): robots and rams
// destroy the beasts they catch, running men turn them into zombeasts and
// now and then throw a brain.
void killerRobot::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || !g->players)
        return;
    if (type == 0x2f)
        g->liveEnemyCount++;
    entity *target;
    beast *b = static_cast<beast *>(prey);
    if (b && !b->rescued) {
        target = b;
    } else {
        prey = nullptr;
        target = nullptr;
        for (entity *e = g->beasts; e; e = e->next) {
            if (e->type != 1)
                continue; // (powerups share the list)
            beast *c = static_cast<beast *>(e);
            if (!c->rescued && c->f164 == 0) {
                c->f164 = 1;
                prey = e;
                target = e;
                break;
            }
        }
        if (!target)
            target = g->players;
    }
    g->moveTowards(this, target, 0);
    age++;
    scaleX = vel.x < 0.0 ? -baseScaleX : baseScaleX;
    if (g->levelEnding)
        g->derez(this, 1, 0.2f, 0x78);
    int t = type;
    if (t == 0x26 || t == 0x15 || t == 0x2f) {
        if (t == 0x26)
            col = g->gcol[2];
        spriteIndex = baseSprite + ((age >> 4) & 1);
        if (prey && targetDistance < 0.01f) {
            g->derez(prey, 2, 1.0f, 0x3c);
            if (!g->demo)
                g->playSample(0x1c);
            g->f1198[0]++;
            prey = nullptr;
        }
        return;
    }
    if (t != 0x17)
        return;
    col = g->gcol[1];
    g->liveEnemyCount++;
    spriteIndex = baseSprite + ((age >> 2) & 7);
    if (prey && targetDistance < 0.01f) {
        g->derez(prey, 3, 0.1f, 0x78);
        g->f1198[0]++;
        zombeast *z = new zombeast();
        z->spriteIndex = 0x80;
        z->baseSprite = 0x80;
        z->game = g;
        z->size = 0.025f;
        z->seekSpeed = 0.0024f;
        z->type = 0x19;
        z->pos = g->newPosition(0.3f);
        z->baseScaleX = z->scaleX;
        z->baseScaleY = z->scaleY;
        z->pos = prey->pos;
        g->addEntity(z, &g->enemies);
        if (neonRand() & 1) {
            if (!g->demo)
                g->playSample(0x2e);
        } else if (neonRand() & 1) {
            if (!g->demo)
                g->playSample(0x26);
        } else {
            if (!g->demo)
                g->playSample(0x27);
        }
        prey = nullptr;
    }
    int chance = brainChance;
    if (int(neonRand() & 0x1fff) < chance) {
        brainBull *bb = new brainBull();
        bb->init(g);
        bb->pos = pos;
        g->addEntity(bb, &g->enemyShots);
        if (!g->demo)
            g->playSample(0x2a);
    }
}

void killerRobot::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    int t = type;
    if (t == 0x2f || t == 0x17) {
        g->damage(by, this, 1, 150, 0);
        if (hp < 1) {
            g->derez(this, 1, 0.2f, 0x78);
            if (!g->demo)
                g->playSample(0x2c);
        } else if (!g->demo) {
            g->playSample(0x29);
        }
        t = type;
    }
    if (t == 0x15) {
        if (!g->demo)
            g->playSample(0x22);
        t = type;
    } else if (t == 0x2f) {
        if (!g->demo)
            g->playSample(0x22);
        size = float(double(size) * 1.1);
        seekSpeed = float(double(seekSpeed) * 1.1);
        t = type;
    }
    if (t == 0x26 && !g->demo)
        g->playSample(0x23);
    flashFrames = 0x14;
    vel = by->vel * 0.4;
    moveEdge(this, 0);
}

// ============================================================= zombeast ====

void zombeast::run() {
    theGame *g = game;
    age++;
    spriteIndex = baseSprite + ((age >> 2) & 1);
    col = getDynamicColour(5, 1.0f, age);
    g->moveTowards(this, g->players, 0);
    scaleX = vel.x <= 0.0 ? baseScaleX : -baseScaleX;
}

void zombeast::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 50, 0);
    }
    if (hp < 1) {
        g->derez(this, 0, 3.0f, 0x3c);
        g->zombeastSplit(this);
    }
}

// ================================================================ lavvy ====
// The toilet: throws toilet rolls; when destroyed it sprays turds.

void lavvy::run() {
    theGame *g = game;
    float sz = size;
    age++;
    if (0.09 < double(sz))
        size = float(double(sz) * 0.9);
    g->liveEnemyCount++;
    spriteIndex = baseSprite + ((age & 0x1f) >> 3);
    if (0x85 < spriteIndex)
        spriteIndex = 0x84;
    g->liveEnemyCount++;
    if (g->spawnCount > 0 || drawDelay != 0)
        return;
    entity *p = g->players;
    if (!p || p->drawDelay != 0)
        return;
    moveEdge(this, 2);
    if (--rollTimer < 1) {
        rollTimer = rollReload;
        if (!g->demo)
            g->playSample(0x18);
        bogroll *b = new bogroll();
        b->game = g;
        b->spriteIndex = 0x86;
        b->baseSprite = 0x86;
        b->seekSpeed = 0.0041f;
        b->seekHeading = float(oxRNG::rand(-kPi, kPi));
        b->seekTurnRate = 0.02f;
        b->type = 0x1c;
        b->pos = pos;
        g->addEntity(b, &g->enemyShots);
    }
}

void lavvy::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 1000, 0);
    }
    size = 0.12f;
    rollTimer = 0x1e;
    if (hp > 0)
        return;
    bool odd = neonRand() & 1;
    if (!g->demo) {
        g->playSample(odd ? 0x30 : 0x2f);
        if (g->goalAllowed(0x10))
            g->goal(4, 0x10, 8);
    }
    for (int k = 0; k < 100; k++) {
        turd *t = new turd();
        t->init(g);
        t->pos = pos;
        g->addEntity(t, &g->enemyShots);
    }
    g->derez(this, 1, 0.5f, 0x3c);
}

// =============================================================== yinYang ===

void yinYang::run() {
    theGame *g = game;
    if (g->spawnCount > 0 || !g->players)
        return;
    g->liveEnemyCount++;
    g->inertialSeek(this, g->players, 2);
    float r = rot;
    mineTimer--;
    rot = float(double(r) + 0.1);
    if (mineTimer < 1) {
        mineTimer = mineReload;
        yyMine *m = new yyMine();
        m->game = g;
        m->spriteIndex = 0x90;
        m->life = 300;
        m->size = 0.01f;
        m->pos = pos;
        g->addEntity(m, &g->enemyShots);
    }
}

void yinYang::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    g->damage(by, this, 1, 50, 1);
    if (hp < 1 && !g->demo)
        g->playSample(0x2d);
    vel = vel + by->vel * 0.2;
}

void yyMine::run() {
    if (--life < 1)
        dead = true;
}

void yyMine::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 5, 1);
    }
    expandaBull *x = new expandaBull();
    x->game = g;
    x->spriteIndex = 0x91;
    x->size = 0.01f;
    x->type = 0x31;
    x->pos = pos;
    g->addEntity(x, &g->projectiles);
    if (!g->demo)
        g->playSample(0x19);
}

// ================================================================ teapot ===

void teapot::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    double vx;
    if (stun > 0) {
        stun--;
        spriteIndex = baseSprite - 1;
        rot = 0.7853982f;
        size = float(double(size) * 1.002);
        vec2d np = pos + vel;
        double sz = double(size);
        vx = vel.x;
        if ((vx > 0.0 && 0.44999998807907104 < sz + np.x) ||
            (vx < 0.0 && np.x - sz < -0.46000000834465027)) {
            vel.x = 0;
            vx = 0;
        } else {
            pos.x = np.x;
        }
        double vy = vel.y;
        if ((vy > 0.0 && 0.44999998807907104 < sz + np.y) ||
            (vy < 0.0 && np.y - sz < -0.3799999952316284))
            vel.y = 0;
        else
            pos.y = np.y;
        drag = vec2d(0.7, 0.7);
    } else {
        if (0.03 < double(size))
            size = float(double(size) * 0.99);
        g->inertialSeek(this, g->players, 2);
        vx = vel.x;
        spriteIndex = baseSprite;
        drag = vec2d(0.998, 0.998);
        rot = 0;
    }
    scaleX = vx < 0.0 ? 1.0 : -1.0;
}

void teapot::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    if (stun == 0) {
        g->damage(by, this, 1, 150, 1);
        vel = vel + by->vel * 0.6;
    }
    stun = 0x1e;
    if (!g->demo)
        g->playSample(0x3a);
}

// ================================================================= skier ===

void skier::init(theGame *g) {
    game = g;
    spriteIndex = 0xa0;
    stun = 0;
    col.red(1.0);
    baseSprite = spriteIndex;
    gfx = spriteIndex;
    seekSpeed = float(oxRNG::rand(0.0011, 0.0036));
    seekTurnRate = float(oxRNG::rand(0.03, 0.07));
    seekHeading = float(oxRNG::rand(-kPi, kPi));
    pos = g->newPosition(0.35f);
    g->rez(this, 1, 1.0f, 0x1e);
    type = 0x25;
    moveType = 0;
    stunDrag = 0.8f;
    size = 0.025f;
    baseSize = 0.025f;
    hp = 5;
}

void skier::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    age++;
    if (g->spawnCount > 0 || !g->players)
        return;
    bool tailA = false;
    if (stun == 0) {
        switch (moveType) {
        case 0:
            spriteIndex = baseSprite;
            rot = float((double(seekHeading) - kHalfPi) - 0.7853981635);
            g->vectorSeek(this, g->players, 0);
            break;
        case 1:
            spriteIndex = baseSprite;
            rot = float(double(seekHeading) - kHalfPi);
            g->vectorSeek(this, g->players, 0);
            break;
        case 2:
            rot = float(std::atan2(-vel.y, vel.x) - kHalfPi);
            g->inertialSeek(this, g->players, 2);
            drag = vec2d(1.0, 1.0);
            break;
        case 3:
            rot = float(double(rot) + 0.2);
            g->inertialSeek(this, g->players, 2);
            drag = vec2d(1.0, 1.0);
            break;
        }
    } else {
        // Knocked back: slides (and spins) with the shot's momentum.
        switch (moveType) {
        case 0:
            rot = float(double(rot) + 0.4);
            spriteIndex = gfx;
            moveEdge(this, 0);
            break;
        case 1:
            rot = 0;
            spriteIndex = baseSprite + 3;
            moveEdge(this, 0);
            break;
        case 2:
            moveEdge(this, 2);
            rot = float(std::atan2(-vel.y, vel.x) - kHalfPi);
            break;
        case 3:
            moveEdge(this, 2);
            rot = float(double(rot) + 0.4);
            break;
        }
        tailA = true;
    }
    if (tailA) {
        vel = vel * drag;
        if (--stun < 1)
            drag = vec2d(1.0, 1.0);
    }
    if (baseSize < size)
        size = float(double(size) * 0.95);
}

void skier::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 150, 1);
        vel = by->vel;
    }
    if (!g->demo)
        g->playSample(hp < 1 ? 0x3b : 0x1b);
    size = baseSize + baseSize;
    stun = 0x1e;
    drag = vec2d(double(stunDrag), double(stunDrag));
}

void licker::init(theGame *g) {
    stun = 0;
    spriteIndex = 0xa1;
    baseSprite = 0xa1;
    game = g;
    seekSpeed = float(oxRNG::rand(0.0021, 0.003));
    seekTurnRate = float(oxRNG::rand(0.027, 0.035));
    seekHeading = float(oxRNG::rand(-kPi, kPi));
    pos = g->newPosition(0.35f);
    g->rez(this, 1, 1.0f, 0x1e);
    moveType = 1;
    size = 0.025f;
    baseSize = 0.025f;
    stunDrag = 0.9f;
    hp = 5;
    trail = 0;
}

// Leaves a mine every 0.02 of distance (type 0x27 still mines, 0x32
// homing ones).
void licker::run() {
    vec2d old = pos;
    skier::run();
    float t = float(double(trail) + (pos - old).length());
    trail = t;
    if (0.02 < double(t)) {
        trail = float(double(t) - 0.02);
        theGame *g = game;
        if (type == 0x27)
            g->shootFrom(this, vec2d(0, 0) - pos, 1, 0x9a, 2, 0.01f, g->gcol[0].packRGBA());
        if (type == 0x32)
            g->shootFrom(this, vec2d(0, 0) - pos, 6, 0x9a, 3, 0.01f, g->gcol[0].packRGBA());
    }
}

void firingBiscuit::run() {
    skier::run();
    if (--shotTimer < 1) {
        theGame *g = game;
        shotTimer = shotReload;
        entity *p = g->players;
        int c = rockCols[neonRand() & 3];
        vec2d at = p ? p->pos : vec2d(0, 0);
        g->shootFrom(this, at - pos, 2, 0xa0, 1, 0.015f, uint32_t(c));
    }
}

// ============================================================== bullets ====

void shootableEnemyBullet::init(theGame *g) {
    type = 8;
    game = g;
    spinPerFrame = 0.2f;
    invulnerableFrames = 2;
    edgeBehavior = 0;
    flightMode = 0;
    baseSprite = 0x5d;
    spriteIndex = 0x5d;
    killingShotSurvives = false;
    framesRemaining = 150;
}

void shootableEnemyBullet::run() {
    theGame *g = game;
    if (invulnerableFrames > 0)
        invulnerableFrames--;
    age++;
    g->liveEnemyCount++;
    rot += spinPerFrame;
    switch (flightMode) {
    case 0:
        moveEdge(this, edgeBehavior);
        break;
    case 1:
        g->vectorSeek(this, g->players, edgeBehavior);
        rot = seekHeading + spriteAngleOffset;
        break;
    case 3:
        g->moveTowards(this, g->players, edgeBehavior);
        // fall through
    case 2:
        moveEdge(this, edgeBehavior);
        spriteIndex = baseSprite + ((age >> 4) & 1);
        break;
    }
    if (--framesRemaining < 0)
        dead = true;
}

void shootableEnemyBullet::hitBy(entity *by) {
    if (invulnerableFrames != 0 || !by)
        return;
    theGame *g = game;
    killIfPlayer(by);
    g->damage(by, this, 1, 0, 1);
    if (hp < 1 && !g->demo)
        g->playSample(7);
    if (killingShotSurvives && hp < 1 && by->type != 3)
        by->dead = false;
}

void genericEnemyBull::run() {
    moveEdge(this, 1);
}

void genericEnemyBull::hitBy(entity *by) {
    if (!by)
        return;
    if (by->type == 3) {
        static_cast<playerMinotaur *>(by)->kill();
        return;
    }
    game->damage(by, this, 1, 5, 1);
    if (hittingShotSurvives)
        by->dead = false;
}

void skateboard::launch(theGame *g, vec2d v) {
    vel = v * 0.4;
    if (vel.length() < 0.001) {
        vel = vec2d(0.001, 0);
        vel.rotateBy(oxRNG::rand(6.2831853072));
    }
    type = 0x23;
    game = g;
    scaleX = 0.4;
    spriteIndex = 0x98;
    double h = std::atan2(-vel.y, vel.x);
    rot = float(h - kHalfPi);
    float r = float(oxRNG::rand(-0.1, 0.1));
    rot = rot + r;
    vel.rotateBy(double(r));
}

void skateboard::run() {
    moveEdge(this, 1);
}

void skateboard::hitBy(entity *by) {
    killIfPlayer(by);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
