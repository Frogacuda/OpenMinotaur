// mod_cellgrid.cpp - Minotaur Rescue's cellGrid module: a 40x24 character
// screen the game prints its text and HUD to, plus background attribute
// blocks used for the playfield borders. Glyphs come from literal 0 (the
// 512x512 game sheet); the font sits at y = 432.
#include "neon_modules.h"
#include "neon/grid_storage.h"

namespace NEON_GAME {
namespace Neon_Internals {

// ============================================================== gridCell ==

gridCell::gridCell() {
    col.white(1.0);
    frame = 0x20;
}

void gridCell::outch(NeonVertex *out, cellGrid *grid, int chr, const colour &c) {
    z = 90.0;
    int idx = chr < grid->firstChar ? 0 : chr - grid->firstChar;
    int gw = grid->glyphW, gh = grid->glyphH;
    double u = (double(gw * (idx % 32)) - 1.0) * 0.001953125;
    double v = (double((gh + 2) * (idx / 32) + grid->fontY) - 1.0) * 0.001953125;
    float u0 = float(u), v0 = float(v);
    float v1 = float(v + double(gh) * 0.001953125);
    float u1 = float(u + (double(gw) - 1.0) * 0.001953125);

    vec2d a = vec2d(0, 0) + pos;
    vec2d b = vec2d(0, height + 0.0) + pos;
    vec2d cc = vec2d(width + 0.0, height + 0.0) + pos;
    vec2d d = vec2d(width + 0.0, 0) + pos;
    uint32_t argb = c.packARGB();
    float zz = float(z);
    auto V = [&](const vec2d &p, float uu, float vv) {
        NeonVertex &n = *out++;
        n.x = float(p.x);
        n.y = -float(p.y);
        n.z = zz;
        n.u0 = n.u1 = n.u2 = uu;
        n.v0 = n.v1 = n.v2 = vv;
        n.colour = argb;
    };
    V(a, u0, v0);
    V(b, u0, v1);
    V(cc, u1, v1);
    V(cc, u1, v1);
    V(d, u1, v0);
    V(a, u0, v0);
}

void gridCell::background(NeonVertex *out, cellGrid *grid) {
    outch(out, grid, grid->blockChar, bg);
    if (attr < 2 || attr > 4)
        return;
    double sw = width, sh = height;
    if (attr == 3) {
        frame = 0xbc;
        width = grid->width * 0.08;
        height = grid->height * 0.11;
        pos.x += width * 0.5;
    } else {
        frame = 0xbb;
        width = grid->width * 0.3;
        height = grid->height * 0.1;
        pos.x += width * -0.5;
        if (attr == 2)
            pos.y = pos.y + height * -0.5 + height / 6.0;
        else
            pos.y = grid->height * 0.5 + (pos.y + grid->height * -0.5) * 1.03;
    }
    col.white(1.0);
    setupSimple(out); // replaces the block quad
    width = sw;
    height = sh;
}

// ============================================================== cellGrid ==

cellGrid::cellGrid() {
    D3DXMatrixIdentity(&ident);
    mode = 600;
    kind = 2;
}

cellGrid::~cellGrid() {
    neon::grid::releaseRows(*this);
}

void cellGrid::setBasics() {
    kind = 1;
    if (mode == 600)
        iconIndex = 3;
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
    glyphLiteral = 0;
    firstChar = 0x20;
    glyphW = 16;
    device = dev;
    glyphH = 16;
    fontY = 0x1b0;
    blockChar = 0x7f;
    if (stack && stack->vb0 >= 0) {
        if (!stack->render.initialised) {
            stack->render.initialised = 1;
            stack->render.Init(device);
            stack->render.shader = 0;
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
    // (The original leaks the previous rows here.)
    neon::grid::releaseRows(*this);
    neon::grid::clampDimensions(c, r);
    cols = c;
    rowsN = r;
    neon::grid::allocateRows(*this, [](int) {}, [](int x) { return x + 0x20; });
    neon::grid::resetStorage(*this, &gridCell::attr);
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
    neon::grid::clearFieldRow(*this, row, &gridCell::attr);
}

void cellGrid::clattr() {
    neon::grid::clearField(*this, &gridCell::attr);
}

void cellGrid::ccol(colour c) {
    for (int y = 0; y <= rowsN; y++)
        for (int x = 0; x <= cols; x++) {
            gridCell &g = rows[y][x];
            g.col = c;
            setLabel(g);
        }
}

// Prints `s` at (x, y). "~RGBA" (four digits 0-9, each /9) changes the
// colour; '&' places attribute-4 sprite background in a blank cell.
void cellGrid::printAt(int x, int y, const char *s) {
    neon::grid::printCharacters(*this, x, y, s, [](auto &cell, char c) {
        if (c == '&') {
            cell.ch = 0x20;
            cell.attr = 4;
        } else
            cell.ch = int(c);
    });
}

void cellGrid::cPrintAt(int y, const char *s) {
    if (!s)
        return;
    int n = neon::grid::visibleColumns(s);
    printAt(cols / 2 - n / 2, y, s);
}

void cellGrid::poke(int x, int y, char c) {
    neon::grid::pokeCharacter(*this, x, y, c);
}

// Label of the cell under p (p in -0.5..0.5 screen units).
int cellGrid::getLabel(vec2d pt) {
    const auto [x, y] = neon::grid::addressedCell(*this, pt);
    if (x < 0 || y < 0 || x > cols || y > rowsN)
        return 0;
    return rows[y][x].label;
}

cellGrid::CellPos cellGrid::getPos(vec2d pt) {
    return {int((pt.x + 0.5) * double(float(cols))), int((pt.y + 0.5) * double(float(rowsN)))};
}

// Background attribute of the cell under p; 0 outside the inner area
// (x 0..cols-1, y 2..rowsN-1).
int cellGrid::getAttr(vec2d pt) {
    int x = int((pt.x + 0.5) * double(float(cols)));
    if (x < 0)
        return 0;
    int y = int((pt.y + 0.5) * double(float(rowsN)));
    if (y < 2 || x >= cols || y >= rowsN)
        return 0;
    return rows[wrapRow(y + rowOffset)][wrapCol(x + colOffset)].attr;
}

// Background run on row y from x0 to x1, colour fading from c0 to c1.
void cellGrid::hlin_a(int a, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) {
    neon::grid::horizontalFieldLine(*this, &gridCell::attr, a, c0, c1, x0, x1, y, alpha);
}

void cellGrid::qhlin_a(int a, uint32_t c, int x0, int x1, int y, float alpha) {
    hlin_a(a, c, c, x0, x1, y + 2, alpha);
    hlin_a(a, c, c, x0, x1, 0x17 - y, alpha);
    hlin_a(a, c, c, 0x27 - x1, 0x27 - x0, y + 2, alpha);
    hlin_a(a, c, c, 0x27 - x1, 0x27 - x0, 0x17 - y, alpha);
}

void cellGrid::qvlin_a(int a, uint32_t c, int y0, int y1, int x, float alpha) {
    auto run = [&](int row0, int count, int col) {
        int r0 = wrapRow(row0 + rowOffset);
        int cx = wrapCol(col + colOffset);
        for (int i = 0; i < count; i++) {
            int r = i + r0;
            if (r > rowsN)
                r = 0;
            gridCell &g = rows[r][cx];
            setLabel(g);
            g.attr = a;
            g.bg.unpackRGBA(c);
            g.bg = g.bg * alpha;
        }
    };
    int n = (y1 + 3) - (y0 + 2);
    run(y0 + 2, n, x);
    run(y0 + 2, n, 0x27 - x);
    int m = (0x18 - y0) - (0x17 - y1);
    run(0x17 - y1, m, x);
    run(0x17 - y1, m, 0x27 - x);
}

int cellGrid::Run_Module() {
    if (!initialised)
        return -1;
    GamePresentationHost *game = stack->effect ? stack->effect->gameHost : nullptr;
    if (!game || !game->hasGame()) {
        cls(0x20);
        clattr();
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
    float hw = float(stack->rtW) * 0.5f, hh = float(stack->rtH) * 0.5f;
    SetModuleOutputRT(&r);
    SetModuleSourceTextures(&r);
    device->SetupDefaultProjection(int(hw + hw), int((hh + hh) * stack->effect->aspect),
                                   PROJ_ORTHO);
    device->view = stack->matrices[stack->loopDepth];
    {
        _input_info src{stack->id, index, glyphLiteral, INPUT_LITERAL}, dst{stack->id, index, 0, 0};
        stack->effect->AssignTextureInput(&src, &dst);
    }
    r.primitive = D3DPT_TRIANGLELIST;
    r.world = ident;
    FillShaderConstants(r.shader, false, 0.0f, 0, int(hw), int(hh), 1.0f, 1.0f, nullptr, nullptr,
                        r.constants, r.constEnabled);
    for (int i = 0; i < 4; i++)
        r.constants[20 + i] = float(p(24 + i));
    r.constEnabled[5] = true;

    double cw = width / double(cols), chH = height / double(rowsN);
    NeonVertex *vp = stack->verts;
    int count = 0;
    double y = height * -0.5 - chH * scrollY;
    for (int ry = 0; ry <= rowsN; ry++, y += chH) {
        int ri = wrapRow(ry + rowOffset);
        double x = width * -0.5 - cw * scrollX;
        for (int cx = 0; cx <= cols; cx++, x += cw) {
            gridCell &g = rows[ri][wrapCol(cx + colOffset)];
            g.pos = vec2d(x, y);
            if (g.attr) {
                g.background(vp, this);
                vp += 6;
                count += 6;
            }
            if (g.fgOn) {
                if (shrink)
                    g.pos.x *= 0.985;
                g.outch(vp, this, g.ch, g.col);
                vp += 6;
                count += 6;
            }
            if (count >= 0x1f88) {
                r.SetVertexBufferWO(stack->vb0);
                r.UpdateBufferFromMem(stack->verts, count);
                r.Render(count);
                count = 0;
                vp = stack->verts;
            }
        }
    }
    if (count > 0) {
        r.SetVertexBufferWO(stack->vb0);
        r.UpdateBufferFromMem(stack->verts, count);
        r.Render(count);
    }
    return 0;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
