// Read-only classification of completed game updates for optional presentation.
#pragma once
#include "ball_flight.h"
namespace mino::deflex_presentation {
inline int reflectionTurn(neon::vec2d before, const deflex::BallFlight &after,
                          const deflex::LevelSetup &level) {
    const bool wasHorizontal = before.x != 0 && before.y == 0;
    const bool wasVertical = before.x == 0 && before.y != 0;
    const bool isHorizontal = after.velocity.x != 0 && after.velocity.y == 0;
    const bool isVertical = after.velocity.x == 0 && after.velocity.y != 0;
    if (!((wasHorizontal && isVertical) || (wasVertical && isHorizontal)) || !after.turnedInCell)
        return 0;
    const auto cell = level.grid.getCellCoords(after.logical);
    int glyph = level.grid.peek(cell.x, cell.y);
    if (glyph != 101 && glyph != 102)
        return 0;
    // Flipping boards changed the cell before the update returned.
    if (level.flippingDeflectors)
        glyph = glyph == 101 ? 102 : 101;
    // Screen Y increases downward: 101 is /, 102 is backslash.
    const bool clockwise = wasHorizontal ? glyph == 102 : glyph == 101;
    return clockwise ? 1 : -1;
}
} // namespace mino::deflex_presentation
