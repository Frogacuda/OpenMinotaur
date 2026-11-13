// Embossed Settings artwork. Metal bevels are baked here; glass remains a
// separate launcher overlay, shared with every other home-screen icon.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mino::iox {
struct SettingsIcon {
    static constexpr int size = 512;
    struct Point {
        float x, y;
    };
    static std::vector<uint8_t> pixels() {
        constexpr float pi = 3.14159265359f;
        constexpr int teeth = 12;
        std::vector<Point> outline(size_t(teeth) * 4);
        for (int tooth = 0; tooth < teeth; ++tooth)
            for (int corner = 0; corner < 4; ++corner) {
                const float phase[] = {-.5f, -.25f, .25f, .5f};
                const float angle = (tooth + phase[corner]) * 2 * pi / teeth;
                constexpr float root = .265f;
                const float radius = corner == 0 || corner == 3 ? root : .345f;
                outline[size_t(tooth * 4 + corner)] = {std::cos(angle) * radius,
                                                       std::sin(angle) * radius};
            }
        const auto gearDistance = [&](float x, float y) {
            float distance = 10;
            bool inside = false;
            for (size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++) {
                const auto a = outline[i], b = outline[j];
                const float dx = b.x - a.x, dy = b.y - a.y;
                const float t =
                    std::clamp(((x - a.x) * dx + (y - a.y) * dy) / (dx * dx + dy * dy), 0.f, 1.f);
                distance = std::min(distance, std::hypot(x - a.x - t * dx, y - a.y - t * dy));
                if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
                    inside = !inside;
            }
            return std::max(inside ? -distance : distance, .105f - std::hypot(x, y));
        };
        const auto box = [](float x, float y, float half, float radius) {
            const float qx = std::abs(x) - (half - radius), qy = std::abs(y) - (half - radius);
            return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) +
                   std::min(std::max(qx, qy), 0.f) - radius;
        };
        const auto borderDistance = [&](float x, float y) {
            return std::max(box(x, y, .466f, .135f), -box(x, y, .424f, .100f));
        };
        const auto emboss = [](const auto &distance, float x, float y, float base, float bevel) {
            constexpr float e = 1.f / size;
            const auto height = [&](float sx, float sy) {
                return std::clamp(-distance(sx, sy) / bevel, 0.f, 1.f);
            };
            const float nx = -(height(x + e, y) - height(x - e, y)) * .026f / (2 * e);
            const float ny = -(height(x, y + e) - height(x, y - e)) * .026f / (2 * e);
            const float diffuse = (-nx * .55f - ny * .65f + 1) / std::sqrt(nx * nx + ny * ny + 1);
            return base + 75 * (diffuse - 1);
        };
        std::vector<uint8_t> rgba(size_t(size) * size * 4);
        for (int iy = 0; iy < size; ++iy)
            for (int ix = 0; ix < size; ++ix) {
                const float x = (ix + .5f) / size - .5f, y = (iy + .5f) / size - .5f;
                const float gear = gearDistance(x, y), border = borderDistance(x, y);
                float shade = 88 - 20 * y + 7 * std::cos(x * 4);
                const float cast =
                    std::clamp(.5f - gearDistance(x - .003f, y - .008f) / .012f, 0.f, 1.f);
                shade *= 1 - .42f * cast;
                if (gear < 0) {
                    shade = emboss(gearDistance, x, y, 158, .014f);
                    const float radius = std::hypot(x, y);
                    shade -= 16 * std::exp(-std::pow((radius - .184f) / .004f, 2.f));
                    shade += 9 * std::exp(-std::pow((radius - .191f) / .004f, 2.f));
                }
                if (border < 0)
                    shade = emboss(borderDistance, x, y, 195, .009f);
                const size_t p = size_t(iy * size + ix) * 4;
                rgba[p] = rgba[p + 1] = rgba[p + 2] = uint8_t(std::clamp(shade, 0.f, 255.f));
                rgba[p + 3] = 255;
            }
        return rgba;
    }
};

} // namespace mino::iox
