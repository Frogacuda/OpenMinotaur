#ifdef NDEBUG
#undef NDEBUG
#endif
#include "neon/foundation_rules.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc, char **argv) {
    using namespace neon;
    assert(argc == 2);
    std::ifstream in(argv[1]);
    assert(in);
    std::string line;
    int rows = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::string recipe;
        int type;
        uint32_t mask, off, on;
        std::istringstream row(line);
        row >> recipe >> type >> std::hex >> mask >> off >> on;
        assert(row);
        const PaletteRules rules = recipe == "pastel"
                                       ? PaletteRules{WashRule::PastelRamp, 3, 8}
                                       : PaletteRules{WashRule::TriangularRainbow, 12, 4};
        for (uint32_t f = 0; f < 256; ++f) {
            const auto c = paletteColour(type, .35f, f, rules);
            const auto rgb = (f & mask) ? off : on;
            assert(c.r == double((rgb >> 16) & 255) / 255 &&
                   c.g == double((rgb >> 8) & 255) / 255 && c.b == double(rgb & 255) / 255);
            assert(c.a == double(.35f));
        }
        ++rows;
    }
    assert(rows == 24);
    constexpr PaletteRules rainbow{WashRule::TriangularRainbow, 12, 4},
        pastel{WashRule::PastelRamp, 3, 8};
    for (uint32_t f = 0; f < 256; ++f) {
        const auto c = paletteColour(6, -.5f, f, rainbow);
        assert(c.a == 1 && c.r + c.g == 1 && c.b == 0 && c.g >= 0 && c.g <= .9375);
    }
    auto c = paletteColour(0, 2, 0, rainbow);
    assert(c.r == .94375 && c.g == .94375 && c.b == .94375 && c.a == 2);
    c = paletteColour(0, -1, 2, pastel);
    assert(c.r == .8125 && c.g == .8 && c.b == .8 && c.a == -1);
    c = paletteColour(14, 0, 255, pastel);
    assert(c.r == 1 && c.g == 1 && c.b == 1 && c.a == 1);
    assert(whiteAlpha(2, WhiteAlphaRule::ClampUnit) == 1 &&
           whiteAlpha(-1, WhiteAlphaRule::ClampUnit) == 0);
    assert(whiteAlpha(2, WhiteAlphaRule::Unclamped) == 2 &&
           whiteAlpha(-1, WhiteAlphaRule::Unclamped) == -1);
    double x = -0., y = 1;
    rotateClockwise(x, y, 0, ZeroAngleRule::PreserveCoordinates);
    assert(std::signbit(x) && y == 1);
    x = -0.;
    rotateClockwise(x, y, 0, ZeroAngleRule::EvaluateRotation);
    assert(!std::signbit(x) && y == 1);
    x = 1;
    y = 2;
    rotateClockwise(x, y, 1.5707963267948966, ZeroAngleRule::EvaluateRotation);
    assert(std::abs(x - 2) < 1e-15 && std::abs(y + 1) < 1e-15);
    assert(normalizedSpriteHeight(16, 512, SpriteHeightRule::Exact) == .03125);
    assert(normalizedSpriteHeight(16, 512, SpriteHeightRule::AddSevenHundredths) == .03138671875);
    std::cout << "Source pulse worksheet: 6144 channel cases; rotation/alpha/UV variants passed\n";
}
