// cell_grid_addressing.h - Deflex playfield addressing: the cellGrid methods the game
// entities use to convert between screen positions and playfield cells, and to read and
// write a cell's character.
//
// In Deflex the playfield is the cellGrid. The addressing
// foundation below comes from Deflex_armv7 (SHA-256 e0da6984...67cb21):
//
//   getPos          0x42d90   cell -> centre position          (theGame::newLevel,
//   mainGameBall::init) getCellCoords   0x42e00   position -> cell, truncating
//   (mainGameBall/hazard/numberedTarget::run) getCellCoords2  0x42e5c   position -> cell, +0.5
//   rounding  (mainGameBall::run) getPosInCell    0x4543c   position -> offset from cell
//   (mainGameBall::run) peek            0x435fc   cell character, 999 outside
//   (mainGameBall/hazard/numberedTarget) poke(x,y,c)     0x43554   set cell character
//   (mainGameBall::run)
//
// Positions are screen units, -0.5..0.5 across the grid. `cols`/`rows` are the values
// passed to cellGrid::setSize: the largest cell index, so a row has cols + 1 cells.
// Position <-> cell conversions ignore the scroll offsets; peek/poke apply them.
// Cell effects and gameplay share this store; rendering and script execution
// are provided by their respective game components.
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include "neon/math.h"
#include "neon/source_random.h"

namespace deflex {

using vec2d = neon::vec2d;
struct CellCoords {
    int x = 0, y = 0;
}; // returned by value (8 bytes) by getCellCoords*

// Named cell state shared by addressing and effects. run/smash reuse the
// instruction-identical Minotron mechanisms; drawing remains module-specific.
struct GridCell {
    // Object offsets are four bytes below offsets measured from a row allocation:
    // the original row has a four-byte new[] cookie before the first gridCell.
    int ch = 0x20; // object +258 / first row cell +25c; setSize writes space
    int fgOn = 0;  // object +244 / first row cell +248; setSize writes zero
    int label = 0; // object +40c / first row cell +410
    int bgOn = 0;
    int fgState = -1, bgState = -1;
    unsigned fadeFlags = 0;
    float scale = 1, decay = 1;
    float fgT = 0, fgSpeed = 0, bgT = 0, bgSpeed = 0;
    float fgAngle = 0, bgAngle = 0; // constructor +15c/+160; independent atlas spins
    int fgIcon = 0, bgIcon = 0;
    unsigned attr = 0;  // +e8: bit 0 atlas background, bit 1 atlas foreground
    int durability = 0; // +408; newLevel copies +d04, theGame::blow decrements
    const int *fgSeq = nullptr;
    const int *bgSeq = nullptr;
    neon::colour col, bg, fgFrom, bgFrom, fadeTo, bgTo;
    neon::vec3d rot, rotVel, offset, vel;

    // These are the original cell effects, not a substitute entity model.
    // Unwritten timing/motion fields above start at host-safe zero; a state
    // setter must supply them before use.
    void setFSequence(float rate, const int *sequence);
    void setBSequence(float rate, const int *sequence);
    void setBCC(float rate, uint32_t rgba);
    void blendBCC(float rate, uint32_t rgba, float amount);
    void dingDeflector();
    void smash(neon::DarwinRandom &random);
    void run(const neon::colour *palette);
};

class CellGrid {
  public:
    static constexpr int kOutside = 999; // peek result outside 0..cols x 0..rows

    // Shapes the cell store as setSize(cols, rows) does for the fields above.
    void setSize(int cols, int rows);

    vec2d getPos(int x, int y) const;
    CellCoords getCellCoords(vec2d p) const;
    CellCoords getCellCoords2(vec2d p) const;
    vec2d getPosInCell(vec2d p) const;
    int peek(int x, int y) const;
    // No bounds check in the original: x must lie in 0..cols and y in 0..rows (after
    // the scroll offset the index wraps once). Callers pass cells they have peeked.
    void poke(int x, int y, char c);

    void blendRadius(float radius, vec2d centre, float rate, uint32_t rgba, float amount);
    void fadeLines(int first, int last);
    void doDisplace(neon::vec3d &position) const;

    // Original effects address physical rows directly, unlike peek/poke's
    // scrolling view. Populated by the game presentation binding.
    bool radiusBlendDisabled = false; // cellGrid +2a69
    struct MessageLine {
        int state = 0, count = 0;
    }; // original proportionalLine +98/+9c
    std::array<MessageLine, 129> messageLines{};
    struct Displacement {
        vec2d centre;
        double strength = 0;
    };
    std::array<Displacement, 5> displacement{}; // cellGrid +2a70, step 24 bytes

    GridCell &cell(int row, int col) {
        return cells_[size_t(row)][size_t(col)];
    }
    const GridCell &cell(int row, int col) const {
        return cells_[size_t(row)][size_t(col)];
    }

    // cellGrid fields (Deflex offsets; corresponding Minotron offset is +0x508)
    int colOffset = 0, rowOffset = 0; // +0x2624, +0x2628 scroll offsets in cells
    int cols = 40, rows = 24;         // +0x263c, +0x2640 largest column / row index
    bool labelMode = false;           // +0x2478 (read as signed byte, non-zero = on)
    int curLabel = 0;                 // +0x257c

  private:
    int wrapCol(int c) const {
        return c > cols ? c - cols - 1 : c;
    }
    int wrapRow(int r) const {
        return r > rows ? r - rows - 1 : r;
    }
    std::vector<std::vector<GridCell>> cells_; // +0x2644 row pointers, 0x420-byte cells
};

} // namespace deflex
