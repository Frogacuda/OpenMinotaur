#include "primitive_mesh.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace neon::effects {
const char *primitiveName(int p) {
    static const char *names[] = {"Plane", "Cube", "Sphere", "Cylinder", "Torus"};
    return p >= 0 && p < 5 ? names[p] : "Unknown primitive";
}
neon::pc::CubeFace primitiveMesh(const Component &c, double seconds) {
    using neon::pc::CubeFace;
    using neon::pc::CubeVertex;
    if (c.profile < 0 || c.profile > 4)
        throw std::runtime_error("Invalid primitive family");
    int columns = std::clamp(int(std::lround(c.values[PrimitiveSegments])), 3, 96),
        rows = std::clamp(int(std::lround(c.values[PrimitiveRings])), 3, 48);
    float w = c.values[PrimitiveWidth] * 200, h = c.values[PrimitiveHeight] * 200,
          d = c.values[PrimitiveDepth] * 200;
    float phase = c.values[Phase] * 6.2831853f +
                  (c.animated ? float(seconds) * c.values[Speed] * 6.2831853f : 0);
    auto vertex = [&](float x, float y, float z, float u, float v) {
        float wave = c.values[PrimitiveDistortion] *
                     std::sin((x + y + z) * 3.14159265f * c.values[PrimitiveWaves] + phase);
        x *= 1 + wave;
        y *= 1 + wave;
        z *= 1 + wave;
        return CubeVertex{0xffffffffu,
                          u * c.values[RepeatX],
                          v * c.values[RepeatY],
                          u * c.values[RepeatX],
                          v * c.values[RepeatY],
                          x * w,
                          y * h,
                          z * d};
    };
    CubeFace mesh;
    auto quad = [&](CubeVertex a, CubeVertex b, CubeVertex c, CubeVertex d) {
        for (auto v : {a, b, c, a, c, d})
            mesh.push_back(v);
    };
    if (c.profile == 0) {
        quad(vertex(-1, -1, 0, 0, 1), vertex(1, -1, 0, 1, 1), vertex(1, 1, 0, 1, 0),
             vertex(-1, 1, 0, 0, 0));
    } else if (c.profile == 1) {
        const float corners[6][12] = {{-1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1},
                                      {1, -1, -1, -1, -1, -1, -1, 1, -1, 1, 1, -1},
                                      {-1, -1, -1, -1, -1, 1, -1, 1, 1, -1, 1, -1},
                                      {1, -1, 1, 1, -1, -1, 1, 1, -1, 1, 1, 1},
                                      {-1, 1, 1, 1, 1, 1, 1, 1, -1, -1, 1, -1},
                                      {-1, -1, -1, 1, -1, -1, 1, -1, 1, -1, -1, 1}};
        for (auto &f : corners)
            quad(vertex(f[0], f[1], f[2], 0, 1), vertex(f[3], f[4], f[5], 1, 1),
                 vertex(f[6], f[7], f[8], 1, 0), vertex(f[9], f[10], f[11], 0, 0));
    } else {
        constexpr float pi = 3.141592653589793f;
        auto point = [&](int x, int y) {
            float u = float(x) / columns, v = float(y) / rows, a = u * 2 * pi, b = v * pi;
            if (c.profile == 2)
                return vertex(std::sin(b) * std::cos(a), std::cos(b), std::sin(b) * std::sin(a), u,
                              v);
            if (c.profile == 3)
                return vertex(std::cos(a), 1 - 2 * v, std::sin(a), u, v);
            float tube = c.values[PrimitiveTube], r = 1 - tube + tube * std::cos(v * 2 * pi);
            return vertex(r * std::cos(a), tube * std::sin(v * 2 * pi), r * std::sin(a), u, v);
        };
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x)
                quad(point(x, y), point(x + 1, y), point(x + 1, y + 1), point(x, y + 1));
        if (c.profile == 3)
            for (int side : {-1, 1})
                for (int x = 0; x < columns; ++x) {
                    float a = float(x) * 2 * pi / columns, b = float(x + 1) * 2 * pi / columns;
                    auto center = vertex(0, float(side), 0, .5f, .5f),
                         p = vertex(std::cos(a), float(side), std::sin(a), .5f + .5f * std::cos(a),
                                    .5f + .5f * std::sin(a)),
                         q = vertex(std::cos(b), float(side), std::sin(b), .5f + .5f * std::cos(b),
                                    .5f + .5f * std::sin(b));
                    for (auto v : {center, p, q})
                        mesh.push_back(v);
                }
    }
    return mesh;
}
} // namespace neon::effects
