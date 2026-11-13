// Deformed cube surfaces: six oriented grids with a shared displacement field.
// Each cell emits two triangles. Texture coordinates can repeat or mirror;
// displacement moves vertices outward along the selected face's normal.
#include "pc_cube.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neon::pc {
namespace {
float textureCoordinate(float value, float extent, bool mirror) {
    return mirror && value > extent ? extent - (value - extent) : value;
}

void emitFace(const ShapeSettings &settings, const CubeField &field, uint32_t rgba, int face,
              int subdivisions, CubeFace &result) {
    const float width = settings.floats[2] * 2.f;
    const float height = settings.floats[3] * 2.f;
    const float depth = settings.floats[5] * 2.f;
    const float stretchX = settings.floats[11] + 1.f;
    const float stretchY = settings.floats[12] + 1.f;
    const float stretchZ = settings.floats[13] + 1.f;
    const float textureExtent = float(settings.integers[8]);
    const bool mirror = (settings.integers[14] & 1) != 0;
    float textureStep = textureExtent / float(subdivisions);
    if (mirror)
        textureStep *= 2.f;

    // U follows the inner loop. Opposite faces reverse that axis so triangle
    // winding stays consistent. Top/bottom use Z for rows; other faces use Y.
    const bool horizontalFace = face == 3 || face == 5;
    const float rowStep = (horizontalFace ? depth : height) / float(subdivisions);
    const float columnStep = (face == 2 || face == 4 ? depth : width) / float(subdivisions);
    const bool reverseColumns = face == 1 || face == 2 || face == 3;
    const float signedColumnStep = reverseColumns ? -columnStep : columnStep;
    const float columnExtent = face == 2 || face == 4 ? depth : width;
    const float columnStart = columnExtent * (reverseColumns ? .5f : -.5f);
    const float rowStretch = horizontalFace ? stretchZ : stretchY;
    const float columnStretch = face == 2 || face == 4 ? stretchZ : stretchX;
    float rowPosition = (horizontalFace ? depth : height) * -.5f;
    float rawV = 0.f;

    for (int row = 0; row < subdivisions; ++row) {
        const float row0 = rowStretch * rowPosition;
        // Stretch offsets each cell's origin; cell width itself stays unchanged.
        const float row1 = row0 + rowStep;
        const float v0 = textureCoordinate(rawV, textureExtent, mirror);
        const float nextV = rawV + textureStep;
        const float v1 = textureCoordinate(nextV, textureExtent, mirror);
        const int nextRow = (row + 1) % subdivisions;
        float columnPosition = columnStart;
        float rawU = 0.f;

        for (int column = 0; column < subdivisions; ++column) {
            const float column0 = columnStretch * columnPosition;
            const float column1 = column0 + signedColumnStep;
            const float u0 = textureCoordinate(rawU, textureExtent, mirror);
            const float nextU = rawU + textureStep;
            const float u1 = textureCoordinate(nextU, textureExtent, mirror);
            const int nextColumn = (column + 1) % subdivisions;

            // The field has a fixed 32-float stride. Wrap at the authored grid
            // size so the final cells join the first displacement samples.
            auto vertex = [&](float u, float v, float across, float along, int x, int y) {
                const float displacement = field[x * 32 + y];
                float px = 0.f, py = 0.f, pz = 0.f;
                switch (face) {
                case 0:
                    px = across;
                    py = along;
                    pz = depth * .5f * stretchZ + displacement;
                    break;
                case 1:
                    px = across;
                    py = along;
                    pz = depth * -.5f * stretchZ - displacement;
                    break;
                case 2:
                    px = width * .5f * stretchX + displacement;
                    py = along;
                    pz = across;
                    break;
                case 3:
                    px = across;
                    py = height * .5f * stretchY + displacement;
                    pz = along;
                    break;
                case 4:
                    px = width * -.5f * stretchX - displacement;
                    py = along;
                    pz = across;
                    break;
                case 5:
                    px = across;
                    py = height * -.5f * stretchY - displacement;
                    pz = along;
                    break;
                }
                return CubeVertex{rgba, u, v, u, v, px, py, pz};
            };
            const auto a = vertex(u0, v0, column0, row0, column, row);
            const auto b = vertex(u0, v1, column0, row1, column, nextRow);
            const auto c = vertex(u1, v1, column1, row1, nextColumn, nextRow);
            const auto d = vertex(u1, v0, column1, row0, nextColumn, row);
            result.insert(result.end(), {a, b, c, c, d, a});
            columnPosition += signedColumnStep;
            rawU = nextU;
        }
        rowPosition += rowStep;
        rawV = nextV;
    }
}
} // namespace

std::array<float, 16> cubeRotation(float a, float b, float c) {
    const double ca = std::cos(double(a)), sa = std::sin(double(a)), cb = std::cos(double(b)),
                 sb = std::sin(double(b)), cc = std::cos(double(c)), sc = std::sin(double(c));
    return {float(ca * cb),
            float(sa * cc + ca * sb * sc),
            float(sa * sc - sb * cc * ca),
            0,
            float(-sa * cb),
            float(ca * cc - sb * sc * sa),
            float((ca + sb * cc) * sa),
            0,
            float(sb),
            float(-cb * sc),
            float(cb * cc),
            0,
            0,
            0,
            0,
            1};
}
CubeField cubeField(const ShapeSettings &s) {
    CubeField field{};
    int order = s.integers[10];
    if (order < 0 || order > 28)
        throw std::runtime_error("Cube subdivision must be between 0 and 28");
    int n = order + 2;
    float scale = std::min({s.floats[2], s.floats[3], s.floats[5]});
    for (int y = 0; y < (n >> 1) + 1; y++)
        for (int x = 0; x < (n >> 1) + 1; x++) {
            float value =
                float(x * y) * scale * s.floats[9] + s.floats[10] * 32.f / float(30 - order);
            field[y * 32 + x] = field[(n - y) * 32 + x] = field[(n - y) * 32 + n - x] =
                field[y * 32 + n - x] = value;
        }
    return field;
}
CubeFace cubeFace(const ShapeSettings &s, const CubeField &field, uint32_t rgba, int face) {
    if (face < 0 || face > 5 || s.integers[10] < 0 || s.integers[10] > 28)
        throw std::runtime_error("Invalid cube face/order");
    for (float f : s.floats)
        if (!std::isfinite(f) || std::abs(f) > 100000)
            throw std::runtime_error("Invalid cube parameter");
    CubeFace result;
    int n = s.integers[10] + 2;
    result.reserve(n * n * 6);
    emitFace(s, field, rgba, face, n, result);
    return result;
}
} // namespace neon::pc