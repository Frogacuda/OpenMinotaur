// ent_player.cpp - the minotaur, the helper droid and their shots.
// playerMinotaur::run and ::shoot were re-derived from the disassembly.
#include "game.h"
#include "game_entities.h"
#include "game_tables.h"
#include "neon_audio.h"

namespace NEON_GAME {
namespace Neon_Internals {

namespace {
// Nearest entity of a list (float distance, first wins ties); nullptr if none.
entity *nearest(entity *list, const vec2d &p) {
    entity *best = nullptr;
    float d = 1000.0f;
    for (entity *e = list; e; e = e->next) {
        float l = float((e->pos - p).length());
        if (l < d) {
            best = e;
            d = l;
        }
    }
    return best;
}

double dist(entity *e, const vec2d &p) {
    return (e->pos - p).length();
}

// Snaps an aim vector to one of 8 directions of unit components.
void snapAim(vec2d &a) {
    float r = float(std::fabs(a.y) / (std::fabs(a.x) + 1e-05));
    if (r > 1.5f)
        a.x = 0.0;
    if (r < 0.5f)
        a.y = 0.0;
    if (a.x != 0.0)
        a.x = a.x / std::fabs(a.x);
    if (a.y != 0.0)
        a.y = a.y / std::fabs(a.y);
}

// A touch that moved back towards its start drags the start along (10%).
void dragOrigin(double d, double last, double &start, double now) {
    if (d < 0.0) {
        if (d > last)
            start += (now - start) * 0.1;
    } else if (d > 0.0 && d < last) {
        start += (now - start) * 0.1;
    }
}
} // namespace

// ====================================================== playerMinotaur ====

playerMinotaur::playerMinotaur() {
    spriteIndex = 0;
    scaleY = 1.8;
    drag = vec2d(0.8, 0.8);
    type = 3;
    size = float(double(size) * 1.2);
}

void playerMinotaur::init(theGame *g) {
    game = g;
    g->pixelShatter(nullptr, this, vec2d(1, 1), -60, 1, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(-1, 1), -60, 1, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(0, 1), -60, 1, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(1, 0), -60, 1, 0, 1, 1);

    dead = false;
    drawDelay = 60;
}

int playerMinotaur::draw(sprite *s, vec2d sz) {
    theGame *g = game;
    float yoff = g->isPad ? 0.12f : 0.2f;
    float k = g->isPad ? 1.1f : 1.3f;
    if (g->state == 4 && g->level < 1) {
        // Level 1 shows where the move and aim touches are.
        if (moveCursor) {
            s->col = g->gcol[1];
            s->angle = 0;
            s->frame = 0xdf;
            s->width = sz.x * 0.08 * double(k);
            s->height = double(k) * sz.y * 0.02;
            s->pos = moveTouchPos;
            s->pos.y -= double(yoff);
            s->pos = s->pos * sz;
            s->setupChecking();
        }
        if (g->gameMode != 2 && aimCursor) {
            s->col = g->gcol[2];
            s->angle = 0;
            s->frame = 0xe0;
            s->width = sz.x * 0.08 * double(k);
            s->height = double(k) * sz.y * 0.022;
            s->pos = aimTouchPos;
            s->pos.y -= double(yoff);
            s->pos = s->pos * sz;
            s->setupChecking();
        }
    }
    if (drawDelay < 1) {
        if (!invincible) {
            col.white(1.0);
        } else {
            col = g->gcol[1];
            if (invDT < 0.0f && invT < 2.5f && (age & 2))
                col.blue(0.2);
        }
        double a = col.a;
        if (g->paused || g->demo)
            col.a = double(0.3f);
        int r = entity::draw(s, sz);
        col.a = double(float(a));
        return r;
    }
    drawDelay--;
    col = g->gcol[1];
    if (age & 4)
        return 0;
    s->col = col;
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    if (g->paused || g->demo)
        s->col.a = 0.3;
    s->setupChecking();
    return 6;
}

// Fires the player's shot along aimDir; with the 3-way powerup two more at
// 45 degrees, and with 5-way two more behind those.
void playerMinotaur::shoot() {
    theGame *g = game;
    auto bull = [&](vec2d v, int spr) -> basicBull * {
        basicBull *b = new basicBull();
        b->type = 0xf;
        b->pos = pos;
        b->life = 80;
        b->size = 0.025f;
        b->spriteIndex = spr;
        b->vel = v;
        return b;
    };
    auto flip = [](basicBull *b) {
        if (b->vel.x > 0.0 && b->vel.y < 0.0)
            b->scaleX = -1.0;
        if (b->vel.x < 0.0 && b->vel.y > 0.0)
            b->scaleX = -1.0;
    };
    auto launch = [&](basicBull *b) {
        b->game = g;
        b->pos = b->pos + b->vel;
        g->addEntity(b, &g->projectiles);
    };
    basicBull *m = bull(aimDir * double(g->shotSpeed), 0x31);
    flip(m);
    if (m->vel.x == 0.0) {
        m->spriteIndex = 0x32;
        m->scaleX = 0.4;
    }
    if (m->vel.y == 0.0) {
        m->spriteIndex = 0x33;
        m->scaleY = 0.4;
    }
    launch(m);
    if (!g->spreadShots)
        return;
    bool five = g->fiveWayShots;
    vec2d v = m->vel;
    auto diag = [&](vec2d d) {
        basicBull *b = bull(d, 0x31);
        flip(b);
        launch(b);
    };
    if (m->spriteIndex == 0x32) { // vertical shot: diagonals
        diag(vec2d(v.y, v.y));
        diag(vec2d(-v.y, v.y));
        if (five) {
            diag(vec2d(v.y, v.y) * -1.0);
            diag(vec2d(-v.y, v.y) * -1.0);
        }
    } else if (m->spriteIndex == 0x33) { // horizontal shot: diagonals
        diag(vec2d(v.x, v.x));
        diag(vec2d(v.x, -v.x));
        if (five) {
            diag(vec2d(v.x, v.x) * -1.0);
            diag(vec2d(v.x, -v.x) * -1.0);
        }
    } else if (m->spriteIndex == 0x31) { // diagonal shot: the axes
        basicBull *b = bull(vec2d(0, v.y), 0x32);
        b->scaleX = 0.4;
        launch(b);
        if (five) {
            b = bull(vec2d(0, -v.y), 0x32);
            b->scaleX = 0.4;
            launch(b);
        }
        b = bull(vec2d(v.x, 0), 0x33);
        b->scaleY = 0.4;
        launch(b);
        if (five) {
            b = bull(vec2d(-v.x, 0), 0x33);
            b->scaleY = 0.4;
            launch(b);
        }
    }
}

void playerMinotaur::kill() {
    if (drawDelay != 0)
        return;
    theGame *g = game;
    if (g->state != 4)
        return;
    if (invincible) {
        if (!g->demo) {
            int r1 = neonRand();
            int r2 = neonRand();
            g->playSample(kGruntSamples[(r2 & 1) + (r1 & 3)]);
        }
        return;
    }
    if (g->demo)
        return;
    int n = 0;
    for (entity *e = g->enemies; e; e = e->next)
        if (e->type == 4)
            n++;
    g->playSample(n > 9 ? 10 : 23);
    g->pixelShatter(nullptr, this, vec2d(1, 1), 60, 0, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(-1, 1), 60, 0, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(0, 1), 60, 0, 1, 0, 0);
    g->pixelShatter(nullptr, this, vec2d(1, 0), 60, 0, 0, 1, 1);
    g->deathPauseFrames = 120;
    g->deathsThisRun++;
    g->msgTimer = 20;
    if (g->message != 0)
        g->eraseMessage();
    g->message = 3;
}

void playerMinotaur::run() {
    theGame *g = game;
    age++;
    vec2d saved = pos;

    // Invincibility: the minotaur grows with invT (0..4).
    float t;
    bool clamp = true;
    if (invincible) {
        t = invT + invDT;
        invT = t;
        if (t > 10.0f)
            invDT = -invDT;
        if (t < 0.0f) {
            invincible = false;
            invT = 0;
            t = 0;
            clamp = false;
        }
    } else {
        t = invT;
    }
    if (clamp && t > 4.0f)
        t = 4.0f;
    if (!hard)
        size = float(0.024 + double(t) * 0.01);

    // Auto aim (simplified mode and the demo): the nearest threat.
    vec2d aim(0, 0);
    entity *target = nullptr;
    if (autoAim || g->demo) {
        entity *a = nearest(g->enemies, pos);
        entity *o = nearest(g->obstacles, pos);
        if (a && o && dist(a, pos) > dist(o, pos))
            a = o;
        if (!a)
            a = o;
        entity *s = nearest(g->enemyShots, pos);
        if (a && s) {
            if (dist(a, pos) > dist(s, pos))
                a = s;
        } else if (!a) {
            a = s;
        }
        if (a) {
            target = a;
            aim = a->pos - pos;
        }
    }

    // Which touch moves and which aims (control scheme, saveData + 0xc98).
    int moveT, aimT;
    switch (g->sd(0xc98)) {
    case 1:
        moveT = g->findLeftTouch();
        aimT = g->findRightTouch();
        break;
    case 2:
        aimT = g->findLeftTouch();
        moveT = g->findRightTouch();
        break;
    case 0:
        moveT = g->findOldestTouch(-1);
        aimT = g->findOldestTouch(moveT);
        break;
    default:
        moveT = aimT = -1;
        break;
    }
    const auto &controls = g->controller;
    g->touch(this, g->beasts);
    g->touch(this, g->enemies);
    g->touch(this, g->obstacles);
    g->touch(this, g->enemyShots);

    if (aimT <= -1) {
        // No aim touch: module-supplied digital controller aim or auto aim.
        aimCursor = false;
        lastAim = vec2d(0, 0);
        if (controls.aim.any()) {
            aim.x = (controls.aim.left) ? -0.006000000052154064 : 0.0;
            if (controls.aim.right)
                aim.x = 0.006000000052154064;
            aim.y = (controls.aim.up) ? -0.006000000052154064 : 0.0;
            if (controls.aim.down)
                aim.y = 0.006000000052154064;
        }
        if (target || (controls.fire)) {
            aimDir = aim;
            snapAim(aimDir);
        }
    } else if (!autoAim) {
        // Aim touch: the direction from where it went down.
        vec2d tp = g->touchPos[aimT];
        vec2d d = tp - g->touchStartPos[aimT];
        aimCursor = true;
        aimTouchPos = tp;
        dragOrigin(d.x, lastAim.x, g->touchStartPos[aimT].x, g->touchPos[aimT].x);
        dragOrigin(d.y, lastAim.y, g->touchStartPos[aimT].y, g->touchPos[aimT].y);
        lastAim = d;
        if (d.length() > 0.001) {
            aimDir = d;
            snapAim(aimDir);
        }
    }

    if (moveT <= -1) {
        lastMove = vec2d(0, 0);
        moveTouch = false;
        moveCursor = false;
    } else {
        // Move touch: a virtual joystick around where it went down.
        if (!moveTouch) {
            moveTouch = true;
            g->movementTouchStarted = true;
            g->movementTouchOrigin = g->touchPos[moveT];
        }
        moveCursor = true;
        moveTouchPos = g->touchPos[moveT];
        vec2d m = (g->touchPos[moveT] - g->touchStartPos[moveT]) * 0.1;
        dragOrigin(m.x, lastMove.x, g->touchStartPos[moveT].x, g->touchPos[moveT].x);
        dragOrigin(m.y, lastMove.y, g->touchStartPos[moveT].y, g->touchPos[moveT].y);
        lastMove = m;
        float l = float(m.length());
        if (l > 0.006f)
            m = m * double(0.006f / l);
        vel = m;
    }
    if (controls.move.any()) { // gamepad movement
        vel = vec2d(0, 0);
        if (controls.move.left)
            vel.x = -0.006000000052154064;
        if (controls.move.right)
            vel.x = 0.006000000052154064;
        if (controls.move.up)
            vel.y = -0.006000000052154064;
        if (controls.move.down)
            vel.y = 0.006000000052154064;
    }

    if (g->demo) {
        // Demo: walk to a beast to rescue, else to the nearest type-2
        // entity of the beast list, else wander.
        entity *goal = nullptr;
        for (entity *e = g->beasts; e; e = e->next)
            if (e->type == 1 && !static_cast<beast *>(e)->rescued) {
                goal = e;
                break;
            }
        demoTarget = goal;
        if (!goal) {
            float best = 1000.0f;
            for (entity *e = g->beasts; e; e = e->next) {
                float l = float((e->pos - pos).length());
                if (best > l && e->type == 2) {
                    best = l;
                    goal = e;
                }
            }
        }
        vec2d to;
        if (goal) {
            to = goal->pos;
        } else {
            float a = float(age & 0x3ff) * 0.00390625f;
            to.x = double(std::sin(a * 4.0f) / 3.0f);
            to.y = double(std::cos(a * 5.0f) / 3.0f);
        }
        vel = (to - pos).normalize() * 0.004;
    }

    // Move, clamped to the arena (a clamped move touch re-centres).
    pos = pos + vel;
    if (pos.x < -0.45) {
        if (moveT > -1)
            g->touchStartPos[moveT].x = g->touchPos[moveT].x;
        pos.x = -0.45;
        vel.x = 0;
    }
    if (pos.x > 0.43) {
        if (moveT > -1)
            g->touchStartPos[moveT].x = g->touchPos[moveT].x;
        pos.x = 0.43;
        vel.x = 0;
    }
    if (pos.y < -0.35) {
        if (moveT > -1)
            g->touchStartPos[moveT].y = g->touchPos[moveT].y;
        pos.y = -0.35;
        vel.y = 0;
    }
    if (pos.y > 0.43) {
        if (moveT > -1)
            g->touchStartPos[moveT].y = g->touchPos[moveT].y;
        pos.y = 0.43;
        vel.y = 0;
    }
    vel = vel * drag;

    // Walk animation (16 frames per direction, stepped by position) and
    // the invincible stomp shake.
    double vx = vel.x, vy = vel.y;
    float ratio = float(std::fabs(vy) / (std::fabs(vx) + 1e-05));
    int frame;
    if (double(ratio) < 1.2) {
        int n = int((pos.x + 0.5) * 120.0);
        frame = n & 15;
        if (invT > 0.5f && (n & 7) == 7)
            g->shakeFrames = 10;
        if (vx < 0.0) {
            frame = 15 - frame;
            scaleX = -1.5;
        } else {
            scaleX = 1.5;
        }
    } else {
        int n = int((pos.y + 0.5) * 100.0);
        frame = n & 15;
        if (invT > 0.5f && (n & 7) == 7)
            g->shakeFrames = 10;
        if (vy < 0.0)
            frame = 31 - frame;
        frame += 16;
    }
    spriteIndex = frame;
    if (--fireTimer < 1) {
        fireTimer = g->shotDelay;
        if ((invT < 1.0f || hard) && !g->levelEnding)
            shoot();
    }

    // Breadcrumb trail for the following beasts.
    float d = float(double(trailDist) + (saved - pos).length());
    trailDist = d;
    if (d >= 0.001953125f) {
        trailDist = 0;
        trail[trailIndex] = pos;
        trailIndex = (trailIndex + 1) & 0x3ff;
    }
}

// ============================================================== aiDroid ====

aiDroid::aiDroid(theGame *g) {
    game = g;
    droidType = g->sd(0xc84);
    if (unsigned(droidType) > 9)
        droidType = 0;
    pos = vec2d(0, -0.25);
    demoTarget = nullptr;
    spriteIndex = kDroids[droidType];
    changed = false;
    size = 0.04f;
    scaleX = scaleY = 1.0;
    drag = vec2d(0.8, 0.8);
    trailDist = 0;
    seekSpeed = 0.003f;
    bob = 0;
    aimAngle = 0;
    type = 0x34;
    fireTimer = 15;
    fireReload = 15;
}

void aiDroid::run() {
    theGame *g = game;
    vec2d saved = pos;
    if (g->state == 9)
        rescued = 0;
    spriteIndex = kDroids[droidType];
    demoTarget = nullptr;
    for (entity *e = g->beasts; e; e = e->next)
        if (e->type == 1 && !static_cast<beast *>(e)->rescued) {
            demoTarget = e;
            break;
        }
    g->moveTowards(this, demoTarget ? demoTarget : g->players, 0);
    if (demoTarget && targetDistance < 0.01f) {
        demoTarget->hitBy(this); // collects the beast
        demoTarget = nullptr;
    }
    float moved = float((pos - saved).length());
    trailDist = trailDist + moved;
    bob = bob + moved * 30.0f;
    rot = std::sin(bob) * 0.3f;
    col = g->gcol[0];
    if (!(trailDist < 0.001953125f)) {
        trailDist = 0;
        trail[trailIndex] = pos;
        trailIndex = (trailIndex + 1) & 0x3ff;
    }
    // Aim at the nearest enemy in 45-degree steps.
    entity *t = nullptr;
    float best = 1000.0f;
    for (entity *e = g->enemies; e; e = e->next) {
        float l = float((e->pos - pos).length());
        if (l < best) {
            t = e;
            best = l;
        }
    }
    if (t) {
        vec2d d = pos - t->pos;
        double a = std::atan2(-d.y, d.x);
        int n = int(double(float(a - 1.570796327)) / 0.7853981635);
        aimAngle = float(n) * 0.7853982f;
    }
    if (--fireTimer < 1) {
        fireTimer = fireReload;
        droidBull *b = new droidBull();
        b->game = g;
        b->baseSprite = 0x32;
        b->spriteIndex = 0x32;
        b->scaleX = 0.2;
        b->size = 0.04f;
        b->type = 0xf;
        b->vel = vec2d(0, 0.015);
        b->vel.rotateBy(double(aimAngle));
        b->rot = aimAngle;
        b->col.blue(1.0);
        b->pos = pos + b->vel;
        g->addEntity(b, &g->projectiles);
    }
    // Touching the droid as a movement touch starts cycles its look
    // (as many looks as tens of levels reached).
    if (g->movementTouchStarted) {
        g->movementTouchStarted = false;
        unsigned top = unsigned(g->furthest / 10);
        if (top > 9)
            top = 0;
        if ((g->movementTouchOrigin - pos).length() < 0.05) {
            int n = droidType + 1;
            droidType = n > int(top) ? 0 : n;
            g->sd(0xc84) = droidType;
            g->shellCommand = 17;
            changed = true;
        }
    }
    if (droidType == 9)
        scaleX = vel.x < 0.0 ? -1.0 : 1.0;
    else
        scaleX = 1.0;
}

// ================================================================ shots ====

void basicBull::run() {
    theGame *g = game;
    if (g->hotShotFrames < 1) {
        col.white(1.0);
    } else { // hot bullets flash
        float t = float(g->frameCount & 15) * 0.0625f;
        col.b = 0;
        col.g = double(t) * 0.5 + 0.5;
    }
    g->touch(this, g->obstacles);
    g->touch(this, g->enemies);
    g->touch(this, g->enemyShots);
    pos = pos + vel;
    double x = pos.x;
    if (--life < 1)
        dead = true;
    if (!(x <= -0.46 || x >= 0.45 || pos.y <= -0.38 || pos.y >= 0.45))
        return;
    if (!g->reflectiveShots || g->levelEnding) { // not reflective shots
        dead = true;
        return;
    }
    if (spriteIndex == 0x32) {
        double y = pos.y;
        if (y >= -0.38 && y <= 0.45)
            return;
        vel.y = -vel.y;
    } else if (spriteIndex == 0x33) {
        if (x >= -0.46 && x <= 0.45)
            return;
        vel.x = -vel.x;
    } else if (spriteIndex == 0x31) {
        if (x <= -0.46 || x >= 0.45) {
            vel.x = -vel.x;
            scaleX = -scaleX;
        } else {
            double y = pos.y;
            if ((y >= 0.45 && vel.y > 0.0) || (y <= -0.38 && vel.y < 0.0)) {
                vel.y = -vel.y;
                scaleX = -scaleX;
            } else {
                return;
            }
        }
    } else {
        return;
    }
    pos = pos + vel;
}

void droidBull::run() {
    theGame *g = game;
    g->touch(this, g->obstacles);
    g->touch(this, g->enemies);
    g->touch(this, g->enemyShots);
    vec2d np = pos + vel;
    double sz = double(size);
    if ((vel.x > 0.0 && 0.44999998807907104 < sz + np.x) ||
        (vel.x < 0.0 && np.x - sz < -0.46000000834465027))
        dead = true;
    else
        pos.x = np.x;
    if ((vel.y > 0.0 && 0.44999998807907104 < sz + np.y) ||
        (vel.y < 0.0 && np.y - sz < -0.3799999952316284))
        dead = true;
    else
        pos.y = np.y;
}

void expandaBull::run() {
    theGame *g = game;
    float s = float(double(size) * 1.18);
    size = s;
    if (s > 0.15f) {
        dead = true;
        size = 0.15f;
    }
    col = g->gcol[1];
    col.a = (0.12 - double(size)) / 0.11;
    g->touch(this, g->enemies);
    g->touch(this, g->enemyShots);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
