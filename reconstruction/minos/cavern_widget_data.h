#pragma once
#include "cavern_data.h"
#include <array>

namespace minos::cavern {
struct WidgetRoute {
    int rescues;
    int surfaceRow;
    int goalRow;
};

// Count the M cells and locate the S and G cells with StripMap::getLine.
// The map checks verify this table against the cavern layouts.
inline constexpr std::array<WidgetRoute, kCavernCount> kWidgetRoutes{{
    {8, 24, 117},  {14, 16, 159}, {10, 24, 263}, {13, 24, 215}, {10, 24, 282}, {16, 24, 287},
    {13, 24, 287}, {19, 24, 431}, {17, 24, 335}, {31, 24, 167}, {10, 24, 359}, {13, 24, 311},
    {14, 24, 311}, {13, 24, 335}, {10, 24, 215}, {9, 24, 330},  {35, 24, 503}, {33, 24, 311},
    {30, 24, 287}, {64, 24, 359}, {74, 24, 887},
}};
} // namespace minos::cavern
