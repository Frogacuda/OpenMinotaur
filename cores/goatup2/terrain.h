// GoatUp 2's stage geometry and tile-query policy. This is not actor physics.
#pragma once
#include "neon/math.h"
#include <cstdint>
#include <array>
#include <vector>

namespace goatup2 {
struct TerrainBounds {
    float halfWidth, halfHeight;
};

// These values are derived by the original explicit geometry recalculation.
// They are not proposed constructor defaults for an entire stage template.
struct TerrainLayout {
    int32_t columns, rows;
    neon::vec2d scale;
    neon::vec2d displayReciprocal;
    neon::vec2d cellSize;
    neon::vec2d viewportGap;
};

// 0x50414. A detached template does not write an owner's existing half extents.
// Supported dimensions are 0..100, visible counts positive, scale finite/positive.
TerrainLayout calculateTerrainLayout(int32_t columns, int32_t rows, int32_t visibleColumns,
                                     int32_t visibleRows, neon::vec2d scale,
                                     TerrainBounds *attachedOwnerBounds);

// 0x56400 / 0x564c8. The grid offset is measured before reciprocal/scale
// conversion. Stage coordinates and base-entity/world coordinates are separate.
neon::vec2d stageToWorld(neon::vec2d stagePosition, neon::vec2d gridOffset,
                         const TerrainLayout &layout, TerrainBounds bounds);
neon::vec2d worldToStage(neon::vec2d worldPosition, neon::vec2d gridOffset,
                         const TerrainLayout &layout, TerrainBounds bounds);

// Borrow original working/backup storage without imitating unchecked guest
// memory. An inclusive last-row query can read into the adjacent backup grid.
struct TerrainGridView {
    const std::vector<uint32_t> &working;
    const std::vector<uint32_t> &backup;
    const std::array<uint32_t, 98> *glyphLookup = nullptr;
};
struct MutableTerrainGridView {
    std::vector<uint32_t> &working;
    std::vector<uint32_t> &backup;
    const std::array<uint32_t, 98> *glyphLookup = nullptr;
};
// Same physical arena used by direct source tile readers/writers. A column may
// cross a 100-column physical row. Unsupported reads before/after the arena fail.
uint32_t &physicalTerrainWord(MutableTerrainGridView grid, int32_t column, int32_t row);
// Shared raw-word substitution/timing operation; does not alter the query cursor.
uint32_t resolveTerrainWord(uint32_t raw, uint32_t clock,
                            const std::array<uint32_t, 98> *glyphLookup = nullptr);
struct TerrainCursor {
    int32_t column, row;
};

// 0x511e4. Caller owns the shared template cursor; every normal return writes it.
// Returns the complete original tile word (or zero), not invented collision flags.
// Point inputs must be finite. Unsupported table/storage reads fail explicitly.
uint32_t queryTerrain(TerrainGridView grid, const TerrainLayout &layout, TerrainBounds bounds,
                      float x, float y, uint32_t clock, TerrainCursor &cursor);
} // namespace goatup2
