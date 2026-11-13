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

class gridCell : public sprite {
  public:
    gridCell();
    void draw() override {
        sprite::draw();
    }
    virtual void run(); // vtable[1]
    void smash();
    int outch(NeonVertex *out, cellGrid *grid, unsigned ch, uint32_t colour);

    int attr = 0;               // 0xe8
    unsigned fadeFlags = 0;     // 0xec bit 0: bg pending, bit 1: bg fade
    cellGrid *owner = nullptr;  // 0xf0
    double f4 = 1.0, fc = 1.0;  // 0xf4, 0xfc
    vec3d rot;                  // 0x114 (x, y, z angles)
    vec3d rotVel;               // 0x12c
    vec3d offset;               // 0x144
    float fgAngle = 0;          // 0x15c
    float bgAngle = 0;          // 0x160
    vec3d vel;                  // 0x164
    float baseZ = 610.0f;       // 0x17c
    matrix4f mrx, mry, mrz;     // 0x184, 0x1c4, 0x204
    int fgOn = 1;               // 0x244
    int fgState = -1;           // 0x248
    int bgState = -1;           // 0x24c
    float scale = 1.0f;         // 0x250
    float decay = 1.0f;         // 0x254
    int ch = 0;                 // 0x258
    int bgOn = 0;               // 0x25c
    float fgT = 0, fgSpeed = 0; // 0x260, 0x264
    const int *fgSeq = nullptr; // 0x268
    float bgT = 0, bgSpeed = 0; // 0x26c, 0x270
    const int *bgSeq = nullptr; // 0x274
    colour bg;                  // 0x278
    colour fgFrom;              // 0x2c8
    colour bgFrom;              // 0x318
    colour fadeTo;              // 0x368
    colour bgTo;                // 0x3b8
    int label = 0;              // 0x40c
    int fgIcon = 0;             // 0x410
    int bgIcon = 0;             // 0x414
    int pass = 0;               // 0x418 0 bg, 1 fg, -1 cursor
    int8_t special = 0;         // 0x41c
};

// ------------------------------------------------------------- cellGrid ----

#ifdef MINO_PROPORTIONAL_TEXT
struct proportionalLine {
    void update();                 // proportionalLine::update 0x43398
    vec2d pos;                     // 0x00 current position (x 512 in propLine)
    vec2d scale{1.0, 1.8};         // 0x10 settled scale
    vec2d to;                      // 0x20 settled position
    vec2d from;                    // 0x30 start of the way in
    vec2d away;                    // 0x40 end of the way out
    float inCurve = 1.0f;          // 0x50 interpolate(curve, spacing, t) on the way in
    float outCurve = 1.0f;         // 0x54 ... on the way out
    vec2d inScale{1.0, 1.0};       // 0x58 scale multiplier at t = 0 coming in
    vec2d outScale{1.0, 1.0};      // 0x68 ... going out
    vec2d curScale;                // 0x78
    float t = 0;                   // 0x88
    float inRate = 0.01f;          // 0x8c
    float outRate = 0.02f;         // 0x90
    int hold = -1;                 // 0x94 frames held (-1: until cleared)
    int state = 1;                 // 0x98 0 in, 1 hold, 2 out
    const char *text = nullptr;    // 0x9c
    float spacing = 1.0f;          // 0xa0 letter spacing when settled
    float curSpacing = 0;          // 0xa4
    const uint32_t *seq = nullptr; // 0xa8 colour sequence (defaultCSeq)
    int seqLen = 15;               // 0xac
    uint32_t rgba = 0xffffffffu;   // 0xb0 colour without a sequence
    int align = 1;                 // 0xb4 1 centred, 3 right
    int font = -1;                 // 0xb8 < 0 proportional, else the monospaced font's row base
    float phaseRate = 1.0f;        // 0xc0 colour phase per frame
    float charStep = 0;            // 0xc4 colour phase per character
    float alpha = 1.0f;            // 0xc8
    float phase = 0;               // 0xcc
};

#endif
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

    void setSize(int cols, int rows);
    void scroll(float dx, float dy);
    void cls();
    void cls(int ch);
    void cll(int row);
    void cll(int row, int ch);
    void clla(int row);
    void clattr();
    void cllabel();
    void printAt(int x, int y, const char *s);
    void cPrintAt(int y, const char *s);
    void cPrintAt(int y, const char *s, uint32_t rgba);
    void poke(int x, int y, char c);
    void poke(int x, int y, char c, uint32_t rgba);
    void lpoke(int x, int y, int label);
    int getLabel(vec2d p);
    void hlin_a(int bg, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha);
    void hlin_a(int bg, uint32_t c, int x0, int x1, int y, float alpha) {
        hlin_a(bg, c, c, x0, x1, y, alpha);
    }
    // Not ported (unreachable in Minotron): doInput's typing-script interpreter
    // and the machine boot screens it can start (goVic/goA8/goPet/go64/goSpec,
    // borderCLS), and the per-row proportional-font lines (propLine, rawGlyph,
    // proportionalLine::update) whose text is never set. Five a Day opts into
    // the reconstructed proportional-line path below; other bindings retain this layout.

#ifdef MINO_PROPORTIONAL_TEXT
    void QuickMsg(const char *, vec2d, vec2d, int, int, int, float, float);
    void clm();
    void rawGlyph(sprite *, int, int, int, int, uint32_t);
    void propLine(sprite *, proportionalLine *);
    proportionalLine *lines[kMaxCells] = {};
#endif
    gridCell &cell(int row, int col) {
        return rows[row][col];
    }
    int wrapCol(int c) const {
        return c > cols ? c - cols - 1 : c;
    }
    int wrapRow(int r) const {
        return r > rowsN ? r - rowsN - 1 : r;
    }

    colour palette[16];     // 0x1a70
    colour palette2[16];    // 0x1f70
    int labelChar[64] = {}; // 0x2474
    vec2d labelPos[64];     // 0x2574
    int labelCount = 0;     // 0x2974
    bool labelMode = false; // 0x297c
    bool straightAlphaText =
        false;               // Source game-selected text blend; default retains existing callers.
    bool visible = true;     // 0x297d
    bool f297e = false;      // 0x297e
    bool cursorOn = false;   // 0x297f
    bool cursorPhase = true; // 0x2980
    bool hashMode = false;   // 0x2981
    int maxChar = 0x7f;      // 0x2984
    float typeDecay = 1.0f;  // 0x2988
    float typeScale = 1.0f;  // 0x298c
    const char *script = nullptr;     // 0x2990
    int scriptPos = 0;                // 0x2994
    int machine = 4;                  // 0x2998
    int machineMax = 4;               // 0x299c
    int typeDelay = 0;                // 0x29a0
    int typeDelayReload = 5;          // 0x29a4
    int typeWait = 50;                // 0x29a8
    int typeWait2 = 50;               // 0x29ac
    int winLeft = 2, winRight = 0;    // 0x29b0, 0x29b4
    int winTop = 0, winBottom = 0;    // 0x29b8, 0x29bc
    int blink = 30;                   // 0x29c0
    int blinkReload = 30;             // 0x29c4
    int curX = 0, curY = 0;           // 0x29c8, 0x29cc
    int f29d0 = 7, f29d4 = 1;         // 0x29d0, 0x29d4
    int fgIndex = 0;                  // 0x29dc
    int bgIndex = -1;                 // 0x29e0
    int fillIndex = -1;               // 0x29e4
    int borderIndex = 0;              // 0x29e8
    int cursorChar = 0x7f;            // 0x29ec
    int scrollCount[4] = {};          // 0x29f0
    int loopPos[16] = {};             // 0x2a00
    int loopCount[16] = {};           // 0x2a40
    int loopDepth = 0;                // 0x2a80
    int curLabel = 0;                 // 0x2a84
    int blockChar = 0x7f;             // 0x2a88
    colour printColour;               // 0x2a8c
    matrix4f ident;                   // 0x2adc
    double scrollX = 0, scrollY = 0;  // 0x2b1c, 0x2b24
    int colOffset = 0, rowOffset = 0; // 0x2b2c, 0x2b30
    double width = 0, height = 0;     // 0x2b34, 0x2b3c
    int cols = 40, rowsN = 24;        // 0x2b44, 0x2b48 (largest index, not count)
    gridCell *rows[kMaxCells] = {};   // 0x2b4c (row arrays of kMaxCells cells)
#ifdef NEON_CELL_ROW_OFFSETS
    // Late row header's +4 displacement, in character widths (SOW 37394).
    float rowX[kMaxCells] = {};
#endif
    int fontY = 0;                // 0x2f54
    int bgLiteral = 0;            // 0x2f58
    int glyphLiteral = 1;         // 0x2f5c
    int glyphW = 16, glyphH = 17; // 0x2f60, 0x2f64
    int firstChar = 0x20;         // 0x2f68
    int hashToggle = 0;           // 0x2f6c
    bool waitRelease = false;     // 0x2f70
    bool f2f71 = false, f2f72 = false;
    struct Ripple {
        double x = 0, y = 0, strength = 0;
    } ripples[5];                                  // 0x2f78
    int f30f8 = 16, f30fc = 0;                     // 0x30f8, 0x30fc
    int frameCount = 0;                            // 0x3100
    bool noProp = false;                           // 0x3104
    vec2d passScale[3] = {{1, 1}, {1, 1}, {1, 1}}; // 0x3108
    vec2d origin;                                  // 0x3138 (x), 0x3140 (y)
};

} // namespace Neon_Internals
} // namespace NEON_GAME
