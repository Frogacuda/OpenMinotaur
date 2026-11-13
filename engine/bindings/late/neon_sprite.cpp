// neon_sprite.cpp - sprite drawing and the global batch helpers.
#include "neon_sprite.h"
#include "neon_resources.h"
#include "neon_synth.h"

#include "neon/sprite_draw.h"
#include "neon/immediate_texture.h"

namespace NEON_GAME {
namespace Neon_Internals {

// ------------------------------------------------------------- batching --

void purge() {
    neon::sprites::purge(renderSession().cursor, renderSession().base, renderSession().count,
                         renderSession().sourceTexture,
                         renderSession().context ? &renderSession().context->main : nullptr);
}

// Flushes the batch and selects the texture for what follows: a literal,
// or (literal < 0) the output of the effect being displayed.
void settex(int literal) {
    neon::sprites::selectImmediateTexture(
        renderSession(), literal, neon::sprites::LiteralSelectionRule::AlwaysSelect, purge,
        [](auto &renderer, auto source) { neon::sprites::bindPrimarySources(renderer, source); });
}

void drawing_end() {
    if (renderSession().device)
        renderSession().device->PresentFrame();
}

// --------------------------------------------------------------- sprite --

vec2d sprite::getSize() const {
    if (frame >= 0 && renderSession().resources &&
        frame <= renderSession().resources->spriteCount() - 1) {
        const SpriteEntry *e = renderSession().resources->sprite(frame);
        return {double(e->w), double(e->h)};
    }
    return {100.0, 100.0};
}

// Draws into the global batch, optionally split into tilesX x tilesY pieces
// that are scattered by `shatter`.
void sprite::draw() {
    neon::sprites::draw(
        *this, renderSession().resources, renderSession().cursor, renderSession().count,
        [](double lo, double hi) { return oxRNG::rand(lo, hi); }, settex, purge);
}

void sprite::drawRegion(vec2d uv0, vec2d uv1) {
    neon::sprites::drawRegion(*this, renderSession().resources, renderSession().cursor,
                              renderSession().count, uv0, uv1, settex, purge);
}

// A vertical bar made of three sprite pieces (top cap, body, bottom cap).
void sprite::strip(float w, float extra) {
    sprite save = *this;
    double y = (flags & SPR_CENTRE_Y) ? pos.y + height * -0.5 : pos.y;
    if (flags & SPR_CENTRE_Y)
        pos.y = y;
    double h = height;
    flags = 0;
    pos.x = 0;
    width = double(w);
    frame = 0x12;
    height = 5.0;
    pos.y = y - double(extra);
    draw();
    frame = 0x16;
    pos.y += 3.0;
    double body = (double(extra + extra) + h) - 10.0;
    height = body;
    draw();
    frame = 0x13;
    height = 5.0;
    pos.y += body;
    draw();
    flags = save.flags;
    width = save.width;
    height = save.height;
    pos = save.pos;
}

void sprite::strip(float extra) {
    sprite save = *this;
    double y = (flags & SPR_CENTRE_Y) ? pos.y + height * -0.5 : pos.y;
    if (flags & SPR_CENTRE_Y)
        pos.y = y;
    double h = height;
    flags = 0;
    frame = 0x12;
    height = 5.0;
    pos.y = y - double(extra);
    draw();
    frame = 0x16;
    pos.y += 3.0;
    double body = (double(extra + extra) + h) - 10.0;
    height = body;
    draw();
    frame = 0x13;
    height = 5.0;
    pos.y += body;
    draw();
    flags = save.flags;
    width = save.width;
    height = save.height;
    pos = save.pos;
}

// Writes this sprite's quad (6 vertices) to `out`.
void sprite::setupSimple(NeonVertex *out) {
    neon::sprites::setupSimple(*this, renderSession().resources, out,
                               neon::SpriteHeightRule::AddSevenHundredths);
}

// Appends this sprite to its stack's vertex store, flushing when nearly full.
void sprite::setupChecking() {
    neon::sprites::setupChecking(*this, NeonStack::kSpriteFlushAfter);
}

void sprite::setupPurge() {
    neon::sprites::setupPurge(*this, neon::sprites::StackPurgeRule::ResetCount);
}

// ----------------------------------------------------- shader constants --

void FillShaderConstants(int, bool cycle, float, int beat, int, int, float, float, float *, float *,
                         float *c, bool *enabled) {
    renderSession().shaderCycles.fill(cycle, beat, c, enabled);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
