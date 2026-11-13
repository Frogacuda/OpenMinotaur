// cavern_draw_emit.h - stm::draw's sprite protocol (0x5750c) for a MapDrawPass, over an
// engine sprite that supports neon::sprites::SPR_EXPLICIT_UV (engine/include/neon/
// sprite_draw.h).
//
// The original uses one scratch sprite for the whole map: it sets flag 0x1000, points the
// explicit-UV member at a local {u, v, du, dv}, selects frame 2 and the cell size once, then
// for every quad sets the position and colour and calls sprite::setupChecking; at the end it
// clears 0x1000. The pointer is left dangling in the original (harmless once the flag is
// clear); here it is reset to nullptr as a host safety default.
//
// Header-only template so the module does not fix the host sprite type yet. `setColour(s, c)`
// applies a QuadColour: palette entry copy (map palette, stm+4 + 80*index) or RGBA unpack.
#pragma once
#include "cavern_draw.h"
#include <type_traits>

namespace minos::cavern {

constexpr unsigned kSpriteExplicitUv = 0x1000; // neon::sprites::SPR_EXPLICIT_UV

template <class Sprite, class SetColour>
void emitMapDrawPass(const MapDrawPass &pass, Sprite &s, SetColour &&setColour) {
    using Rect = std::remove_cv_t<std::remove_pointer_t<decltype(s.explicitUv)>>;
    Rect uv{};
    s.flags |= kSpriteExplicitUv;
    s.explicitUv = &uv;
    s.frame = MapDrawPass::kFrame;
    s.width = pass.size.x;
    s.height = pass.size.y;
    for (const TileQuad &q : pass.quads) {
        s.pos.x = q.position.x;
        s.pos.y = q.position.y;
        uv.u = q.uv.u;
        uv.v = q.uv.v;
        uv.du = q.uv.du;
        uv.dv = q.uv.dv;
        setColour(s, q.colour);
        s.setupChecking();
    }
    s.flags &= ~kSpriteExplicitUv;
    s.explicitUv = nullptr;
}

} // namespace minos::cavern
