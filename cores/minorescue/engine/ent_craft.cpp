// ent_craft.cpp - the player vehicles: ships (spaceCraft), jets and tanks,
// with their guns, shots and trails.
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

// Start positions by player (they wrap: 0.75 is -0.25, 0.85 is -0.15).
const float kShipStartX[4] = {0.75f, 0.25f, 0.75f, 0.25f};
const float kShipStartY[4] = {0.85f, 0.25f, 0.25f, 0.85f};
} // namespace

// ================================================================ craft ====

craft::craft() {
    mass = 12.0f;
}

// Move, record the path, wrap, smooth the drawn heading, take gravity from
// the planets and thrust from the player's joystick (heading follows the
// stick) or the autopilot, then run the gun.
void craft::run() {
    pos = pos + vel;
    vel = vel * drag;
    history[historyCount & 0x7f] = pos;
    historyCount++;
    wrapped = false;
    if (!noWrap) {
        double w = wrapWidth, h = wrapHeight;
        double x = pos.x, y = pos.y;
        if (double(float(w * 0.5)) < x) {
            x -= w;
            wrapped = true;
            pos.x = x;
        }
        if (double(float(h * 0.5)) < y) {
            y -= h;
            wrapped = true;
            pos.y = y;
        }
        if (x < double(-float(w * 0.5))) {
            wrapped = true;
            pos.x = x + w;
        }
        if (y < double(-float(h * 0.5))) {
            wrapped = true;
            pos.y = y + h;
        }
    }
    // Turn the drawn (and aimed) heading towards rot the short way round.
    // (Port fix: the original computed 2 pi - d instead of d - 2 pi here, so
    // crossing straight down could spin the craft the long way; it also let
    // drawAngle drift outside -pi..pi.)
    double dd = std::remainder(double(rot) - double(drawAngle), kTwoPi);
    drawAngle = float(std::remainder(double(drawAngle) + dd * double(turnRate), kTwoPi));

    if (game->planets)
        force = game->feelTheForce(this, game->planets);
    else
        force = vec2d(0, 0);

    if (!autopilot) {
        const auto control = game->playerControl(joy);
        if (control.active) {
            const vec2d j(control.x, control.y);
            rot = float(std::atan2(-j.y, j.x) - kHalfPi);
            double p = double(power);
            thrusting = true;
            thrust = vec2d(p * j.x, p * j.y);
            double m = double(mass * 1000.0f);
            force.x += thrust.x / m;
            force.y += thrust.y / m;
        } else {
            thrusting = false;
        }
    } else {
        doAutoPilot();
    }
    vec2d nv = vel + force;
    if (nv.length() < double(maxSpeed))
        vel = nv;
    if (gun)
        gun->run();
}

// =========================================================== spaceCraft ====

spaceCraft::~spaceCraft() {
    delete gun;
}

// Hit by a saucer, a mine or a saucer shot.
void spaceCraft::hitBy(entity *by) {
    if (unsigned(by->type - 5) < 2u)
        damage();
}

void spaceCraft::init(spaceWars *g, int player) {
    power = 2.0f;
    mass = 1.0f;
    game = g;
    col.unpackRGBA(teamColours[player]);
    spriteIndex = 0x9c;
    hp = 9;
    phase[0] = phase[1] = phase[2] = 0;
    weapon = 0;
    phaseSpeed[0] = 0.1f;
    phaseSpeed[1] = 0.13f;
    phaseSpeed[2] = 0.11f;
    trailDist = 0;
    type = 1;
    float e = 0.8f;
    if (g->sunOn)
        e = 0.4f;
    if (!g->shields3)
        e = 1.2f;
    elasticity = e;
    spaceCraftGun *gn = new spaceCraftGun();
    gn->reload = 40;
    gn->speed = 0.25f;
    gun = gn;
    gn->owner = this;
    gn->disabled = false;
    joy = player;
    group = player + 1;
    reSpawn();
}

// Back to the start position with full shields (spaceCraft::init's tail,
// repeated inline wherever a ship comes back).
void spaceCraft::reSpawn() {
    int s = game->shields3 ? 3 : 9;
    game->shields[joy] = s;
    hp = s;
    spin = 0.05f;
    weapon = 0;
    mass = 1.0f;
    power = game->isPad ? 3.5f : 2.5f;
    spinDecay = 0.1f;
    pos = vec2d(double(kShipStartX[joy]), double(kShipStartY[joy]));
    vel = vec2d(0, 0);
    dead = false;
    if (game->demo) {
        apThrustTimer = 0;
        apReturning = false;
        autopilot = true;
        apTimer = 20;
    } else {
        autopilot = false;
    }
}

// Lose a shield (unless still spinning from the last hit); out of shields:
// explode, lose a life and come back if any are left.
void spaceCraft::damage() {
    spaceWars *g = game;
    if (fc != 0 || spin > 0.0f)
        return;
    hp--;
    g->shields[joy] = hp;
    if (hp < 1) {
        if (!g->demo)
            g->playSample(0xb);
        size = 0.04f;
        blowUp(unsigned(neonRand()), 0x100);
        size = 0.02f;
        if (!g->demo)
            g->lives[joy]--;
        if (g->lives[joy] > 0) {
            reSpawn();
            fc = 120;
        }
    } else {
        spin = 1.0f;
        if (!g->demo)
            g->playSample(2);
    }
}

void spaceCraft::run() {
    spaceWars *g = game;
    age++;
    if (((g->level >> 2) & 3) == 3)
        vel = vel + g->feelTheForce(this, g->saucers);
    g->touch(this, g->minotaurs);
    int hitAny = g->bounceOff(this, g->players);
    if (g->shields3)
        hitAny = 0;
    if (fc == 0 && !g->shields3)
        hitAny |= g->bounceOff(this, g->bullets);
    int h2 = g->bounceOff(this, g->saucers);
    int h3 = g->bounceOff(this, g->planets);
    if (hp < 0) {
        hitAny = 1;
        spin = 0;
    } else {
        hitAny = h2 | hitAny | h3;
    }
    if (fc == 0) {
        if (hitAny)
            damage();
    } else {
        fc--;
    }

    float savedRot = rot, savedDraw = drawAngle;
    craft::run();
    if (spin > 0.0f) {
        float r = savedRot + 0.3f;
        rot = r;
        drawAngle = savedDraw + 0.3f;
        if (3.1415927f < r)
            rot = r - 6.2831855f;
        double da = double(drawAngle);
        if (kPi < da)
            drawAngle = float(da - kTwoPi);
    }
    if (!g->blackHole)
        mass = 1.0f;
    else
        mass = float(double(mass) * 1.001);

    if (thrusting) {
        vec2d dir = normalized(thrust);
        gravParticle *gp = new gravParticle();
        gp->fade = 0.04f;
        gp->pos = pos + dir * -0.01;
        gp->vel = (thrust * -0.02) * oxRNG::rand(0.4);
        gp->mass = 5.0f;
        gp->game = g;
        gp->from.unpackRGBA(teamColours[group - 1]);
        gp->to = gp->from;
        gp->from.a = 1.0;
        gp->to.a = 0.0;
        gp->sizeStart = 0.2f;
        gp->sizeEnd = 0.3f;
        g->addEntity(gp, &g->particles);
    }

    // A trail of dots every 0.01 travelled while moving fast enough.
    double speed = vel.length();
    trailDist += float(speed);
    if (0.01f < trailDist) {
        float fs = float(speed);
        trailDist -= 0.01f;
        if (0.005 < double(fs)) {
            for (int i = 0; i < 3; i++) {
                phase[i] += phaseSpeed[i];
                if (kTwoPi < double(phase[i]))
                    phase[i] = float(double(phase[i]) - kTwoPi);
            }
            _particle *t = new _particle();
            t->from = getDynamicColour(group, 1.0f, g->frameCount);
            t->to = t->from;
            t->from.a = 1.0;
            t->to.a = 0.0;
            t->fade = 0.05f;
            t->spriteIndex = 0x9f;
            t->sizeStart = 0.6f;
            t->sizeEnd = 1.2f;
            t->size = float((double(fs) - 0.005) * 8.0 + 0.01);
            t->pos = history[(historyCount - 4) & 0x7f];
            t->game = g;
            g->addEntity(t, &g->particles);
        }
    }
}

int spaceCraft::draw(sprite *s, vec2d sz) {
    spaceWars *g = game;
    s->frame = spriteIndex + 1;
    s->angle = double(drawAngle);
    s->col = col;
    s->width = double(size + size) * sz.x;
    s->height = double(size + size) * sz.y;
    bool flashing = fc != 0;
    if (spin > 0.0f) {
        spin -= spinDecay;
        if (fc == 0) {
            if (age & 2) {
                s->col.unpackRGBA(teamColours[joy]);
                s->width *= 2.8;
                s->height *= 2.8;
            } else {
                s->col.yellow(1.0);
                s->width *= 1.8;
                s->height *= 1.8;
            }
        }
    }
    if (flashing) {
        s->col = getDynamicColour(group + 9, 1.0f, g->frameCount);
        if (age & 2) {
            s->width *= 2.0;
            s->height *= 2.0;
        }
    }
    s->pos = pos * sz;
    if (spriteIndex == 0x9c)
        s->angle -= 0.7853981635;
    if (g->paused)
        s->col.a *= 0.3;
    s->setupChecking();
    return 6;
}

// Demo / CPU ships: every so often pick a target (a minotaur, a saucer, a
// planet, or a random point) and thrust towards it for a while; near the
// sun, head outwards.
void spaceCraft::doAutoPilot() {
    spaceWars *g = game;
    if (apThrustTimer > 0) {
        if (!apReturning)
            apThrustTimer--;
        rot = float(std::atan2(-apDir.y, apDir.x) - kHalfPi);
        double p = double(power), a = double(apThrust);
        thrusting = true;
        thrust = vec2d(p * apDir.x * a, p * apDir.y * a);
        double m = double(mass * 1000.0f);
        force.x += thrust.x / m;
        force.y += thrust.y / m;
        if (pos.length() < 0.2) {
            if (!apReturning) {
                apThrustTimer = 0;
                apTimer = 0;
            }
        } else {
            apReturning = false;
        }
        return;
    }
    if (pos.length() < 0.2) {
        apThrustTimer = 0;
        apTimer = -1;
    } else if (--apTimer > 0) {
        return;
    }
    apTimer = oxRNG::irand(50.0) + 30;
    apThrustTimer = oxRNG::irand(30.0) + 20;
    vec2d target;
    if (pos.length() < 0.2) {
        target = pos * 2.0;
        apReturning = true;
        apThrust = 0.3f;
    } else {
        entity *t = nullptr;
        for (entity *e = g->minotaurs; e; e = e->next)
            if (float((e->pos - pos).length()) < 1000.0f)
                t = e;
        if (!t)
            for (entity *e = g->saucers; e; e = e->next)
                if (float((e->pos - pos).length()) < 1000.0f)
                    t = e;
        if (!t)
            for (entity *e = g->planets; e; e = e->next)
                if (float((e->pos - pos).length()) < 1000.0f && e->type == 2)
                    t = e;
        if (t) {
            target = t->pos;
        } else {
            target = vec2d(0, 0.4);
            target.rotateBy(oxRNG::rand(kTwoPi));
        }
        apThrust = float(oxRNG::rand(0.1, 0.3));
    }
    apDir = target - pos; // (its normalised copy is computed and dropped)
}

// ======================================================== spaceCraftGun ====

// Fire rate rises with the weapon level (always maximum near a black hole)
// and with speed; faster ships also fire slower shots.
void spaceCraftGun::run() {
    craft *c = static_cast<craft *>(owner);
    spaceWars *g = c->game;
    int base = g->sunOn ? 3 : 5;
    timer--;
    int w = static_cast<spaceCraft *>(c)->weapon;
    if (g->blackHole)
        w = 10;
    if (timer > 0)
        return;
    int r = 30 - 2 * w;
    float f = r < 5 ? 5.0f : float(r);
    float speedNow = float(c->vel.length());
    timer = int(float(base) + speedNow * f);
    speed = speedNow * -0.5f + 0.5f;
    shoot();
}

void spaceCraftGun::shoot() {
    craft *c = static_cast<craft *>(owner);
    spaceCraftBull *b = new spaceCraftBull();
    b->drag = vec2d(0.999, 0.999);
    b->type = 3;
    b->pos = c->pos;
    b->gun = this;
    b->fade = shotFade;
    angle = c->drawAngle;
    b->flags = 1;
    vec2d v(0, double(-(speed * c->size)));
    v.rotateBy(double(angle));
    b->vel = c->vel + v;
    b->game = c->game;
    b->group = c->group;
    b->power = static_cast<spaceCraft *>(c)->weapon;
    b->mass = 10.0f;
    b->game->addEntity(b, &b->game->bullets);
}

int spaceCraftBull::draw(sprite *s, vec2d sz) {
    s->col.unpackRGBA(teamColours[group - 1]);
    s->angle = 0;
    s->frame = 0x8d;
    float t = float(oxRNG::rand());
    s->col.a = game->paused ? 0.3 : 1.0;
    double k = double(t * 0.2f + 0.4f);
    s->width = double(size + size) * sz.x * k;
    s->height = k * double(size + size) * sz.y;
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// Bent by the planets (and the saucers on some levels); swallowed by the sun.
void spaceCraftBull::run() {
    spaceWars *g = game;
    vel = vel + g->feelTheForce(this, g->planets);
    if (((g->level >> 2) & 3) > 1)
        vel = vel + g->feelTheForce(this, g->saucers);
    hit = false;
    bull::run();
    g->bounceOff(this, g->bullets);
    if (g->sun && pos.length() <= double(g->sun->size) * 1.5)
        dead = true;
}

// ================================================================== jet ====

namespace {
// Formation offsets (in shot sizes) and start positions by player.
const float kJetSlotX[6] = {0, -2, 2, -3, 0, 3};
const float kJetSlotY[6] = {0, 2, 2, 5, 6, 5};
} // namespace

void jet::init(spaceWars *g, int player, int s) {
    power = 8.0f;
    cruise = 0.8f;
    mass = 1.0f;
    game = g;
    life = 0;
    col.unpackRGBA(teamColours[player]);
    spriteIndex = 0x9b;
    hp = 0x13;
    type = 1;
    weapon = 0;
    elasticity = 1.4f;
    class gun *gn = new class gun();
    gun = gn;
    gn->bounceShots = true;
    gn->owner = this;
    gn->disabled = false;
    gn->shotFade = type == 7 ? 0.03f : 0.02f;
    gn->shotSprite = 0x8c;
    gn->shotDrag = 0.99f;
    gn->shotSize = 0.005f;
    gn->reload = type == 7 ? 20 : 10;
    joy = player;
    drag = vec2d(0.75, 0.75);
    group = player + 1;
    slot = s;
    reSpawn();
    aiPhase = float(oxRNG::rand(kTwoPi));
    aiPhaseSpeed = float(oxRNG::rand(0.01, 0.03));
    aiTurn = float(oxRNG::rand(0.01, 0.02));
}

void jet::reSpawn() {
    game->shields[joy] = 9;
    hp = 9;
    distance = 0;
    weapon = 0;
    trailCountdown = 0;
    spin = 0;
    mass = 1.0f;
    power = 2.5f;
    spinDecay = 0.1f;
    float sz = size;
    pos.x = double(kJetSlotX[slot] * sz + kJetSlotX[slot] * sz + kShipStartX[joy]);
    pos.y = double(kJetSlotY[slot] * sz + kJetSlotY[slot] * sz + kShipStartY[joy]);
    dead = false;
    vel = vec2d(0, 0);
}

// Jets always fly forwards; the stick turns them (x) and changes speed (y).
void jet::run() {
    spaceWars *g = game;
    cellGrid *grid = g->grid;
    if (g->demo)
        gun->disabled = true;
    g->touch(this, g->minotaurs);
    g->bounceOff(this, g->planets);
    if (g->bounceOff(this, g->bullets))
        damage();
    pos = pos + vel;
    vel = vel * drag;
    col.a = g->paused ? 0.3 : 1.0;
    if (0.5 < pos.x)
        pos.x -= 1.0;
    if (0.5 < pos.y)
        pos.y -= 1.0;
    if (pos.x < -0.5)
        pos.x += 1.0;
    if (pos.y < -0.5)
        pos.y += 1.0;

    // Vapour trail: a dot every `size` flown.
    float sp = float(vel.length());
    distance += sp;
    trailCountdown -= sp;
    if (trailCountdown < 0.0f) {
        trailCountdown = size + trailCountdown;
        jetTrail *t = new jetTrail();
        t->pos = pos;
        t->spriteIndex = 0xb8;
        t->size = size * 0.5f;
        t->baseSize = t->size;
        t->col.unpackRGBA(teamColours[joy]);
        t->col.a = 1.0;
        t->rot = float(oxRNG::rand(kTwoPi));
        t->startDist = distance;
        t->group = group;
        t->life = life;
        t->game = g;
        t->dist = 0;
        t->owner = this;
        t->smoke = false;
        if (double(hp) < oxRNG::rand(9.0) && oxRNG::rand(10.0) < 6.0)
            t->smoke = true;
        g->addEntity(t, &g->particles);
    }

    thrust = vec2d(0, double(-cruise));
    if (!autopilot) {
        const auto control = g->playerControl(joy);
        if (control.active) {
            thrust.y += double(power) * control.y;
            if (-0.2 < thrust.y)
                thrust.y = -0.2;
            float r = float(double(rot) + control.x * -0.8);
            rot = r;
            if (3.1415927f < r)
                rot = r - 6.2831855f;
            if (double(rot) < -kPi)
                rot = float(double(rot) + kTwoPi);
        }
    } else {
        aiPhase += aiPhaseSpeed;
        float sn = std::sin(aiPhase);
        double c = double(-cruise);
        thrust.y = c + c * (double(sn) + double(sn) + 0.5) * 0.2;
        entity *t = nullptr;
        for (entity *e = g->players; e; e = e->next)
            if (float((e->pos - pos).length()) < 1000.0f && e->type == 1)
                t = e;
        if (t) {
            vec2d d = t->pos - pos;
            float want = float(std::atan2(-d.y, d.x) - kHalfPi);
            if (want <= rot)
                rot -= aiTurn;
            else
                rot += aiTurn;
        }
    }
    drawAngle = rot;
    thrust.rotateBy(double(rot));
    double m = double(mass * 1000.0f);
    col.a = grid && grid->getAttr(pos) == 1 ? 0.3 : 1.0; // in a cloud
    thrusting = true;
    vec2d nv = vel + vec2d(thrust.x / m, thrust.y / m);
    if (nv.length() < 0.015)
        vel = nv;
    if (gun)
        gun->run();
}

void jet::damage() {
    spaceWars *g = game;
    explosion *x = new explosion();
    x->game = g;
    x->from.white(1.0);
    x->to.blue(0.0);
    x->rot = 0;
    x->seed = unsigned(neonRand());
    x->pos = pos;
    x->vel = vec2d(0, 0);
    x->size = 0.02f;
    x->sizeStart = 0.5f;
    x->sizeEnd = 0.3f;
    x->count = 10;
    x->radius = 4.0f;
    x->fade = 0.03f;
    g->addEntity(x, &g->particles);
    if (spin > 0.0f)
        return;
    hp--;
    g->shields[joy] = hp;
    if (hp < 1) {
        if (!g->demo)
            g->playSample(0x18);
        size = 0.06f;
        blowUp(unsigned(neonRand()), 0x100);
        size = 0.02f;
        life++;
        g->lives[joy]--;
        if (g->lives[joy] > 0)
            reSpawn();
    } else {
        if (!g->demo)
            g->playSample(0x1b);
        spin = 1.0f;
    }
}

// =============================================================== trails ====

int jetTrail::draw(sprite *s, vec2d sz) {
    if (game->paused)
        col.a *= 0.3;
    return entity::draw(s, sz);
}

// Fades and grows with the distance its jet has flown since; once the jet
// is gone, keeps fading on its own.
void jetTrail::run() {
    spaceWars *g = game;
    entity *e = g->players;
    for (; e; e = e->next)
        if (e == owner && life == static_cast<jet *>(owner)->life)
            break;
    if (e) {
        dist = static_cast<jet *>(owner)->distance - startDist;
        if (dist > size * 5.0f) {
            dead = true;
            return;
        }
    } else {
        if (double(dist) < 0.0001) {
            dead = true;
            return;
        }
        dist = float(double(dist) + 0.002);
        if (dist > size * 5.0f) {
            dead = true;
            return;
        }
    }
    float t = dist / (size * 5.0f);
    colour a, b;
    if (!smoke) {
        a.unpackRGBA(teamColours[group - 1]);
        b.white(0.0);
        col = a.lerp(b, t);
        size = baseSize + t * 0.04f;
    } else {
        a.unpackRGBA(0xff8000ffu);
        b.unpackRGBA(0x30303000u);
        col = a.lerp(b, t);
        size = baseSize + t * 0.12f;
    }
    if (g->paused)
        col.a *= 0.3;
}

int tankTrail::draw(sprite *s, vec2d sz) {
    if (game->paused)
        col.a *= 0.3;
    return entity::draw(s, sz);
}

void tankTrail::run() {
    spaceWars *g = game;
    entity *e = g->players;
    for (; e; e = e->next)
        if (static_cast<craft *>(e)->joy == group - 1)
            break;
    if (e) {
        dist = static_cast<tank *>(owner)->distance - startDist;
    } else {
        if (double(dist) < 0.0001) {
            dead = true;
            return;
        }
        dist = float(double(dist) + 0.002);
    }
    float lim = size * 20.0f;
    if (dist <= lim) {
        col.a = double(1.0f - dist / lim);
        return;
    }
    dead = true;
}

// ================================================================= tank ====

tank::~tank() {
    delete gun;
}

void tank::init(spaceWars *g, int player) {
    power = 4.0f;
    mass = 1.0f;
    hp = 9;
    powerLevel = 0;
    game = g;
    distance = 0;
    trailCountdown = 0;
    type = 1;
    noWrap = true;
    elasticity = 3.4f;
    size = 0.03f;
    tankGun *gn = new tankGun();
    gn->reload = 0x50;
    gn->speed = 0.4f;
    gun = gn;
    gn->owner = this;
    gn->disabled = false;
    bumped = false;
    apTimer = 0;
    apDir = vec2d(0, -1.0);
    joy = player;
    group = player + 1;
    drag = vec2d(0.8, 0.8);
    g->shields[player] = 3;
    hp = 3;
    static const float kX[4] = {-0.25f, 0.25f, -0.25f, 0.25f};
    static const float kY[4] = {-0.35f, 0.25f, 0.25f, -0.35f};
    powerLevel = 0;
    spin = 0;
    maxSpeed = 0.005f;
    mass = 1.0f;
    power = 8.5f;
    spinDecay = 0.02f;
    pos = vec2d(double(kX[joy]), double(kY[joy]));
    vel = vec2d(0, 0);
    dead = false;
}

int tank::draw(sprite *s, vec2d sz) {
    spaceWars *g = game;
    s->frame = 0x9a;
    s->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    s->angle = double(drawAngle);
    s->col.unpackRGBA(teamColours[joy]);
    s->width = double(size + size) * sz.x;
    s->height = double(size + size) * sz.y * 0.8;
    if (0.0f < spin) {
        spin -= spinDecay;
        if (spin < 0.0f && powerLevel > 0)
            powerLevel--;
        if ((age & 2) == 0) {
            s->col.yellow(1.0);
            s->width *= 1.8;
            s->height *= 1.8;
        } else {
            s->col.unpackRGBA(teamColours[joy]);
            s->width *= 2.8;
            s->height *= 2.8;
        }
    }
    s->pos = pos * sz;
    s->col.a = g->paused ? 0.3 : 1.0;
    s->setupChecking();
    // The turret.
    s->flags = SPR_CENTRE_X | SPR_FLIP_Y;
    s->frame = 0x9b;
    s->angle = double(rot) + kPi;
    s->width = double(size + size) * sz.x;
    s->col.unpackRGBA(teamColours[joy]);
    s->col.a = g->paused ? 0.3 : 1.0;
    s->setupChecking();
    s->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    return 12;
}

void tank::checkPath(vec2d from, vec2d to) {
    pathResult = 0;
    vec2d step = (to - from);
    step.x /= 40.0;
    step.y /= 40.0;
    vec2d p = from;
    cellGrid *grid = game->grid;
    for (int i = 0; i < 39; i++) {
        p = p + step;
        if (grid->getAttr(p) == 1) {
            pathResult = 1;
            pathLength = float((p - from).length());
            return;
        }
        if (0.5 < p.x || p.x < -0.5) {
            pathResult = 2;
            pathLength = float((p - from).length());
            return;
        }
        if (0.5 < p.y || p.y < -0.5) {
            pathResult = 3;
            pathLength = float((p - from).length());
            return;
        }
    }
    pathLength = float((to - from).length());
}

void tank::run() {
    spaceWars *g = game;
    cellGrid *grid = g->grid;
    age++;
    vec2d saved = pos;
    g->touch(this, g->minotaurs);
    g->bounceOff(this, g->planets);
    g->bounceOff(this, g->players);
    g->bounceOff(this, g->bullets);
    float savedRot = rot, savedDraw = drawAngle;
    craft::run();
    if (spin <= 0.0f) {
        // Track marks every `size` driven.
        float moved = float((pos - saved).length());
        distance += moved;
        trailCountdown -= moved;
        if (trailCountdown < 0.0f) {
            trailCountdown = size + trailCountdown;
            tankTrail *t = new tankTrail();
            t->pos = pos;
            t->spriteIndex = 0xb7;
            t->size = size;
            t->col.unpackRGBA(teamColours[joy]);
            t->col.a = 1.0;
            t->rot = drawAngle;
            if (50.0 < oxRNG::rand(100.0))
                t->rot = t->rot + 3.1415927f;
            t->startDist = distance;
            t->group = group;
            t->game = g;
            t->dist = 0;
            t->owner = this;
            g->addEntity(t, &g->particles);
        }
    } else {
        float r = savedRot + 0.3f;
        rot = r;
        drawAngle = savedDraw + 0.3f;
        if (3.1415927f < r)
            rot = r - 6.2831855f;
        if (3.1415927f < drawAngle)
            drawAngle = drawAngle - 6.2831855f;
    }
    // Walls: probe one radius ahead; on a wall, step back.
    vec2d ahead(0, double(-size));
    ahead.rotateBy(double(drawAngle));
    int a = grid ? grid->getAttr(pos + ahead) : 0;
    if (a != 999) {
        if (a == 0) {
            bumped = false;
        } else {
            if (a < 0)
                ahead.y += pos.y < 0.0 ? -0.01 : 0.01;
            pos = saved + ahead * -1.0;
            bumped = true;
            apTimer = 0;
        }
    }
    if (0.48 < std::fabs(pos.x))
        pos = pos * 0.95;
    if (pos.y < -0.42 || 0.48 < pos.y)
        pos = pos * 0.95;
}

// CPU tanks: head for an enemy tank or a minotaur when the way is clear,
// otherwise pick one of 16 directions with a clear run (> 0.08).
void tank::doAutoPilot() {
    spaceWars *g = game;
    if (--apTimer <= 0) {
        apTimer = (neonRand() + 30) & 0x7f;
        vec2d target;
        bool found = false;
        entity *t = nullptr;
        for (entity *e = g->players; e; e = e->next)
            if (float((e->pos - pos).length()) < 1000.0f && e->type == 1)
                t = e;
        if (t) {
            checkPath(pos, t->pos);
            if (pathResult == 0) {
                target = t->pos;
                found = true;
            }
        }
        if (!found) {
            t = nullptr;
            for (entity *e = g->minotaurs; e; e = e->next)
                if (float((e->pos - pos).length()) < 1000.0f && e->type == 4)
                    t = e;
            if (t) {
                checkPath(pos, t->pos);
                if (pathResult == 0) {
                    target = t->pos;
                    found = true;
                }
            }
        }
        if (!found) {
            vec2d options[16], best;
            int n = 0;
            float bestLen = 0, ang = 0;
            for (int i = 0; i < 16; i++) {
                vec2d d(0, 1.0);
                d.rotateBy(double(ang));
                ang += 0.3926991f;
                checkPath(pos, pos + d);
                if (0.08f < pathLength)
                    options[n++] = d;
                if (bestLen < pathLength) {
                    bestLen = pathLength;
                    best = d;
                }
            }
            vec2d pick = !bumped && n > 0 ? options[oxRNG::irand(double(n))] : best;
            target = pos + pick;
        }
        apDir = target - pos;
    }
    rot = float(std::atan2(-apDir.y, apDir.x) - kHalfPi);
    thrusting = true;
    double p = double(power);
    thrust = vec2d(p * apDir.x * 0.03, p * apDir.y * 0.03);
    double m = double(mass * 1000.0f);
    force.x += thrust.x / m;
    force.y += thrust.y / m;
}

// ============================================================== tankGun ====

void tankGun::run() {
    tank *t = static_cast<tank *>(owner);
    spaceWars *g = t->game;
    if (g->demo)
        return;
    if (0.0f < t->spin) {
        burst = 0;
        timer = 1;
        return;
    }
    if (burst == 0) {
        if (--timer < 1) {
            if (!g->demo)
                g->playSample(0x17);
            timer = reload - t->powerLevel * 8;
            if (t->type == 7)
                timer *= 2;
            if (timer < 0x14)
                timer = 0x14;
            shoot();
        }
    } else {
        if (--timer < 1) {
            if (burst == 30 && !g->demo)
                g->playGrunt();
            timer = 2;
            burst--;
            shoot();
        }
    }
}

void tankGun::shoot() {
    tank *t = static_cast<tank *>(owner);
    if (t->spin > 0.0f)
        return;
    tankBull *b = new tankBull();
    b->fade = 0.007f;
    b->drag = vec2d(0.999, 0.999);
    b->type = 3;
    b->pos = t->pos;
    b->gun = this;
    angle = t->rot;
    b->flags = 1;
    vec2d v(0, double(-(speed * t->size)));
    v.rotateBy(double(angle));
    b->vel = t->vel + v;
    b->game = t->game;
    b->group = t->group;
    b->power = t->powerLevel;
    b->mass = 10.0f;
    b->game->addEntity(b, &b->game->bullets);
}

int tankBull::draw(sprite *s, vec2d sz) {
    s->col.unpackRGBA(teamColours[group - 1]);
    s->angle = 0;
    s->frame = 0x8d;
    float t = float(oxRNG::rand());
    s->col.a = game->paused ? 0.3 : 1.0;
    double k = double(t * 0.3f + 0.7f);
    s->width = double(size + size) * sz.x * k;
    s->height = k * double(size + size) * sz.y;
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// A hit on another team's tank scores the shooter's power level and spins
// the victim (who loses its own power).
void tankBull::hitBy(entity *by) {
    spaceWars *g = game;
    tank *shooter = static_cast<tank *>(gun->owner);
    if (g->demo || !(by->type == 7 || by->type == 1) || group == by->group)
        return;
    tank *victim = static_cast<tank *>(by);
    if (victim->spin > 0.0f)
        return;
    g->playSample(0x18);
    g->lives[group - 1] += shooter->powerLevel;
    victim->spin = 1.0f;
    victim->powerLevel = 0;
    scoreExplosion *x = new scoreExplosion();
    x->count = 0x80;
    x->from.unpackRGBA(teamColours[group - 1]);
    x->to = x->from;
    x->to.a = 0.2;
    x->rot = rot;
    x->seed = unsigned(neonRand());
    x->pos = pos;
    x->vel = vel * 0.1;
    x->size = size * 4.0f;
    x->sizeStart = 0.5f;
    x->game = g;
    x->sizeEnd = 0.2f;
    x->setScore(shooter->powerLevel + 1, pos);
    x->colourType = group;
    x->digitScale = 1.4f;
    g->addEntity(x, &g->particles);
}

// Bounces off the arena edges and (if enabled) the maze walls, sparking.
void tankBull::run() {
    spaceWars *g = game;
    cellGrid *grid = g->grid;
    hit = false;
    vec2d saved = pos;
    bull::run();
    bool bounced = false;
    if (pos.x >= 0.48 && vel.x > 0.0) {
        vel.x = -vel.x;
        bounced = true;
    }
    if (pos.x <= -0.48 && vel.x < 0.0) {
        bounced = true;
        vel.x = -vel.x;
    }
    if (0.48 <= pos.y && 0.0 < vel.y) {
        bounced = true;
        vel.y = -vel.y;
    }
    if (pos.y <= -0.42 && vel.y < 0.0) {
        vel.y = -vel.y;
        bounced = true;
    }
    if (bounced) {
        pos = pos + vel;
        makeSpark(g, pos);
        if (!g->demo)
            g->playSample(0x19);
    }
    int a = grid ? grid->getAttr(pos) : 0;
    if (a != 999 && a != 0) {
        makeSpark(g, pos);
        if (!g->bouncyShots) {
            dead = true;
        } else {
            vec2d probe(vel.x + saved.x, saved.y);
            if (grid->getAttr(probe) == 0)
                vel.y = -vel.y;
            else
                vel.x = -vel.x;
        }
    }
    if (0.02 < vel.length())
        vel = normalized(vel) * 0.02;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
