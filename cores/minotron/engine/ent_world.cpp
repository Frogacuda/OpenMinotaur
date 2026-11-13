// ent_world.cpp - beasties, powerups, bonuses, obstacles and the particle
// effects (explosions, shatter pieces, droplets).
#include "neon/particles.h"
#include "game.h"
#include "game_entities.h"
#include "game_tables.h"
#include "neon_audio.h"

namespace NEON_GAME {
namespace Neon_Internals {

namespace {

// Shared arena edge handling with bounce (theGame's moveClipped, edge 2).
void moveBounce(entity *e) {
    vec2d np = e->pos + e->vel;
    double sz = double(e->size);
    double vx = e->vel.x;
    if ((vx > 0.0 && 0.44999998807907104 < sz + np.x) ||
        (vx < 0.0 && np.x - sz < -0.46000000834465027))
        e->vel.x = -(vx * double(e->elasticity));
    else
        e->pos.x = np.x;
    double vy = e->vel.y;
    if ((vy > 0.0 && 0.44999998807907104 < sz + np.y) ||
        (vy < 0.0 && np.y - sz < -0.3799999952316284))
        e->vel.y = -(vy * double(e->elasticity));
    else
        e->pos.y = np.y;
}

// The same with edge 1: leaving the arena kills.
void moveOrDie(entity *e) {
    vec2d np = e->pos + e->vel;
    double sz = double(e->size);
    if ((e->vel.x > 0.0 && 0.44999998807907104 < sz + np.x) ||
        (e->vel.x < 0.0 && np.x - sz < -0.46000000834465027))
        e->dead = true;
    else
        e->pos.x = np.x;
    if ((e->vel.y > 0.0 && 0.44999998807907104 < sz + np.y) ||
        (e->vel.y < 0.0 && np.y - sz < -0.3799999952316284))
        e->dead = true;
    else
        e->pos.y = np.y;
}

void killIfPlayer(entity *by) {
    if (by && by->type == 3)
        static_cast<playerMinotaur *>(by)->kill();
}
} // namespace

// ================================================================ beast ====

void beast::init(theGame *g) {
    pos = g->newPosition(0.1f);
    initRaw(g);
}

void beast::initRaw(theGame *g) {
    game = g;
    type = 1;
    f164 = 0;
    int k = neonRand() % 5;
    gfx = kBeastGraphics[k];
    spriteIndex = gfx;
    timer = oxRNG::irand(30.0) + 30;
    vel.x = oxRNG::rand(0.001, 0.002);
    vel.y = oxRNG::rand(-0.001, 0.001);
    if (neonRand() & 1)
        vel.x = -vel.x;
    age = 0;
    size = 0.033333335f;
    scaleX = 0.5;
    scaleY = 0.7;
    if (k == 4)
        scaleY = 1.4;
    else if (k == 3)
        scaleY = 1.0;
    g->pixelShatter(nullptr, this, vec2d(2, 2), -20, 0, 1, 0, 2);
    dead = false;
    drawDelay = 20;
    rescued = false;
}

int beast::draw(sprite *s, vec2d sz) {
    double a = col.a;
    if (game->paused || game->demo)
        col.a = double(0.3f);
    entity::draw(s, sz);
    if (game->beastLoveFrames > 0) { // beast love: a pulsing heart
        age++;
        float t = float(age & 63) * 0.015625f;
        float k = std::sin(t * 6.2831855f);
        double dk = double(k);
        s->col.white(1.0);
        s->col.a = col.a;
        s->frame = 0x3b;
        vec2d wh = vec2d(scaleX, scaleY) * double(size) * 1.5 * (dk * 0.1 + 0.6);
        wh = wh * sz;
        s->width = wh.x;
        s->height = wh.y;
        s->pos = pos * sz;
        s->pos.y -= (dk * 0.01 + 0.05) * sz.y;
        s->setupChecking();
    }
    col.a = double(float(a));
    return 6;
}

void beast::run() {
    theGame *g = game;
    double vx;
    if (!rescued) {
        timer--;
        if (g->beastLoveFrames < 1 && g->beastAttractionFrames < 1) {
            if (timer < 0) {
                timer = oxRNG::irand(30.0) + 30;
                vel.x = oxRNG::rand(0.001, 0.002);
                vel.y = oxRNG::rand(-0.001, 0.001);
                if (neonRand() & 1)
                    vel.x = -vel.x;
            }
            pos = pos + vel;
            if (pos.x < -0.45 || pos.x > 0.45) {
                vel.x = -vel.x;
                pos.x += vel.x;
            }
            vx = vel.x;
            if (pos.y < -0.35 || pos.y > 0.45) {
                vel.y = -vel.y;
                pos.y += vel.y;
            }
        } else {
            // Besotted (beast love): run to the minotaur.
            if (!g->players)
                return;
            vel = (g->players->pos - pos).normalize() * 0.004;
            pos = pos + vel;
            vx = vel.x;
        }
    } else {
        // Rescued: walk the leader's trail, 8 crumbs per place in the line.
        entity *lead = nullptr;
        if (rescuerType == 3) {
            lead = g->players;
        } else {
            float best = 1000.0f;
            for (entity *e = g->particles; e; e = e->next) {
                float l = float((e->pos - pos).length());
                if (l < best && e->type == 0x34) {
                    best = l;
                    lead = e;
                }
            }
        }
        if (!lead)
            return;
        playerMinotaur *p = static_cast<playerMinotaur *>(lead);
        vec2d old = pos;
        pos = p->trail[(p->trailIndex - 10 - followingSlot * 8) & 0x3ff];
        vec2d d = pos - old;
        if (d.length() > 0.001953125)
            vel = d;
        vx = vel.x;
    }
    int n = int((pos.x + 0.5) * 100.0) & 1;
    if (vx < 0.0) {
        scaleX = 1.0;
    } else {
        n = 1 - n;
        scaleX = -1.0;
    }
    spriteIndex = n + gfx;
}

void beast::hitBy(entity *by) {
    if (rescued)
        return;
    theGame *g = game;
    rescued = true;
    rescuerType = by->type;
    if (rescuerType == 3) {
        int n = ++g->f1198[1]; // beasts rescued in total
        if (n == 100) {
            if (g->goalAllowed(1))
                g->goal(0, 1, 4);
        } else if (n == 500 && g->goalAllowed(2)) {
            g->goal(1, 2, 5);
        }
    }
    if (g->rescueChain <= 8)
        g->rescueChain++;
    littleBonus *b = new littleBonus();
    b->game = g;
    b->spriteIndex = g->rescueChain + 0x3e;
    g->addScore(g->rescueChain * 100);
    b->growthPerFrame = 1.0f;
    b->framesRemaining = 300;
    b->paletteIndex = 0;
    b->size = 0.02f;
    b->scaleY = 1.1;
    b->vel.y = -0.0005;
    b->pos = pos;
    g->addEntity(b, &g->particles);
    if (g->beastLoveFrames < 1)
        g->beastAttractionFrames += g->extraLove;
    else
        g->beastLoveFrames += g->extraLove;
    playerMinotaur *p = static_cast<playerMinotaur *>(by);
    followingSlot = p->rescued;
    p->rescued++;
    if (double(g->shotSpeed) <= 0.034)
        g->shotSpeed = float(double(g->shotSpeed) + 0.005);
    g->hotShotFrames = 50;
    if (!g->levelEnding) {
        int left = 0;
        for (entity *e = g->beasts; e; e = e->next)
            if (e->type == 1 && !static_cast<beast *>(e)->rescued)
                left++;
        if (left < 1) {
            // "Last Beast GET!"
            if (g->message != 0)
                g->eraseMessage();
            g->message = 2;
            if (!g->demo)
                g->playSample(0x25);
            g->msgTimer = 60;
            if (!useRobot) {
                if (g->f1198[0] == 0) {
                    g->f1198[3]++;
                    g->rescuedAllBeforeKills = 1;
                } else {
                    g->f1198[3] = 0;
                    g->rescuedAllBeforeKills = 0;
                }
                if (!g->enemies) {
                    g->spreadPowerupDue = true;
                } else {
                    g->powerupTimer = 1;
                    g->forcedPowerupKind = 3;
                }
            }
            return;
        }
    }
    if (!g->demo)
        g->playSample(9);
}

// ============================================================== powerup ====

void powerup::init(theGame *g) {
    type = 2;
    game = g;
    int r = g->restarts;
    seekSpeed = float(oxRNG::rand(0.001, 0.002));
    double d = oxRNG::rand(100.0);
    if (r > 1)
        r = 2;
    kind = kPowerupProbs[r * 21 + int(float(d) / 5.0f)];
    if (g->forcedPowerupKind != 0)
        kind = g->forcedPowerupKind;
    g->forcedPowerupKind = 0;
    size = 0.03f;
    if (reducedPowerups) {
        if (kind == 3 || kind > 6)
            kind = 5;
    }
    if (hard && kind == 7)
        kind = 5;
    scaleY = 1.03;
    phase = 0.0f;
    life = 1.0f;
}

int powerup::draw(sprite *s, vec2d sz) {
    theGame *g = game;
    float k = std::sin(phase);
    double dk = double(k);
    float a0 = float(col.a);
    double a = (g->paused || g->demo) ? 0.30000001192092896 : double(a0);
    vec2d p = pos;
    col.a = a;
    s->col = col;
    s->frame = kPowerupIcons[kind] + kLitBase;
    s->angle = double(rot);
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    switch (kind) {
    case 0:
        s->height *= dk * -0.2 + 0.6;
        s->width *= dk * 0.2 + 0.6;
        break;
    case 1:
        p.x += dk * 0.002;
        p.y += dk * 0.003;
        s->angle = dk * 0.2;
        s->width *= 0.7;
        s->height *= 0.7;
        s->col.b = 0;
        s->col.g = 0.2 + double(float(g->frameCount & 7) / 7.0f) * 0.8;
        break;
    case 2: {
        int f = (g->frameCount >> 1) & 1;
        double m = double(float(f)) * 0.1 + 0.7;
        s->width *= m;
        s->height *= m;
        s->frame += f;
        break;
    }
    case 3:
        s->height *= dk * -0.2 + 0.8;
        s->width *= dk * 0.2 + 0.8;
        s->angle = dk * 0.1;
        break;
    case 4:
        s->frame += (g->frameCount >> 3) & 1;
        s->width *= 0.6;
        s->height *= 0.3;
        break;
    case 5:
    case 7: // spinning coin
        s->width *= dk * 0.7;
        s->height *= dk * -0.2 + 0.8;
        break;
    case 6:
        s->width *= dk * 0.4 + 1.4;
        s->height *= dk * -0.4 + 1.4;
        break;
    case 8:
        s->height *= dk * -0.2 + 0.8;
        s->width *= dk * 0.2 + 0.8;
        s->angle = dk * 0.1;
        s->col = g->gcol[0];
        s->col.a = a;
        break;
    }
    s->width *= 0.8;
    s->height *= 0.8;
    s->pos = p * sz;
    s->setupChecking();
    if (kind != 6) { // the ring around it
        s->frame = 0x3a;
        s->angle = double(rot);
        s->width = sz.x * double(size + size) * scaleX * (dk * 0.2 + 1.2);
        s->height = sz.y * double(size + size) * scaleY * (dk * -0.2 + 1.2);
        s->pos = pos * sz;
        s->col.r = s->col.g = s->col.b = 1.0;
        s->setupChecking();
        col.a = double(a0);
    }
    return 6;
}

void powerup::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    phase += 0.1f;
    entity *p = g->players;
    if (!p)
        return;
    pos = pos + (p->pos - pos).normalize() * double(seekSpeed); // drifts to the player
    float l = life - 0.003f;
    life = l;
    if (l < 0.4f) { // about to vanish: blink
        age++;
        col.a = (age & 2) ? 0.0 : 1.0;
    }
    if (l < 0.0f) {
        dead = true;
        g->bonusPickupChain = 0;
    }
}

void powerup::hitBy(entity *by) {
    if (!by)
        return;
    theGame *g = game;
    dead = true;
    if (g->beastLoveFrames < 1)
        g->beastAttractionFrames += g->extraLove;
    else
        g->beastLoveFrames += g->extraLove;
    if (++g->bonusPickupChain == 50 && g->goalAllowed(0x100))
        g->goal(8, 0x100, 12);
    if (kind == 6) { // extra life
        g->lives[0]++;
        if (g->lives[0] > 19 && g->goalAllowed(4))
            g->goal(2, 4, 6);
        littleBonus *b = new littleBonus();
        b->pos = by->pos;
        b->game = g;
        b->spriteIndex = 0x92;
        b->framesRemaining = 50;
        b->paletteIndex = 0;
        b->scaleY = 1.1;
        b->vel.y = -0.0008;
        b->size = 0.04f;
        b->growthPerFrame = 1.01f;
        g->addEntity(b, &g->particles);
        if (!g->demo)
            g->playSample(0xf);
        return;
    }
    // Every other pickup: a rising bonus worth 100 more each time (max 1000).
    littleBonus *b = new littleBonus();
    b->pos = by->pos;
    b->game = g;
    b->spriteIndex = g->pickupBonusIndex + 0x3f;
    g->addScore((g->pickupBonusIndex + 1) * 100);
    b->scaleY = 1.1;
    b->vel.y = -0.0005;
    b->growthPerFrame = 1.0f;
    b->size = 0.03f;
    b->framesRemaining = 300;
    b->paletteIndex = 0;
    b->vel = b->vel * oxRNG::rand(2.0, 3.0);
    g->addEntity(b, &g->particles);
    if (g->pickupBonusIndex <= 8)
        g->pickupBonusIndex++;
    switch (kind) {
    case 0: // beast love
        g->beastLoveFrames += 400;
        if (!g->demo)
            g->playSample(0xb);
        break;
    case 1: // hot bullets for the rest of the level
        g->hotShotFrames = 1;
        g->hotShotsPermanent = true;
        if (!g->demo)
            g->playSample(0x33);
        break;
    case 2: { // bomb
        auto bomb = [&](entity *list) {
            for (entity *e = list; e; e = e->next) {
                int t = e->type;
                if (hard && (t == 12 || t == 16 || t == 17 || t == 13))
                    continue;
                g->damage(nullptr, e, g->bombsCollected * 10 + 2, bombPoints[t], 1);
            }
        };
        bomb(g->enemyShots);
        bomb(g->enemies);
        bomb(g->obstacles);
        if (g->message != 0)
            g->eraseMessage();
        g->message = 1;
        g->msgTimer = 20;
        g->fb90[12] = 1.0f; // flash
        g->beastLoveFrames += 200;
        g->bombsCollected++;
        if (!g->demo)
            g->playSample(0x28);
        break;
    }
    case 3: // 3-way shot (5-way the second time, explosive the third)
        if (g->spreadShots) {
            if (g->fiveWayShots)
                g->levelFlags |= 8;
            g->fiveWayShots = true;
            g->spreadPowerupDue = true;
        }
        g->spreadShots = true;
        {
            int n = 0;
            for (entity *e = g->enemies; e; e = e->next)
                if (e->type == 4)
                    n++;
            if (n == 0)
                g->spreadPowerupDue = true;
        }
        if (!g->demo)
            g->playSample(8);
        break;
    case 4: // reflective shots
        g->reflectiveShots = true;
        if (!g->demo)
            g->playSample(0x36);
        break;
    case 5: { // Floyd bonus
        littleBonus *f = new littleBonus();
        f->pos = by->pos;
        f->spriteIndex = 0x48;
        f->game = g;
        g->addScore(1000);
        f->scaleY = 1.1;
        f->vel.y = -0.0005;
        f->growthPerFrame = 1.0f;
        f->size = 0.03f;
        f->framesRemaining = 300;
        f->paletteIndex = 0;
        f->vel = f->vel * oxRNG::rand(5.0, 6.0);
        g->addEntity(f, &g->particles);
        if (!g->demo)
            g->playSample(0x32);
        break;
    }
    case 7: // explosive shots
        g->levelFlags |= 8;
        if (!g->demo)
            g->playSample(0x31);
        break;
    case 8: { // invincibility
        if (!g->demo)
            g->playSample(0x37);
        g->shakeFrames = 10;
        playerMinotaur *p = static_cast<playerMinotaur *>(by);
        p->invT = 0;
        p->invDT = 0.04f;
        p->invincible = true;
        break;
    }
    default:
        break;
    }
}

// ========================================================== littleBonus ====

void littleBonus::init(theGame *g) {
    game = g;
    int n = g->levelBonusStep;
    spriteIndex = n + 0x3f;
    int pts = (n + 1) * 100;
    g->addScore(pts);
    g->levelBonusPoints += pts;
    if (n < 9)
        g->levelBonusStep = n + 1;
    framesRemaining = 300;
    paletteIndex = 0;
    size = 0.02f;
    growthPerFrame = 1.0f;
    scaleY = 1.1;
    vel.y = -0.0005;
}

void littleBonus::run() {
    col = game->gcol[paletteIndex];
    pos = pos + vel;
    float k = growthPerFrame;
    if (1.0f < k) {
        scaleX *= double(k);
        scaleY *= double(k);
        float e = float(scaleX - 1.0);
        if (e <= 1.0f) {
            col.a = double(1.0f - e);
        } else {
            dead = true;
            col.a = 0.0;
        }
    }
    if (--framesRemaining < 1)
        dead = true;
}

// ======================================================= staticObstacle ====

void staticObstacle::init(theGame *g) {
    game = g;
    type = 5;
    pos = g->newPosition(0.1f);
    float *fb = g->fb90; // 0xb90.. per-obstacle colour phases
    f168 = fb[3];
    fb[3] += fb[0];
    f16c = fb[4];
    fb[4] += fb[1];
    f170 = fb[5];
    fb[5] += fb[2];
    size = 0.025f;
    int i = g->level % 10;
    scaleX = double(kObstacleXScales[i]);
    scaleY = double(kObstacleYScales[i]);
    spriteIndex = kObstacleFrames[i];
    bool h = (neonRand() & 1) == 0;
    g->pixelShatter(nullptr, this, h ? vec2d(4, 0) : vec2d(0, 4), -10, 0, !h, h, h);
    dead = false;
    drawDelay = 10;
}

void staticObstacle::run() {
    col = game->gcol[game->level % 11];
}

void staticObstacle::hitBy(entity *by) {
    theGame *g = game;
    if (by) {
        killIfPlayer(by);
        if (g->hotShotFrames < 1 && by->type != 3)
            by->dead = true;
    }
    if (!g->demo)
        g->playSample(7);
    bool h = (neonRand() & 1) == 0;
    g->pixelShatter(nullptr, this, h ? vec2d(1.4, 0) : vec2d(0, 1.4), 0x32, 0, !h, h, h);
    g->addScore(12);
    g->checkPowerup(pos);
}

// ============================================================ particles =====

explosion::explosion() {
    count = 16;
    radius = 16.0f;
    from.yellow(0.4);
    to.red(0.0);
    sizeStart = 0.3f;
    sizeEnd = 0.2f;
    seed = unsigned(neonRand());
}

// Draws `count` blobs scattered by a generator reseeded from `seed`, so the
// pattern is stable from frame to frame while it expands.
int explosion::draw(sprite *s, vec2d sz) {
    s->frame = 0x8e;
    s->angle = double(rot);
    return neon::particles::drawSeededBurst(
        *this, *s, sz, [] { return neonRand(); }, [](unsigned seed) { neonSrand(seed); },
        [](double low, double high) { return oxRNG::rand(low, high); });
}

void droplet::run() {
    vel = vel + game->gravity;
    if (vel.y > 0.0 && 0.44999998807907104 < pos.y) {
        vel.y *= -0.8;
        pos.y += vel.y;
    }
    double vx = vel.x;
    if (vx > 0.0 && 0.44999998807907104 < pos.x) {
        vx = -vx;
        vel.x = vx;
        pos.x += vx;
    }
    if (vx < 0.0 && pos.x < -0.46000000834465027) {
        vel.x = -vx;
        pos.x += -vx;
    }
    _particle::run();
}

int shatterBit::draw(sprite *s, vec2d sz) {
    s->col = col;
    s->regionX = texX;
    s->regionY = texY;
    s->regionW = pieceW;
    s->regionH = pieceH;
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->flags |= SPR_REGION;
    if (game->paused || game->demo)
        s->col.a = 0.3;
    s->setupChecking();
    s->flags &= ~unsigned(SPR_REGION);
    return 6;
}

void shatterBit::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    if (type == 1) { // pieces of the minotaur follow him while he rezzes
        entity *p = g->players;
        if (p) {
            vec2d d = p->pos - target;
            pos = pos + d + vel;
            target = p->pos;
            spriteIndex = p->spriteIndex;
            if (p->scaleX < 0.0 && scaleX > 0.0)
                scaleX = -scaleX;
            if (p->scaleX > 0.0 && scaleX < 0.0)
                scaleX = -scaleX;
        }
    } else {
        pos = pos + vel;
    }
    float f = f19c - rate;
    f19c = f;
    if (f < 0.0f) {
        f19c = 0.0f;
        dead = true;
    }
}

void raindrop::run() {
    theGame *g = game;
    g->liveEnemyCount++;
    pos = pos + g->worldDrift;
    rot = g->driftAngle;
    if (pos.x < -0.46000000834465027 || pos.x > 0.44999998807907104) {
        g->splash(15, pos);
        dead = true;
    }
    if (0.44999998807907104 < pos.y) {
        g->splash(15, pos);
        dead = true;
    }
}

void raindrop::hitBy(entity *by) {
    killIfPlayer(by);
    game->splash(15, pos);
    dead = true;
}

void shrapnel::run() {
    age++;
    moveOrDie(this);
    col = getDynamicColour(5, 1.0f, age);
}

void shrapnel::hitBy(entity *by) {
    if (!by)
        return;
    killIfPlayer(by);
    game->damage(by, this, 1, 10, 1);
}

void turd::init(theGame *g) {
    game = g;
    spin = float(oxRNG::rand(-0.3, 0.3));
    vel.x = oxRNG::rand(-0.002, 0.002);
    vel.y = oxRNG::rand(-0.004, -0.001);
    size = float(oxRNG::rand(0.02, 0.03));
    double d = oxRNG::rand(0.95, 1.0);
    spriteIndex = 0x88;
    type = 0x1d;
    harmlessFrames = 40;
    hp = 3;
    drag = vec2d(d, d);
}

void turd::run() {
    theGame *g = game;
    if (harmlessFrames != 0)
        harmlessFrames--;
    g->liveEnemyCount++;
    rot += spin;
    vel = vel + g->gravity * 0.1;
    moveBounce(this);
}

void turd::hitBy(entity *by) {
    theGame *g = game;
    if (harmlessFrames < 1 && by) {
        killIfPlayer(by);
        g->damage(by, this, 1, 0x32, 1);
    }
    if (!g->demo)
        g->playSample(0x35);
    if (hp > 1 && by)
        vel = by->vel * 0.3;
}

void bogroll::run() {
    age++;
    spriteIndex = baseSprite + ((age >> 2) & 1);
    game->vectorSeek(this, game->players, 1);
    rot = seekHeading;
}

void bogroll::hitBy(entity *by) {
    if (by) {
        killIfPlayer(by);
        game->damage(by, this, 1, 10, 0);
    }
    if (hp < 1)
        game->derez(this, 2, 2.5f, 0x3c);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
