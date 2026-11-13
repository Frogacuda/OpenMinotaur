// mod_cellgrid.cpp - the cellGrid module and its gridCell cells: an 8-bit
// style character screen the game prints its text and HUD to. Each cell can
// draw a background block and a glyph, both from minotron.png, and can fade,
// cycle colours, tumble ("smash") and ripple.
#include "neon_modules.h"
#include "neon/grid_geometry.h"
#include "neon/grid_storage.h"
#include "neon/grid_cell_effects.h"

#include <cstring>

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
    neon::grid::allocateRows(*this, [](int) {}, [](int) { return 9999; });
    neon::grid::resetStorage(*this, &gridCell::bgOn);
}

void cellGrid::scroll(float dx, float dy) {
    neon::grid::scrollStorage(*this, dx, dy);
}

void cellGrid::cls(int c) {
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

int cellGrid::Run_Module() {
    sprite spr;
    if (!initialised)
        return -1;
    GamePresentationHost *game = stack->effect ? stack->effect->gameHost : nullptr;
    if (!game || !game->hasGame()) {
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
    // (doInput(0) would run here; its script is never set in Minotron.)

    neon::grid::traverseAnimatedCells(
        *this, stack->verts, cw, [](int, double base, double) { return base; },
        [](auto &, int, int, auto *&, int &) {},
        [&](const auto *vertices, int count) {
            r.SetVertexBufferWO(stack->vb0);
            r.UpdateBufferFromMem(vertices, count);
            r.Render(count);
        });
    r.device->SetAlphaBlend(0x302, 0x303);
    spr.setupPurge();
    for (auto &rp : ripples)
        rp.strength *= 0.99;
    return 0;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
