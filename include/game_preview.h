// Read-only title previews rendered from imported artwork. These do not run a
// game, change its selection, or imply that a playable core is registered.
#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace mino {
struct PreviewSprite {
    int x, y, w, h;
    float cx, cy, rx, ry;
};
struct GamePreview {
    int columns = 0, rows = 0;
    std::vector<uint16_t> cells;
    std::array<uint32_t, 32> palette{};
    std::vector<PreviewSprite> sprites;
};
GamePreview gridrunnerTitlePreview(bool classic);
} // namespace mino
