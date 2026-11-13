// cell_grid_addressing.cpp - see cell_grid_addressing.h. Arithmetic follows the original
// instructions: where the original rounds to float (vdiv.f32, vmul.f32) the float is
// kept explicitly, and multiply-adds are separate (vmla.f64 is not fused).
#include "cell_grid_addressing.h"
#include "neon/grid_storage.h"

namespace deflex {

void CellGrid::setSize(int c, int r) {
    // cellGrid::setSize 0x474d0 clamps both to 1..128 (columns: <=0 -> 1, >=128 -> 128;
    // rows: <=0 -> 1, >128 -> 128), resets the scroll offsets and finally sets every
    // cell's character to a space, label to curLabel when labelMode is on (else 0) and
    // fgOn to 0. Only those fields are modelled here.
    neon::grid::clampDimensions(c, r);
    cols = c;
    rows = r;
    colOffset = rowOffset = 0;
    cells_.assign(size_t(rows) + 1, std::vector<GridCell>(size_t(cols) + 1));
    for (auto &row : cells_)
        for (GridCell &g : row) {
            g.ch = 0x20;
            g.label = labelMode ? curLabel : 0;
            g.fgOn = 0;
        }
}

// Centre of cell (x, y): x / cols in float, then in double -0.5 + half a cell.
vec2d CellGrid::getPos(int x, int y) const {
    vec2d p;
    const float fx = float(x) / float(cols);
    p.x = (double(fx) - 0.5) + (1.0 / double(cols)) * 0.5;
    const float fy = float(y) / float(rows);
    p.y = (double(fy) - 0.5) + (1.0 / double(rows)) * 0.5;
    return p;
}

// Cell under p, truncating toward zero (a position just left of the grid gives 0).
CellCoords CellGrid::getCellCoords(vec2d p) const {
    return {int((p.x + 0.5) * double(cols)), int((p.y + 0.5) * double(rows))};
}

// As getCellCoords but with 0.5 added before truncation: the nearest cell boundary
// (mainGameBall uses it alongside getCellCoords).
CellCoords CellGrid::getCellCoords2(vec2d p) const {
    return {int(0.5 + (p.x + 0.5) * double(cols)), int(0.5 + (p.y + 0.5) * double(rows))};
}

// Offset of p from the centre of its cell (screen units). The cell width 1/cols and the
// cell's left edge are float products; the subtraction is done in double.
vec2d CellGrid::getPosInCell(vec2d p) const {
    const float cellW = 1.0f / float(cols);
    const float cellH = 1.0f / float(rows);
    const double ux = p.x + 0.5, uy = p.y + 0.5;
    const int cx = int(ux * double(cols));
    const int cy = int(uy * double(rows));
    vec2d r;
    r.x = (ux - double(float(cx) * cellW)) + double(cellW * -0.5f);
    r.y = (uy - double(float(cy) * cellH)) + double(cellH * -0.5f);
    return r;
}

int CellGrid::peek(int x, int y) const {
    if (x < 0 || x > cols || y < 0 || y > rows)
        return kOutside;
    return cell(wrapRow(y + rowOffset), wrapCol(x + colOffset)).ch;
}

void CellGrid::poke(int x, int y, char c) {
    GridCell &g = cell(wrapRow(y + rowOffset), wrapCol(x + colOffset));
    // sxtb: preserve signed-byte interpretation explicitly for this binding.
    neon::grid::writeCharacter(g, int(static_cast<signed char>(c)), labelMode, curLabel);
}

} // namespace deflex
