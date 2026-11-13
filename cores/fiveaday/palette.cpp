#include "world.h"
#include "original_tables.h"
#include <stdexcept>
namespace fiveaday {
Colour World::palette(int index) const {
    constexpr int dynamic[] = {-1, 8, 5, 0, 4, 2};
    if (index < 0 || index >= 6)
        throw std::out_of_range("Five a Day palette index");
    if (index)
        return dynamicColour(dynamic[index], ticks);
    // theGame::run -> getColFromSequence -> iclerp. The first game-facing
    // palette is an Atari colour sequence; it is not dynamic colour type 0.
    const float phase = float(float((ticks & 31) << 1) / 63.f);
    const double scaled = double(phase) * (double(float(original::AtariSequence[0])) + .99);
    const auto slot = size_t(int(scaled));
    const double t = double(float(float(scaled) - float(slot)));
    const auto a = unpack(original::AtariSequence.at(slot + 1)),
               b = unpack(original::AtariSequence.at(slot + 2));
    const auto byte = [&](double x, double y) {
        return double(int((x + (y - x) * t) * 255.)) / 255.;
    };
    return {byte(a.r, b.r), byte(a.g, b.g), byte(a.b, b.b), byte(a.a, b.a)};
}
} // namespace fiveaday
