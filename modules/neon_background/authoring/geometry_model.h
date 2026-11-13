#pragma once
#include "pc_animation.h"
#include "pc_geometry.h"
#include <array>
#include <iosfwd>
namespace neon::pc {
struct GeometryStudy {
    bool enabled = false;
    int routing = 2, placement = 0, copies = 9;
    float size = .7f, spread = .82f, spin = .15f, opacity = .8f, glow = .4f;
    std::array<ShapeSettings, 3> shapes{};
    std::array<bool, 3> visible = {true, true, true};
    std::array<float, 3> layerOpacity = {1, 1, 1};
    std::array<std::array<int, 2>, 3> motion{};
    std::array<std::array<float, 2>, 3> rate = {{{1, 1}, {1, 1}, {1, 1}}};
    void reset(const std::array<ShapeSettings, 3> &originals);
    bool valid() const;
};
void writeGeometry(std::ostream &, const GeometryStudy &);
GeometryStudy readGeometry(std::istream &);
// Viewer placement is separate from the recovered generator. Result is clipped
// to the viewport before submission, with a hard 20k-triangle budget.
Mesh composeGeometry(const GeometryStudy &, const Clock &, int width, int height,
                     const std::array<float, 3> &fit, const std::array<Point, 3> &centers,
                     const std::array<std::array<uint32_t, 4>, 3> &palettes);
} // namespace neon::pc
