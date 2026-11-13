// game_core.cpp - controlBlock (touch zones, entity lists, collision,
// gravity), the entity base class, particles, explosions, shots and guns.
#include "neon/entity_motion.h"
#include "neon/particles.h"
#include "neon/entity_list.h"
#include "neon/entity_contact.h"
#include "game.h"
#include "game_entities.h"

#include <cmath>

namespace NEON_GAME {
namespace Neon_Internals {

const uint32_t teamColours[4] = {0xff0000ffu, 0x0000ffffu, 0x00ff00ffu, 0xff00ffffu};
const char *const teamWinMessages[5] = {
    "~9009BETTER RED THAN DEAD", "~0099A WINNAR IS BLUE", "~0909MEAN GREEN WINNING MACHINE",
    "~9099PURPLE PWNAGE",        "~9999*** DRAW ***",
};

int *extractScore(int *buf, int n, int value, int suffix) {
    int i = -1;
    if (n > 0) {
        for (int k = 0; k < n; k++)
            buf[k] = 0;
        i = n - 1;
    }
    if (suffix != 0) {
        buf[i] = suffix == -2 ? 0 : suffix;
        i--;
    }
    if (i >= 0 && value > 0) {
        bool more;
        do {
            buf[i] = value % 10;
            i--;
            value /= 10;
            more = i >= 0 && value > 0;
        } while (more);
    }
    return buf + i + 1;
}

vec2d vlerp(const vec2d &a, const vec2d &b, float t) {
    double k = t < -1.0f ? -1.0 : (t > 1.0f ? 1.0 : double(t));
    return {a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k};
}

vec2d normalized(const vec2d &v) {
    double l = std::sqrt(v.y * v.y + v.x * v.x);
    if (l > 0.0)
        return {v.x / l, v.y / l};
    return {0, 0};
}

void makeSpark(spaceWars *g, vec2d p) {
    explosion *x = new explosion();
    x->game = g;
    x->from.white(1.0);
    x->to.blue(0.0);
    x->rot = 0;
    x->seed = unsigned(neonRand());
    x->pos = p;
    x->vel = vec2d(0, 0);
    x->size = 0.02f;
    x->sizeStart = 0.5f;
    x->sizeEnd = 0.3f;
    x->count = 10;
    x->radius = 4.0f;
    x->fade = 0.03f;
    g->addEntity(x, &g->particles);
}

// ========================================================= controlBlock ====

controlBlock::controlBlock() = default;

void controlBlock::run() {}

neon::input::PlayerMotion controlBlock::playerControl(int player) const {
    const auto &pad = controllers.motion(player);
    return pad.active ? pad
                      : neon::input::PlayerMotion{joyActive[player], joyDelta[player].x,
                                                  joyDelta[player].y};
}

void controlBlock::purgeTouches() {
    controllers.clearOwners();
    neon::input::purgeTouchState(*this);
}

void controlBlock::processJoyUp(unsigned id) {
    neon::input::endTouch(*this, id);
}

// A touch near the centre arms the pause (and flashes); a second one within
// 30 frames pauses. Each touch takes a free touch slot and, if its zone's
// joystick is free, becomes that joystick (zones: 2 players left / right
// halves, 3-4 players quarters).
void controlBlock::processJoyDown(unsigned id, vec2d p) {
    if (!pauseArmed) {
        paused = false;
        pauseTapTimer = 0;
    } else if (std::sqrt(p.y * p.y + p.x * p.x) < 0.05) {
        if (pauseTapTimer < 1) {
            pauseTapTimer = 30;
            flash = 1.0f;
        } else {
            paused = true;
            flash = 0;
        }
    }
    if (inputFlipped)
        p = p * -1.0;
    neon::input::beginTouch(*this, id, p, neon::input::TouchTracking::Basic);
    int zone;
    if (joyZones == 2)
        zone = p.x > 0.0;
    else if (joyZones < 2 || joyZones > 4)
        zone = 0;
    else
        zone = ((p.y > 0.0) != (p.x > 0.0)) * 2 + (p.x > 0.0);
    if (demo)
        zone = 0;
    if (!controllers.motion(zone).active)
        neon::input::claimTouchJoystick(*this, zone, id, p);
}

void controlBlock::processJoyMove(unsigned id, vec2d p) {
    if (inputFlipped)
        p = p * -1.;
    neon::input::moveTouch(*this, id, p, neon::input::TouchTracking::Basic);
}

void controlBlock::addEntity(entity *e, entity **list) {
    neon::entities::prepend(*list, *e);
}

// Circle test of `a` against every live entity of `b`. On contact the other
// entity is told (wasShot if a's flags & 3, then hitBy). With `bounce` both
// are pushed apart and exchange momentum by mass and elasticity.
int controlBlock::collide(entity *a, entity *b, bool bounce) {
    return neon::entities::collideEarly(a, b, bounce, [](const entity &target) { return false; });
}

vec2d controlBlock::attract(entity *a, entity *list, float strength) {
    vec2d result(0, 0);
    float nearest = 512.0f;
    if (list) {
        float k = a->mass * 1e-06f;
        float ks = k * strength;
        double fx = 0, fy = 0;
        for (entity *e = list; e; e = e->next) {
            if (e == a || a->fc != 0)
                continue;
            double dx = (e->pos.x - a->pos.x) * 512.0;
            double dy = (e->pos.y - a->pos.y) * 512.0;
            double d = std::sqrt(dy * dy + dx * dx);
            float df = float(d);
            if (nearest > df)
                nearest = df;
            double nx = 0, ny = 0;
            if (d > 0.0) {
                nx = dx / d;
                ny = dy / d;
            } else {
                continue; // (the original divides by zero here)
            }
            float f = strength > 0.0f ? ks / (df * df) : (k * e->mass) / (df * df);
            fx += nx * double(f);
            fy += ny * double(f);
        }
        result = vec2d(fx, fy);
    }
    a->nearest = nearest;
    return result;
}

// =============================================================== entity ====

entity::entity() = default;

void entity::run() {
    neon::entities::integrate(pos, vel, drag, age, noWrap, wrapped, vec2d{wrapWidth, wrapHeight});
}

void entity::wasShot(entity *by) {
    hp -= static_cast<bull *>(by)->damage;
    if (hp < 0) {
        blowUp(unsigned(neonRand()), 0x40);
        dead = true;
    }
}

int entity::draw(sprite *s, vec2d sz) {
    s->col = col;
    s->angle = double(rot);
    s->frame = spriteIndex + 1;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

// An explosion in this entity's colour (fading to half alpha); the entity dies.
void entity::blowUp(unsigned seed, int count) {
    explosion *x = new explosion();
    neon::particles::initializeColourExplosion(*x, *this, seed, count);
    x->game = game;
    game->addEntity(x, &game->particles);
    dead = true;
}

// ============================================================ _particle ====

_particle::_particle() {
    spriteIndex = 0x8c;
    size = 0.005f;
}

int _particle::draw(sprite *s, vec2d sz) {
    s->angle = 0;
    s->frame = spriteIndex + 1;
    const auto animation = neon::particles::scaleFrame(life, sizeStart, sizeEnd);
    const auto extent = neon::particles::dimensions(size, animation.scale, sz, scaleX, scaleY);
    s->width = extent.x;
    s->height = extent.y;
    s->col = from.lerp(to, animation.progress);
    s->pos = pos * sz;
    s->setupChecking();
    return 6;
}

void _particle::run() {
    entity::run();
    neon::particles::updateAfterIntegration(*this, [](int kind, float alpha, int frame) {
        return getDynamicColour(kind, alpha, frame);
    });
}

// ============================================================ explosion ====

explosion::explosion() {
    from.yellow(0.4);
    to.red(0.0);
    sizeStart = 0.3f;
    sizeEnd = 0.2f;
    seed = unsigned(neonRand());
}

int explosion::draw(sprite *s, vec2d sz) {
    s->frame = 0x8d;
    s->angle = double(rot);
    return neon::particles::drawSeededBurst(
        *this, *s, sz, [] { return neonRand(); }, [](unsigned seed) { neonSrand(seed); },
        [](double low, double high) { return oxRNG::rand(low, high); });
}

void scoreExplosion::setScore(int value, vec2d at) {
    digitPtr = extractScore(digits, 8, value, -1);
    digitPos = at;
}

// The burst, then the digits centred on digitPos, drifting.
int scoreExplosion::draw(sprite *s, vec2d sz) {
    explosion::draw(s, sz);
    s->angle = 0;
    s->width = double(size * digitScale) * sz.x;
    s->height = sz.y * double(size * digitScale);
    s->pos.y = sz.y * (digitPos.y + double(1.0f - life) * 0.05);
    s->pos.x = digitPos.x * sz.x;
    s->col = getDynamicColour(colourType, life, game->frameCount);
    if (!game->paused)
        digitPos.x *= 0.99;
    else
        s->col.a *= 0.3;
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

// ================================================================= bull ====

bull::bull() {
    size = 0.01f;
}

int bull::draw(sprite *s, vec2d sz) {
    if (game->paused)
        col.a = 0.3; // (never restored)
    return entity::draw(s, sz);
}

void bull::run() {
    _particle::run();
    if (bounceShots)
        game->bounceOff(this, game->bullets);
    game->bounceOff(this, game->saucers);
    game->bounceOff(this, game->planets);
}

void enemyBull::run() {
    _particle::run();
    game->bounceOff(this, game->players);
    game->bounceOff(this, game->planets);
}

// ================================================================== gun ====

void gun::run() {
    if (disabled)
        return;
    if (--timer < 1) {
        timer = reload;
        shoot();
    }
}

// The jets' gun: a shot from the owner's nose along its heading.
void gun::shoot() {
    bull *b = new bull();
    b->col = owner->col;
    b->col.a = 1.0;
    b->pos = owner->pos;
    b->bounceShots = bounceShots;
    b->fade = shotFade;
    b->spriteIndex = shotSprite;
    angle = owner->rot;
    b->flags = 1;
    vec2d v(0, double(-(speed * owner->size)));
    v.rotateBy(double(angle));
    b->vel = owner->vel + v;
    b->game = owner->game;
    b->group = owner->group;
    b->size = shotSize;
    b->mass = 50.0f;
    b->drag = vec2d(double(shotDrag), double(shotDrag));
    b->hp = 1;
    b->game->addEntity(b, &b->game->bullets);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
