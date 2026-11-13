// Render-side bridge for the common Neon modules. The gameplay translation
// remains separate while the original Gridrunner entity classes are restored.
#pragma once
#include "neon_sprite.h"
#include <functional>
namespace NEON_GAME {
namespace Neon_Internals {
class cellGrid;
class theGame {
  public:
    void _draw(sprite *spr, vec2d size) {
        if (draw)
            draw(spr, size);
    }
    void _bgdraw(sprite *spr, vec2d size) {
        if (background)
            background(spr, size);
    }
    std::function<void(sprite *, vec2d)> draw, background;
    cellGrid *grid = nullptr;
    bool running = true, inputFlipped = false;
    int bgLayer = 0;
    float flash = 0, fbRot = 0, fbX = 0, fbY = 0;
    float fbScaleX = .999f, fbScaleY = .999f;
    float fbColour[4] = {0, 0, 0, 0};
};
} // namespace Neon_Internals
} // namespace NEON_GAME
