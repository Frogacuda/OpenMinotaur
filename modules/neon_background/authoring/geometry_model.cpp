#include "geometry_model.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace neon::pc {
namespace {
constexpr float tau = 6.2831855f;
bool supported(int kind) {
    return kind == 1 || kind == 2 || kind == 5 || kind == 7 || kind == 31;
}
float cycle(float p) {
    return p - std::floor(p);
}
uint32_t mixColour(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8)
        out |= (uint32_t(std::lround(float((a >> s) & 255) +
                                     (float((b >> s) & 255) - float((a >> s) & 255)) * t)) &
                255)
               << s;
    return out;
}
ShapeVertex lerp(ShapeVertex a, ShapeVertex b, float t) {
    return {{a.position.x + (b.position.x - a.position.x) * t,
             a.position.y + (b.position.y - a.position.y) * t, 0},
            mixColour(a.colour, b.colour, t)};
}
void clippedTriangle(Mesh &destination, const ShapeVertex *triangle, int width, int height) {
    std::vector<ShapeVertex> polygon(triangle, triangle + 3), next;
    for (int edge = 0; edge < 4 && !polygon.empty(); ++edge) {
        auto distance = [&](const ShapeVertex &v) {
            switch (edge) {
            case 0:
                return v.position.x;
            case 1:
                return float(width) - v.position.x;
            case 2:
                return v.position.y;
            default:
                return float(height) - v.position.y;
            }
        };
        next.clear();
        auto a = polygon.back();
        float da = distance(a);
        for (auto b : polygon) {
            float db = distance(b);
            if ((da >= 0) != (db >= 0))
                next.push_back(lerp(a, b, da / (da - db)));
            if (db >= 0)
                next.push_back(b);
            a = b;
            da = db;
        }
        polygon.swap(next);
    }
    for (size_t i = 1; i + 1 < polygon.size() && destination.size() + 3 <= 60000; i++) {
        destination.push_back(polygon[0]);
        destination.push_back(polygon[i]);
        destination.push_back(polygon[i + 1]);
    }
}
} // namespace
void GeometryStudy::reset(const std::array<ShapeSettings, 3> &originals) {
    *this = GeometryStudy{};
    shapes = originals;
    motion[2][0] = 1;
}
void writeGeometry(std::ostream &out, const GeometryStudy &s) {
    out << "\nGEOMETRY 1\n"
        << s.enabled << ' ' << s.routing << ' ' << s.placement << ' ' << s.copies << ' ' << s.size
        << ' ' << s.spread << ' ' << s.spin << ' ' << s.opacity << ' ' << s.glow << '\n';
    for (int l = 0; l < 3; l++) {
        out << s.visible[l] << ' ' << s.layerOpacity[l] << ' ';
        for (int c = 0; c < 2; c++)
            out << s.motion[l][c] << ' ' << s.rate[l][c] << ' ';
        for (auto v : s.shapes[l].integers)
            out << v << ' ';
        for (auto v : s.shapes[l].floats)
            out << v << ' ';
        out << '\n';
    }
}
GeometryStudy readGeometry(std::istream &in) {
    GeometryStudy s;
    std::string tag;
    int version;
    in >> tag >> version;
    if (tag != "GEOMETRY" || version != 1)
        throw std::runtime_error("Invalid geometry header");
    in >> s.enabled >> s.routing >> s.placement >> s.copies >> s.size >> s.spread >> s.spin >>
        s.opacity >> s.glow;
    for (int l = 0; l < 3; l++) {
        in >> s.visible[l] >> s.layerOpacity[l];
        for (int c = 0; c < 2; c++)
            in >> s.motion[l][c] >> s.rate[l][c];
        for (auto &v : s.shapes[l].integers)
            in >> v;
        for (auto &v : s.shapes[l].floats)
            in >> v;
    }
    if (!in || !s.valid())
        throw std::runtime_error("Invalid geometry parameters");
    return s;
}
bool GeometryStudy::valid() const {
    if (routing < 0 || routing > 2 || placement < 0 || placement > 2 || copies < 1 || copies > 12)
        return false;
    if (!std::isfinite(size) || size < .1f || size > 2 || !std::isfinite(spread) || spread < 0 ||
        spread > 1 || !std::isfinite(spin) || spin < -2 || spin > 2 || !std::isfinite(opacity) ||
        opacity < 0 || opacity > 1 || !std::isfinite(glow) || glow < 0 || glow > 1)
        return false;
    for (int layer = 0; layer < 3; layer++) {
        const auto &s = shapes[layer];
        if (!supported(s.integers[13] & 31) || !supported((s.integers[13] >> 5) & 31))
            return false;
        if (!std::isfinite(layerOpacity[layer]) || layerOpacity[layer] < 0 ||
            layerOpacity[layer] > 1 || s.integers[12] < -255 || s.integers[12] > 255)
            return false;
        for (int i = 8; i < 12; i++)
            if (s.integers[i] < 0 || s.integers[i] > 255)
                return false;
        for (int c = 0; c < 2; c++) {
            if (s.integers[2 * c] < 0 || s.integers[2 * c] > 23 || s.integers[2 * c + 1] < 0 ||
                s.integers[2 * c + 1] > 11 || motion[layer][c] < 0 || motion[layer][c] > 3 ||
                !std::isfinite(rate[layer][c]) || rate[layer][c] < .1f || rate[layer][c] > 4)
                return false;
        }
        for (float f : s.floats)
            if (!std::isfinite(f) || std::abs(f) > 8)
                return false;
    }
    return true;
}
Mesh composeGeometry(const GeometryStudy &study, const Clock &clock, int width, int height,
                     const std::array<float, 3> &fit, const std::array<Point, 3> &centers,
                     const std::array<std::array<uint32_t, 4>, 3> &palettes) {
    Mesh output;
    if (!study.enabled)
        return output;
    if (!study.valid())
        throw std::runtime_error("Invalid geometry study");
    std::array<Mesh, 3> meshes;
    std::vector<int> active;
    for (int layer = 0; layer < 3; layer++)
        if (study.visible[layer]) {
            auto settings = study.shapes[layer];
            for (int c = 0; c < 2; c++) {
                float p = cycle(clock.phase * study.rate[layer][c]);
                switch (study.motion[layer][c]) {
                case 1:
                    settings.floats[c * 2] = p;
                    break;
                case 2:
                    settings.floats[c * 2] = (std::sin(p * tau) + 1) * .5f;
                    break;
                case 3:
                    settings.floats[c * 2] = 1 - std::abs(p * 2 - 1);
                    break;
                default:
                    break;
                }
            }
            auto colours = palettes[layer];
            for (int i = 0; i < 4; i++)
                colours[i] = (colours[i] & 0x00ffffff) | (uint32_t(settings.integers[8 + i]) << 24);
            meshes[layer] = generateShapes(settings, colours);
            if (!meshes[layer].empty())
                active.push_back(layer);
        }
    if (active.empty())
        return output;
    for (int instance = 0; instance < study.copies && output.size() < 60000; instance++) {
        const int layer = active[instance % active.size()];
        float angle = tau * (float(instance) / study.copies + clock.phase * study.spin);
        float cx = width * .5f, cy = height * .5f;
        if (study.placement == 0) {
            cx += std::cos(angle) * width * .41f * study.spread;
            cy += std::sin(angle) * height * .38f * study.spread;
        } else if (study.placement == 1) {
            cx = width * (.5f + (float(instance) / std::max(1, study.copies - 1) - .5f) *
                                    study.spread * .9f);
            cy += std::sin(angle) * height * .12f * study.spread;
        }
        const float scale = height * .20f * study.size * fit[layer];
        const float rotation = study.placement == 2 ? angle : clock.phase * tau * study.spin;
        const float co = std::cos(rotation), si = std::sin(rotation);
        auto mesh = meshes[layer];
        for (auto &v : mesh) {
            float x = (v.position.x - centers[layer].x) * scale,
                  y = (v.position.y - centers[layer].y) * scale;
            v.position = {cx + x * co - y * si, cy + x * si + y * co, 0};
            v.colour = (v.colour & 0x00ffffff) |
                       uint32_t(study.opacity * study.layerOpacity[layer] * float(v.colour >> 24))
                           << 24;
        }
        for (size_t i = 0; i + 2 < mesh.size() && output.size() < 60000; i += 3)
            clippedTriangle(output, mesh.data() + i, width, height);
    }
    return output;
}

} // namespace neon::pc
