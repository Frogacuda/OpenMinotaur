#pragma once
#include "native_head.h"
#include "native_random.h"
#include <functional>
namespace gr::reconstruction::ios {
// Original xpart, used by ac::exp, displayBonus, and expanding outlines.
class Xpart final : public Entity {
  public:
    explicit Xpart(const std::array<Rgba, 33> &sourcePalette) : palette(sourcePalette) {}
    const std::array<Rgba, 33> &palette;
    float fade = 0, fadeStep = 0, growth = 1; // +16c,+170,+174
    int32_t colour = 0;                       // +178
    void run() override;                      // iOS 0x4fe18
};
struct Effects {
    EntityLists &lists;
    RandomStream &random;
    const std::array<Rgba, 33> &palette;
    // Original texture lookup uses sprite+2; return original width/height.
    std::function<Vec2(int32_t)> textureSize;
    void explode(const HeadExplosion &spec);             // ac::exp 0x4fc30
    void bonus(const HeadBonus &spec, const Ship &ship); // 0x4f318
    void outline(const HeadOutline &spec);               // inline construction in 0x4f518
};
Rgba unpackRgba(uint32_t packed); // colour::unpack 0x9440
} // namespace gr::reconstruction::ios
