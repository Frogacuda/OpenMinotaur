// Original mechanisms with explicit behavior choices. No game IDs or hidden
// selected-game state. Modules/adapters select only the rules they use.
#pragma once
#include <cstdint>
namespace neon {
enum class ZeroAngleRule { EvaluateRotation, PreserveCoordinates };
enum class WhiteAlphaRule { Unclamped, ClampUnit };
enum class WashRule { TriangularRainbow, PastelRamp };
enum class SpriteHeightRule { Exact, AddSevenHundredths };
struct PaletteRules {
    WashRule wash;
    uint32_t magentaWhiteMask;
    uint32_t redYellowMask;
};
struct PaletteChannels {
    double r = 1, g = 1, b = 1, a = 1;
};
void rotateClockwise(double &x, double &y, double angle, ZeroAngleRule);
// Unclamped binary32 interpolation, preserving subtract/multiply/add boundaries.
float interpolateFloat(float low, float high, float progress);
double whiteAlpha(double alpha, WhiteAlphaRule);
PaletteChannels paletteColour(int32_t type, float alpha, uint32_t clock, const PaletteRules &);
double normalizedSpriteHeight(double height, double pageHeight, SpriteHeightRule);
} // namespace neon
