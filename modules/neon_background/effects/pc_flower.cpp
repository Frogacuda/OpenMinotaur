#include "pc_flower.h"
#include <cmath>
#include <stdexcept>
namespace neon::pc {
namespace {
Matrix4 axis(int a, float v) {
    auto m = identityMatrix();
    float c = std::cos(v), s = std::sin(v);
    if (a == 0) {
        m[5] = c;
        m[6] = s;
        m[9] = -s;
        m[10] = c;
    }
    if (a == 1) {
        m[0] = c;
        m[2] = -s;
        m[8] = s;
        m[10] = c;
    }
    if (a == 2) {
        m[0] = c;
        m[1] = s;
        m[4] = -s;
        m[5] = c;
    }
    return m;
}
const std::array<float, 20480> &sineTable() {
    static const auto table = [] {
        std::array<float, 20480> t{};
        // 00411810: float step, extended-precision accumulator; cosine is
        // the same table offset by 4096, not a separately evaluated cosine.
        constexpr float step = 6.2831855f / 16384;
        for (size_t n = 0; n < t.size(); ++n)
            t[n] = float(std::sin(double(n) * step));
        return t;
    }();
    return table;
}
} // namespace
Matrix4 flowerModelView(const ShapeSettings &s, const Matrix4 &parent) {
    auto &f = s.floats;
    auto r = multiplyMatrix(axis(2, f[8]), multiplyMatrix(axis(1, f[6]), axis(0, f[7])));
    return multiplyMatrix(multiplyMatrix(r, translationMatrix(0, 0, f[3])), parent);
}
CubeFace flowerMesh(const ShapeSettings &s) {
    const auto &i = s.integers;
    const auto &f = s.floats;
    if (i[0] < 1 || i[0] > 2048)
        throw std::runtime_error("Flower segments outside supported bounds");
    for (float v : f)
        if (!std::isfinite(v))
            throw std::runtime_error("Nonfinite flower geometry");
    CubeFace out;
    out.reserve(size_t(i[0]) * 12 * ((i[9] & 1) ? 2 : 1));
    const uint32_t edge = (uint32_t(i[7]) | 0xffffff00u) << 8, middle = edge | uint32_t(i[2]);
    const float repeats = float(i[3]), du = repeats / float(i[0]) * ((i[9] & 2) ? 2 : 1);
    const auto &table = sineTable();
    float depth1 = f[4], depth2 = f[5];
    auto quad = [&](CubeVertex a, CubeVertex b, CubeVertex c, CubeVertex d) {
        for (auto v : {a, b, c, c, d, a})
            out.push_back(v);
    };
    auto vertex = [](uint32_t colour, float u, float v, float x, float y, float z) {
        return CubeVertex{colour, u, v, u, v, x, y, z};
    };
    for (int pass = 0; pass < ((i[9] & 1) ? 2 : 1); ++pass) {
        uint32_t angle = (uint32_t(i[1]) & 0x3fff) << 16;
        float u = 0, dz1 = depth1 * f[11], dz2 = (depth2 - depth1) * f[11];
        for (int n = 0; n < i[0]; ++n) {
            unsigned a = (angle >> 16) & 0x3fff;
            angle += 0x40000000u / unsigned(i[0]);
            unsigned b = (angle >> 16) & 0x3fff;
            float sx = table[a], sy = table[a + 4096], tx = table[b], ty = table[b + 4096];
            float u0 = u, u1 = u + du;
            u += du;
            if (i[9] & 2) {
                if (u0 > repeats)
                    u0 = 2 * repeats - u0;
                if (u1 > repeats)
                    u1 = 2 * repeats - u1;
            }
            float dx = (sx * 1057.f - sx * f[0]) * f[9], dy = (sy * 1057.f - sy * f[0]) * f[10];
            auto v = [&](uint32_t colour, float uv, float vv, float x, float y, float z) {
                return vertex(colour, uv, vv, x + dx, y + dy, z);
            };
            quad(v(edge, u0, 0, sx * f[0], sy * f[0], dz1),
                 v(middle, u0, .5f, sx * f[1], sy * f[1], depth1 + dz1),
                 v(middle, u1, .5f, tx * f[1], ty * f[1], depth1 + dz1),
                 v(edge, u1, 0, tx * f[0], ty * f[0], dz1));
            quad(v(middle, u0, .5f, sx * f[1], sy * f[1], depth1 + dz2),
                 v(edge, u0, 1, sx * f[2], sy * f[2], depth2 + dz2),
                 v(edge, u1, 1, tx * f[2], ty * f[2], depth2 + dz2),
                 v(middle, u1, .5f, tx * f[1], ty * f[1], depth1 + dz2));
        }
        depth1 = -depth1;
        depth2 = -depth2;
    }
    return out;
}
} // namespace neon::pc
