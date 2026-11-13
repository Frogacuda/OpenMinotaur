// neon_sprite.h - the sprite type and the immediate-mode drawing helpers.
//
// Sprites are drawn two ways:
//   * draw()/drawRegion(): into the drawing context's global batch (the
//     CDrawingObject's render object), flushed by purge()/settex().
//   * setupChecking()/setupSimple(): into a stack's own vertex store
//     (batchStack/batchPtr/batchCount), flushed by setupPurge(). The game's
//     sprites, drawn from inside the showJoy module, use this path.
#include "neon/module_context.h"
#pragma once

#include "neon_base.h"
#include "neon/sprite_draw.h"
#include "neon/render_state.h"
#include "neon_device.h"

namespace NEON_GAME {
namespace Neon_Internals {

class NeonResManager;
class NeonStack;
class NeonEffect;

// Sprite flags (sprite::flags, +0x5c)
using SpriteFlags = neon::sprites::SpriteFlags;
using neon::sprites::SPR_AREA_RT;
using neon::sprites::SPR_CENTRE_X;
using neon::sprites::SPR_CENTRE_Y;
using neon::sprites::SPR_CORNERS;
using neon::sprites::SPR_FLIP_X;
using neon::sprites::SPR_FLIP_Y;
using neon::sprites::SPR_KEEP_ASPECT;
using neon::sprites::SPR_REGION;
using neon::sprites::SPR_ROT_CCW;
using neon::sprites::SPR_ROT_CW;
using neon::sprites::SPR_ROTATE;

constexpr int kLitBase = 2; // sprite-map entries below this are whole pages

class sprite {
  public:
    sprite() = default;
    virtual ~sprite() = default;
    virtual void draw(); // vtable[0]

    void drawRegion(vec2d uv0, vec2d uv1);
    void strip(float w, float extra);
    void strip(float extra);
    vec2d getSize() const;
    void setupSimple(NeonVertex *out);
    void setupChecking();
    void setupPurge();

    vec2d pos;                                                 // 0x04
    float zOffset = 0;                                         // 0x14 (added to z by setupSimple)
    double z = 610.0;                                          // 0x18
    double angle = 0;                                          // 0x20 radians
    double width = 0, height = 0;                              // 0x28, 0x30
    double regionX = 0, regionY = 0, regionW = 0, regionH = 0; // 0x38..0x50
    int frame = 0;                   // 0x58 sprite-map index; <0 = effect output
    unsigned flags = 0;              // 0x5c SpriteFlags
    int tilesX = 1, tilesY = 1;      // 0x60, 0x64 subdivision (shatter)
    double gapX = 0, gapY = 0;       // 0x68, 0x70
    colour col;                      // 0x78
    uint32_t magic = 0xf1055baa;     // 0xc8
    float shatter = 0;               // 0xcc
    int literalOverride = -1;        // 0xd0 (frame -999)
    int batchCount = 0;              // 0xd4
    const double *corners = nullptr; // 0xd8 (SPR_CORNERS) x0,y0,...,x3,y3
    NeonVertex *batchPtr = nullptr;  // 0xdc
    NeonStack *batchStack = nullptr; // 0xe0
};

// The CDrawingObject's drawing state used by the helpers below.
struct NeonDrawContext {
    NeonBaseRender main;          // CDrawingObject + 0x350 (kind 0 vertices)
    NeonBaseRender aux;           // CDrawingObject + 0x8a4 (kind 1 vertices)
    NeonEffect *effect = nullptr; // effect shown by settex(-1) (+0x1464)
    int effectStack = 0;          // its stack index (+0x1468)
};

constexpr int kLocalVerts = 3600;
using RenderSession =
    neon::ModuleRenderContext<NeonVertex, kLocalVerts, NeonResManager, NeonDevice, NeonDrawContext>;
using ScopedRenderSession = neon::ScopedRenderBinding<RenderSession>;
inline RenderSession &renderSession() {
    return ScopedRenderSession::current();
}
void purge();
void settex(int literal);
void drawing_end();

// FillShaderConstants: animated effect-shader constants (see neon_sprite.cpp).
void FillShaderConstants(int shader, bool cycle, float f, int beat, int w, int h, float sx,
                         float sy, float *a, float *b, float *constants, bool *enabled);

} // namespace Neon_Internals
} // namespace NEON_GAME
