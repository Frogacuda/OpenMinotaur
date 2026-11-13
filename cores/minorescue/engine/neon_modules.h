// neon_modules.h - the three light-synth modules Minotron's effect uses:
//   Mixer    (type 2..: feedback/compositing quad; mode 5 is driven by the game)
//   showJoy  (type 500: hosts the game and draws its sprites)
//   cellGrid (type 600: the 8-bit style character display, with gridCell cells)
//
// The cellGrid's text cursor is only switched on by the unreachable script
// interpreter, so its drawing path is omitted.
#pragma once

#include "neon_synth.h"
#include "neon_sprite.h"

namespace NEON_GAME {
namespace Neon_Internals {

class cellGrid;

// --------------------------------------------------------------- Mixer ----

class Mixer : public NeonModule_Base {
  public:
    Mixer();
    int Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) override;
    void Release_Module_Resources() override;
    int Run_Module() override;
    void setDefaults() override;
    void setBasics() override;

    NeonVertex *vptr = nullptr; // 0x1a70
    matrix4f ident;             // 0x1a74
    NeonVertex quad[8];         // 0x1ab8
    int vb = -1;                // 0x1bf8
};

// -------------------------------------------------------------- showJoy ----

class showJoy : public NeonModule_Base {
  public:
    showJoy();
    ~showJoy() override;
    int Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) override;
    void Release_Module_Resources() override;
    int Run_Module() override;
    void setDefaults() override;
    void setBasics() override;

    matrix4f ident; // 0x1fc8
};

// ------------------------------------------------------------- gridCell ----
// Minotaur Rescue's cells are much simpler than Minotron's: no animation
// (run() is empty), no ripples or tumbling. A cell is a sprite (0x8c bytes)
// plus the fields below; the grid sets pos and size each frame.

class gridCell : public sprite {
  public:
    gridCell();
    void draw() override {
        sprite::draw();
    }
    virtual void run() {} // vtable[1] (empty in Rescue)
    // Writes the glyph quad for character `chr` (6 vertices).
    void outch(NeonVertex *out, cellGrid *grid, int chr, const colour &c);
    // Background for attr != 0: the block glyph, or (attr 2-4) a sprite
    // drawn over the same 6 vertices.
    void background(NeonVertex *out, cellGrid *grid);

    double f90 = 0.8, f98 = 0.8; // 0x90, 0x98 (unused)
    int fgOn = 1;                // 0xb0 glyph visible
    int ch = 0x20;               // 0xb4
    int attr = 0;                // 0xb8 background: 0 none, 1 block, 2-4 sprites
    colour bg;                   // 0xbc
    int label = 0;               // 0xdc
};

// ------------------------------------------------------------- cellGrid ----

class cellGrid : public NeonModule_Base {
  public:
    static constexpr int kMaxCells = 129;

    cellGrid();
    ~cellGrid() override;
    int Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) override;
    void Release_Module_Resources() override;
    int Run_Module() override;
    void setDefaults() override;
    void setBasics() override;

    struct CellPos {
        int x, y;
    };

    void setSize(int cols, int rows);
    void scroll(float dx, float dy);
    void cls();
    void cls(int ch);
    void cll(int row);
    void cll(int row, int ch);
    void clla(int row);
    void clattr();
    void ccol(colour c);
    void printAt(int x, int y, const char *s);
    void cPrintAt(int y, const char *s);
    void poke(int x, int y, char c);
    int getLabel(vec2d p);
    int getAttr(vec2d p);
    CellPos getPos(vec2d p);
    void passOverCell(vec2d, colour) {}
    void hlin_a(int attr, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha);
    void hlin_a(int attr, uint32_t c, int x0, int x1, int y, float alpha) {
        hlin_a(attr, c, c, x0, x1, y, alpha);
    }
    // Four-way symmetric lines on the 40x24 screen (mirrored about the centre).
    void qhlin_a(int attr, uint32_t c, int x0, int x1, int y, float alpha);
    void qvlin_a(int attr, uint32_t c, int y0, int y1, int x, float alpha);

    gridCell &cell(int row, int col) {
        return rows[row][col];
    }
    int wrapCol(int c) const {
        return c > cols ? c - cols - 1 : c;
    }
    int wrapRow(int r) const {
        return r > rowsN ? r - rowsN - 1 : r;
    }
    void setLabel(gridCell &g) {
        if (labelMode)
            g.label = curLabel;
    }

    bool labelMode = false;           // 0x1a70
    bool visible = true;              // 0x1a71
    bool shrink = false;              // 0x1a72 (glyph x *= 0.985 each frame)
    int scrollCount[4] = {};          // 0x1a74
    int curLabel = 0;                 // 0x1a84
    int blockChar = 0x7f;             // 0x1a88
    colour printColour;               // 0x1a8c
    matrix4f ident;                   // 0x1aac
    double scrollX = 0, scrollY = 0;  // 0x1aec, 0x1af4
    int colOffset = 0, rowOffset = 0; // 0x1afc, 0x1b00
    double width = 0, height = 0;     // 0x1b04, 0x1b0c
    int cols = 40, rowsN = 24;        // 0x1b14, 0x1b18 (largest index, not count)
    gridCell *rows[kMaxCells] = {};   // 0x1b1c
    int fontY = 0x1b0;                // 0x1d20
    int glyphLiteral = 0;             // 0x1d24
    int glyphW = 16, glyphH = 16;     // 0x1d28, 0x1d2c
    int firstChar = 0x20;             // 0x1d30
};

} // namespace Neon_Internals
} // namespace NEON_GAME
