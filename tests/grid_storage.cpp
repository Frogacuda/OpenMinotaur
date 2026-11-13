// Differential port regression plus independent source-derived storage rules.
// Frozen baselines deliberately do not call neon/grid_storage.h.
#if defined(CELL_STORAGE_RESCUE)
#define NEON_GAME minorescue
#include "cores/minorescue/engine/neon_modules.h"
#elif defined(CELL_STORAGE_MINOS)
#define NEON_GAME minos
#include "cores/minos/engine/neon_modules.h"
#elif defined(CELL_STORAGE_LATE)
#include "engine/bindings/late/neon_modules.h"
#else
#define NEON_GAME minotron
#include "cores/minotron/engine/neon_modules.h"
#endif
#include "neon/source_random.h"
#ifdef _MSC_VER
#pragma warning(push)
// Throwing test callbacks can eliminate a traversal specialization's tail.
#pragma warning(disable : 4702)
#endif
#include "neon/grid_geometry.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "neon/grid_storage.h"
#include "neon/grid_cell_effects.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace NEON_GAME::Neon_Internals;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4458) // Frozen reference names are part of the control.
#endif
#if defined(CELL_STORAGE_RESCUE)
#include "fixtures/cellgrid-reference/rescue.h"
#elif defined(CELL_STORAGE_MINOS)
#include "fixtures/cellgrid-reference/minos.h"
#include "fixtures/cellgrid-reference/minos-cell.h"
#elif defined(CELL_STORAGE_LATE)
#include "fixtures/cellgrid-reference/late.h"
#include "fixtures/cellgrid-reference/late-cell.h"
#else
#include "fixtures/cellgrid-reference/minotron.h"
#include "fixtures/cellgrid-reference/minotron-cell.h"
#endif
#if defined(CELL_STORAGE_MINOS)
#include "fixtures/cellgrid-reference/minos-traversal.h"
#elif defined(CELL_STORAGE_LATE)
#include "fixtures/cellgrid-reference/late-traversal.h"
#elif !defined(CELL_STORAGE_RESCUE)
#include "fixtures/cellgrid-reference/minotron-traversal.h"
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined(CELL_STORAGE_BOUND_RANDOM)
namespace NEON_GAME::Neon_Internals {
extern thread_local neon::DarwinRandom *boundRandom;
}
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
static_assert(sizeof(LegacyGrid) == sizeof(cellGrid));
using Bytes = std::vector<unsigned char>;
template <class T> Bytes bytes(const T &value) {
    Bytes b(sizeof(value));
    std::memcpy(b.data(), &value, b.size());
    return b;
}
struct State {
    Bytes grid;
    std::vector<Bytes> rows;
};
State snapshot(const cellGrid &g) {
    State s{bytes(g), {}};
    for (int y = 0; y < cellGrid::kMaxCells; ++y)
        if (g.rows[y]) {
            Bytes row(size_t(g.cols + 2) * sizeof(gridCell));
            std::memcpy(row.data(), g.rows[y], row.size());
            s.rows.push_back(std::move(row));
        }
    return s;
}
// Compare every modified byte, including writes to otherwise unrelated state.
// Ignore only bytes unchanged in both objects: their padding, row pointers and
// base/derived vptrs need not have the same initial representation.
bool sameChanges(const Bytes &a0, const Bytes &a1, const Bytes &b0, const Bytes &b1) {
    if (a0.size() != a1.size() || a0.size() != b0.size() || a0.size() != b1.size())
        return false;
    for (size_t i = 0; i < a0.size(); ++i) {
        const bool a = a0[i] != a1[i], b = b0[i] != b1[i];
        if (a != b || (a && a1[i] != b1[i]))
            return false;
    }
    return true;
}
bool sameChanges(const State &a0, const State &a1, const State &b0, const State &b1) {
    if (!sameChanges(a0.grid, a1.grid, b0.grid, b1.grid) || a0.rows.size() != a1.rows.size() ||
        a0.rows.size() != b0.rows.size() || a0.rows.size() != b1.rows.size())
        return false;
    for (size_t i = 0; i < a0.rows.size(); ++i)
        if (!sameChanges(a0.rows[i], a1.rows[i], b0.rows[i], b1.rows[i]))
            return false;
    return true;
}
void seed(cellGrid &g, int columns, int rows) {
    g.cols = columns;
    g.rowsN = rows;
    g.rowOffset = rows;
    g.colOffset = columns;
    g.scrollX = .75;
    g.scrollY = .25;
    g.curLabel = 91;
    g.labelMode = false;
    for (int i = 0; i < 4; ++i)
        g.scrollCount[i] = 10 + i;
    g.printColour.r = .125;
    g.printColour.g = .25;
    g.printColour.b = .5;
    g.printColour.a = .75;
    g.printColour.phase[1] = 3.25f;
    g.printColour.speed[2] = .625f;
    // One extra column and (where possible) row are sentinels, beyond the
    // inclusive logical bounds. Allocation policy itself is outside this test.
    for (int y = 0; y <= rows + (rows < 128); ++y) {
        g.rows[y] = new gridCell[columns + 2];
        for (int x = 0; x < columns + 2; ++x) {
            auto &c = g.rows[y][x];
            c.ch = 65 + (x + y) % 20;
            c.label = 1000 + 31 * y + x;
            c.fgOn = (x + y) % 2;
            c.col.r = .25;
            c.col.a = .5;
            c.col.phase[1] = 1.25f;
            c.width = 17.5 + x;
#if defined(CELL_STORAGE_RESCUE)
            c.attr = (x + y) % 5;
#else
            c.bgOn = (x + y) % 3;
            c.attr = 7;
            c.scale = .375f;
            c.fgState = 4;
            c.bgState = 2;
#endif
        }
    }
}
int operations = 0;
template <class Op> void compare(cellGrid &actual, LegacyGrid &legacy, Op op) {
    auto a0 = snapshot(actual), b0 = snapshot(legacy);
    neonSrand(917);
    const int expectedRandom = neonRand();
    neonSrand(917);
    op(actual);
    op(legacy);
    CHECK(neonRand() == expectedRandom);
    CHECK(sameChanges(a0, snapshot(actual), b0, snapshot(legacy)));
    ++operations;
}
void sourceRules(cellGrid &a, LegacyGrid &b) {
    a.labelMode = b.labelMode = false;
    const int oldLabel = a.rows[0][0].label;
    a.rowOffset = b.rowOffset = 0;
    compare(a, b, [](auto &g) { g.cll(0); });
    CHECK(a.rows[0][0].label == oldLabel && a.rows[0][a.cols].ch == 32 &&
          a.rows[0][a.cols].fgOn == 0);
    a.labelMode = b.labelMode = true;
    a.curLabel = b.curLabel = -17;
    compare(a, b, [](auto &g) { g.cll(0, 65); });
    CHECK(a.rows[0][0].label == -17 && a.rows[0][a.cols].ch == 65 && a.rows[0][a.cols].fgOn == 1);
#if !defined(CELL_STORAGE_RESCUE)
    compare(a, b, [](auto &g) { g.cllabel(); });
    CHECK(a.rows[0][0].label == 0 && a.rows[0][0].fgOn == 1);
#endif
    a.labelMode = b.labelMode = false;
    const auto counters = a.scrollCount; // array copy below is explicit
    std::array<int, 4> before{};
    for (int i = 0; i < 4; ++i)
        before[i] = counters[i];
    compare(a, b, [](auto &g) { g.cls(); });
    CHECK(a.rowOffset == 0 && a.colOffset == 0 && a.scrollX == 0 && a.scrollY == 0);
    CHECK(a.printColour.r == 1 && a.printColour.g == 1 && a.printColour.b == 1 &&
          a.printColour.a == 1);
    for (int i = 0; i < 4; ++i)
        CHECK(a.scrollCount[i] == before[i]);
    for (int y = 0; y <= a.rowsN; ++y)
        for (int x = 0; x <= a.cols; ++x)
            CHECK(a.rows[y][x].ch == 32 && a.rows[y][x].label == 0 && a.rows[y][x].fgOn == 0);
    const auto print = bytes(a.printColour);
    compare(a, b, [](auto &g) { g.clattr(); });
    CHECK(bytes(a.printColour) == print);
#if defined(CELL_STORAGE_RESCUE)
    CHECK(a.rows[a.rowsN][a.cols].attr == 0);
#else
    CHECK(a.rows[a.rowsN][a.cols].bgOn == 0 && a.rows[a.rowsN][a.cols].attr == 7);
#endif
}
void scrollRules(cellGrid &a, LegacyGrid &b) {
    struct Case {
        float delta;
        int negative, positive, offset;
        double fraction;
    };
    const float tiny = std::numeric_limits<float>::denorm_min();
    const Case cases[] = {{0, 0, 0, 0, 0},
                          {-0.f, 0, 0, 0, 0},
                          {tiny, 0, 0, 0, double(tiny)},
                          {-tiny, 1, 1, 0, 0},
                          {1, 0, 1, 1, 0},
                          {-1, 1, 0, a.cols, 0},
                          {3.75f, 0, 3, 3 % (a.cols + 1), .75},
                          {-3.75f, 4, 0, ((a.cols + 1) - 4 % (a.cols + 1)) % (a.cols + 1), .25}};
    for (int axis = 0; axis < 2; ++axis)
        for (const auto &c : cases) {
            a.scrollX = b.scrollX = 0;
            a.scrollY = b.scrollY = 0;
            a.colOffset = b.colOffset = 0;
            a.rowOffset = b.rowOffset = 0;
            for (int i = 0; i < 4; ++i)
                a.scrollCount[i] = b.scrollCount[i] = 0;
            compare(a, b,
                    [&](auto &g) { g.scroll(axis == 0 ? c.delta : 0, axis == 1 ? c.delta : 0); });
            const int period = (axis == 0 ? a.cols : a.rowsN) + 1;
            const int expected = ((c.positive - c.negative) % period + period) % period;
            CHECK((axis == 0 ? a.scrollX : a.scrollY) == c.fraction &&
                  (axis == 0 ? a.colOffset : a.rowOffset) == expected);
            CHECK(a.scrollCount[axis * 2] == c.negative &&
                  a.scrollCount[axis * 2 + 1] == c.positive);
            CHECK((axis == 0 ? a.scrollY : a.scrollX) == 0 &&
                  (axis == 0 ? a.rowOffset : a.colOffset) == 0);
            CHECK(a.scrollCount[(1 - axis) * 2] == 0 && a.scrollCount[(1 - axis) * 2 + 1] == 0);
        }
}
void sensitivity(cellGrid &a, LegacyGrid &b) {
    auto a0 = snapshot(a), b0 = snapshot(b);
    a.rows[a.rowsN][a.cols + 1].col.phase[1] += 1;
    CHECK(!sameChanges(a0, snapshot(a), b0, snapshot(b)));
    a.rows[a.rowsN][a.cols + 1].col.phase[1] -= 1;
    a0 = snapshot(a);
    b0 = snapshot(b);
    a.printColour.phase[1] += 1;
    CHECK(!sameChanges(a0, snapshot(a), b0, snapshot(b)));
    a.printColour.phase[1] -= 1;
}

void editing(cellGrid &a, LegacyGrid &b) {
#if defined(CELL_STORAGE_RESCUE)
    a.rowOffset = b.rowOffset = 0;
    a.colOffset = b.colOffset = 0;
    compare(a, b, [](auto &g) { g.printAt(0, 0, "&"); });
    CHECK(a.rows[0][0].ch == 32 && a.rows[0][0].attr == 4 && a.rows[0][0].fgOn == 1);
#endif
    const char *strings[] = {"A", "&", "~1234A", "~9999", "~0900 ", nullptr};
    for (const char *text : strings) {
        compare(a, b, [&](auto &g) { g.printAt(0, 0, text); });
        compare(a, b, [&](auto &g) { g.cPrintAt(0, text); });
    }
    for (int c = 0; c < 256; ++c) {
        const int x = c % (a.cols + 1), y = (c / 3) % (a.rowsN + 1);
        a.labelMode = b.labelMode = (c & 1) != 0;
        a.curLabel = b.curLabel = c - 100;
        compare(a, b, [&](auto &g) { g.poke(x, y, char(c)); });
#if !defined(CELL_STORAGE_RESCUE)
        compare(a, b, [&](auto &g) { g.poke(x, y, char(c), 0x12345678u + uint32_t(c)); });
        compare(a, b, [&](auto &g) { g.lpoke(x, y, c - 37); });
#endif
    }
    for (double y : {-.5, -.49, 0., .49, .5})
        for (double x : {-.5, -.49, 0., .49, .5})
            CHECK(a.getLabel(vec2d(x, y)) == b.getLabel(vec2d(x, y)));
    // Include one-cell lines, reversed ranges and wrap across the last column.
    for (int x0 = 0; x0 <= a.cols; ++x0)
        for (int x1 = 0; x1 <= a.cols; ++x1)
            compare(a, b,
                    [&](auto &g) { g.hlin_a(3, 0x10203040u, 0xfedcba98u, x0, x1, 0, .625f); });
}
void allocationRules() {
    auto g = std::make_unique<cellGrid>();
    g->width = 640;
    g->height = 480;
    g->labelMode = true;
    g->curLabel = -37;
    for (auto dim : {std::array<int, 2>{0, -1}, {3, 2}, {129, 130}, {1, 1}}) {
        g->setSize(dim[0], dim[1]);
        CHECK(g->cols == (dim[0] < 1 ? 1 : dim[0] > 127 ? 128 : dim[0]));
        CHECK(g->rowsN == (dim[1] < 1 ? 1 : dim[1] > 128 ? 128 : dim[1]));
        CHECK(g->colOffset == 0 && g->rowOffset == 0 && g->scrollX == 0 && g->scrollY == 0);
        for (int y = 0; y <= g->rowsN; ++y)
            for (int x = 0; x <= g->cols; ++x) {
                auto &c = g->rows[y][x];
                CHECK(c.width == 640. / g->cols && c.height == 480. / g->rowsN && c.ch == 32 &&
                      c.label == -37 && c.fgOn == 0);
            }
#if defined(CELL_STORAGE_MINOS) || defined(NEON_CELL_ROW_OFFSETS)
        for (int y = 0; y <= g->rowsN; ++y)
            CHECK(g->rowX[y] == 0);
        g->rowX[0] = .5f;
#endif
    }
#if defined(CELL_STORAGE_MINOS) || defined(NEON_CELL_ROW_OFFSETS)
    g->rowX[0] = .5f;
    g->rowX[g->rowsN] = -.25f;
    g->cll(0);
    CHECK(g->rowX[0] == .5f);
    g->cls();
    CHECK(g->rowX[0] == 0 && g->rowX[g->rowsN] == 0);
    CHECK(neon::grid::displacedRowX(640., .25, 3., 16., .5f) == -313.);
#endif
}
#if !defined(CELL_STORAGE_RESCUE)

void traversalOperations() {
    for (auto dim : {std::array<int, 2>{7, 5}, {40, 24}, {128, 128}})
        for (int mode = 0; mode < 4; ++mode) {
            auto a = std::make_unique<cellGrid>();
            auto b = std::make_unique<cellGrid>();
            seed(*a, dim[0], dim[1]);
            seed(*b, dim[0], dim[1]);
            for (auto *grid : {a.get(), b.get()}) {
                grid->width = 640;
                grid->height = 480;
                grid->origin = {3., 7.};
                grid->hashMode = mode >= 2;
                grid->hashToggle = 0;
                grid->labelCount = 63;
                grid->firstChar = 32;
                grid->maxChar = 127;
                grid->glyphW = 16;
                grid->glyphH = 17;
                grid->fontY = 218;
#if defined(CELL_STORAGE_MINOS)
                grid->cursorOn = mode != 0;
                grid->cursorPhase = true;
                grid->curX = grid->cols;
                grid->curY = grid->rowsN;
                grid->fgIndex = 1;
                grid->cursorChar = 65;
#endif
#ifdef NEON_CELL_ROW_OFFSETS
                for (int y = 0; y <= grid->rowsN; ++y)
                    grid->rowX[y] = float(y % 3) * .25f;
#endif
                for (int y = 0; y <= grid->rowsN + (grid->rowsN < 128); ++y)
                    for (int x = 0; x < grid->cols + 2; ++x) {
                        auto &cell = grid->rows[y][x];
                        cell.owner = grid;
                        cell.fgState = cell.bgState = -1;
                        cell.fgOn = mode >= 2;
                        cell.bgOn = mode != 0;
                        cell.attr = (x + y) % 4;
                        cell.ch = "#AB#C D"[(x + y) % 7];
                        cell.pos = {-99., -99.};
                    }
            }
            const auto a0 = snapshot(*a), b0 = snapshot(*b);
            std::array<NeonVertex, 8194> av, bv;
            std::memset(av.data(), 0xa5, sizeof av);
            std::memset(bv.data(), 0xa5, sizeof bv);
            std::vector<int> acounts, bcounts;
            Bytes atrace, btrace;
            auto sink = [&](auto &counts, auto &trace, const auto *vertices, int count) {
                CHECK(count > 0 && count <= 8192 && count % 6 == 0);
                counts.push_back(count);
                const auto *bytes = reinterpret_cast<const unsigned char *>(vertices);
                trace.insert(trace.end(), bytes, bytes + sizeof(NeonVertex) * size_t(count));
            };
            const double capturedWidth = mode == 3 ? 17.25 : a->width / double(a->cols);
            neon::grid::traverseAnimatedCells(
                *a, av.data() + 1, capturedWidth,
                [&](int row, double base, double step) {
#if defined(CELL_STORAGE_MINOS) || defined(NEON_CELL_ROW_OFFSETS)
                    return neon::grid::applyRowDisplacement(base, step, a->rowX[row]);
#else
                    return base;
#endif
                },
                [&](auto &cell, int row, int column, auto *&output, int &count) {
#if defined(CELL_STORAGE_MINOS)
                    if (a->cursorOn && a->cursorPhase && a->curY == row && a->curX == column) {
                        cell.pass = -1;
                        cell.special = 0;
                        int n = cell.outch(output, a.get(), unsigned(a->cursorChar),
                                           a->palette[a->fgIndex].packARGB());
                        output += n;
                        count += n;
                    }
#endif
                },
                [&](const auto *vertices, int count) { sink(acounts, atrace, vertices, count); });
            legacyTraversal(*b, bv.data() + 1, capturedWidth, [&](const auto *vertices, int count) {
                sink(bcounts, btrace, vertices, count);
            });
            CHECK(acounts == bcounts && atrace == btrace);
            CHECK(sameChanges(a0, snapshot(*a), b0, snapshot(*b)));
            std::array<unsigned char, sizeof(NeonVertex)> guard;
            guard.fill(0xa5);
            CHECK(std::memcmp(&av.front(), guard.data(), guard.size()) == 0 &&
                  std::memcmp(&av.back(), guard.data(), guard.size()) == 0);
            CHECK(std::memcmp(&bv.front(), guard.data(), guard.size()) == 0 &&
                  std::memcmp(&bv.back(), guard.data(), guard.size()) == 0);
            if (mode == 0)
                CHECK(acounts.empty());
            if (mode == 3 && dim[0] == 128)
                CHECK(acounts.size() > 1); // ordered flush threshold exercised
            ++operations;
        }
#if defined(CELL_STORAGE_MINOS)
    // Separate source correction: row +4 is a displacement, not an array cookie.
    auto grid = std::make_unique<cellGrid>();
    seed(*grid, 4, 2);
    grid->width = 640;
    grid->height = 480;
    grid->origin = {3, 7};
    grid->colOffset = grid->rowOffset = 0;
    grid->scrollX = .25;
    grid->rowX[0] = .5f;
    grid->hashMode = false;
    for (int y = 0; y <= grid->rowsN; ++y)
        for (int x = 0; x <= grid->cols; ++x) {
            auto &cell = grid->rows[y][x];
            cell.owner = grid.get();
            cell.fgState = cell.bgState = -1;
            cell.bgOn = cell.fgOn = 0;
        }
    std::array<NeonVertex, 8192> output;
    neon::grid::traverseAnimatedCells(
        *grid, output.data(), 160.,
        [&](int row, double base, double step) {
            return neon::grid::applyRowDisplacement(base, step, grid->rowX[row]);
        },
        [](auto &, int, int, auto *&, int &) {}, [](const auto *, int) { CHECK(false); });
    CHECK(grid->rows[0][0].pos.x == -277. && grid->rows[1][0].pos.x == -357.);
#endif
}
void cellOperations() {
    auto grid = std::make_unique<cellGrid>();
    for (int i = 0; i < 16; ++i) {
        auto &c = grid->palette[i];
        c.r = .03125 * i;
        c.g = .0625 * i;
        c.b = .125 * i;
        c.a = .75;
    }
    auto a = std::make_unique<gridCell>();
    auto b = std::make_unique<LegacyCell>();
    int seq[128];
    for (int i = 0; i < 128; ++i)
        seq[i] = i % 4 == 3 ? -1 : i % 16;
    for (int bg = -1; bg <= 6; ++bg)
        for (int fg = -1; fg <= 6; ++fg)
            for (int flags = 0; flags < 4; ++flags)
                for (float scale : {.005f, .05f, .099f, 1.f, 1.5f, 16.f}) {
                    *a = gridCell();
                    a->owner = grid.get();
                    a->bgState = bg;
                    a->fgState = fg;
                    a->fadeFlags = flags;
                    a->scale = scale;
                    a->decay = .98f;
                    a->ch = flags & 1 ? 101 : 102;
                    a->bgIcon = 13;
                    a->bgT = .99f;
                    a->bgSpeed = .35f;
                    a->fgT = .25f;
                    a->fgSpeed = .11f;
                    a->fgSeq = seq;
                    a->bgSeq = seq;
                    a->fgAngle = flags * .5f;
                    a->rotVel = {.01, -.02, .03};
                    a->vel = {1., 2., 3.};
                    a->fgFrom = {.2, .4, .6, .8};
                    a->bgFrom = {.1, .3, .5, .7};
                    a->fadeTo = {.9, .8, .7, .6};
                    a->bgTo = {.8, .6, .4, .2};
                    static_cast<gridCell &>(*b) = *a;
                    for (int frame = 0; frame < 4; ++frame) {
                        auto a0 = bytes(*a), b0 = bytes(static_cast<gridCell &>(*b));
                        a->run();
                        b->run();
                        CHECK(sameChanges(a0, bytes(*a), b0, bytes(static_cast<gridCell &>(*b))));
                        ++operations;
                    }
                }
    for (int i = 0; i < 32; ++i) {
        *a = gridCell();
        static_cast<gridCell &>(*b) = *a;
        neonSrand(123 + i);
        a->smash();
        int after = neonRand();
        neonSrand(123 + i);
        b->smash();
        CHECK(neonRand() == after);
        CHECK(a->rotVel.x == b->rotVel.x && a->rotVel.y == b->rotVel.y &&
              a->rotVel.z == b->rotVel.z);
        CHECK(a->vel.x == b->vel.x && a->vel.y == b->vel.y && a->vel.z == b->vel.z &&
              a->scale == b->scale && a->decay == b->decay && a->fgState == b->fgState);
        neonSrand(123 + i);
        for (int n = 0; n < 6; ++n)
            neonRand();
        CHECK(neonRand() == after);
        ++operations;
    }
    grid->firstChar = 32;
    grid->maxChar = 127;
    grid->glyphW = 16;
    grid->glyphH = 17;
    grid->fontY = 218;
    for (auto &p : grid->passScale)
        p = {.98, 1.};
    grid->ripples[0].strength = .125;
    grid->ripples[0].x = 3.;
    grid->ripples[0].y = 4.;
    *a = gridCell();
    a->pass = 0;
    a->pos = {0, 0};
    a->width = 16;
    a->height = 17;
    std::array<NeonVertex, 6> sourceUV{}, insetUV{};
    CHECK(neon::grid::emitAnimatedCell(*a, sourceUV.data(), grid.get(), 65, 0xffffffffu,
                                       neon::grid::BackgroundUV::Original) == 6);
    CHECK(neon::grid::emitAnimatedCell(*a, insetUV.data(), grid.get(), 65, 0xffffffffu,
                                       neon::grid::BackgroundUV::InsetInterior) == 6);
    CHECK(sourceUV[0].u0 == float(double(16 * (33 % 32) - 1) / 512.));
    CHECK(sourceUV[0].v0 == float(double(17 * (33 / 32) + (33 / 32) + 218 - 1) / 512.));
    CHECK(insetUV[0].u0 == float(double(sourceUV[0].u0) + 1.5 / 512.));
    for (unsigned chr = 0; chr < 260; ++chr)
        for (int pass = -1; pass <= 2; ++pass)
            for (int special = 0; special < 2; ++special)
                for (int rotation = 0; rotation < 3; ++rotation) {
                    *a = gridCell();
                    a->owner = nullptr;
                    a->pass = pass;
                    a->special = special != 0;
                    a->bgIcon = pass == 0 ? -1 : 2;
                    a->fgIcon = 7;
                    a->pos = {3., 5.};
                    a->width = 16.;
                    a->height = 17.;
                    a->bgAngle = .25f;
                    a->fgAngle = .5f;
                    if (rotation == 1)
                        a->rot = {.1, .2, .3};
                    if (rotation == 2)
                        a->rot.y = .2;
                    a->offset = {2., 4., 6.};
                    static_cast<gridCell &>(*b) = *a;
                    std::array<NeonVertex, 8> av, bv;
                    std::memset(av.data(), 0xa5, sizeof av);
                    std::memset(bv.data(), 0xa5, sizeof bv);
                    auto a0 = bytes(*a), b0 = bytes(static_cast<gridCell &>(*b));
                    const auto gridBefore = bytes(*grid);
                    int ac = a->outch(av.data() + 1, grid.get(), chr, 0x12345678u),
                        bc = b->outch(bv.data() + 1, grid.get(), chr, 0x12345678u);
                    CHECK(ac == bc && std::memcmp(av.data(), bv.data(), sizeof av) == 0);
                    CHECK(sameChanges(a0, bytes(*a), b0, bytes(static_cast<gridCell &>(*b))));
                    CHECK(gridBefore == bytes(*grid));
                    ++operations;
                }
}
#endif
int main() {
    try {
#if defined(CELL_STORAGE_BOUND_RANDOM)
        neon::DarwinRandom stream(917);
        boundRandom = &stream;
#endif
        allocationRules();
#if !defined(CELL_STORAGE_RESCUE)
        cellOperations();
        traversalOperations();
#endif
        for (auto size : {std::array<int, 2>{1, 1}, {3, 2}, {40, 24}, {128, 128}}) {
            auto a = std::make_unique<cellGrid>();
            auto b = std::make_unique<LegacyGrid>();
            seed(*a, size[0], size[1]);
            seed(*b, size[0], size[1]);
            sensitivity(*a, *b);
            sourceRules(*a, *b);
            scrollRules(*a, *b);
            if (size[0] > 3)
                continue;
            editing(*a, *b);
            for (bool labels : {false, true})
                for (int code = -1; code <= 256; ++code) {
                    a->labelMode = b->labelMode = labels;
                    a->curLabel = b->curLabel = code - 41;
                    for (int row = 0; row <= a->rowsN; ++row) {
                        a->rowOffset = b->rowOffset = row;
                        compare(*a, *b, [&](auto &g) { g.cll(row, code); });
                        compare(*a, *b, [&](auto &g) { g.clla(row); });
                    }
                    compare(*a, *b, [&](auto &g) { g.cls(code); });
                }
            uint32_t random = 0x12345678;
            for (int n = 0; n < 3000; ++n) {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                const int row = int(random % uint32_t(a->rowsN + 1));
                a->labelMode = b->labelMode = (random & 8) != 0;
                switch (random % 7) {
                case 0:
                    compare(*a, *b, [&](auto &g) {
                        g.scroll(float(int(random % 257) - 128) / 16.f,
                                 float(int((random >> 9) % 257) - 128) / 16.f);
                    });
                    break;
                case 1:
                    compare(*a, *b, [&](auto &g) { g.cll(row); });
                    break;
                case 2:
                    compare(*a, *b, [&](auto &g) { g.cll(row, int(random & 255)); });
                    break;
                case 3:
                    compare(*a, *b, [&](auto &g) { g.clla(row); });
                    break;
                case 4:
                    compare(*a, *b, [](auto &g) { g.clattr(); });
                    break;
                case 5:
                    compare(*a, *b, [](auto &g) { g.cls(); });
                    break;
                case 6:
#if !defined(CELL_STORAGE_RESCUE)
                    compare(*a, *b, [](auto &g) { g.cllabel(); });
#else
                    compare(*a, *b, [](auto &g) { g.cls(255); });
#endif
                    break;
                }
            }
        }
        std::cout
            << "Cell-grid family: " << operations
            << " old/new state comparisons, source rules, sentinels and RNG preservation passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << " after " << operations << " comparisons\n";
        return 1;
    }
}
