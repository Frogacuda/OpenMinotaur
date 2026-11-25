#include "pc_geometry.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace neon::pc {
namespace {
constexpr float pi = 3.1415927f, tau = 6.2831855f;
Point add(Point a, Point b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Point ring(float angle, float radius) {
    return {-std::sin(angle) * radius, std::cos(angle) * radius, 0};
}
uint32_t blend(uint32_t a, uint32_t b, float t) {
    uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float x = float((a >> shift) & 255), y = float((b >> shift) & 255);
        result |= (uint32_t(std::nearbyint(x + (y - x) * t)) & 255) << shift;
    }
    return result;
}
void triangle(Mesh &mesh, Point a, Point b, Point c, const std::array<uint32_t, 3> &colours,
              float scale, bool mirror) {
    for (auto p : {a, b, c}) {
        p.x *= scale;
        p.y *= scale;
        p.z *= scale;
        mesh.push_back({p, colours[(mesh.size() % 3)]});
    }
    if (mirror) {
        for (auto p : {a, b, c}) {
            p.x *= -scale;
            p.y *= scale;
            p.z *= scale;
            mesh.push_back({p, colours[(mesh.size() % 3)]});
        }
    }
}
} // namespace
Mesh generateShapes(const ShapeSettings &s, const std::array<uint32_t, 4> &colours) {
    Mesh mesh;
    const auto &f = s.floats;
    const auto &integers = s.integers;
    for (int component = 0; component < 2; ++component) {
        const int kind = (integers[13] >> (component * 5)) & 31;
        if (kind == 31)
            continue;
        if (kind != 1 && kind != 2 && kind != 5 && kind != 7)
            throw std::runtime_error("Unimplemented shape case in this component");
        const bool mirror = (integers[13] & (0x800 << component)) != 0;
        int order = std::clamp(integers[component * 2] + 1, 1, 128),
            secondary = std::clamp(integers[component * 2 + 1] + 1, 1, 128);
        float phase = f[component * 2] * tau, wavePhase = f[component * 2 + 1] * tau;
        const float radius = f[4 + component * 2] * 400.f, ripple = f[5 + component * 2] * 400.f;
        Point offset = component == 0 ? Point{f[11] * 400.f, f[12] * 400.f, 0}
                                      : Point{f[14] * 400.f, f[15] * 400.f, 0};
        const float scaleX = f[8 + component * 2], scaleY = f[component == 0 ? 9 : 13];
        std::array<uint32_t, 3> c = {colours[component * 2], colours[component * 2],
                                     colours[component * 2 + 1]};
        if (kind == 2 || kind == 7) {
            const int count = order * 4;
            const float arc = kind == 7 ? pi : tau, inc = arc / count,
                        waveInc = arc * secondary / count;
            Point a = add(ring(phase, std::sin(wavePhase) * ripple + radius), offset), first = a;
            Point b =
                add(ring(phase + inc, std::sin(wavePhase + waveInc) * ripple + radius), offset);
            phase += inc;
            wavePhase += 2 * waveInc;
            const float scale = kind == 7 ? (float(integers[12]) * (1.f / 255.f) - .5f) * 2.f : 1.f;
            for (int i = 0; i < count; ++i) {
                triangle(mesh, a, b, offset, c, scale, mirror);
                if (kind == 2 || i < count - 1) {
                    phase += inc;
                    a = b;
                    b = add(ring(phase, std::sin(wavePhase) * ripple + radius), offset);
                    wavePhase += waveInc;
                }
            }
            if (kind == 7) {
                Point cap = add(ring(phase + 1.5707964f, ripple + radius), offset);
                Point middle = {(first.x + b.x) * .5f, (first.y + b.y) * .5f, 0};
                c[0] = colours[2];
                triangle(mesh, cap, first, middle, c, scale, mirror);
                triangle(mesh, cap, b, middle, c, scale, mirror);
            }
        } else if (kind == 5) {
            Point left = {offset.x - ripple, offset.y + radius * .3f, 0},
                  top = {offset.x, offset.y - radius, 0}, right = {offset.x + ripple, left.y, 0};
            std::array<uint32_t, 3> all = {colours[0], colours[1], colours[2]};
            triangle(mesh, left, top, offset, all, 1, mirror);
            triangle(mesh, right, top, offset, all, 1, mirror);
            all[1] = all[0];
            triangle(mesh, left, right, offset, all, 1, mirror);
        } else {
            // The original temporarily seeds the MSVC generator for stable clusters.
            uint32_t randomState = 0xf1055baa;
            auto random = [&](float extent) {
                randomState = randomState * 214013u + 2531011u;
                return float((randomState >> 16) & 32767) * (1.f / 32768.f) * extent;
            };
            float currentRadius = scaleX * 50.f;
            const float inc = wavePhase / order;
            for (int cluster = 0; cluster < secondary; ++cluster) {
                currentRadius *= scaleY;
                Point center = {random(radius) - radius * .5f + offset.x,
                                random(ripple) - ripple * .5f + offset.y, 0};
                float t = float(cluster) / secondary;
                c = {blend(colours[0], colours[1], t), blend(colours[0], colours[1], t),
                     colours[2]};
                Point previous = {std::sin(phase) * currentRadius + center.x,
                                  std::cos(phase) * currentRadius + center.y, 0};
                offset.x *= scaleY;
                offset.y *= scaleY;
                phase += inc;
                for (int segment = 0; segment < order; ++segment) {
                    Point next = {std::sin(phase) * currentRadius + center.x,
                                  std::cos(phase) * currentRadius + center.y, 0};
                    phase += inc;
                    triangle(mesh, previous, next, center, c, 1, mirror);
                    previous = next;
                }
            }
        }
    }
    return mesh;
}
} // namespace neon::pc
