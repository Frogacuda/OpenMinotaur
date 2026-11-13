#include "terrain.h"
#include "terrain_tables.h"
#include "stage_bank.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace goatup2 {
namespace {
void requireLayout(int32_t columns, int32_t rows, neon::vec2d scale) {
    if (columns < 0 || columns > 100 || rows < 0 || rows > 100 || !std::isfinite(scale.x) ||
        !std::isfinite(scale.y) || scale.x <= 0 || scale.y <= 0)
        throw std::invalid_argument("unsupported terrain dimensions or scale");
}

// The original VCVT truncates and saturates instead of invoking C++ undefined
// behavior on an overflowing cast. NaN is outside the query input contract.
int32_t tileIndex(double value) {
    if (std::isnan(value))
        throw std::invalid_argument("undefined terrain quotient");
    if (value >= double(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    if (value <= double(std::numeric_limits<int32_t>::min()))
        return std::numeric_limits<int32_t>::min();
    return int32_t(value);
}

neon::vec2d translatedOffset(neon::vec2d gridOffset, const TerrainLayout &layout) {
    // Keep two separate binary64 products, as in the two vector helper calls.
    return (gridOffset * layout.displayReciprocal) * layout.scale;
}
} // namespace

TerrainLayout calculateTerrainLayout(int32_t columns, int32_t rows, int32_t visibleColumns,
                                     int32_t visibleRows, neon::vec2d scale,
                                     TerrainBounds *attachedOwnerBounds) {
    requireLayout(columns, rows, scale);
    if (visibleColumns <= 0 || visibleRows <= 0)
        throw std::invalid_argument("terrain visible dimensions must be positive");
    const neon::vec2d reciprocal{double(1.f / float(visibleColumns)),
                                 double(1.f / float(visibleRows))};
    if (attachedOwnerBounds) {
        // 0x504a6 multiplies reciprocal by count BEFORE multiplying by scale.
        attachedOwnerBounds->halfWidth =
            float(scale.x * (reciprocal.x * double(float(columns))) * .5);
        attachedOwnerBounds->halfHeight =
            float(scale.y * (reciprocal.y * double(float(rows))) * .5);
    }
    return {columns,
            rows,
            scale,
            reciprocal,
            scale * reciprocal,
            {double(float(columns - visibleColumns)), double(float(rows - visibleRows))}};
}

neon::vec2d stageToWorld(neon::vec2d position, neon::vec2d gridOffset, const TerrainLayout &layout,
                         TerrainBounds bounds) {
    const auto offset = translatedOffset(gridOffset, layout);
    return {(position.x + offset.x) + (double(bounds.halfWidth) - .5),
            (position.y + offset.y) + (double(bounds.halfHeight) - .5)};
}

neon::vec2d worldToStage(neon::vec2d position, neon::vec2d gridOffset, const TerrainLayout &layout,
                         TerrainBounds bounds) {
    const auto offset = translatedOffset(gridOffset, layout);
    return {(position.x - offset.x) - (double(bounds.halfWidth) - .5),
            (position.y - offset.y) - (double(bounds.halfHeight) - .5)};
}

uint32_t queryTerrain(TerrainGridView grid, const TerrainLayout &layout, TerrainBounds bounds,
                      float x, float y, uint32_t clock, TerrainCursor &cursor) {
    requireLayout(layout.columns, layout.rows, layout.scale);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(bounds.halfWidth) ||
        !std::isfinite(bounds.halfHeight) || !std::isfinite(layout.displayReciprocal.x) ||
        !std::isfinite(layout.displayReciprocal.y) || layout.displayReciprocal.x <= 0 ||
        layout.displayReciprocal.y <= 0)
        throw std::invalid_argument("unsupported terrain coordinate state");

    const float shiftedX = bounds.halfWidth + x;
    cursor.column = tileIndex(double(shiftedX) / (layout.scale.x * layout.displayReciprocal.x));
    const float shiftedY = bounds.halfHeight + y;
    cursor.row = std::max(
        int32_t(0), tileIndex(double(shiftedY) / (layout.scale.y * layout.displayReciprocal.y)));
    if (cursor.column < 0 || cursor.column > layout.columns || cursor.row > layout.rows ||
        !(x > -bounds.halfWidth && x < bounds.halfWidth && y < bounds.halfHeight)) {
        cursor = {0, 0};
        return 0;
    }
    if (grid.working.size() != GridCells)
        throw std::invalid_argument("terrain working grid must contain 10000 words");
    const auto linear = std::size_t(cursor.row) * 100 + std::size_t(cursor.column);
    const uint32_t raw =
        linear < GridCells ? grid.working[linear] : grid.backup.at(linear - GridCells);
    return resolveTerrainWord(raw, clock, grid.glyphLookup);
}

uint32_t resolveTerrainWord(uint32_t raw, uint32_t clock,
                            const std::array<uint32_t, 98> *glyphLookup) {
    const uint32_t id = raw & 255U;
    const uint32_t value =
        (raw & 0x100000U) ? (glyphLookup ? *glyphLookup : terrain_data::GlyphLookup).at(id) : raw;
    const uint32_t phase = (((raw >> 21) & 0x78U) + clock) & 127U;
    if (id == 2U && phase > 96U)
        return 0;
    if ((id | 16U) == 26U && uint32_t(phase - 16U) > 32U)
        return 0;
    return value;
}

uint32_t &physicalTerrainWord(MutableTerrainGridView grid, int32_t column, int32_t row) {
    if (grid.working.size() != GridCells || grid.backup.size() != GridCells)
        throw std::invalid_argument("physical terrain needs working and backup grids");
    const int64_t index = int64_t(row) * 100 + int64_t(column);
    if (index < 0 || index >= int64_t(GridCells) * 2)
        throw std::out_of_range("direct tile access outside supported physical arena");
    return index < int64_t(GridCells) ? grid.working[std::size_t(index)]
                                      : grid.backup[std::size_t(index) - GridCells];
}
} // namespace goatup2
