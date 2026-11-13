// game_gen.cpp - theGame::genLevel: spawns the next queued enemy (one per
// call, from the back of spawnQueue). Many enemies are set up inline here,
// as in the original.
#include "game.h"
#include "game_entities.h"
#include "game_tables.h"

namespace NEON_GAME {
namespace Neon_Internals {

namespace {
int ggDelay = 1;
constexpr double kPi = 3.141592654;
} // namespace

void theGame::genLevel() {
    if (--ggDelay > 0)
        return;
    ggDelay = 1;
    spawnCount--;
    int t = spawnQueue[spawnCount];
    entity *e = nullptr;
    switch (t) {
    case 4: {
        gruntle *g = new gruntle();
        g->init(this);
        e = g;
        break;
    }
    case 6:
    case 0x2b: {
        fhg *f = new fhg();
        f->type = t;
        f->init(this);
        e = f;
        break;
    }
    case 9: {
        spiny *s = new spiny();
        s->init(this);
        e = s;
        break;
    }
    case 10: {
        mandy *m = new mandy();
        m->init(this);
        e = m;
        break;
    }
    case 0xb: {
        shootingMandy *m = new shootingMandy();
        m->init(this);
        m->type = 0xb;
        int rate = frontFireRate[level / 10];
        m->shotReload = rate;
        m->rockReload = m->game->level < 0x3a ? -1 : rate;
        m->shotSpeed = 0.01f;
        m->shotTimer = m->shotReload;
        m->rockTimer = m->rockReload;
        e = m;
        break;
    }
    case 0xc: {
        sixteenTons *s = new sixteenTons();
        s->type = 0xc;
        s->spriteIndex = 100;
        s->pos = vec2d(0, -0.28);
        s->game = this;
        s->size = 0.06f;
        s->falling = false;
        s->scaleY = 1.3;
        s->trackSpeed = 0.0004f;
        s->riseSpeed = 0.004f;
        s->fallAccel = 0.0004f;
        rez(s, 2, 2.0f, 0x3c);
        e = s;
        break;
    }
    case 0xd: {
        brolly *b = new brolly();
        b->init(this);
        e = b;
        break;
    }
    case 0x10:
    case 0x11: {
        hlaser *l = t == 0x10 ? new hlaser() : new vlaser();
        l->type = 0x10;
        l->spriteIndex = 0x68;
        l->game = this;
        l->size = 0.02f;
        l->pos = newPosition(0.35f, 0.2f, 0.02f);
        l->scaleY = 2.0;
        l->speedLimitX = oxRNG::rand(0.0005) + 0.004;
        l->speedLimitY = oxRNG::rand(0.0005) + 0.004;
        l->accelX = 4e-05;
        l->accelY = 3e-05;
        if (t == 0x11) {
            l->rot = 1.5707964f;
            l->type = 0x11;
        }
        e = l;
        break;
    }
    case 0x12:
    case 0x13: {
        bigBlobby *b = t == 0x12 ? new bigBlobby() : new smallBlobby();
        b->type = 0x12;
        b->spriteIndex = 0x6b;
        b->baseSprite = 0x6b;
        b->game = this;
        double a = oxRNG::rand(-kPi, kPi);
        b->seekTurnRate = 0.05f;
        b->seekHeading = float(a);
        b->seekSpeed = 0.0012f;
        b->pos = newPosition(0.3f);
        b->size = 0.03f;
        b->scaleY = 1.3;
        rez(b, 0, 1.0f, 0x1e);
        if (t == 0x13) {
            b->type = 0x13;
            b->spriteIndex = 0x6e;
            b->baseSprite = 0x6e;
            b->size = 0.02f;
            b->seekSpeed = b->seekSpeed * 1.5f;
            b->scaleY = 1.3;
            b->seekTurnRate = b->seekTurnRate * 0.8f;
        }
        e = b;
        break;
    }
    case 0x14: {
        spinner *s = new spinner();
        s->type = 0x14;
        s->spriteIndex = 0x70;
        s->baseSprite = 0x70;
        s->game = this;
        double sp = oxRNG::rand(0.01, 0.012);
        s->hp = 5;
        s->seekAccelX = 8e-05;
        s->seekAccelY = 5e-05;
        s->speedLimitY = sp;
        s->speedLimitX = sp;
        s->pos = newPosition(0.4f, 0.4f, 0.03f);
        s->drag = vec2d(0.99, 0.99);
        s->size = 0.03f;
        s->elasticity = 0.3f;
        rez(s, 0, 1.0f, 0x1e);
        e = s;
        break;
    }
    case 0x15:
    case 0x2f: {
        killerRobot *k = new killerRobot();
        k->type = t;
        k->init(this);
        e = k;
        break;
    }
    case 0x16: {
        pointyArrow *p = new pointyArrow();
        p->type = 0x16;
        p->game = this;
        double sp = oxRNG::rand(0.005, 0.008);
        p->spriteIndex = 0x73;
        p->hp = 2;
        p->baseSprite = 0x73;
        p->drag = vec2d(0.98, 0.98);
        p->size = 0.04f;
        p->seekSpeed = float(sp);
        p->pos = newPosition(0.3f, 0.3f, 0.03f);
        p->baseScaleX = p->scaleX;
        p->baseScaleY = p->scaleY;
        rez(p, 1, 1.0f, 0x1e);
        p->mode = 0;
        p->timer = 0;
        e = p;
        break;
    }
    case 0x17: {
        runningMan *r = new runningMan();
        r->init(this);
        e = r;
        break;
    }
    case 0x19: {
        zombeast *z = new zombeast();
        z->spriteIndex = 0x80;
        z->baseSprite = 0x80;
        z->game = this;
        z->size = 0.025f;
        z->seekSpeed = 0.0024f;
        z->type = 0x19;
        z->pos = newPosition(0.3f);
        z->baseScaleX = z->scaleX;
        z->baseScaleY = z->scaleY;
        e = z;
        break;
    }
    case 0x1b: {
        lavvy *l = new lavvy();
        l->type = 0x1b;
        l->game = this;
        l->pos = vec2d(0, -0.28);
        l->vel = vec2d(0, 0.001);
        l->spriteIndex = 0x83;
        l->baseSprite = 0x83;
        l->hp = 0x32;
        l->size = 0.09f;
        l->scaleY = 1.4;
        l->baseScaleX = l->scaleX;
        l->baseScaleY = l->scaleY;
        l->rollTimer = level > 0x50 ? 0xf : 0x14;
        l->rollReload = l->rollTimer;
        e = l;
        break;
    }
    case 0x1e:
    case 0x2c:
    case 0x2d:
    case 0x2e: {
        astroSmashCannon *c = new astroSmashCannon();
        c->spriteIndex = 0x8a;
        c->hp = 10;
        c->type = t;
        c->game = this;
        c->rockReload = -1;
        c->size = 0.03f;
        c->seekSpeed = 0.001f;
        c->shotTimer = 20;
        c->shotReload = 20;
        c->shotSprite = 0x8b;
        c->f1ac = 0;
        switch (t) {
        case 0x1e: // bottom edge, firing up; also drops rocks
            c->lockX = 1.0;
            c->lockY = 0;
            c->rot = 0;
            c->shotVel = vec2d(0, -0.015);
            c->pos = vec2d(double(-0.43f), double(0.42f));
            c->f1a0 = 2.0;
            c->f198 = 0.5;
            c->rockTimer = 0x78;
            c->rockReload = 0x78;
            break;
        case 0x2c: // top edge, firing down
            c->lockX = 1.0;
            c->lockY = 0;
            c->rot = 3.1415927f;
            c->shotVel = vec2d(0, 0.015);
            c->pos = vec2d(double(0.42f), double(-0.35f));
            c->f1a0 = 2.0;
            c->f198 = 0.5;
            break;
        case 0x2d: // left edge, firing right
            c->lockX = 0;
            c->lockY = 1.0;
            c->rot = -1.5707964f;
            c->shotVel = vec2d(0.015, 0);
            c->pos = vec2d(double(-0.43f), double(-0.35f));
            c->f198 = 2.0;
            c->f1a0 = 0.5;
            break;
        case 0x2e: // right edge, firing left
            c->lockX = 0;
            c->lockY = 1.0;
            c->rot = 1.5707964f;
            c->shotVel = vec2d(-0.015, 0);
            c->pos = vec2d(double(0.42f), double(0.48f));
            c->f198 = 2.0;
            c->f1a0 = 0.5;
            break;
        }
        e = c;
        break;
    }
    case 0x1f: {
        yinYang *y = new yinYang();
        y->type = 0x1f;
        y->spriteIndex = 0x8f;
        y->baseSprite = 0x8f;
        y->game = this;
        double sp = oxRNG::rand(0.008, 0.016);
        y->hp = 1;
        y->speedLimitY = sp;
        y->speedLimitX = sp;
        y->seekAccelX = 4e-05;
        y->seekAccelY = 4e-05;
        y->pos = newPosition(0.4f, 0.3f, 0.03f);
        y->drag = vec2d(0.99, 0.99);
        y->size = 0.03f;
        y->mineTimer = 0x14;
        y->mineReload = 0x14;
        y->elasticity = 0.3f;
        rez(y, 0, 1.0f, 0x1e);
        e = y;
        break;
    }
    case 0x20: {
        recogniser *r = new recogniser();
        r->init(this);
        e = r;
        break;
    }
    case 0x21: {
        sheepipede *s = new sheepipede();
        s->init(this);
        e = s;
        break;
    }
    case 0x22: {
        teapot *p = new teapot();
        p->spriteIndex = 0x97;
        p->baseSprite = 0x97;
        p->game = this;
        p->pos = newPosition(0.3f);
        p->hp = 3;
        p->stun = 0;
        p->size = 0.03f;
        double sp = oxRNG::rand(0.01, 0.012);
        p->seekAccelX = 2e-05;
        p->seekAccelY = 2e-05;
        p->drag = vec2d(0.998, 0.998);
        p->speedLimitY = sp;
        p->speedLimitX = sp;
        rez(p, 2, 1.0f, 0x28);
        p->type = 0x22;
        e = p;
        break;
    }
    case 0x24:
    case 0x30: {
        falseTeeth *f = new falseTeeth();
        f->type = t;
        f->init(this);
        e = f;
        break;
    }
    case 0x25: {
        skier *s = new skier();
        s->init(this);
        e = s;
        break;
    }
    case 0x26: {
        killerRam *k = new killerRam();
        k->init(this);
        e = k;
        break;
    }
    case 0x27:
    case 0x32: {
        licker *l = new licker();
        l->type = t;
        l->init(this);
        e = l;
        break;
    }
    case 0x28:
    case 0x29:
    case 0x2a: {
        skier *b;
        if (t == 0x28)
            b = new biscuit();
        else if (t == 0x29)
            b = new firingBiscuit();
        else
            b = new beachball();
        b->stun = 0;
        int drawSprite = t == 0x2a ? 0xa6 : 0xa5;
        b->spriteIndex = drawSprite;
        b->baseSprite = drawSprite;
        b->game = this;
        double sp = t == 0x2a ? oxRNG::rand(0.016, 0.022) : oxRNG::rand(0.01, 0.022);
        b->seekAccelX = 2.5e-05;
        b->seekAccelY = 2.7e-05;
        b->speedLimitY = sp;
        b->speedLimitX = sp;
        b->pos = newPosition(0.35f);
        rez(b, 1, 1.0f, 0x1e);
        b->type = t;
        if (t == 0x2a) {
            b->size = 0.045f;
            b->baseSize = 0.045f;
            b->stunDrag = 0.917f;
            b->moveType = 3;
            b->hp = 3;
        } else {
            b->moveType = 2;
            b->size = 0.035f;
            b->baseSize = 0.035f;
            b->stunDrag = 0.97f;
            b->hp = 10;
            if (t == 0x29) {
                firingBiscuit *fb = static_cast<firingBiscuit *>(b);
                fb->shotTimer = 5;
                fb->shotReload = 5;
            }
        }
        e = b;
        break;
    }
    case 0x33: {
        kevinToms *k = new kevinToms();
        k->init(this);
        e = k;
        break;
    }
    default:
        break;
    }
    if (e)
        addEntity(e, &enemies);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
