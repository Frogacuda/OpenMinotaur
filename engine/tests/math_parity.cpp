#include "neon/math.h"
#include "legacy_base.h"
#include <iostream>
#include <vector>
#include <cmath>

// ADL dispatches the same operations to the engine and frozen reference types.
template <class V2, class V3, class V4, class M, class C> std::vector<double> exercise() {
    std::vector<double> values;
    auto matrix = [&](const M &m) {
        for (const auto &row : m.m)
            for (float x : row)
                values.push_back(x);
    };
    auto colour = [&](const C &c) {
        values.insert(values.end(), {c.r, c.g, c.b, c.a});
        values.push_back(c.packRGBA());
        values.push_back(c.packARGB());
        for (int i = 0; i < 3; i++)
            values.insert(values.end(), {c.phase[i], c.speed[i], c.amp[i], c.base[i]});
    };
    for (int i = -12; i <= 12; i++) {
        V2 a(i * 0.125, i * 0.25), b(0.3, -0.75);
        a.rotateBy(i * 0.2);
        a = a.lerp(b, i * 0.15f);
        values.insert(values.end(), {a.x, a.y, a.length()});
        a = a.normalize();
        values.insert(values.end(), {a.x, a.y});
        M m, n, t;
        D3DXMatrixIdentity(&m);
        matrix(m);
        D3DXMatrixScaling(&m, 1.2f, -0.7f, 3.0f);
        matrix(m);
        D3DXMatrixTranslation(&n, i * 0.5f, -3.0f, 2.0f);
        matrix(n);
        t = m * n;
        matrix(t);
        D3DXMatrixTranspose(&t, &t);
        matrix(t);
        D3DXMatrixRotationX(&m, i * 0.2f);
        matrix(m);
        D3DXMatrixRotationY(&m, i * 0.2f);
        matrix(m);
        D3DXMatrixRotationZ(&m, i * 0.2f);
        matrix(m);
        V3 axis{1, 2, 3}, eye{1, 2, -4}, at{0, 0, 0}, up{0, 1, 0};
        D3DXMatrixRotationAxis(&m, &axis, i * 0.2f);
        matrix(m);
        D3DXMatrixLookAtLH(&m, &eye, &at, &up);
        matrix(m);
        D3DXMatrixOrthoLH(&m, 480, 320, 0.1f, 100);
        matrix(m);
        D3DXMatrixPerspectiveFovLH(&m, 0.8f, 1.5f, 0.1f, 100);
        matrix(m);
        V4 projected;
        D3DXVec3Transform(&projected, &axis, &m);
        values.insert(values.end(), {projected.x, projected.y, projected.z, projected.w});
        C c(i * 0.1, 0.3, 0.8, i * 0.05), other(1, 0, 0, 0.5);
        c.phase[0] = 0.1f;
        c.speed[0] = 0.03f;
        c.amp[0] = 0.7f;
        c.base[0] = 0.2f;
        colour(c);
        c.generate();
        colour(c);
        colour(c.lerp(other, 0.3f));
        colour(c * 0.7f);
        colour(clerp(c, other, -0.3f));
        c.unpackRGBA(0x1846abff);
        colour(c);
        c.unpackARGB(0x8143c7ef);
        colour(c);
        colour(c.red(0.4));
        colour(c.orange(0.3));
        colour(c.green(0.8));
        colour(c.blue(0.7));
        colour(c.yellow(0.2));
        colour(c.purple(0.5));
        colour(c.white(i * 0.1));
    }
    return values;
}
int main() {
    using namespace reference::Neon_Internals;
    const auto engine =
        exercise<neon::vec2d, neon::D3DXVECTOR3, neon::D3DXVECTOR4, neon::matrix4f, neon::colour>();
    const auto legacy = exercise<vec2d, D3DXVECTOR3, D3DXVECTOR4, matrix4f, colour>();
    if (engine != legacy) {
        std::cerr << "Foundation output changed\n";
        return 1;
    }
    neon::colour masked;
    for (int i = 0; i < 3; ++i) {
        masked.phase[i] = .1f;
        masked.speed[i] = .03f;
        masked.amp[i] = .7f;
        masked.base[i] = .2f;
    }
    masked.generate(6);
    if (masked.phase[0] != .1f || masked.phase[1] != .1f + .03f || masked.phase[2] != .1f + .03f ||
        masked.r != double(.2f) + (std::sin(.1f) * .5 + .5) * double(.7f))
        return 1;
    // Deflex e788 original-execution captures with these
    // explicitly supplied coefficients; not inferred original game defaults.
    const float phase[] = {.1f, .2f, .3f}, speed[] = {.03f, .06f, .09f};
    for (int i = 0; i < 3; ++i) {
        masked.phase[i] = phase[i];
        masked.speed[i] = speed[i];
    }
    masked.generate(6);
    if (std::fabs(masked.r - .5849416939179097) > 1e-12 ||
        std::fabs(masked.g - .6399781897242036) > 1e-12 ||
        std::fabs(masked.b - .6830659471142981) > 1e-12)
        return 1;
    neon::matrix4f translation;
    neon::D3DXMatrixTranslation(&translation, 2, 3, 4);
    neon::D3DXVECTOR3 p{1, 1, 1};
    neon::D3DXVECTOR4 q;
    neon::D3DXVec3Transform(&q, &p, &translation);
    if (q.x != 3 || q.y != 4 || q.z != 5 || q.w != 1)
        return 1;
    std::cout << "Foundation parity: " << engine.size()
              << " scalar results; row-vector translation verified\n";
}
