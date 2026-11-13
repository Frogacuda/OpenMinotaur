#pragma once
#include <type_traits>
#include <utility>
#include <cstdint>

namespace neon::grid {
// Circular character-grid storage shared by the early bindings. Dimensions are
// inclusive largest indices, not element counts. Allocation, rendering, text
// parsing, animation and later row-displacement metadata stay with the binding.
// Callers supply finite scroll deltas, valid offsets/rows and allocated cells.
template <class Grid> void scrollStorage(Grid &grid, float dx, float dy) {
    grid.scrollX += double(dx);
    grid.scrollY += double(dy);
    // Preserve both directional counters and the original double-precision
    // step order. A modulo/floor shortcut changes subnormal boundary behavior.
    while (grid.scrollX < 0.0) {
        grid.scrollCount[0]++;
        grid.scrollX += 1.0;
        if (--grid.colOffset < 0)
            grid.colOffset = grid.cols;
    }
    while (grid.scrollX >= 1.0) {
        if (++grid.colOffset > grid.cols)
            grid.colOffset = 0;
        grid.scrollCount[1]++;
        grid.scrollX -= 1.0;
    }
    while (grid.scrollY < 0.0) {
        grid.scrollCount[2]++;
        if (--grid.rowOffset < 0)
            grid.rowOffset = grid.rowsN;
        grid.scrollY += 1.0;
    }
    while (grid.scrollY >= 1.0) {
        if (++grid.rowOffset > grid.rowsN)
            grid.rowOffset = 0;
        grid.scrollCount[3]++;
        grid.scrollY -= 1.0;
    }
}

template <class Grid> void clearCharacters(Grid &grid, int character) {
    for (int y = 0; y <= grid.rowsN; ++y)
        for (int x = 0; x <= grid.cols; ++x) {
            auto &cell = grid.rows[y][x];
            cell.ch = character;
            cell.label = grid.labelMode ? grid.curLabel : 0;
            cell.fgOn = character != 0x20;
        }
    grid.printColour.white(1.0);
    grid.rowOffset = grid.colOffset = 0;
    grid.scrollX = grid.scrollY = 0;
}

template <class Grid> void clearCharacterRow(Grid &grid, int row, int character) {
    const int physicalRow = grid.wrapRow(row + grid.rowOffset);
    for (int x = 0; x <= grid.cols; ++x) {
        auto &cell = grid.rows[physicalRow][x];
        cell.ch = character;
        // Row clearing preserves old labels when label mode is disabled;
        // whole-screen clearing above resets them. Do not combine these rules.
        if (grid.labelMode)
            cell.label = grid.curLabel;
        cell.fgOn = character != 0x20;
    }
}

// The caller explicitly selects the field: Rescue attributes and animated-cell
// background enable flags are different state, despite similar clear operations.
template <class Grid, class Member> void clearFieldRow(Grid &grid, int row, Member member) {
    const int physicalRow = grid.wrapRow(row + grid.rowOffset);
    for (int x = 0; x <= grid.cols; ++x)
        grid.rows[physicalRow][x].*member = 0;
}

template <class Grid, class Member> void clearField(Grid &grid, Member member) {
    for (int y = 0; y <= grid.rowsN; ++y)
        for (int x = 0; x <= grid.cols; ++x)
            grid.rows[y][x].*member = 0;
}

// Allocation policy is separate from generation-specific row metadata and
// proportional-line ownership. Keep those hooks at their original binding stage.
template <class Grid> void releaseRows(Grid &grid) {
    for (auto *&row : grid.rows) {
        delete[] row;
        row = nullptr;
    }
}
inline void clampDimensions(int &columns, int &rows) {
    if (columns < 1)
        columns = 1;
    else if (columns > 127)
        columns = 128;
    if (rows < 1)
        rows = 1;
    else if (rows > 128)
        rows = 128;
}
template <class Grid, class RowInit, class InitialCharacter>
void allocateRows(Grid &grid, RowInit initRow, InitialCharacter initialCharacter) {
    using Cell = std::remove_reference_t<decltype(*grid.rows[0])>;
    const double ch = grid.height / double(grid.rowsN), cw = grid.width / double(grid.cols);
    for (int y = 0; y <= grid.rowsN; ++y) {
        grid.rows[y] = new Cell[Grid::kMaxCells];
        initRow(y);
        for (int x = 0; x <= grid.cols; ++x) {
            auto &cell = grid.rows[y][x];
            cell.width = cw;
            cell.height = ch;
            cell.col.white(1.0);
            cell.ch = initialCharacter(x);
        }
    }
}
template <class Grid, class Member> void resetStorage(Grid &grid, Member background) {
    grid.scrollX = grid.scrollY = 0;
    grid.colOffset = grid.rowOffset = 0;
    for (int &count : grid.scrollCount)
        count = 0;
    clearCharacters(grid, 0x20);
    clearField(grid, background);
}

// Colour control tokens consume four following characters using float division,
// promoted to the colour's channel type. The established trailing-token guard is
// retained; incomplete tokens and strings longer than the caller's row capacity
// remain outside this API's original valid-input contract.
inline int visibleColumns(const char *text) {
    int i = 0, n = 0;
    while (text[i]) {
        if (text[i] == '~')
            i += 5;
        else {
            ++i;
            ++n;
        }
    }
    return n;
}
template <class Grid, class WriteCharacter>
void printCharacters(Grid &grid, int x, int y, const char *text, WriteCharacter writeCharacter) {
    if (!text)
        return;
    int cx = grid.wrapCol(x + grid.colOffset), row = grid.wrapRow(y + grid.rowOffset), i = 0;
    while (text[i]) {
        const int column = cx > grid.cols ? cx - grid.cols - 1 : cx;
        if (text[i] == '~') {
            grid.printColour.r = double(float(text[i + 1] - '0') / 9.0f);
            grid.printColour.g = double(float(text[i + 2] - '0') / 9.0f);
            grid.printColour.b = double(float(text[i + 3] - '0') / 9.0f);
            grid.printColour.a = double(float(text[i + 4] - '0') / 9.0f);
            i += 5;
            if (!text[i])
                break;
        }
        auto &cell = grid.rows[row][column];
        writeCharacter(cell, text[i]);
        cell.col = grid.printColour;
        if (grid.labelMode)
            cell.label = grid.curLabel;
        cell.fgOn = text[i] != ' ';
        ++cx;
        ++i;
    }
}
template <class Cell> void writeCharacter(Cell &cell, int character, bool labels, int label) {
    cell.ch = character;
    if (labels)
        cell.label = label;
    cell.fgOn = character != 0x20;
}
template <class Grid> void pokeCharacter(Grid &grid, int x, int y, char character) {
    auto &cell = grid.rows[grid.wrapRow(y + grid.rowOffset)][grid.wrapCol(x + grid.colOffset)];
    writeCharacter(cell, int(character), grid.labelMode, grid.curLabel);
}
template <class Grid>
void pokeColouredCharacter(Grid &grid, int x, int y, char character, uint32_t rgba) {
    auto &cell = grid.rows[grid.wrapRow(y + grid.rowOffset)][grid.wrapCol(x + grid.colOffset)];
    cell.ch = int(character);
    cell.scale = 1.0f;
    cell.decay = 1.0f;
    cell.col.unpackRGBA(rgba);
    if (grid.labelMode)
        cell.label = grid.curLabel;
    cell.fgOn = character != 0x20;
}
template <class Grid, class Point>
std::pair<int, int> addressedCell(const Grid &grid, Point point) {
    int x = int((point.x + 0.5) * double(float(grid.cols))) + grid.colOffset;
    int y = int((point.y + 0.5) * double(float(grid.rowsN))) + grid.rowOffset;
    if (x > grid.cols)
        x = x - grid.cols - 1;
    if (y > grid.rowsN)
        y = y - grid.rowsN - 1;
    return {x, y};
}
template <class Grid, class Member>
void horizontalFieldLine(Grid &grid, Member background, int value, uint32_t c0, uint32_t c1, int x0,
                         int x1, int y, float alpha) {
    const int n = x1 - x0;
    const float step = 1.0f / float(n);
    const int sx = grid.wrapCol(x0 + grid.colOffset), row = grid.wrapRow(y + grid.rowOffset);
    using Colour = std::remove_reference_t<decltype(grid.printColour)>;
    Colour a, b;
    a.unpackRGBA(c0);
    b.unpackRGBA(c1);
    float t = 0.0f;
    if (n + 1 <= 0)
        return;
    for (int i = 0;; ++i) {
        int x = i + sx;
        if (x > grid.cols)
            x = 0;
        auto &cell = grid.rows[row][x];
        if (grid.labelMode)
            cell.label = grid.curLabel;
        cell.*background = value;
        cell.bg = a.lerp(b, t) * alpha;
        if (i == n)
            break;
        t += step;
    }
}

} // namespace neon::grid
