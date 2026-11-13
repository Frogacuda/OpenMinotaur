// Deflex's typed character console and all five original machine presets.
#pragma once
#include "grid_view.h"
#include <vector>
namespace deflex {
class Console {
    std::string script_;
    size_t at_ = 0;
    struct Loop {
        size_t at;
        int count;
    };
    std::vector<Loop> loops_;
    int delay_ = 0, wait_ = 0;
    bool waitRelease_ = false;
    int number();
    void newline();
    void put(unsigned char);
    void rectangle();

  public:
    enum Mode { Vic = 0, C64 = 1, Spectrum = 2, Atari8 = 3, Pet = 4 };
    CellGrid &grid;
    GridView &view;
    neon::DarwinRandom &random;
    std::array<neon::colour, 16> palette;
    int mode = Pet, left = 2, right = 41, top = 2, bottom = 25, x = 2, y = 2;
    int foreground = 7, background = 1, fill = 1, border = 1, delayReload = 0;
    float scale = 1, decay = 1;
    bool cursor = true;
    Console(CellGrid &g, GridView &v, neon::DarwinRandom &r) : grid(g), view(v), random(r) {}
    void setMode(int);
    void clear();
    void borderClear();
    void smash(neon::DarwinRandom &);
    void poke(int x, int y, int glyph, uint32_t rgba);
    void start(const std::string &script) {
        script_ = script;
        at_ = 0;
        wait_ = 0;
        loops_.clear();
    }
    bool busy() const {
        return !script_.empty();
    }
    void run(bool held = false, bool immediate = false);
};
} // namespace deflex
