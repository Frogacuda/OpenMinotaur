#include "neon/foundation_rules.h"
#include <cmath>
namespace neon {
float interpolateFloat(float low, float high, float progress) {
    const float difference = high - low;
    const float product = difference * progress;
    return product + low;
}
void rotateClockwise(double &x, double &y, double angle, ZeroAngleRule rule) {
    if (rule == ZeroAngleRule::PreserveCoordinates && angle == 0)
        return;
    const double s = std::sin(angle), c = std::cos(angle), oldX = x;
    x = s * y + c * oldX;
    y = y * c - oldX * s;
}
double whiteAlpha(double alpha, WhiteAlphaRule rule) {
    if (rule == WhiteAlphaRule::Unclamped)
        return alpha;
    return alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
}
PaletteChannels paletteColour(int32_t type, float alpha, uint32_t f, const PaletteRules &rules) {
    PaletteChannels c;
    const auto set = [&](double r, double g, double b) { c = {r, g, b, double(alpha)}; };
    const auto tri = [](int32_t n) { return float(n < 0 ? -n : n) * .0625f; };
    switch (type) {
    case 0:
        if (rules.wash == WashRule::PastelRamp)
            set(double(float((f >> 1) & 15) * .0625f) * .2 + .8,
                double(float((f >> 3) & 15) * .0625f) * .1 + .8,
                double(float((f >> 5) & 15) * .0625f) * .2 + .8);
        else
            set(double(tri(int32_t(f & 31) - 15)) * .9 + .1,
                double(tri(int32_t((f & 63) >> 1) - 15)) * .9 + .1,
                double(tri(int32_t((f & 15) * 2) - 15)) * .9 + .1);
        break;
    case 1:
        set(1, (f & 3) ? 0 : 1, 0);
        break;
    case 2:
        set((f & 3) ? 0 : 1, 0, 1);
        break;
    case 3:
        set((f & 3) ? 0 : 1, 1, 0);
        break;
    case 4:
        set(1, (f & rules.magentaWhiteMask) ? 0 : 1, 1);
        break;
    case 5:
        set(0, 1, (f & 24) ? 0 : 1);
        break;
    case 6: {
        const float ramp = float((f & 31) >> 1) * .0625f;
        set(double(1.f - ramp), double(ramp), 0);
        c.a = 1;
        break;
    }
    case 7:
        set((f & 6) ? 0 : 1, 1, 0);
        break;
    case 8:
        set(1, (f & 14) ? 0 : 1, 0);
        break;
    case 9:
        if (f & 14)
            set(0, 0, 1);
        else
            set(1, 1, 0);
        break;
    case 10:
        set(1, (f & rules.redYellowMask) ? 0 : 1, 0);
        break;
    case 11:
        if (f & 8)
            set(1, 1, 0);
        else
            set(0, 0, 1);
        break;
    case 12:
        set((f & 8) ? 1 : 0, 1, 0);
        break;
    case 13:
        if (f & 8)
            set(1, 1, 0);
        else
            set(1, 0, 1);
        break;
    default:
        break;
    }
    return c;
}
double normalizedSpriteHeight(double height, double pageHeight, SpriteHeightRule rule) {
    const double bias = rule == SpriteHeightRule::AddSevenHundredths ? .07 : 0.;
    return (height + bias) / pageHeight;
}
} // namespace neon
