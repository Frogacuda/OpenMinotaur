#include "stage_camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace goatup2 {
void StageCamera::measure(const TerrainLayout &layout, TerrainGridView grid, int vw, int vh) {
    // 0x492c8: extents are occupied cell indices, not an exclusive rectangle.
    // Empty templates retain the original full-grid extents. The source reverse
    // scans exclude index zero (already covered by the forward scan).
    if (vw <= 0 || vh <= 0 || layout.columns <= 0 || layout.rows <= 0 || layout.columns > 100 ||
        layout.rows > 100 || grid.working.size() != 10000)
        throw std::invalid_argument("unsupported camera template");
    int left = 0, right = layout.columns, top = 0, bottom = layout.rows;
    auto occupied = [&](int x, int y) { return grid.working.at(size_t(y) * 100 + size_t(x)) != 0; };
    bool found = false;
    for (int x = 0; x < layout.columns && !found; ++x)
        for (int y = 0; y < layout.rows; ++y)
            if (occupied(x, y)) {
                left = x;
                found = true;
                break;
            }
    for (int x = layout.columns - 1; x > 0; --x) {
        bool hit = false;
        for (int y = 0; y < layout.rows; ++y)
            if (occupied(x, y)) {
                hit = true;
                break;
            }
        if (hit) {
            right = x;
            break;
        }
    }
    found = false;
    for (int y = 0; y < layout.rows && !found; ++y)
        for (int x = 0; x < layout.columns; ++x)
            if (occupied(x, y)) {
                top = y;
                found = true;
                break;
            }
    for (int y = layout.rows - 1; y > 0; --y) {
        bool hit = false;
        for (int x = 0; x < layout.columns; ++x)
            if (occupied(x, y)) {
                hit = true;
                break;
            }
        if (hit) {
            bottom = y;
            break;
        }
    }
    if (right - left - vw < 0) {
        const int pad = (right - left - vw) / 2;
        right -= pad;
        left += pad;
    }
    if (bottom - top - vh < 0) {
        const int pad = (bottom - top - vh) / 2;
        bottom -= pad;
        top += pad;
    }
    left = std::max(0, left);
    top = std::max(0, top);
    right = std::min(layout.columns, right);
    bottom = std::min(layout.rows, bottom);
    upper = {double(float(3 - left)), double(float(4 - top))};
    lower = {double(float(vw - right - 3)), double(float(vh - bottom - 6))};
}
void StageCamera::request(neon::vec2d p, const TerrainLayout &layout, TerrainBounds bounds,
                          double facing, double vy) {
    // 0x50528. Float stores in the normalization and lead precede widening.
    backgroundRequest = p * .5;
    float x = std::max(0.f, float(p.x + double(bounds.halfWidth) - .5)) /
              (bounds.halfWidth + bounds.halfWidth);
    float y = std::max(0.f, float(p.y + double(bounds.halfHeight) - .5)) /
              (bounds.halfHeight + bounds.halfHeight);
    x = x + (facing <= 0 ? .02f : -.02f);
    if (std::fabs(vy) > .001)
        y = float(double(y) + vy * 6.6);
    requested = {double(-(x * float(layout.columns))), double(-(y * float(layout.rows)))};
    // Keep ordered comparisons: templates can produce crossing clamp bounds.
    if (upper.x < requested.x)
        requested.x = upper.x;
    if (requested.x < lower.x)
        requested.x = lower.x;
    if (upper.y < requested.y)
        requested.y = upper.y;
    if (requested.y < lower.y)
        requested.y = lower.y;
}
void StageCamera::finishTerrain(neon::vec2d &offset, neon::vec2d velocity, double impulseX,
                                double impulseY, bool editing, bool attract) {
    // Tail of 0x41d20. Contrary to a convenient render-only camera, this is a
    // once-per-logical-frame mutation AFTER all current actor/tile geometry.
    float rate = editing ? 1.f : .04f;
    if (fastFrames > 0) {
        rate = .3f;
        --fastFrames;
    }
    if (attract)
        rate = .001f;
    const double speed = double(float((velocity + neon::vec2d{impulseX, impulseY}).length()));
    const double limited = speed > .1 ? double(.1f) : speed;
    rate = float(double(rate) + double(1.f - rate) * (limited / .1));
    offset.x = offset.x + double(rate) * (requested.x - offset.x);
    offset.y = offset.y + double(rate) * (requested.y - offset.y);
}
} // namespace goatup2
