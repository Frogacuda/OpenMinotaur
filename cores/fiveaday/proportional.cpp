// Reuse of Minos's matching proportionalLine/QuickMsg/rawGlyph path. Five a
// Day's own font and colour tables are extracted separately, not assumed equal.
#include "neon/proportional_text.h"
#include "backend.h"
namespace fiveaday::Neon_Internals {
namespace {
#include "font_data.inc"
}
void proportionalLine::update() {
    neon::text::updateLine(*this, t, curScale, curSpacing, neon::text::HoldAlpha::Opaque,
                           [&] { text = nullptr; });
}

void cellGrid::QuickMsg(const char *s, vec2d p, vec2d sc, int hold, int row, int colourSeq,
                        float phaseRate, float charStep) {
    proportionalLine *l = lines[row];
    if (l)
        neon::text::quick(l, s, p, sc, hold, colourSeq, phaseRate, charStep, kColourSequences,
                          kColourSequenceLengths);
}

void cellGrid::clm() {
    neon::text::clearBorrowed(lines, rowsN);
}

void cellGrid::rawGlyph(sprite *s, int gx, int gy, int gw, int gh, uint32_t rgba) {
    neon::text::rawGlyph(*this, s, gx, gy, gw, gh, rgba);
}

void cellGrid::propLine(sprite *s, proportionalLine *l) {
    neon::text::drawLine(s, l, kPropFontLineX, kPropFontLineY, noProp,
                         [&](sprite *glyph, int x, int y, int w, int h, uint32_t rgba) {
                             rawGlyph(glyph, x, y, w, h, rgba);
                         });
}

} // namespace fiveaday::Neon_Internals
