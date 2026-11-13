// cavern_draw.h - drawing the visible cave map: stm::draw (0x5750c) of Minos_armv7.
//
// theGame::_bgdraw fills the map's tile buffer with the visible lines (getLine for lines
// top..top+rows, buffer offset (columns+1)*i) and calls stm::draw(sprite, screen). The
// original then walks the (columns+1) x (rows+1) buffer and, for every cell, emits up to two
// quads through one scratch sprite with sprite::setupChecking: the cell's atlas tile and its
// overlay tile. It sets sprite flag 0x1000 for the duration, pointing the sprite's explicit-UV
// member (+0xdc) at a {u, v, du, dv} rectangle, and selects sprite-map frame 2.
//
// Per-cell decisions produce a list of quads. cavern_draw_emit.h submits them
// through the engine sprite path using the 0x1000 explicit-UV variant.
#pragma once
#include "cavern_map.h"
#include <cstdint>
#include <vector>

namespace minos::cavern {

// Tile code bytes as stm::draw reads them.
constexpr int tileOf(int32_t code) {
    return code & 0xff;
} // 0 = none
constexpr int overlayTileOf(int32_t code) {
    return (code >> 8) & 0xff;
} // 0 = none
constexpr int colourOf(int32_t code) {
    return (code >> 16) & 0xff;
}

// Where a quad's colour comes from. Colour bytes 0..63 select the map palette (stm+4,
// 80-byte colour entries; the base tile uses entry c, the overlay entry c + 1). Only 16
// entries exist before stm+0x504, so c > 14 (overlay) / c > 15 would read other map fields;
// shipped tile codes use 0, 2 and 3. Bytes >= 64 select an RGBA from the a8cols sequence.
struct QuadColour {
    bool fromSequence = false;
    int paletteIndex = 0; // when !fromSequence
    uint32_t rgba = 0;    // when fromSequence (the word passed to colour::unpackRGBA)
};

struct UvRect {
    double u = 0, v = 0, du = 0, dv = 0;
}; // the explicit-UV block, in order

struct TileQuad {
    Vec2 position; // sprite +0x04: cell cursor * screen
    UvRect uv;
    QuadColour colour;
};

struct MapDrawPass {
    static constexpr int kFrame = 2; // sprite +0x58 (sprite-map index) for every quad
    Vec2 size;                       // sprite +0x28: cellSize * scale * screen
    std::vector<TileQuad> quads;     // in emission order
};

// Neon_Internals::iclerp (0xf148): per-channel blend of two 0xRRGGBBAA words at t.
uint32_t iclerp(uint32_t from, uint32_t to, float t);
// Neon_Internals::getColFromSequence (0xf2dc) for a sequence {count, c0, c1, ...}.
uint32_t colourFromSequence(float t, const uint32_t *sequence);

// stm::draw (0x5750c). `screen` is draw's vec2d argument (multiplies sizes and positions).
// Preconditions (true for shipped data): colour bytes < 128.
MapDrawPass drawMap(const StripMap &map, Vec2 screen);

// The map part of theGame::_bgdraw (0x7d12c) in play state (theGame+0x8 == 2): the camera
// (theGame+0x320, in map cells) becomes the map's scroll; the fractional part, negated,
// becomes `fraction` (cell = truncation toward zero, so a negative camera gives a positive
// fraction); lines int(camera.y) .. +rows are generated into the tile buffer. Returns the
// value _bgdraw stores at theGame+0x207914: max(0, int(camera.y) / sectorLength).
// Not here (theGame state, kept with theGame when it is reconstructed): palette entries 1/2
// copied from theGame+0x5c8/+0x4d8, the +0x2dc background list's draw calls, clearing
// theGame+0x31e when the byte at theGame+0x203808 is 0, and drawing the map only while
// theGame+0x20380c < 200.
int prepareVisibleLines(StripMap &map, Vec2 camera);

} // namespace minos::cavern
