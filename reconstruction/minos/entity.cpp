// entity.cpp - see entity.h.
#include "entity.h"
#include "neon/entity_motion.h"

namespace minos {

std::optional<ExplosionBirth> centityRun(MapEntity &e, MapEntityWorld &w) {
    e.pos = {e.pos.x - w.scrollDelta.x, e.pos.y - w.scrollDelta.y};
    // Out of the visible band. `bmi` (y < -0.7) then `ble` (y <= 0.7 skips): a NaN y takes
    // the ble on the unordered result and stays alive, as this form does.
    if (e.pos.y < -0.7 || e.pos.y > 0.7) {
        e.dead = true;
        if (e.row >= 0)
            w.map->clearLaunched(e.row, e.column); // may be launched again
    }
    if (w.clearAll == 0 || e.burstOnClear != 1)
        return std::nullopt;
    ExplosionBirth x; // new explo: entity::entity defaults, then:
    x.fromScale = 0.5f;
    x.toScale = 2.0f;
    x.scaleRate = 0.06f;
    x.spriteIndex = 4;
    x.pos = e.pos;
    x.rot = float(w.rand->range(-3.141592654, 3.141592654)); // one global rand() draw
    x.progress = 0;
    x.baseScale = {1.0, 1.0};
    x.game = w.game;
    e.dead = true; // the original stores burstOnClear (== 1)
    return x;
}

void Entity::run() {
    neon::entities::integrate(pos, vel, drag, age, noWrap, wrapped, wrap);
}

void MapEntity::run() {
    MapEntityWorld w;
    w.scrollDelta = game->scrollDelta();
    w.clearAll = game->clearAll();
    w.map = &game->map();
    w.rand = &game->rand();
    w.game = game;
    if (auto x = centityRun(*this, w))
        game->addMapExplosion(*x);
}

bool centityDrawVisible(MapEntity &e) {
    if (e.hidden) {
        e.drawDelay = 1;
        return false;
    }
    return true;
}

} // namespace minos
