#pragma once
#include <algorithm>
namespace mino {
// Tube axes follow the virtual screen's long edge, never the game's rotation or the
// playfield's aspect. Canonical tube X is the beam's horizontal sweep direction.
struct TubeLayout {
    bool clockwise = false;
    int width = 1, height = 1, inputTurns = 0, sourceWidth = 480, sourceHeight = 320;
};
inline TubeLayout tubeLayout(int viewW, int viewH, int gameTurns, int sourceW = 480,
                             int sourceH = 320) {
    const bool clockwise = viewH > viewW;
    const int turns = gameTurns - int(clockwise);
    const bool transpose = turns % 2 != 0;
    return {clockwise,
            std::max(1, clockwise ? viewH : viewW),
            std::max(1, clockwise ? viewW : viewH),
            turns,
            std::max(1, transpose ? sourceH : sourceW),
            std::max(1, transpose ? sourceW : sourceH)};
}
} // namespace mino
