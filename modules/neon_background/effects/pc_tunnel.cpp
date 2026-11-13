#include "pc_tunnel.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace neon::pc {
namespace {
Point add(Point a, Point b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Point lerp(Point a, Point b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
Point normalized(Point a) {
    float n = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    if (n != 0) {
        a.x /= n;
        a.y /= n;
        a.z /= n;
    }
    return a;
}
Point transform(Point p, const Matrix4 &m) {
    return {p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12],
            p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
            p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14]};
}
Matrix4 axis(int a, float angle) {
    auto m = identityMatrix();
    float c = std::cos(angle), s = std::sin(angle);
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
} // namespace
TunnelProfiles TunnelProfiles::load(const std::filesystem::path &folder,
                                    const AssetReader &reader) {
    TunnelProfiles p;
    for (auto entry : {std::pair<const char *, std::array<float, 14 * 195> *>{
                           "profile-positions.bin", &p.positions},
                       {"profile-normals.bin", &p.normals}}) {
        auto f = assetStream(folder / entry.first, reader);
        if (!f.read((char *)entry.second->data(), sizeof(*entry.second)) ||
            f.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("Invalid original tunnel profiles");
        for (float value : *entry.second)
            if (!std::isfinite(value))
                throw std::runtime_error("Nonfinite tunnel profile");
    }
    return p;
}
float TunnelPlayback::scalar(size_t o) const {
    float f;
    auto v = word(o);
    std::memcpy(&f, &v, 4);
    return f;
}
void TunnelPlayback::scalar(size_t o, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    word(o, v);
}
Point TunnelPlayback::point(size_t o) const {
    return {scalar(o), scalar(o + 4), scalar(o + 8)};
}
void TunnelPlayback::point(size_t o, Point p) {
    scalar(o, p.x);
    scalar(o + 4, p.y);
    scalar(o + 8, p.z);
}
void TunnelPlayback::reset() {
    data_ = {};
    initialized_ = false;
}
void TunnelPlayback::readState(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    decltype(data_) pending;
    if (!f.read((char *)pending.data(), sizeof pending) ||
        f.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Invalid original tunnel state extent");
    data_ = pending;
    initialized_ = true;
}
void TunnelPlayback::tick(const ShapeSettings &n, const TunnelProfiles &profiles, bool advance) {
    if (!initialized_) {
        word(0x271f8, uint32_t(-3));
        word(0x2820c, 1);
        scalar(0x28204, .01f);
        scalar(0x28208, .01f);
        initialized_ = true;
    }
    if (n.integers[7] < 0 || n.integers[7] >= 14 || n.integers[10] < 0 || n.integers[10] >= 14 ||
        n.integers[6] < 1 || n.integers[6] > 2000)
        throw std::runtime_error("Unsupported original tunnel bounds");
    scalar(0x28230, n.floats[0]);
    scalar(0x2822c, n.floats[1]);
    scalar(0x271d0, n.floats[2]);
    scalar(0x28250, n.floats[3]);
    word(0x271cc, n.integers[10]);
    scalar(0x271d4, float(n.integers[8] * 8) * .015625f);
    scalar(0x271d8, float(n.integers[9] * 4) * .015625f);
    word(0x271c8, n.integers[7]);
    word(0x28224, n.integers[5]);
    word(0x28228, n.integers[6] * 4);
    word(0x271b8, n.integers[11]);
    scalar(0x28234, float(n.integers[12]));
    word(0x28238, 0);
    word(0x2823c, n.integers[14]);
    scalar(0x28214, float(n.integers[5] - 30) * .005882353f);
    if (advance)
        word(0x28218, word(0x28218) + uint32_t(n.integers[2]) * 2);
    word(0x271b4, uint32_t(int32_t(word(0x28218)) >> 28));
    word(0x2821c, n.integers[3]);
    word(0x28220, n.integers[4]);
    generate(profiles);
}
void TunnelPlayback::generate(const TunnelProfiles &profiles) {
    float spacing = -float(int32_t(word(0x28228)));
    if (word(0x2820c)) {
        word(0x271a0, 0);
        for (int i = 0; i < 128; i++) {
            point(0x271fc + i * 12, {1, 1, spacing});
            scalar(0x277fc + i * 4, .01f);
            scalar(0x279fc + i * 4, .01f);
        }
        word(0x2820c, 0);
        scalar(0x28204, .01f);
        scalar(0x28208, .01f);
        word(0x271f4, 96);
        word(0x281fc, 0);
    }
    uint32_t oldCounter = word(0x281fc);
    word(0x28200, oldCounter);
    if (word(0x271f8) != word(0x271b4)) {
        word(0x28240, word(0x28240) + 8);
        word(0x28244, word(0x28244) + 4);
        word(0x271f8, word(0x271b4));
        uint32_t cursor = word(0x271f4);
        scalar(0x277fc + cursor * 4, scalar(0x2822c));
        scalar(0x279fc + cursor * 4, scalar(0x28230));
        word(0x281fc, oldCounter + 1);
        word(0x271f4, (cursor + 1) & 31);
        scalar(0x28204, scalar(0x28204) + scalar(0x277fc + (oldCounter & 31) * 4));
        scalar(0x28208, scalar(0x28208) + scalar(0x279fc + (oldCounter & 31) * 4));
    }
    float yaw = scalar(0x28204), pitch = scalar(0x28208), blend = scalar(0x271d0);
    int a = int(word(0x271c8)), b = int(word(0x271cc));
    if (blend == 1)
        a = b;
    if (blend < 0 || blend > 1)
        throw std::runtime_error("Invalid tunnel profile blend");
    uint32_t cursor = word(0x281fc) & 31;
    auto rotation = identityMatrix();
    Point centre{};
    for (int ring = 0; ring < 16; ring++) {
        point(0x5b68 + ring * 36, centre);
        for (int corner = 0; corner < 8; corner++) {
            size_t i = size_t(a) * 195 + corner * 24, j = size_t(b) * 195 + corner * 24;
            auto read = [&](const auto &array, size_t at) {
                return Point{array[at], array[at + 1], array[at + 2]};
            };
            auto p = read(profiles.positions, i), normal = read(profiles.normals, i);
            if (blend > 0 && blend < 1) {
                p = lerp(p, read(profiles.positions, j), blend);
                normal = lerp(normal, read(profiles.normals, j), blend);
            }
            size_t at = size_t(corner * 65 + ring) * 36;
            point(at, add(transform(p, rotation), centre));
            point(at + 12, normalized(transform(normal, rotation)));
        }
        size_t close = size_t(8 * 65 + ring) * 36;
        point(close, point(ring * 36));
        point(close + 12, point(ring * 36 + 12));
        cursor = (cursor + 1) & 31;
        auto step = transform(point(0x271fc + cursor * 12), rotation);
        point(0x271fc + cursor * 12, {1, 1, spacing});
        centre = add(centre, step);
        yaw += scalar(0x277fc + cursor * 4);
        pitch += scalar(0x279fc + cursor * 4);
        rotation = multiplyMatrix(axis(1, yaw * 4 * .0055555557f * 3.1415927f),
                                  axis(0, pitch * 4 * .0055555557f * 3.1415927f));
    }
}
Point TunnelPlayback::path(uint32_t phase) const {
    uint32_t ring = (uint32_t(int32_t(phase) >> 28) - 8) & 15;
    float t = float(phase & 0xffffffc) * 3.7252903e-9f;
    return lerp(point(0x5b68 + ring * 36), point(0x5b68 + ((ring + 1) & 15) * 36), t);
}
Point TunnelPlayback::normal(uint32_t phase) const {
    int ring = (int32_t(phase) >> 28) + 8;
    float t = float(phase & 0xffffffc) * 3.7252903e-9f;
    return normalized(lerp(point(ring * 36 + 12), point(((ring + 1) & 31) * 36 + 12), t));
}
TunnelFrame TunnelPlayback::frame(const ShapeSettings &n, const Matrix4 &parent) const {
    if (!initialized_)
        throw std::runtime_error("Tunnel must tick before drawing");
    TunnelFrame out;
    uint32_t phase = word(0x28218) & 0xffffffc;
    out.eye = path(phase);
    out.target = path(phase + 0xf0000004);
    out.up = normal(phase);
    auto model =
        multiplyMatrix(multiplyMatrix(axis(2, n.floats[12]),
                                      multiplyMatrix(axis(1, n.floats[10]), axis(0, n.floats[11]))),
                       translationMatrix(n.floats[7], -n.floats[8], n.floats[9]));
    model = multiplyMatrix(model, parent);
    out.modelView = multiplyMatrix(lookAtMatrix(out.eye, out.target, out.up), model);
    bool mirror = (n.integers[11] & 2) != 0;
    float uStep = scalar(0x271d4) * (mirror ? 2 : 1), vStep = scalar(0x271d8) * (mirror ? 2 : 1);
    float v0 = vStep * float(word(0x281fc) & 31), v1 = v0;
    int previousBand = 0;
    for (int ring = 0; ring < 16; ring++) {
        int side = 8 - ring, band = (7 - std::min(std::abs(side), 7)) * 32;
        if (mirror) {
            float period = float(n.integers[9] * 2);
            if (v0 > period)
                v0 -= period;
            if (v1 > period)
                v1 -= period;
        }
        float nextV0 = v0 + vStep, nextV1 = v1 + vStep, drawV0 = v0, drawV1 = v1,
              drawNextV0 = nextV0, drawNextV1 = nextV1;
        if (mirror) {
            float edge = float(n.integers[9]);
            auto reflect = [&](float v) { return v > edge ? edge - (v - edge) : v; };
            drawV0 = reflect(v0);
            drawV1 = reflect(v1);
            drawNextV0 = reflect(nextV0);
            drawNextV1 = reflect(nextV1);
        }
        if (side < 7 && int32_t(word(0x2821c)) > 0)
            for (int corner = 1; corner < 9; corner++) {
                auto current = point((corner * 65 + ring) * 36),
                     previous = point(((corner - 1) * 65 + ring) * 36),
                     last = point(((corner - 1) * 65 + ((ring - 1) & 15)) * 36),
                     lastCurrent = point((corner * 65 + ((ring - 1) & 15)) * 36);
                Point delta = {(current.x - out.eye.x) * n.floats[4],
                               (current.y - out.eye.y) * n.floats[5],
                               (current.z - out.eye.z) * n.floats[6]};
                float u0 = scalar(0x28250) + (corner - 1) * uStep, u1 = u0 + uStep;
                if (mirror) {
                    float edge = float(n.integers[8]);
                    if (u0 > edge)
                        u0 = edge - (u0 - edge);
                    if (u1 > edge)
                        u1 = edge - (u1 - edge);
                }
                uint32_t c0 = 0xffff0000u | (uint32_t(n.integers[14]) << 8) |
                              uint32_t(previousBand * n.integers[13] >> 8),
                         c1 = 0xffff0000u | (uint32_t(n.integers[14]) << 8) |
                              uint32_t(band * n.integers[13] >> 8);
                auto emit = [&](Point p, uint32_t c, float u, float v, float vv) {
                    p = add(p, delta);
                    out.vertices.push_back({c, u, v, u, vv, p.x, p.y, p.z});
                };
                emit(last, c0, u0, drawV0, drawV1);
                emit(previous, c1, u0, drawNextV0, drawNextV1);
                emit(current, c1, u1, drawNextV0, drawNextV1);
                emit(current, c1, u1, drawNextV0, drawNextV1);
                emit(lastCurrent, c0, u1, drawV0, drawV1);
                emit(last, c0, u0, drawV0, drawV1);
            }
        previousBand = band;
        v0 = nextV0;
        v1 = nextV1;
    }
    return out;
}
} // namespace neon::pc
