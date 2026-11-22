#pragma once
#include "pc_geometry.h"
#include <algorithm>
#include <cmath>
namespace neon::effects {
// Symmetric bounds in authored coordinates. A value of one covers the reference
// frame; larger values request more scene content without changing its scale.
struct LayerCoverage {
    float x = 1, y = 1;
    void include(LayerCoverage other) {
        x = std::max(x, other.x);
        y = std::max(y, other.y);
    }
};
// Barrel inputs must also cover axis rotations between the cardinal directions.
inline LayerCoverage barrelCoverage(LayerCoverage output, const pc::ShapeSettings &terminal,
                                    bool convex) {
    const float strength = std::clamp(terminal.floats[4], 0.f, 1.f);
    if (strength == 0)
        return output;
    const float angle = terminal.floats[5] * 1.5707963267948966f, ax = std::cos(angle),
                ay = std::sin(angle), sx = std::exp2(std::clamp(terminal.floats[9], -4.f, 4.f)),
                sy = std::exp2(std::clamp(terminal.floats[10], -4.f, 4.f));
    const float cx = terminal.floats[6], cy = terminal.floats[7];
    float maxAlong = 0;
    for (float x : {-.5f * output.x, .5f * output.x})
        for (float y : {-.5f * output.y, .5f * output.y})
            maxAlong = std::max(
                maxAlong, std::abs((.5f + x - cx) * 2 / sx * ax + (.5f + y - cy) * 2 / sy * ay));
    const float factor = convex ? 1 + 4 * strength * maxAlong * maxAlong : 0;
    LayerCoverage result = output;
    for (float x : {-.5f * output.x, .5f * output.x})
        for (float y : {-.5f * output.y, .5f * output.y}) {
            const float px = (.5f + x - cx) * 2 / sx, py = (.5f + y - cy) * 2 / sy,
                        along = px * ax + py * ay, across = -px * ay + py * ax;
            result.x = std::max(
                result.x, 2 * std::abs(cx + .5f * (ax * along - ay * across * factor) * sx - .5f));
            result.y = std::max(
                result.y, 2 * std::abs(cy + .5f * (ay * along + ax * across * factor) * sy - .5f));
        }
    result.x = std::min(result.x, 64.f);
    result.y = std::min(result.y, 64.f);
    return result;
}
inline LayerCoverage transformedCoverage(LayerCoverage output, const pc::ShapeSettings &n,
                                         int input) {
    const int base = input == 1 ? 5 : 0;
    const float sx = std::max(.0001f, n.floats[base] + 1),
                sy = std::max(.0001f, n.floats[base + 1] + 1), c = std::cos(n.floats[base + 4]),
                s = std::sin(n.floats[base + 4]);
    LayerCoverage result;
    for (float x : {-output.x * .5f, output.x * .5f})
        for (float y : {-output.y * .5f, output.y * .5f}) {
            const float px = x + n.floats[base + 2] / sx, py = y + n.floats[base + 3] / sy;
            result.x = std::max(result.x, 2 * std::abs((px * c - py * s) * sx));
            result.y = std::max(result.y, 2 * std::abs((px * s + py * c) * sy));
        }
    // Bound pathological authoring/feedback transforms, not normal viewport sizes.
    result.x = std::min(result.x, 64.f);
    result.y = std::min(result.y, 64.f);
    return result;
}
} // namespace neon::effects
