#pragma once
#include "native_entity.h"
namespace gr::reconstruction::ios {
// controlBlock::touch (0x9058), the non-bouncing branch of collide (0x8d00).
// The original stores these last-contact outputs on the game controlBlock.
struct TouchCollider {
    Vec2 lastPosition, normal;
    bool touch(Entity &mover, ListNode *first);
};
} // namespace gr::reconstruction::ios
