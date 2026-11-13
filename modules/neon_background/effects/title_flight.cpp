#include "title_flight.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace neon::pc {
CubeFace titleFlightBillboards(const ShapeSettings &settings,
                               const std::array<PobParticle, 1024> &particles, double seconds,
                               float size, float colourPeriod, float hueOffset, float opacity,
                               int selectedSprite) {
    CubeFace out;
    out.reserve(size_t(settings.integers[0]) * 6);
    for (int i = 0; i < settings.integers[0]; i++) {
        auto &a = particles[i].data;
        int32_t active;
        std::memcpy(&active, &a[15], 4);
        if (active < 0)
            continue;
        // Use a common square world-space canvas. Letterboxing in the atlas carries
        // each sprite's proportions, so source resolution cannot change its size.
        float half = (settings.floats[3] + 1) * a[9] * 128 * size;
        float c = std::cos(a[18]), sn = std::sin(a[18]),
              z = (settings.integers[13] & 2) ? -a[2] : a[2];
        int sprite = selectedSprite ? std::clamp(selectedSprite - 1, 0, 6) : i % 7;
        float hue = float(
            std::fmod(seconds / std::max(1.f, colourPeriod) + hueOffset + i * .61803398875, 1.));
        if (hue < 0)
            hue += 1;
        float rgb[3];
        for (int k = 0; k < 3; k++) {
            float t = std::fmod(hue + float(3 - k) / 3, 1.f) * 6;
            rgb[k] = .12f + .88f * std::clamp(std::abs(t - 3) - 1, 0.f, 1.f);
        }
        uint32_t birth;
        std::memcpy(&birth, &a[16], 4);
        auto byte = [](float v) { return uint32_t(std::clamp(v, 0.f, 255.f)); };
        uint32_t colour = (byte(rgb[0] * 255) << 24) | (byte(rgb[1] * 255) << 16) |
                          (byte(rgb[2] * 255) << 8) | byte(float(birth & 255) * opacity);
        CubeVertex q[4];
        for (int k = 0; k < 4; k++) {
            float x = (k == 1 || k == 2) ? half : -half, y = k >= 2 ? half : -half;
            float u = (sprite % 4 + (k == 1 || k == 2 ? 1.f : 0.f)) * .25f,
                  v = (sprite / 4 + (k >= 2 ? 1.f : 0.f)) * .5f;
            q[k] = {colour, u, v, u, v, x * c - y * sn + a[0], y * c + x * sn + a[1], z};
        }
        for (int k : {0, 1, 2, 2, 3, 0})
            out.push_back(q[k]);
    }
    return out;
}
} // namespace neon::pc
