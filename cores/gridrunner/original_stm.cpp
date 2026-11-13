#include "original_stm.h"
namespace gridrunner {
void OriginalStm::draw(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
                       uint32_t gridColour) const {
    drawCells(spr, size, frame, gridColour, false);
}
void OriginalStm::drawOverlay(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
                              uint32_t gridColour) const {
    drawCells(spr, size, frame, gridColour, true);
}
void OriginalStm::drawCells(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
                            uint32_t gridColour, bool overlay) const {
    using namespace Neon_Internals;
    // Original: glyph page 0; atlas origin (0,320), stride 18, 28 glyphs/row,
    // inner origin (1,1), 16x16 ink. The screen includes its sentinel row/col.
    const double cw = size.x / cols, ch = size.y / rows;
    const auto flags = spr->flags;
    if (overlay)
        settex(0);
    spr->frame = 0;
    spr->width = cw;
    spr->height = ch;
    spr->angle = 0;
    for (int y = 0; y <= rows; ++y)
        for (int x = 0; x <= cols; ++x) {
            const unsigned value = unsigned(peek(x, y)), glyph = value & 255;
            if (!glyph)
                continue;
            const unsigned pal = (value >> 8) & 31;
            const uint32_t rgba = pal == 15  ? gridColour
                                  : pal < 16 ? data::acPalette[pal]
                                             : data::flashes[((pal - 16) & 7) * 16 + (frame & 15)];
            spr->col.unpackRGBA(rgba);
            spr->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
            const double origin = overlay ? 0 : -.5;
            spr->pos = {(origin + (x + 1.0) / cols) * size.x, (origin + (y + 1.0) / rows) * size.y};
            spr->frame = 0;
            // setupSimple's region fields are pixel coordinates within the frame.
            spr->regionX = 1 + (glyph % 28) * 18;
            spr->regionY = 321 + (glyph / 28) * 18;
            spr->regionW = 16;
            spr->regionH = 16;
            spr->flags |= SPR_REGION;
            if (overlay) {
                if (renderSession().count + 6 > kLocalVerts)
                    purge();
                spr->setupSimple(renderSession().cursor);
                renderSession().cursor += 6;
                renderSession().count += 6;
            } else
                spr->setupChecking();
        }
    spr->flags = flags;
    spr->corners = nullptr;
}
} // namespace gridrunner
