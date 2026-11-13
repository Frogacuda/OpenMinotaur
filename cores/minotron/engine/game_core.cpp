// game_core.cpp - controlBlock (touch slots, entity lists, collision), the
// entity base class and the particle class.
#include "neon/entity_motion.h"
#include "neon/particles.h"
#include "neon/entity_list.h"
#include "neon/entity_contact.h"
#include "game.h"
#include "game_entities.h"

namespace NEON_GAME {
namespace Neon_Internals {

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

// ========================================================= controlBlock ====

controlBlock::controlBlock() = default;

void controlBlock::run() {}

void controlBlock::drawList(entity *e) {
    for (; e; e = e->next)
        e->draw(spr, drawSize);
}

void controlBlock::drawList(entity *e, float alpha) {
    if (alpha > 1.0f)
        alpha = 1.0f;
    else if (alpha < 0.0f)
        alpha = 0.0f;
    for (; e; e = e->next) {
        double a = e->col.a;
        e->col.a = a * double(alpha);
        e->draw(spr, drawSize);
        e->col.a = double(float(a));
    }
}

int controlBlock::findOldestTouch(int exclude) {
    return neon::input::oldestTouch(*this, exclude, frameCount);
}

int controlBlock::findLeftTouch() {
    return neon::input::firstSlot(TouchCapacity,
                                  [&](int i) { return touchActive[i] && touchStartPos[i].x < 0.; });
}

int controlBlock::findRightTouch() {
    return neon::input::firstSlot(TouchCapacity,
                                  [&](int i) { return touchActive[i] && touchStartPos[i].x > 0.; });
}

void controlBlock::purgeTouches() {
    neon::input::purgeTouchState(*this);
}

void controlBlock::processJoyUp(unsigned id) {
    neon::input::endTouch(*this, id);
}

void controlBlock::processJoyDown(unsigned id, vec2d p) {
    if (inputFlipped)
        p = p * -1.;
    neon::input::beginTouch(*this, id, p, neon::input::TouchTracking::MoveCounter);
    neon::input::claimTouchJoystick(*this, 0, id, p);
}

void controlBlock::processJoyMove(unsigned id, vec2d p) {
    if (inputFlipped)
        p = p * -1.;
    neon::input::moveTouch(*this, id, p, neon::input::TouchTracking::MoveCounter);
}

void controlBlock::addEntity(entity *e, entity **list) {
    neon::entities::prepend(*list, *e);
}

void controlBlock::joyWatchdog() {
    neon::input::watchTouchIdle(*this, [&] { purgeTouches(); });
}

// Circle test of `a` against every live entity of `list`. On contact the
// other entity is told (wasShot if a's flags & 3, then hitBy). With `bounce`
// both are pushed apart and exchange momentum by mass and elasticity.
int controlBlock::collide(entity *a, entity *b, bool bounce) {
    if (a->drawDelay != 0 || a->dead)
        return 0;
    return neon::entities::collideEarly(a, b, bounce,
                                        [](const entity &target) { return target.drawDelay != 0; });
}

// =============================================================== entity ====

entity::entity() = default;

void entity::run() {
    neon::entities::integrate(pos, vel, drag, age, noWrap, wrapped, vec2d{wrapWidth, wrapHeight});
}

// Only called from collide() for an entity whose flags & 3, and no Minotron
// entity sets flags, so this is unreachable. (The original subtracts the int
// at offset 0x21c of the "bull", past the end of every bullet class.)
void entity::wasShot(entity *by) {
    health -= by->hp;
    if (health < 0) {
        blowUp(unsigned(neonRand()), 0x40);
        dead = true;
    }
}

int entity::draw(sprite *s, vec2d sz) {
    if (spriteIndex < 0)
        return 0;
    s->col = col;
    if (flashFrames != 0) {
        flashFrames--;
        s->col.b = 0;
        float t = float(flashFrames & 7) / 7.0f;
        s->col.g = double(t);
        s->col.r = double(t) * 0.5 + 0.5;
    }
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    s->width = double(size + size) * sz.x * scaleX;
    s->height = double(size + size) * sz.y * scaleY;
    s->pos = pos * sz;
    s->zOffset = zOffset;
    if (drawDelay > 0) {
        drawDelay--;
        return 0;
    }
    s->setupChecking();
    return 6;
}

// The original allocates an `explosion` particle (yellow fading to red, then
// overwritten with this entity's colour) and removes this entity.
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
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    const auto animation = neon::particles::scaleFrame(life, sizeStart, sizeEnd);
    const auto extent = neon::particles::dimensions(size, animation.scale, sz, scaleX, scaleY);
    s->width = extent.x;
    s->height = extent.y;
    s->col = clerp(from, to, animation.progress);
    s->pos = pos * sz;
    s->zOffset = zOffset;
    s->setupChecking();
    return 6;
}

void _particle::run() {
    entity::run();
    neon::particles::updateAfterIntegration(*this, [](int kind, float alpha, int frame) {
        return getDynamicColour(kind, alpha, frame);
    });
}

} // namespace Neon_Internals
} // namespace NEON_GAME
