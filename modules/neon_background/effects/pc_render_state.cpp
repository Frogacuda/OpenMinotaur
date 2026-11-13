#include "pc_render_state.h"
#include "pc_cube.h"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
namespace neon::pc {
Matrix4 identityMatrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
Matrix4 multiplyMatrix(const Matrix4 &a, const Matrix4 &b) {
    Matrix4 out{};
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            for (int k = 0; k < 4; k++)
                out[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
    return out;
}
Matrix4 translationMatrix(float x, float y, float z) {
    auto m = identityMatrix();
    m[12] = x;
    m[13] = y;
    m[14] = z;
    return m;
}
Matrix4 lookAtMatrix(Point eye, Point target, Point up) {
    auto normalize = [](Point p) {
        float n = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        if (!std::isfinite(n) || n < 1.e-12f)
            throw std::runtime_error("Degenerate original camera");
        return Point{p.x / n, p.y / n, p.z / n};
    };
    auto cross = [](Point a, Point b) {
        return Point{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    };
    auto dot = [](Point a, Point b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    Point z = normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z}),
          x = normalize(cross(up, z)), y = cross(z, x);
    return {x.x, y.x, z.x, 0, x.y,          y.y,          z.y,          0,
            x.z, y.z, z.z, 0, -dot(x, eye), -dot(y, eye), -dot(z, eye), 1};
}
Matrix4 originalPerspective(float top, float bottom, float left, float right, float nearZ,
                            float farZ) {
    constexpr float epsilon = .00001f;
    if (std::abs(top - bottom) < epsilon) {
        top += epsilon;
        bottom -= epsilon;
    }
    if (std::abs(left - right) < epsilon) {
        left -= epsilon;
        right += epsilon;
    }
    if (std::abs(farZ - nearZ) < epsilon) {
        nearZ -= epsilon;
        farZ += epsilon;
    }
    return {2 * nearZ / (right - left),
            0,
            0,
            0,
            0,
            2 * nearZ / (top - bottom),
            0,
            0,
            (left + right) / (left - right),
            (top + bottom) / (bottom - top),
            farZ / (farZ - nearZ),
            1,
            0,
            0,
            nearZ * farZ / (nearZ - farZ),
            0};
}
Matrix4 originalStackView() {
    return lookAtMatrix({0, 0, -100}, {0, 0, 100}, {0, 1, 0});
}
Matrix4 originalStackProjection(int width, int height, bool mixer) {
    if (width < 1 || height < 1 || width > 8192 || height > 8192)
        throw std::runtime_error("Invalid original viewport");
    return originalPerspective(float(height >> 1), float(-height >> 1), float(-width >> 1),
                               float(width >> 1), 400, mixer ? 10024.f : 14100.f);
}
Matrix4 originalCubeModelView(const ShapeSettings &s, const Matrix4 &parent) {
    return multiplyMatrix(multiplyMatrix(cubeRotation(s.floats[8], s.floats[6], s.floats[7]),
                                         translationMatrix(s.floats[0], -s.floats[1], s.floats[4])),
                          parent);
}
std::vector<ShapeDisplayInstance> originalShapeDisplay(const ShapeSettings &s,
                                                       const Matrix4 &parent, float pitchInput,
                                                       float yawInput, float seconds,
                                                       int list12Count) {
    constexpr float pi = 3.1415927f, tau = 6.2831855f;
    int list = s.integers[14], count = list == 3 ? 10 : list == 12 ? list12Count : 1;
    if (count < 1 || count > 128 || !std::isfinite(pitchInput) || !std::isfinite(yawInput) ||
        !std::isfinite(seconds))
        throw std::runtime_error("Missing or invalid original shape display state");
    auto axis = [](int index, float angle) {
        auto m = identityMatrix();
        float c = std::cos(angle), v = std::sin(angle);
        if (index == 0) {
            m[5] = c;
            m[6] = v;
            m[9] = -v;
            m[10] = c;
        }
        if (index == 1) {
            m[0] = c;
            m[2] = -v;
            m[8] = v;
            m[10] = c;
        }
        if (index == 2) {
            m[0] = c;
            m[1] = v;
            m[4] = -v;
            m[5] = c;
        }
        return m;
    };
    std::vector<ShapeDisplayInstance> result;
    float time = seconds * .1f, angle = 1.5707964f;
    for (int i = 0; i < count; i++) {
        float scale = list == 3 ? (i == 0 ? .5f : .2f) : list == 12 ? .3f : .5f;
        auto sizing = identityMatrix();
        sizing[0] = sizing[5] = scale;
        auto rotation = multiplyMatrix(
            axis(2, angle), multiplyMatrix(axis(1, yawInput * pi), axis(0, pitchInput * pi)));
        float x = list == 3 ? 200.f - float(i) * 40.f : 0.f;
        auto world = multiplyMatrix(
            sizing, multiplyMatrix(multiplyMatrix(rotation, translationMatrix(x, 0, 500)), parent));
        std::array<float, 4> uniform = {time + .05875f, time * 2, time * 3, 1};
        time += .05875f;
        result.push_back({world, uniform});
        if (list == 12)
            angle += tau / count;
    }
    return result;
}
StackRenderSetup stackRenderSetup(const ShapeSettings &terminal) {
    const auto &i = terminal.integers;
    if (i[0] < 0 || i[0] > 13 || i[1] < 0 || i[1] > 13)
        throw std::runtime_error("Unsupported original render-target extent");
    StackRenderSetup s{};
    s.width = 1 << i[0];
    s.height = 1 << i[1];
    s.clearScratch = i[8] == 0;
    s.addressU = ((unsigned(i[7]) & 2) != 0) + 1;
    s.addressV = ((unsigned(i[7]) & 1) != 0) + 1;
    for (int c = 0; c < 4; c++) {
        s.constant25[c] = terminal.floats[4 + c];
        s.constant26[c] = terminal.floats[9 + c];
    }
    return s;
}
TextureBinding originalTextureBinding(int selector) {
    if (selector <= 0)
        return {TextureSourceKind::Retained, selector};
    if (selector < 31)
        return {TextureSourceKind::Root, selector - 1};
    if (selector < 47)
        return {TextureSourceKind::Stack, selector - 31};
    return {TextureSourceKind::Alternate, selector - 47};
}
RootPixels loadOriginalRoot(const std::filesystem::path &folder, unsigned selector,
                            const AssetReader &reader) {
    if (selector > 30)
        throw std::runtime_error("Invalid root selector");
    auto file = assetStream(folder / (std::to_string(selector) + ".argb"), reader);
    RootPixels pixels{};
    if (!file.read((char *)pixels.data(), sizeof(pixels)) ||
        file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Missing or invalid original root texture: " +
                                 std::to_string(selector));
    return pixels;
}
} // namespace neon::pc
