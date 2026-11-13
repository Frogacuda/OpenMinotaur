#pragma once
#include <array>
#include <cstdint>
namespace gr::reconstruction::ios {
// Original colour: four doubles followed by twelve floats. The iOS default
// constructor initializes only RGBA; host defaults define the otherwise-unused
// tail to zero rather than retaining indeterminate memory.
struct Rgba {
    double r = 1, g = 1, b = 1, a = 1;
    std::array<float, 3> phase{}, speed{}, amplitude{}, base{};
    void unpack(uint32_t); // iOS 0x9440; changes channels, preserves cycler
    void white(double);    // iOS 0x9998; same preservation, clamped alpha
    void generate(); // named Android colour::generate 0x4e808; no identified iOS gameplay caller
};
static_assert(sizeof(Rgba) == 0x50, "Complete original colour state");
Rgba dynamicColour(int32_t type, float alpha, uint32_t clock); // iOS 0x9068
} // namespace gr::reconstruction::ios
