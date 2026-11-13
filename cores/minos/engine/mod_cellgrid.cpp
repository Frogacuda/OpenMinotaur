// mod_cellgrid.cpp - the cellGrid module and its gridCell cells: an 8-bit
// style character screen the game prints its text and HUD to. Each cell can
// draw a background block and a glyph, both from minotron.png, and can fade,
// cycle colours, tumble ("smash") and ripple.
#include "neon/proportional_text.h"
#include "neon_modules.h"
#include "neon/grid_geometry.h"
#include "neon/grid_cell_effects.h"
#include "neon/grid_storage.h"

#include <cstring>
#include <cmath>

namespace NEON_GAME {
namespace Neon_Internals {

// ============================================================== gridCell ==

gridCell::gridCell() {
    D3DXMatrixIdentity(&mrx);
    D3DXMatrixIdentity(&mry);
    D3DXMatrixIdentity(&mrz);
    col.white(1.0);
    frame = 0x20;
}

// Sends the cell tumbling away (used when a screen "shatters").
void gridCell::smash() {
    neon::grid::smashCell(*this, [](double low, double high) { return oxRNG::rand(low, high); });
}

// Per-frame animation. Pass 0 runs the background state, pass 1 the glyph state.
//   0: fade (scale *= decay) from fgFrom/bgFrom towards fadeTo, then blank
//   1: step through a colour-index sequence (palette colours)
//   2: tumble (smash)            3: background fade (fadeFlags bit 1)
//   4: glyph colour fade (+ glyph 'e'/'f' rotate)   5: block icon wipe
void gridCell::run() {
    neon::grid::runCellEffects(*this, owner ? owner->palette : nullptr);
}

// Writes the cell's quad for one pass (background block, glyph or icon) to
// `out`; returns the number of vertices written (0 or 6).
int gridCell::outch(NeonVertex *out, cellGrid *grid, unsigned chr, uint32_t colour) {
    return neon::grid::emitAnimatedCell(*this, out, grid, chr, colour,
                                        neon::grid::BackgroundUV::InsetInterior);
}

// ============================================================== cellGrid ==

cellGrid::cellGrid() {
    D3DXMatrixIdentity(&ident);
    mode = 600;
    kind = 2;
    blinkReload = blink;
}

cellGrid::~cellGrid() {
    neon::grid::releaseRows(*this);
    for (proportionalLine *&l : lines) {
        delete l;
        l = nullptr;
    }
}

// ============================================================ Minos: proportional lines ==
namespace {
#include "cellgrid_text_data.inc"
}

void proportionalLine::update() {
    neon::text::updateLine(*this, t, curScale, curSpacing, neon::text::HoldAlpha::Opaque,
                           [&] { text = nullptr; });
}

void cellGrid::QuickMsg(const char *s, vec2d p, vec2d sc, int hold, int row, int colourSeq,
                        float phaseRate, float charStep) {
    proportionalLine *l = lines[row];
    if (l)
        neon::text::quick(l, s, p, sc, hold, colourSeq, phaseRate, charStep, kColourSequences,
                          kColourSequenceLengths);
}

void cellGrid::clm() {
    neon::text::clearBorrowed(lines, rowsN);
}

void cellGrid::rawGlyph(sprite *s, int gx, int gy, int gw, int gh, uint32_t rgba) {
    neon::text::rawGlyph(*this, s, gx, gy, gw, gh, rgba);
}

void cellGrid::propLine(sprite *s, proportionalLine *l) {
    neon::text::drawLine(s, l, kPropFontLineX, kPropFontLineY, noProp,
                         [&](sprite *glyph, int x, int y, int w, int h, uint32_t rgba) {
                             rawGlyph(glyph, x, y, w, h, rgba);
                         });
}

void cellGrid::setBasics() {
    kind = 1;
    if (mode == 600)
        iconIndex = 4;
}

void cellGrid::setDefaults() {
    setBasics();
    for (int i = 24; i <= 31; i++)
        p(i) = 1.0;
}

void cellGrid::Release_Module_Resources() {
    initialised = 0;
    neon::grid::releaseRows(*this);
}

int cellGrid::Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) {
    index = idx;
    stack = st;
    width = double(st->rtW);
    height = double(st->rtH);
    setSize(cols, rowsN);
    machine = 4;
    machineMax = 4;
    loopDepth = 0;
    device = dev;
    scriptPos = 0;
    bgLiteral = 0;
    glyphLiteral = 1;
    fontY = 0;
    bgIndex = -1;
    fillIndex = -1;
    firstChar = 0x20;
    glyphW = 16;
    glyphH = 17;
    f2f72 = false;
    blockChar = 0x7f;
    f29d0 = 7;
    f29d4 = 1;
    script = nullptr;
    scriptPos = 0;
    typeDelay = 0;
    typeDelayReload = 5;
    typeWait2 = 50;
    typeWait = 50;
    winLeft = 2;
    winRight = cols - 4;
    if (stack && stack->vb0 >= 0) {
        if (!stack->render.initialised) {
            stack->render.initialised = 1;
            stack->render.Init(device);
            stack->render.shader = 5;
        }
        if (!initialised) {
            for (int &e : paramEnabled)
                e = 1;
            initialised = 1;
        }
    }
    return 0;
}

void cellGrid::setSize(int c, int r) {
    neon::grid::releaseRows(*this);
    neon::grid::clampDimensions(c, r);
    cols = c;
    rowsN = r;
    neon::grid::allocateRows(*this, [&](int y) { rowX[y] = 0; }, [](int) { return 9999; });
    neon::grid::resetStorage(*this, &gridCell::bgOn);
    // Minos: one proportional line per row (setSize 0x45ed0). Their geometry before QuickMsg
    // (an animated default layout) is not used by Minos, which only shows QuickMsg text.
    for (proportionalLine *&l : lines) {
        delete l;
        l = nullptr;
    }
    for (int y = 0; y <= rowsN; y++) {
        lines[y] = new proportionalLine;
        lines[y]->seq = kDefaultCSeq;
        lines[y]->hold = 500 + 100 * y;
    }
}

void cellGrid::scroll(float dx, float dy) {
    neon::grid::scrollStorage(*this, dx, dy);
}

void cellGrid::cls(int c) {
    for (int y = 0; y <= rowsN; ++y)
        rowX[y] = 0;
    neon::grid::clearCharacters(*this, c);
}

void cellGrid::cls() {
    cls(0x20);
}

void cellGrid::cll(int row, int c) {
    neon::grid::clearCharacterRow(*this, row, c);
}

void cellGrid::cll(int row) {
    cll(row, 0x20);
}

void cellGrid::clla(int row) {
    neon::grid::clearFieldRow(*this, row, &gridCell::bgOn);
}

void cellGrid::clattr() {
    neon::grid::clearField(*this, &gridCell::bgOn);
}

void cellGrid::cllabel() {
    neon::grid::clearField(*this, &gridCell::label);
}

// Prints `s` at (x, y). "~RGBA" (four digits 0-9, each /9) changes the colour.
void cellGrid::printAt(int x, int y, const char *s) {
    neon::grid::printCharacters(*this, x, y, s, [](auto &cell, char c) { cell.ch = int(c); });
}

void cellGrid::cPrintAt(int y, const char *s) {
    if (!s)
        return;
    printAt(cols / 2 - neon::grid::visibleColumns(s) / 2, y, s);
}

void cellGrid::cPrintAt(int y, const char *s, uint32_t rgba) {
    printColour.unpackRGBA(rgba);
    if (!s)
        return;
    printAt(cols / 2 - neon::grid::visibleColumns(s) / 2, y, s);
}

void cellGrid::poke(int x, int y, char c) {
    neon::grid::pokeCharacter(*this, x, y, c);
}

void cellGrid::poke(int x, int y, char c, uint32_t rgba) {
    neon::grid::pokeColouredCharacter(*this, x, y, c, rgba);
}

void cellGrid::lpoke(int x, int y, int lab) {
    rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)].label = lab;
}

// Label of the cell under p (p in -0.5..0.5 screen units).
int cellGrid::getLabel(vec2d pt) {
    const auto [x, y] = neon::grid::addressedCell(*this, pt);
    return rows[y][x].label;
}

// Background-block run on row y from x0 to x1, colour fading from c0 to c1.
void cellGrid::hlin_a(int bgv, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) {
    neon::grid::horizontalFieldLine(*this, &gridCell::bgOn, bgv, c0, c1, x0, x1, y, alpha);
}

// ------------------------------------------------- Minos boot screen + typing ----

// borderCLS 0x468d8: clear the attributes and the text, then paint the border colour around
// the typing window (rows above and below it across the grid, columns left and right of it
// over the window's rows) and the fill colour inside it; the cursor goes to the window's
// top left. The loops' bounds are the original's (rows below the window stop before rowsN,
// columns right of it before cols).
void cellGrid::borderCLS() {
    for (int y = 0; y <= rowsN; ++y)
        rowX[y] = 0;
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++)
            rows[y][x].bgOn = 0;
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++) {
            gridCell &g = rows[y][x];
            g.ch = 0x20;
            g.label = labelMode ? curLabel : 0;
            g.fgOn = 0;
        }
    printColour.white(1.0);
    rowOffset = colOffset = 0;
    scrollX = scrollY = 0;
    uint32_t border = palette[borderIndex].packRGBA();
    uint32_t fill = palette[fillIndex].packRGBA();
    for (int y = 0; y < winTop; y++)
        hlin_a(999, border, border, 0, cols, y, 1.0f);
    for (int y = winBottom + 1; y < rowsN; y++)
        hlin_a(999, border, border, 0, cols, y, 1.0f);
    auto side = [&](int x) {
        int c = x + colOffset;
        if (c > cols)
            c = c - cols - 1;
        int r0 = winTop + rowOffset;
        if (r0 > rowsN)
            r0 = r0 - rowsN - 1;
        int n = winBottom + 1 - winTop;
        for (int i = 0; i < n; i++) {
            int r = i + r0;
            if (r > rowsN)
                r = 0;
            gridCell &g = rows[r][c];
            if (labelMode)
                g.label = curLabel;
            g.bgOn = 999;
            g.bg.unpackRGBA(border);
            g.bg = g.bg * 1.0f;
        }
    };
    for (int x = 0; x < winLeft; x++)
        side(x);
    for (int x = winRight + 1; x < cols; x++)
        side(x);
    for (int y = winTop; y <= winBottom; y++)
        hlin_a(999, fill, fill, winLeft, winRight, y, 1.0f);
    curX = winLeft;
    curY = winTop;
}

// goA8 0x485a8: the Atari 8-bit boot screen. Sixteen palette entries (two copies of eight),
// a grid of the window plus its borders, the window, font row 0xa2, colours (text 2, paper 1,
// fill 1, border 0), borderCLS, then the boot script typed at once; the cursor is shown.
void cellGrid::goA8(int c, int r, int bx, int by) {
    static const char kScript[0x11] = "/x2/y4READY./n/q";
    static const uint32_t kA8[8] = {0xff000000u, 0xff7f5300u, 0xffd3b35fu, 0xff00ff00u,
                                    0xffff00ffu, 0xffffff00u, 0xff00ffffu, 0xffffffffu};
    for (int i = 0; i < 16; i++)
        palette[i].unpackARGB(kA8[i & 7]);
    setSize(bx * 2 + c, by * 2 + r);
    winLeft = bx;
    winTop = by;
    winRight = c;
    winBottom = r;
    fontY = 0xa2;
    fgIndex = 2;
    bgIndex = 1;
    fillIndex = 1;
    borderIndex = 0;
    borderCLS();
    const char *saved = script;
    int savedPos = scriptPos;
    script = kScript;
    scriptPos = 0;
    doInput(true);
    script = saved;
    scriptPos = savedPos;
    cursorOn = true;
}

// doInput's window scroll (its /n and line-wrap paths): rows winTop..winBottom-1 take the
// colour, glyph, glyph/background flags and background of the row below, over the window's
// columns; the bottom row's glyphs go off and its background takes the fill colour. Rows
// and columns are not offset by the scroll.
void cellGrid::typeScrollWindow() {
    int y = winTop;
    for (; y < winBottom; y++)
        for (int x = winLeft; x <= winRight; x++) {
            gridCell &d = rows[y][x];
            const gridCell &s = rows[y + 1][x];
            d.col = s.col;
            d.ch = s.ch;
            d.fgOn = s.fgOn;
            d.bgOn = s.bgOn;
            d.bg = s.bg;
        }
    for (int x = winLeft; x <= winRight; x++) {
        rows[y][x].fgOn = 0;
        rows[y][x].bg = palette[fillIndex];
    }
}

static int getnumb(int &pos, const char *s) { // Neon_Internals::getnumb 0xe144
    int i = pos, v = 0;
    while (unsigned(uint8_t(s[i]) - 0x30u) < 10u)
        v = v * 10 + (s[i++] - 0x30);
    pos = i;
    return v;
}

// doInput 0x4724c (the reachable part, see the header). Not immediate: typeWait, then
// typeDelay frames per character; immediate (or while theGame's touch 0 is down): to /q.
void cellGrid::doInput(bool immediate) {
    if (!script)
        return;
    bool skip = typeSkip && *typeSkip;
    if (!skip) {
        waitRelease = false;
        if (!immediate) {
            if (typeWait > 0 && --typeWait != 0)
                return;
            if (--typeDelay > 0)
                return;
        }
    } else {
        if (waitRelease)
            return;
        immediate = true;
    }
    bool more = true;
    do {
        char c = script[scriptPos++];
        while (c == '/') {
            int at = scriptPos;
            char cmd = script[at];
            scriptPos = at + 1;
            switch (cmd) {
            case 'n':
                curX = winLeft;
                if (++curY > winBottom) {
                    curY = winBottom;
                    typeScrollWindow();
                }
                if (!immediate)
                    return;
                break;
            case 'q':
                typeDelay = 0;
                typeWait = 0;
                scriptPos = 0;
                loopDepth = 0;
                script = nullptr;
                return;
            case 'x':
            case 'y': {
                int &v = cmd == 'x' ? curX : curY;
                int lo = cmd == 'x' ? winLeft : winTop, hi = cmd == 'x' ? winRight : winBottom;
                char a = script[at + 1];
                scriptPos = at + 2;
                if (a == '+') {
                    if (++v > hi)
                        v = hi;
                } else if (a == '-') {
                    if (--v < lo)
                        v = lo;
                } else {
                    scriptPos = at + 1;
                    v = getnumb(scriptPos, script);
                }
                break;
            }
            default:
                break; // (other commands: not ported, no Minos script uses them)
            }
            c = script[scriptPos++];
        }
        typeDelay = typeDelayReload;
        if (skip)
            typeWait = 0;
        if (skip)
            typeDelay = 0;
        gridCell &g = rows[curY][curX];
        g.col = palette[fgIndex];
        g.bg = palette[bgIndex];
        g.scale = typeScale;
        g.decay = typeDecay;
        if (g.decay == 1.0f) {
            g.bgState = -1;
            g.fgState = -1;
            g.rot = vec3d();
            g.rotVel = g.rot;
            g.offset = g.rot;
            g.vel = g.rot;
            g.fgSeq = nullptr;
            g.bgSeq = nullptr;
            g.fadeFlags = 0;
        } else {
            g.bgFrom = g.bg;
            g.fgFrom = g.col;
            g.fadeTo = palette[fillIndex];
            g.fgState = 0;
        }
        g.ch = int(c);
        g.bgOn = 999;
        g.fgOn = c != 0x20;
        if (++curX > winRight) {
            curX = winLeft;
            if (++curY > winBottom) {
                curY = winBottom;
                typeScrollWindow();
            }
        }
        if (!immediate)
            more = typeDelayReload < 1;
    } while (more);
}

int cellGrid::Run_Module() {
    sprite spr;
    if (!initialised)
        return -1;
    GamePresentationHost *game = stack->effect ? stack->effect->gameHost : nullptr;
    if (!game || !game->hasGame()) {
        for (int y = 0; y <= rowsN; ++y)
            rowX[y] = 0;
        for (int y = 0; y <= rowsN; y++)
            for (int x = 0; x <= cols; x++) {
                gridCell &g = rows[y][x];
                g.ch = 0x20;
                g.label = labelMode ? curLabel : 0;
                g.fgOn = 0;
            }
        printColour.white(1.0);
        rowOffset = colOffset = 0;
        scrollX = scrollY = 0;
        for (int y = 0; y <= rowsN; y++)
            for (int x = 0; x <= cols; x++)
                rows[y][x].bgOn = 0;
        printAt(cols / 2 - 9, rowsN / 2, "~9009*** UNASSIGNED ***");
        visible = true;
    } else {
        game->attachTextGrid(*this);
        if (!visible)
            return 0;
    }

    NeonBaseRender &r = stack->render;
    D3DXMatrixIdentity(&ident);
    r.shader = 0;
    r.shaderAux = 0;
    r.texMatrix[0] = ident;
    r.device->SetAlphaBlend(0x302, 0x303);
    int rtW = stack->rtW, rtH = stack->rtH;
    float hw = float(rtW) * 0.5f, hh = float(rtH) * 0.5f;
    if (game && game->hasBackground()) {
        _input_info src{stack->id, index, bgLiteral, INPUT_LITERAL}, dst{stack->id, index, 0, 0};
        stack->effect->AssignTextureInput(&src, &dst);
    }
    SetModuleOutputRT(&r);
    SetModuleSourceTextures(&r);
    device->SetupDefaultProjection(int(hw + hw), int((hh + hh) * stack->effect->aspect), 0);
    device->SetupDefaultCamera();
    spr.batchStack = stack;
    spr.batchCount = 0;
    NeonVertex *vp = stack->verts;
    r.primitive = D3DPT_TRIANGLELIST;
    r.world = ident;
    spr.batchPtr = vp;
    FillShaderConstants(r.shader, false, 0.0f, 0, int(hw), int(hh), 1.0f, 1.0f, nullptr, nullptr,
                        r.constants, r.constEnabled);
    for (int i = 0; i < 4; i++)
        r.constants[20 + i] = float(p(24 + i));
    r.constEnabled[5] = true;
    frameCount++;
    spr.flags |= SPR_CENTRE_X | SPR_CENTRE_Y;
    if (game && game->hasBackground()) {
        game->drawBackground(spr, double(rtW), double(rtH));
        spr.setupPurge();
        spr.batchCount = 0;
        spr.batchPtr = stack->verts;
    }
    {
        _input_info src{stack->id, index, glyphLiteral, INPUT_LITERAL}, dst{stack->id, index, 0, 0};
        stack->effect->AssignTextureInput(&src, &dst);
    }
    SetModuleSourceTextures(&r);
    r.device->SetAlphaBlend(1, 0x303);

    double cw = width / double(cols);
    hashToggle = 0;
    if (--blink < 1) {
        blink = blinkReload;
        cursorPhase = !cursorPhase;
    }
    doInput(false);

    neon::grid::traverseAnimatedCells(
        *this, stack->verts, cw,
        [&](int row, double base, double step) {
            return neon::grid::applyRowDisplacement(base, step, rowX[row]);
        },
        [&](auto &cell, int row, int column, auto *&output, int &count) {
            if (cursorOn && cursorPhase && curY == row && curX == column) {
                cell.pass = -1;
                cell.special = 0;
                const int n =
                    cell.outch(output, this, unsigned(cursorChar), palette[fgIndex].packARGB());
                output += n;
                count += n;
            }
        },
        [&](const auto *vertices, int count) {
            r.SetVertexBufferWO(stack->vb0);
            r.UpdateBufferFromMem(vertices, count);
            r.Render(count);
        });
    r.device->SetAlphaBlend(0x302, 0x303);
    spr.batchCount = 0;
    spr.batchPtr = stack->verts;
    for (int i = 0; i < rowsN; i++)
        propLine(&spr, lines[i]); // Minos: the proportional lines
    spr.setupPurge();
    for (auto &rp : ripples)
        rp.strength *= 0.99;
    return 0;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
