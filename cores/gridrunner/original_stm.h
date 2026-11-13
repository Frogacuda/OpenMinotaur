// Gridrunner's stm character screen, not cellGrid's other-page font.
// iOS STM text routines (0x45ad8/0x45cc0/0x45d14); Android stm::{stringAt,draw,scoreAt}.
#pragma once
#include "neon_sprite.h"
#include "ui_data.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace gridrunner {
class OriginalStm {
  public:
    static constexpr int cols = 28, rows = 36;
    void clear(int value = 0) {
        cells_.fill(value);
    }
    void poke(int x, int y, int value) {
        if (x >= 0 && x <= cols && y >= 0 && y <= rows)
            cells_[size_t(y * (cols + 1) + x)] = value;
    }
    int peek(int x, int y) const {
        return x >= 0 && x <= cols && y >= 0 && y <= rows ? cells_[size_t(y * (cols + 1) + x)] : 0;
    }
    void line(int y, int value = 0) {
        for (int x = 0; x <= cols; ++x)
            poke(x, y, value);
    }
    void hline(int x0, int x1, int y, int value) {
        if (x0 > x1)
            std::swap(x0, x1);
        for (int x = x0; x <= x1; ++x)
            poke(x, y, value);
    }
    void vline(int x, int y0, int y1, int value) {
        if (y0 > y1)
            std::swap(y0, y1);
        for (int y = y0; y <= y1; ++y)
            poke(x, y, value);
    }
    void stringAt(int x, int y, const std::string &text, int base = 0x76) {
        int pal = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char c = text[i];
            if ((c == '^' || c == '_') && i + 2 < text.size()) {
                pal = int(static_cast<unsigned char>(text[++i])) - (c == '^' ? '0' : ' ');
                c = static_cast<unsigned char>(text[++i]);
            }
            poke(x++, y, c == ' ' ? 0x76 : (int(c) + base - 0x20) | (pal << 8));
        }
    }
    void scoreAt(int x, int y, int score) {
        char text[32];
        std::snprintf(text, sizeof text, "%8.2d", score);
        for (int i = 0; i < 8; ++i)
            poke(x + i, y, (int(text[i]) + 0x56) | 0x200);
    }
    void livesAt(int x, int y, int lives) {
        char text[32];
        std::snprintf(text, sizeof text, "%3.2d", lives);
        for (int i = 0; i < 3; ++i)
            poke(x + i, y, (int(text[i]) + 0x56) | 0x200);
    }
    void draw(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
              uint32_t gridColour = data::acPalette[15]) const;
    void drawOverlay(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
                     uint32_t gridColour = data::acPalette[15]) const;

  private:
    void drawCells(Neon_Internals::sprite *spr, Neon_Internals::vec2d size, int frame,
                   uint32_t gridColour, bool overlay) const;
    std::array<int, (cols + 1) * (rows + 1)> cells_{};
};
} // namespace gridrunner
