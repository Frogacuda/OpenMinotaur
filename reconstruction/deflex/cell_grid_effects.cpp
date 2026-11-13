// Deflex cell effects, using the cell store.
// Source: Deflex ARMv7 gridCell/cellGrid methods.
#include "cell_grid_addressing.h"
#include "neon/grid_cell_effects.h"
#include "neon/grid_geometry.h"
#include <algorithm>
#include <cmath>

namespace deflex {
void GridCell::setFSequence(float rate, const int *sequence) {
    fgT = 0;
    fgSpeed = rate;
    if (!fgSeq)
        fgFrom = col;
    fgSeq = sequence;
    fgState = 1;
}
void GridCell::setBSequence(float rate, const int *sequence) {
    bgT = 0;
    bgSpeed = rate;
    if (!bgSeq)
        bgFrom = bg;
    bgSeq = sequence;
    bgState = 1;
}
void GridCell::setBCC(float rate, uint32_t rgba) {
    bgSpeed = rate;
    bgT = 0;
    bgFrom.unpackRGBA(rgba); // flash colour (beginning of fade)
    if (!(fadeFlags & 1))
        bgTo = bg; // retain the original restore colour
    bgState = 3;
    fadeFlags |= 3;
}
void GridCell::blendBCC(float rate, uint32_t rgba, float amount) {
    neon::colour current, target;
    current.unpackRGBA(bg.packRGBA());
    target.unpackRGBA(rgba);
    // Original iclerp quantizes through packed RGBA before the fade begins.
    setBCC(rate, current.lerp(target, amount).packRGBA());
    bg = neon::clerp(bgFrom, bgTo, bgT);
}
void GridCell::dingDeflector() {
    scale = float(double(scale) - 0.2);
    fadeTo.purple(0.2);
    fgFrom.white(1.0);
    fadeTo = fadeTo.lerp(fgFrom, scale);
    fgT = 0;
    fgSpeed = 0.04f;
    if (double(scale) < 0.1) {
        ch = 0x20;
        fgOn = 0;
    }
    // The caller sets the foreground state; a ding does not change it.
}
void GridCell::smash(neon::DarwinRandom &random) {
    neon::grid::smashCell(*this, [&](double low, double high) { return random.range(low, high); });
}
void GridCell::run(const neon::colour *palette) {
    neon::grid::runCellEffects(*this, palette);
}
void CellGrid::blendRadius(float radius, vec2d centre, float rate, uint32_t rgba, float amount) {
    if (radiusBlendDisabled)
        return;
    const float absoluteRadius = std::fabs(radius);
    const double r = absoluteRadius;
    const int left = std::max(0, int(((centre.x - r) + 0.5) * double(cols)));
    const int top = std::max(0, int(((centre.y - r) + 0.5) * double(rows)));
    const int right = std::min(cols, int(double(cols) * ((r + centre.x) + 0.5)));
    const int bottom = std::min(rows, int(double(rows) * ((centre.y + r) + 0.5)));
    const float pixelRadius = absoluteRadius * 512.0f;
    const vec2d origin = centre * 512.0;
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            const vec2d delta = origin - getPos(x, y) * 512.0;
            const float fraction = float(delta.length() / double(pixelRadius));
            if (fraction < 1.0f)
                cell(y, x).blendBCC(rate, rgba, (1.0f - fraction) * amount);
        }
}
void CellGrid::fadeLines(int first, int last) {
    for (int row = first; row <= last; ++row)
        if (messageLines[size_t(row)].count)
            messageLines[size_t(row)].state = 2;
}
void CellGrid::doDisplace(neon::vec3d &position) const {
    neon::grid::applyDisplacements(position, displacement,
                                   [](const auto &source) { return source.centre; });
}
} // namespace deflex
