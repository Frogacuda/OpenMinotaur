#include "pc_shapes.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace neon::pc {
namespace {
constexpr float tau = 6.2831855f, pi = 3.1415927f;
Point add(Point a, Point b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Point ring(float a, float r) {
    return {-std::sin(a) * r, std::cos(a) * r, 0};
}
Point rotate(Point p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
}
uint32_t blend(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int k = 0; k < 32; k += 8) {
        double x = double((a >> k) & 255), y = double((b >> k) & 255);
        out |= (uint32_t(x + (y - x) * double(t)) & 255) << k;
    }
    return out;
}
void triangle(Mesh &out, Point a, Point b, Point c, std::array<uint32_t, 3> colours, float scale,
              bool mirror) {
    if (out.size() + size_t(mirror ? 6 : 3) > ShapeVertexLimit)
        throw std::runtime_error("Generated shape vertex budget exceeded");
    for (int pass = 0; pass < (mirror ? 2 : 1); ++pass) {
        int k = 0;
        for (auto p : {a, b, c}) {
            p.x *= pass ? -scale : scale;
            p.y *= scale;
            p.z *= scale;
            out.push_back({p, colours[k++]});
        }
    }
}
void quadFan(Mesh &out, std::array<Point, 4> q, uint32_t edge, uint32_t centre, bool mirror) {
    Point mid = {(q[0].x + q[2].x) * .5f, (q[0].y + q[2].y) * .5f, 0};
    for (int k = 0; k < 4; ++k)
        triangle(out, q[k], q[(k + 1) % 4], mid, {edge, edge, centre}, 1, mirror);
}
} // namespace
bool recoveredShapeKind(int k) {
    return k == 0 || k == 6 || k == 1 || k == 2 || k == 3 || k == 4 || k == 5 || k == 7 || k == 8 ||
           k == 9 || k == 30;
}
const char *shapeKindName(int k) {
    switch (k) {
    case 0:
        return "Soft spokes";
    case 6:
        return "Threshold disc";
    case 1:
        return "Seeded clusters";
    case 2:
        return "Ripple disc";
    case 3:
        return "Gradient rectangle";
    case 4:
        return "Elliptic ribbon";
    case 5:
        return "Ship / arrow";
    case 7:
        return "Capped half-disc";
    case 8:
        return "Recursive triangles";
    case 9:
        return "Sine ribbon";
    case 30:
        return "Empty";
    default:
        return "Unrecovered";
    }
}
Mesh generatedShapeMesh(const ShapeSettings &s, const std::array<uint32_t, 256> &palette,
                        float gateDrive) {
    if (!std::isfinite(gateDrive))
        throw std::runtime_error("Nonfinite shape gate drive");
    for (float f : s.floats)
        if (!std::isfinite(f))
            throw std::runtime_error("Nonfinite generated shape parameter");
    const auto &f = s.floats;
    const auto &i = s.integers;
    Mesh out;
    std::array<uint32_t, 4> col{};
    for (int k = 0; k < 4; k++) {
        if (i[k + 4] < 0 || i[k + 4] > 255 || i[k + 8] < 0 || i[k + 8] > 255)
            throw std::runtime_error("Shape palette/alpha outside original bounds");
        col[k] = (palette[i[k + 4]] & 0xffffff00u) | uint32_t(i[k + 8]);
    }
    Point trailing{}; // Shared fourth corner survives between the two original families.
    for (int component = 0; component < 2; component++) {
        int kind = (unsigned(i[13]) >> (component * 5)) & 31;
        if (kind == 16 || kind >= 19)
            continue;
        if (!recoveredShapeKind(kind))
            throw std::runtime_error("Unrecovered generated shape family");
        int a = component * 2, order = i[a] + 1, secondary = i[a + 1] + 1;
        if (order < 1 || order > 64 || secondary < 1 || secondary > 64)
            throw std::runtime_error("Shape order outside supported bounds");
        float phase = f[a] * tau, wave = f[a + 1] * tau, radius = f[4 + a] * 400,
              ripple = f[5 + a] * 400;
        float sx = f[8 + a], sy = f[component ? 13 : 9];
        Point offset =
            component ? Point{f[14] * 400, f[15] * 400, 0} : Point{f[11] * 400, f[12] * 400, 0};
        bool mirror = (i[13] & (0x800 << component)) != 0;
        auto colours = std::array<uint32_t, 3>{col[a], col[a], col[a + 1]};
        if (kind == 0) {
            Point input = {offset.x, offset.y + radius, 0};
            float inc = wave / order, t = 0;
            Point previous = rotate(input, phase);
            input.x *= sx;
            input.y *= sy;
            phase += inc;
            Point next = rotate(input, phase);
            input.x *= sx;
            input.y *= sy;
            for (int n = 0; n < order; n++) {
                Point centre = {previous.x * .5f + next.x * .25f + offset.x * .25f,
                                previous.y * .5f + next.y * .25f + offset.y * .25f, 0};
                uint32_t c = blend(col[a], col[a + 1], t);
                t += 1.f / order;
                for (auto edge :
                     {std::array<Point, 2>{previous, next}, std::array<Point, 2>{next, offset},
                      std::array<Point, 2>{offset, previous}})
                    triangle(out, centre, edge[0], edge[1], {c, 0, 0}, f[5 + a], mirror);
                phase += inc;
                previous = next;
                next = rotate(input, phase);
                input.x *= sx;
                input.y *= sy;
            }
        } else if (kind == 2 || kind == 7 || kind == 6) {
            int count = order * 4;
            float arc = kind == 7 ? pi : tau, inc = arc / count, wi = arc * secondary / count;
            Point p = add(ring(phase, std::sin(wave) * ripple + radius), offset), first = p;
            auto sampleRadius = [&](float w) {
                float v = std::sin(w);
                if (kind == 6) {
                    if (v > sx) {
                        v = sy * gateDrive;
                        colours = {col[2], col[2], col[3]};
                    } else
                        colours = {col[0], col[0], col[1]};
                }
                return v * ripple + radius;
            };
            phase += inc;
            Point q = add(ring(phase, sampleRadius(wave + wi)), offset);
            wave += 2 * wi;
            float scale = kind == 7 ? (float(i[12]) * (1.f / 255.f) - .5f) * 2 : 1;
            for (int n = 0; n < count; n++) {
                triangle(out, p, q, offset, colours, scale, mirror);
                if (kind != 7 || n < count - 1) {
                    p = q;
                    phase += inc;
                    q = add(ring(phase, sampleRadius(wave)), offset);
                    wave += wi;
                }
            }
            if (kind == 7) {
                Point cap = add(ring(phase + 1.5707964f, ripple + radius), offset),
                      middle = {(first.x + q.x) * .5f, (first.y + q.y) * .5f, 0};
                colours[0] = col[2];
                triangle(out, cap, first, middle, colours, scale, mirror);
                triangle(out, cap, q, middle, colours, scale, mirror);
            }
        } else if (kind == 3) {
            quadFan(out,
                    {{{offset.x - radius, offset.y - ripple, 0},
                      {offset.x - radius, offset.y + ripple, 0},
                      {offset.x + radius, offset.y + ripple, 0},
                      {offset.x + radius, offset.y - ripple, 0}}},
                    col[0], col[1], mirror);
            trailing = {offset.x + radius, offset.y - ripple, 0};
        } else if (kind == 4) {
            float inc = wave * 3 / order, t = 0;
            Point v{0, radius, 0}, w{0, ripple, 0};
            Point previousV = add(rotate(v, phase), offset),
                  previousW = add(rotate(w, phase), offset);
            // The first rotation precedes the local-axis scaling in the original.
            phase += inc;
            v.x *= sx;
            v.y *= sx;
            w.x *= sx;
            w.y *= sy;
            for (int n = 0; n < order; n++) {
                Point nextV = add(rotate(v, phase), offset), nextW = add(rotate(w, phase), offset);
                quadFan(out, {previousV, previousW, nextW, nextV}, blend(col[0], col[1], t),
                        blend(col[2], col[3], t), mirror);
                t += 1.f / order;
                previousV = nextV;
                previousW = nextW;
                phase += inc;
                v.x *= sx;
                v.y *= sx;
                w.x *= sx;
                w.y *= sy;
            }
            trailing = previousV;
        } else if (kind == 5) {
            Point left{offset.x - ripple, offset.y + radius * .3f, 0},
                top{offset.x, offset.y - radius, 0}, right{offset.x + ripple, left.y, 0};
            triangle(out, left, top, offset, {col[0], col[1], col[2]}, 1, mirror);
            triangle(out, right, top, offset, {col[0], col[1], col[2]}, 1, mirror);
            col[1] = col[0];
            triangle(out, left, right, offset, {col[0], col[1], col[2]}, 1, mirror);
        } else if (kind == 1) {
            uint32_t rng = 0xf1055baa;
            auto random = [&](float extent) {
                rng = rng * 214013u + 2531011u;
                return float((rng >> 16) & 32767) * (1.f / 32768.f) * extent;
            };
            float currentRadius = sx * 50, inc = wave / order, t = 0;
            for (int n = 0; n < secondary; n++) {
                currentRadius *= sy;
                Point centre{random(radius) - radius * .5f + offset.x,
                             random(ripple) - ripple * .5f + offset.y, 0};
                colours = {blend(col[0], col[1], t), blend(col[0], col[1], t), col[2]};
                t += 1.f / secondary;
                Point previous{std::sin(phase) * currentRadius + centre.x,
                               std::cos(phase) * currentRadius + centre.y, 0};
                offset.x *= sy;
                offset.y *= sy;
                phase += inc;
                for (int k = 0; k < order; k++) {
                    Point next{std::sin(phase) * currentRadius + centre.x,
                               std::cos(phase) * currentRadius + centre.y, 0};
                    phase += inc;
                    triangle(out, previous, next, centre, colours, 1, mirror);
                    previous = next;
                }
            }
        } else if (kind == 8) {
            std::array<Point, 4> q = {Point{-ripple, radius * .25f, 0}, Point{0, -radius, 0},
                                      Point{ripple, radius * .25f, 0}, trailing};
            int count = i[component] + 1;
            float angle = phase;
            for (int n = 0; n < count; n++) {
                triangle(out, q[0], q[1], q[3], {col[1], col[0], col[2]}, 1, mirror);
                triangle(out, q[2], q[1], q[3], {col[1], col[0], col[2]}, 1, mirror);
                triangle(out, q[2], q[0], q[3], {col[1], col[1], col[2]}, 1, mirror);
                for (auto &p : q) {
                    p.x *= sx;
                    p.y *= sy;
                    p = add(rotate(p, angle), offset);
                }
                angle += wave * .1f;
            }
            trailing = q[3];
        } else if (kind == 9) {
            float angle = phase, inc = .02454369328916073f, pa = 0, pb = 0, da = order * .01f,
                  db = secondary * .01f;
            Point a0 = add(rotate({0, sx * radius, 0}, angle), offset),
                  b0 = add(rotate({0, sy * ripple, 0}, angle), offset);
            angle += inc;
            pa += da;
            pb += db;
            float scale = (float(i[12]) * (1.f / 255.f) - .5f) * 2;
            for (int n = 0; n < 256; n++) {
                Point a1 = add(rotate({0, (std::sin(pa) + sx) * radius, 0}, angle), offset),
                      b1 = add(rotate({0, (std::sin(pb) + sy) * ripple, 0}, angle), offset);
                triangle(out, b0, a0, a1, {col[1], col[0], col[0]}, scale, mirror);
                triangle(out, b0, a1, b1, {col[1], col[0], col[1]}, scale, mirror);
                a0 = a1;
                b0 = b1;
                angle += inc;
                pa += da;
                pb += db;
            }
            trailing = b0;
        }
    }
    for (const auto &v : out)
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) ||
            !std::isfinite(v.position.z))
            throw std::runtime_error("Generated shape diverged; reduce recursive scaling");
    return out;
}
} // namespace neon::pc
