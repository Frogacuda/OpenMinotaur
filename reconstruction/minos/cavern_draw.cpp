// cavern_draw.cpp - see cavern_draw.h. Multiply-adds are separate operations (vmla.f64 is
// not fused); float steps are kept where the original rounds to binary32.
#include "cavern_draw.h"
#include <cassert>

namespace minos::cavern {

uint32_t iclerp(uint32_t from, uint32_t to, float t) {
    // Both words are unpacked to doubles channel / 255.0 (R = high byte ... A = low byte).
    auto ch = [](uint32_t c, int shift) { return double((c >> shift) & 0xffu) / 255.0; };
    const double tt = double(t);
    auto mix = [&](int shift) {
        const double a = ch(from, shift), b = ch(to, shift);
        return int((a + (b - a) * tt) * 255.0); // truncation toward zero
    };
    const int r = mix(24), g = mix(16), b = mix(8), a = mix(0);
    return (uint32_t(r) << 24) | (uint32_t(g & 0xff) << 16) | (uint32_t(b & 0xff) << 8) |
           uint32_t(a & 0xff);
}

uint32_t colourFromSequence(float t, const uint32_t *seq) {
    // s = t * (count + 0.99) in double, stored as float; index = trunc(s); blend fraction in
    // float. The pair read is seq[1 + index], seq[2 + index].
    const float s = float(double(t) * (double(float(int32_t(seq[0]))) + 0.99));
    const int i = int(s);
    const float frac = s - float(i);
    return iclerp(seq[1 + i], seq[2 + i], frac);
}

namespace {
QuadColour paletteOrSequence(int colour, int paletteIndex, float sequenceT) {
    QuadColour c;
    if (colour <= 63) {
        c.paletteIndex = paletteIndex;
        return c;
    }
    c.fromSequence = true;
    c.rgba = colourFromSequence(sequenceT, kA8ColourSequence);
    return c;
}

UvRect atlasRect(const StripMap::TileAtlas &a, int atlasColumns, int tile) {
    const int row = tile / atlasColumns, col = tile % atlasColumns;
    UvRect r;
    r.u = double(a.atlasX + a.marginX + a.pitchX * col) / double(a.textureW);
    r.v = double(a.atlasY + a.marginY + a.pitchY * row) / double(a.textureH);
    r.du = double(a.tileW) / double(a.textureW);
    r.dv = double(a.tileH) / double(a.textureH);
    return r;
}
} // namespace

MapDrawPass drawMap(const StripMap &m, Vec2 screen) {
    MapDrawPass pass;
    const Vec2 cell{m.cellSize.x * m.scale.x, m.cellSize.y * m.scale.y};
    const Vec2 base{m.origin.x * m.scale.x, m.origin.y * m.scale.y};
    pass.size = {cell.x * screen.x, cell.y * screen.y};

    // Top-left cell centre: the grid is centred on `base`, shifted by the fractional scroll.
    double x0 = base.x;
    x0 = x0 + (cell.x * double(float(m.columns))) * -0.5;
    x0 = x0 + cell.x * 0.5;
    x0 = x0 + cell.x * m.fraction.x;
    double y = base.y;
    y = y + (cell.y * double(float(m.rows))) * -0.5;
    y = y + cell.y * 0.5;
    y = y + cell.y * m.fraction.y;

    for (int row = 0; row <= m.rows; row++) {
        double x = x0;
        for (int col = 0; col <= m.columns; col++) {
            const int32_t code = m.tiles[size_t(row * (m.columns + 1) + col)];
            const Vec2 pos{x * screen.x, y * screen.y};
            const int tile = tileOf(code), overlay = overlayTileOf(code), colour = colourOf(code);
            assert(colour < 128);
            if (tile != 0)
                pass.quads.push_back(
                    {pos, atlasRect(m.atlas, m.atlasColumns, tile),
                     paletteOrSequence(colour, colour, float(colour - 64) / 63.0f)});
            if (overlay != 0)
                pass.quads.push_back(
                    {pos, atlasRect(m.atlas, m.atlasColumns, overlay),
                     paletteOrSequence(colour, colour + 1, float(colour - 63) / 63.0f)});
            x = x + cell.x;
        }
        y = y + cell.y;
    }
    return pass;
}

int prepareVisibleLines(StripMap &m, Vec2 camera) {
    m.scroll = camera;
    const int cx = int(camera.x), cy = int(camera.y);
    m.fraction.x = -(camera.x - double(float(cx)));
    m.fraction.y = -(camera.y - double(float(cy)));
    int sector = cy / m.sectorLength();
    if (sector < 0)
        sector = 0;
    for (int i = 0; i <= m.rows; i++)
        m.getLine(cx, cy + i, m.columns * i + i);
    return sector;
}

} // namespace minos::cavern
