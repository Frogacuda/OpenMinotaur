#include "native_view.h"
namespace gridrunner {
using namespace Neon_Internals;
namespace {
void colourFromNative(sprite &spr, const gr::reconstruction::ios::Rgba &tint) {
    spr.col.r = tint.r;
    spr.col.g = tint.g;
    spr.col.b = tint.b;
    spr.col.a = tint.a;
    for (size_t i = 0; i < 3; ++i) {
        spr.col.phase[i] = tint.phase[i];
        spr.col.speed[i] = tint.speed[i];
        spr.col.amp[i] = tint.amplitude[i];
        spr.col.base[i] = tint.base[i];
    }
}
} // namespace
void drawNativeGrid(const gr::reconstruction::ios::DrawFrame &frame, sprite *spr, vec2d size) {
    // Redraws only submit prepared values. Counter side effects occur once per
    // logical frame, including each original Entity::draw call in a trail.
    for (const auto &draw : frame.sprites()) {
        spr->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
        spr->frame = draw.frame;
        spr->angle = double(draw.angle);
        spr->zOffset = draw.depth;
        spr->pos = {draw.position.x * size.x, draw.position.y * size.y};
        spr->width = draw.scale.x * size.x * double(draw.diameter);
        spr->height = draw.scale.y * size.y * double(draw.diameter);
        if (draw.beamRow >= 0) {
            spr->pos = {size.x * draw.position.x,
                        size.y * -.35 +
                            double(draw.beamRow) * double(float(size.y * .027333333333333334))};
            spr->width = size.x * double(draw.beamRadius);
            spr->height = double(float(size.y * .027333333333333334));
            if (draw.charging)
                spr->width =
                    double(1.f - draw.chargeFraction) * size.x * double(draw.beamRadius) * 2.3;
        }
        colourFromNative(*spr, draw.tint);
        spr->setupChecking();
    }
}
void drawNativeOverlay(const gr::reconstruction::ios::DrawFrame &frame, sprite *spr, vec2d size) {
    // Score menus draw after the effect stack closes. Use the final-surface
    // batch and pixel origin, not showJoy's temporary stack vertex buffer.
    for (const auto &draw : frame.sprites()) {
        spr->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
        spr->frame = draw.frame;
        spr->angle = double(draw.angle);
        spr->zOffset = draw.depth;
        spr->pos = {(draw.position.x + .5) * size.x, (draw.position.y + .5) * size.y};
        spr->width = draw.scale.x * size.x * double(draw.diameter);
        spr->height = draw.scale.y * size.y * double(draw.diameter);
        colourFromNative(*spr, draw.tint);
        spr->draw();
    }
}
} // namespace gridrunner
