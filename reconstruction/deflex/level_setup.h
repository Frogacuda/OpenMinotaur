// The board-construction part of theGame::newLevel (65368). Entity families
// receive births synchronously, preserving constructor/init/RNG/list order.
#pragma once
#include "level_data.h"
#include "cell_grid_addressing.h"
#include "deflector_controls.h"
#include <functional>

namespace deflex {
class LevelDefinition {
    std::array<uint8_t, level::kRecordSize> bytes_;

  public:
    explicit LevelDefinition(const level::LevelRecordData &source)
        : bytes_(level::recordBytes(source)) {}
    uint32_t word(int offset) const;
    int integer(int offset) const {
        return int32_t(word(offset));
    }
    float scalar(int offset) const;
    double real(int offset) const;
    void setReal(int offset, double value);
};
struct TargetBirth {
    neon::vec2d position, spriteScale;
    int frame = 0;
    uint32_t colour = 0;
    float size = 0;
    int group = 0;
    uint32_t movementFlags = 0, behaviourFlags = 0; // +210/+20c: bit fields, not indices
};
struct LevelSetup {
    LevelDefinition definition;
    CellGrid grid;
    bool mirrored = false, flippingDeflectors = false;
    uint32_t flags = 0;
    int targetsRemaining = 0, targetsCreated = 0, hazardsCreated = 0;
    int deflectorsCreated = 0;
    CellCoords ballCell{};
    explicit LevelSetup(const level::LevelRecordData &source) : definition(source) {}
};
// Production callbacks must construct/init immediately, not defer these events
// into another ordering. Each list remains newest-first in the entity owner.
struct LevelBirths {
    virtual ~LevelBirths() = default;
    virtual void deflector(const DeflectorButton &button) = 0;
    virtual void target(const TargetBirth &birth) = 0; // numberedTarget::init(...,999)
    virtual void hazard(neon::vec2d position, int kind) = 0;
    virtual void weight(neon::vec2d position) = 0;
    virtual void bomb(neon::vec2d position) = 0;
};
void buildLevelBoard(LevelSetup &state, const level::LevelRecordData &record, LevelBirths &births,
                     bool demo = false);
void placeDeflector(CellGrid &grid, int x, int y, int orientation, bool bareBackground);
void blowCell(LevelSetup &state, int x, int y, const std::function<void(neon::vec2d)> &explosion);
} // namespace deflex
