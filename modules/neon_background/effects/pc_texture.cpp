#include "pc_texture.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace neon::pc {
namespace {
struct Random {
    uint32_t state = 0x83640459;
    uint32_t next() {
        state = state * 214013u + 2531011u;
        return (state >> 16) & 32767;
    }
    float fraction() {
        return float(next()) * (1.f / 32768.f);
    }
    int position(int extent) {
        return int(std::nearbyint(fraction() * extent));
    }
};
RootPixels cone(float scale) {
    RootPixels result{};
    for (int y = -32; y < 32; ++y)
        for (int x = -32; x < 32; ++x) {
            int value = std::max(0, int(255.f * (1.f - std::sqrt(float(x * x + y * y)) /
                                                           std::sqrt(2048.f) * scale)));
            uint32_t c = uint32_t(value);
            result[(y + 32) * 64 + x + 32] = c * 0x01010101u;
        }
    return result;
}
void stamp(const RootPixels &sprite, RootPixels &output, int px, int py, int opacity,
           uint32_t colour, bool maximum = false) {
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            unsigned alpha = ((sprite[y * 64 + x] >> 24) * opacity) >> 8;
            uint32_t contribution = alpha << 24;
            for (int shift = 0; shift < 24; shift += 8)
                contribution |= ((((colour >> shift) & 255) * alpha) >> 8) << shift;
            auto &destination = output[((py + y) & 63) * 64 + ((px + x) & 63)];
            uint32_t sum = 0;
            for (int shift = 0; shift < 32; shift += 8) {
                unsigned a = (destination >> shift) & 255, b = (contribution >> shift) & 255;
                sum |= (maximum ? std::max(a, b) : std::min(255u, a + b)) << shift;
            }
            destination = sum;
        }
}
} // namespace
RootPixels rootTexture(unsigned selector) {
    if (selector == 8) {
        RootPixels cross{};
        for (int y = -32; y < 32; ++y)
            for (int x = -32; x < 32; ++x) {
                unsigned value = unsigned(std::max(
                    0, int(255.f * (1.f - float(std::min(std::abs(x), std::abs(y))) / 8.f))));
                cross[(y & 63) * 64 + (x & 63)] = value * 0x01010101u;
            }
        return cross;
    }
    Random random;
    constexpr std::array<uint32_t, 8> colours = {0x06fb08, 0xf8a001, 0xfd4b01, 0xff2502,
                                                 0xfd0260, 0x980363, 0x453cf9, 0xd8c0f2};
    constexpr std::array<uint32_t, 8> blues = {0x8000ff, 0x7000ff, 0x6000ef, 0x5000df,
                                               0x4000cf, 0x3000bf, 0x2000af, 0x10009f};
    auto sprite = cone(2.5f);
    RootPixels first{};
    for (int i = 0; i < 32; ++i) {
        int x = random.position(63), y = random.position(63), a = random.position(128) + 16;
        stamp(sprite, first, x, y, a, colours[i & 7]);
    }
    if (selector == 2)
        return first;
    sprite = cone(random.fraction() * 6.f + 1.5f);
    RootPixels second{};
    for (int i = 0; i < 32; ++i) {
        int x = random.position(63), y = random.position(63), a = random.position(128) + 32;
        stamp(sprite, second, x, y, a, blues[i & 7]);
    }
    if (selector == 3)
        return second;
    if (selector == 4) {
        constexpr std::array<uint32_t, 8> fire = {0xffffff, 0xffff80, 0xffff00, 0xffc000,
                                                  0xff8000, 0xdd4400, 0xaa2200, 0x660000};
        RootPixels trails{};
        for (int pass = 0; pass < 2; ++pass) {
            const float dx = random.fraction() * 20.f - 10.f, dy = random.fraction() * 20.f - 10.f;
            sprite = cone(random.fraction() * 6.f + 1.5f);
            float x = 0, y = 0, opacity = 200.f;
            for (int i = 0; i < 32; ++i) {
                stamp(sprite, trails, int(x), int(y), int(opacity), fire[i & 7], true);
                x += dx;
                y += dy;
                opacity -= 6.25f;
            }
        }
        return trails;
    }
    throw std::runtime_error("Root texture not reconstructed");
}
} // namespace neon::pc
