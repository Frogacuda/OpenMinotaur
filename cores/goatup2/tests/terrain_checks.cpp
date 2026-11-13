// Literal/source-derived rules from iOS 0x50414/0x511e4/0x56400/0x564c8.
// Explicit state setup, not live construction or original-execution evidence.
#include "terrain.h"
#include "stage_bank.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace {
void geometry() {
    goatup2::TerrainBounds bounds{-7, -9};
    const auto layout = goatup2::calculateTerrainLayout(100, 100, 20, 40, {2, 3}, &bounds);
    CHECK(layout.displayReciprocal.x == 0x1.99999ap-5 &&
          layout.displayReciprocal.y == 0x1.99999ap-6);
    CHECK(layout.cellSize.x == 0x1.99999ap-4 && layout.cellSize.y == 0x1.3333338p-4);
    CHECK(layout.viewportGap.x == 80 && layout.viewportGap.y == 60);
    CHECK(bounds.halfWidth == 5 && bounds.halfHeight == 3.75f);
    const auto detached = goatup2::calculateTerrainLayout(100, 100, 20, 40, {2, 3}, nullptr);
    CHECK(detached.cellSize.x == layout.cellSize.x && detached.cellSize.y == layout.cellSize.y);
    // Independent IEEE witness for instruction grouping at 0x504a6. The C
    // export's left-associated scale*reciprocal*count instead gives 1.0f.
    goatup2::calculateTerrainLayout(3, 100, 21, 40, {0x1.c0000133ffffbp+3, 1}, &bounds);
    CHECK(bounds.halfWidth == 0x1.000002p+0f);

    const auto transform = goatup2::calculateTerrainLayout(100, 100, 4, 2, {2, 4}, nullptr);
    const auto world = goatup2::stageToWorld({.125, 2}, {4, -8}, transform, {.5f, .25f});
    CHECK(world.x == 2.125 && world.y == -14.25);
    const auto local = goatup2::worldToStage(world, {4, -8}, transform, {.5f, .25f});
    CHECK(local.x == .125 && local.y == 2);
    const auto unit = goatup2::calculateTerrainLayout(100, 100, 1, 1, {1, 1}, nullptr);
    const auto cancellation = goatup2::stageToWorld({-1e16, -1e16}, {1e16, 1e16}, unit, {1, 1});
    CHECK(cancellation.x == .5 && cancellation.y == .5); // do not reassociate origin into offset
}

void queries(const char *lookupFixture) {
    using namespace goatup2;
    std::vector<uint32_t> cells(GridCells), backup(GridCells);
    const TerrainGridView grid{cells, backup};
    TerrainBounds bounds{0, 0};
    auto layout = calculateTerrainLayout(100, 100, 100, 100, {1, 1}, &bounds);
    CHECK(bounds.halfWidth == .5f && bounds.halfHeight == .5f);
    TerrainCursor cursor{99, 99};
    cells[50] = 0x400043U;
    CHECK(queryTerrain(grid, layout, bounds, 0, -100, 0, cursor) == 0x400043U);
    CHECK(cursor.column == 50 && cursor.row == 0); // lower y clamps to row zero
    for (auto p : {neon::vec2d{-.5, 0}, neon::vec2d{.5, 0}, neon::vec2d{0, .5}}) {
        CHECK(queryTerrain(grid, layout, bounds, float(p.x), float(p.y), 0, cursor) == 0);
        CHECK(cursor.column == 0 && cursor.row == 0);
    }
    const float justInside = std::nextafter(.5f, 0.f);
    cells[100] = 0x400044U;
    CHECK(queryTerrain(grid, layout, bounds, justInside, -.5f, 0, cursor) == 0x400044U);
    CHECK(cursor.column == 100 && cursor.row == 0); // no clamp to 99, physical stride is 100
    backup[50] = 0x400045U;
    CHECK(queryTerrain(grid, layout, bounds, 0, justInside, 0, cursor) == 0x400045U);
    CHECK(cursor.column == 50 && cursor.row == 100);

    auto small = calculateTerrainLayout(5, 5, 5, 5, {1, 1}, &bounds);
    cells[3] = 0x400046U;
    CHECK(queryTerrain(grid, small, bounds, .1f, -.5f, 0, cursor) == 0x400046U);
    CHECK(cursor.column == 3 && cursor.row == 0); // double sum would choose column 2
    layout = calculateTerrainLayout(100, 100, 100, 100, {1, 1}, &bounds);
    layout.cellSize = {9, 9}; // 0x511e4 recomputes scale*reciprocal; it does not use this cache
    cells[50] = 0x400043U;
    CHECK(queryTerrain(grid, layout, bounds, 0, -.5f, 0, cursor) == 0x400043U);
    CHECK(cursor.column == 50 && cursor.row == 0);

    // Independent raw-word export: all 98 substitutions, including stripping
    // high flags. Do not use the native table itself as the expected result.
    std::ifstream fixture(lookupFixture);
    CHECK(fixture.good());
    unsigned id, value, count = 0;
    while (fixture >> id >> std::hex >> value >> std::dec) {
        cells[50] = 0x500000U | id;
        CHECK(queryTerrain(grid, layout, bounds, 0, -.5f, 32, cursor) == value);
        ++count;
    }
    CHECK(count == 98 && fixture.eof());

    // Source-defined inclusive active intervals. Offset is selected by raw
    // tile phase bits; clock subtraction also exercises unsigned wrap.
    std::array<bool, 128> firstWindow{}, secondWindow{};
    std::fill(firstWindow.begin(), firstWindow.begin() + 97, true);
    std::fill(secondWindow.begin() + 16, secondWindow.begin() + 49, true);
    unsigned active2 = 0, active10 = 0, active26 = 0;
    for (unsigned offset = 0; offset < 128; offset += 8)
        for (unsigned phase = 0; phase < 128; ++phase) {
            for (unsigned tile : {2U, 10U, 26U}) {
                cells[50] = 0x100000U | tile | (offset << 21);
                const auto actual =
                    queryTerrain(grid, layout, bounds, 0, -.5f, uint32_t(phase - offset), cursor);
                const bool active = tile == 2 ? firstWindow[phase] : secondWindow[phase];
                const unsigned mapped = tile == 2 ? 2U : (tile == 10 ? 11U : 30U);
                CHECK(actual == (active ? mapped : 0U));
                CHECK(cursor.column == 50 && cursor.row == 0); // inactive != invalid
                if (active) {
                    if (tile == 2)
                        ++active2;
                    else if (tile == 10)
                        ++active10;
                    else
                        ++active26;
                }
            }
        }
    CHECK(active2 == 1552 && active10 == 528 && active26 == 528);
    cells[50] = 0x400002U;
    CHECK(queryTerrain(grid, layout, bounds, 0, -.5f, 96, cursor) ==
          0x400002U); // no map flag: retain word
    CHECK(queryTerrain(grid, layout, bounds, 0, -.5f, 97, cursor) == 0);
    cells[50] = 0;
    CHECK(queryTerrain(grid, layout, bounds, 0, -.5f, 0, cursor) == 0);
    CHECK(cursor.column == 50 && cursor.row == 0);

    // IDs 96/97 are now recovered original words; 98 starts the next
    // unrelated table and must still be rejected.
    bool failed = false;
    cells[50] = 0x100062U;
    try {
        queryTerrain(grid, layout, bounds, 0, -.5f, 0, cursor);
    } catch (const std::out_of_range &) {
        failed = true;
    }
    CHECK(failed); // unsupported remap never becomes silently passable terrain
}
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        geometry();
        queries(argv[1]);
        std::cout
            << "Terrain geometry/conversion, float coordinate boundaries, cursor/backup rules, "
               "98 substitutions and 6144 timed-tile cases passed (source-derived, not original "
               "execution).\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
