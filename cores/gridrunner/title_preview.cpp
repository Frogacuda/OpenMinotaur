#include "game_preview.h"
#include "original_frontend.h"
#include "../../reconstruction/gridrunner/native_classic.h"
#include "text_menus.h"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace mino {
GamePreview gridrunnerTitlePreview(bool classic) {
    GamePreview p;
    if (classic) {
        gr::reconstruction::ios::Classic game;
        const auto &screen = game.screen();
        p.columns = screen.cols;
        p.rows = screen.rows;
        p.cells.assign(screen.cells.begin(), screen.cells.end());
        std::copy(std::begin(screen.palette), std::end(screen.palette), p.palette.begin());
    } else {
        gridrunner::OriginalFrontend frontend;
        for (int frame = 0; frame < 360; ++frame)
            frontend.tick(0, 0, 1, gridrunner::GameMode::Pure);
        p.columns = gridrunner::OriginalStm::cols;
        p.rows = gridrunner::OriginalStm::rows;
        for (int y = 0; y <= p.rows; ++y)
            for (int x = 0; x <= p.columns; ++x)
                p.cells.push_back(uint16_t(frontend.screen().peek(x, y)));
        std::copy(std::begin(gridrunner::data::acPalette), std::end(gridrunner::data::acPalette),
                  p.palette.begin());
        for (int i = 16; i < 24; ++i)
            p.palette[size_t(i)] = gridrunner::data::flashes[(i - 16) * 16 + 8];
        // Static title artwork rectangles from the original resource atlas.
        p.sprites = {{120, 2, 98, 15, 0, .4f, .1f, .1f * 15 / 98},
                     {55, 106, 128, 20, -.3f, -.2f, .03f * 128 / 20, .03f},
                     {55, 126, 192, 20, .2f, -.2f, .03f * 192 / 20, .03f}};
    }
    return p;
}
} // namespace mino
