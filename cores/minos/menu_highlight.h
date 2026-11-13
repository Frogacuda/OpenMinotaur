// Minos's split-gradient buttons can be narrower than their text. Recolour
// the existing artwork only; never create opaque cells behind the overhang.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace minos {
template <class Grid, class Cell> class MenuHighlight {
    using Colour = decltype(Cell::bg);
    struct Saved {
        Cell *cell;
        Colour bg, text, writtenBg, writtenText;
    };
    std::vector<Saved> saved_;

  public:
    void highlight(Grid *grid, int label, unsigned frame, const Colour &cycle) {
        saved_.clear();
        if (!grid || label <= 0)
            return;
        const double pulse = .85 + .15 * std::sin(double(frame) * .15);
        for (int y = 0; y <= grid->rowsN; ++y)
            for (int x = 0; x <= grid->cols; ++x) {
                auto &c = grid->rows[y][x];
                if (c.label != label)
                    continue;
                Saved s{&c, c.bg, c.col, c.bg, c.col};
                if (c.bgOn)
                    std::swap(c.bg.r, c.bg.b);
                // Selected lettering alternates yellow/white above the red gradient.
                // Leave the source colour cycler untouched for unselected labels.
                c.col.b = 1.0 - std::clamp(cycle.g, 0.0, 1.0);
                c.col.r = c.col.g = 1.0;
                c.col.a *= pulse;
                s.writtenBg = c.bg;
                s.writtenText = c.col;
                saved_.push_back(s);
            }
    }
    void restore() {
        for (const auto &s : saved_) {
            if (s.cell->bg.packRGBA() == s.writtenBg.packRGBA())
                s.cell->bg = s.bg;
            if (s.cell->col.packRGBA() == s.writtenText.packRGBA())
                s.cell->col = s.text;
        }
        saved_.clear();
    }
};
} // namespace minos
