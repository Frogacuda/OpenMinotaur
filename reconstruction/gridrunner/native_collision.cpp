#include "neon/entity_motion.h"
#include "native_collision.h"
#include <cmath>
namespace gr::reconstruction::ios {
bool TouchCollider::touch(Entity &mover, ListNode *first) {
    const Vec2 entry = mover.position;
    const float entryRadius = mover.radius;
    if (mover.drawDelay != 0 || mover.dead || !first)
        return false;
    bool blocked = false, hit = false;
    for (ListNode *node = first; node; node = node->next) {
        auto *target = dynamic_cast<Entity *>(node);
        if (!target)
            throw std::logic_error("Original collision list requires Entity records");
        if (target == &mover || target->dead)
            continue;
        const double dx = entry.x - target->position.x, dy = entry.y - target->position.y;
        const float distance = neon::entities::roundedDistance(dx, dy);
        const float combined = entryRadius + target->radius;
        if (combined <= distance) {
            mover.contact08 = false;
            continue;
        }
        if (mover.collisionGroup > 0 && mover.collisionGroup == target->collisionGroup)
            blocked = true;
        if (mover.contact08) {
            blocked = true;
            continue;
        }
        if (target->drawDelay != 0) {
            blocked = true;
            continue;
        }
        if (!target->acceptsContact) {
            blocked = true;
            continue;
        }
        if (blocked)
            continue;
        mover.contact09 = true;
        if (mover.collisionFlags & 3)
            target->wasShot(mover);
        target->hitBy(mover);
        // Callbacks may move either entity. Only the distance test above uses
        // the captured entry position/radius. Do not stop when mover dies.
        lastPosition = target->position;
        const double nx = target->position.x - mover.position.x,
                     ny = target->position.y - mover.position.y;
        const double length = std::sqrt(nx * nx + ny * ny);
        normal = {};
        if (length > 0)
            normal = {nx / length, ny / length};
        blocked = false;
        hit = true;
        // next is deliberately read after callbacks in the for increment.
    }
    return hit;
}
} // namespace gr::reconstruction::ios
