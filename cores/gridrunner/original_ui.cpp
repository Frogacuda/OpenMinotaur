#include "original_frontend.h"
#include <cmath>
#include <cctype>
#include <cstring>
namespace gridrunner {
using namespace Neon_Internals;
void drawUiSprite(sprite *spr, vec2d size, int index, double x, double y, double rx, double ry,
                  uint32_t rgba, double angle) {
    spr->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    spr->frame = index;
    spr->pos = {size.x * x, size.y * y};
    spr->width = size.x * rx * 2;
    spr->height = size.y * ry * 2;
    spr->col.unpackRGBA(rgba);
    spr->angle = angle;
    spr->setupChecking();
}
void OriginalFrontend::draw(sprite *spr, vec2d size) const {
    if (stage_ == 2) {
        // iOS title label and word entities; their original radii, aspect
        // multipliers and delayed accelerating entry (word::init/run).
        drawUiSprite(spr, size, 5, 0, .4, .1, .1 * 15 / 98);
        drawUiSprite(spr, size, 68, -.3, gridY_, .03 * 128 / 20, .03);
        drawUiSprite(spr, size, 69, .2, runnerY_, .03 * 192 / 20, .03);
    }
}
uint32_t menuColour(bool selected, int frame) {
    if (!selected)
        return 0xffffffff;
    const uint32_t brightness =
        uint32_t(255 * (.825 + .175 * std::sin(frame * 6.283185307179586 / 72)));
    return (brightness << 24) | (brightness << 16) | 0xff;
}
void drawMenuText(vec2d size, const char *text, double x, double y, double scale, uint32_t rgba) {
    const size_t length = std::strlen(text);
    const double cw = size.x / OriginalStm::cols * scale, ch = size.y / OriginalStm::rows * scale;
    sprite glyph;
    glyph.frame = 0;
    glyph.flags = SPR_CENTRE_X | SPR_CENTRE_Y | SPR_REGION;
    glyph.width = cw;
    glyph.height = ch;
    glyph.regionW = glyph.regionH = 16;
    glyph.col.unpackRGBA(rgba);
    settex(0);
    for (size_t i = 0; i < length; ++i) {
        const unsigned c = unsigned(std::toupper(static_cast<unsigned char>(text[i])));
        if (c == ' ')
            continue;
        const unsigned code = c + 0x56;
        glyph.pos = {(x + .5) * size.x + (double(i) - (double(length) - 1) * .5) * cw,
                     (y + .5) * size.y};
        glyph.regionX = 1 + (code % 28) * 18;
        glyph.regionY = 321 + (code / 28) * 18;
        if (renderSession().count + 6 > kLocalVerts)
            purge();
        glyph.setupSimple(renderSession().cursor);
        renderSession().cursor += 6;
        renderSession().count += 6;
    }
}
} // namespace gridrunner
