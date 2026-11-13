#include "stage_setup.h"
#include "stage_labels.h"
#include "name_generator.h"
#include <stdexcept>
namespace goatup2 {
LoadedStage loadOriginalStage(const Stage &stage, int index, const neon::ResourceCatalog &resources,
                              StageAllocationInputs allocation, MediumSharedState medium,
                              bool tutorialLabels, const StageRuntime *previous) {
    if (!stage.seed)
        throw std::invalid_argument("original stage has no NSE");
    neon::DarwinRandom random;
    auto name = generateOriginalName(random, uint32_t(stage.seed), NameMode::Level);
    auto world = createOriginalStage(stage, index, resources, allocation, medium, random.state(),
                                     tutorialLabels, previous);
    world->resetCurrents(); // 0x4ca24, after factory/terrain setup, also between levels
    return {std::move(name), std::move(world)};
}
std::unique_ptr<StageRuntime>
createOriginalStage(const Stage &stage, int stageIndex, const neon::ResourceCatalog &resources,
                    StageAllocationInputs allocation, MediumSharedState medium, uint32_t seed,
                    bool tutorialLabels, const StageRuntime *previous, bool actors) {
    TerrainBounds bounds;
    const auto layout =
        calculateTerrainLayout(stage.declaredColumns, stage.declaredRows, stage.displayWidth,
                               stage.displayHeight, {1, 1}, &bounds);
    auto backup = stage.cells;
    // Decoder writes beyond 10000 cells can affect backup storage until the
    // later explicit copy. Preserve that initial constructor/factory view.
    for (size_t i = 0; i < stage.backupPrefix.size(); ++i)
        backup.at(i) = stage.backupPrefix[i];
    medium.cachedVerticalCellSize = layout.cellSize.y;
    auto world = std::make_unique<StageRuntime>(layout, bounds, stage.cells, std::move(backup),
                                                allocation, medium, seed);
    if (previous) {
        // 0x4ca24 destroys stage actors, not the owner/template/global glyph
        // table. These survive while per-level fields receive fresh values.
        world->glyphLookup = previous->glyphLookup;
        world->gridOffset = previous->gridOffset;
        world->terrainPalette = previous->terrainPalette;
        world->rainbow = previous->rainbow;
        world->sourceTime = previous->sourceTime;
        world->dropFruitSequence = previous->dropFruitSequence;
        world->cameraSettleFrames = previous->cameraSettleFrames;
        world->downwardCurrentPhase = previous->downwardCurrentPhase;
        world->upwardCurrentPhase = previous->upwardCurrentPhase;
    }
    world->stageIndex = stageIndex;
    world->highScore = stage.highScore;
    world->bestStageTime =
        stage.bestTime == 0 ? 108000 : stage.bestTime; // 0x48ffc substitutes 0x1a5e0
    for (int i = 0; i < resources.spriteCount() - 2; ++i) {
        const auto *e = resources.sprite(i + 2);
        if (!e || e->w <= 0)
            throw std::invalid_argument("invalid original sprite metadata");
        world->spriteScales.push_back({1, double(e->h) / double(e->w)});
    }
    if (actors) {
        PlacementTable placements;
        placements.load(stage);
        world->spawnOriginalObjects(placements);
    }
    auto cells = stage.cells;
    const auto saved = cells; // 0x46f70 copies BEFORE writing tutorial text
    if (tutorialLabels) {
        if (stageIndex < 0 || stageIndex >= 20)
            throw std::out_of_range("tutorial stage table");
        for (const auto &label : presentation_data::StageLabels[size_t(stageIndex)]) {
            int index = label.row * 100 + label.column;
            const auto ink = uint32_t(label.palette) << 8;
            for (const unsigned char *p = reinterpret_cast<const unsigned char *>(label.text); *p;
                 ++p) {
                const unsigned char c = *p;
                uint32_t value = 0;
                if (c >= 'A' && c <= 'Z')
                    value = uint32_t(c - 14) | ink;
                if (c >= '<' && c <= '?')
                    value = uint32_t(c + 17) | ink;
                if (c >= 'a' && c <= 'g')
                    value = uint32_t(c - 16) | ink;
                if (c == '_')
                    index += 100;
                if (c == '[')
                    --index;
                if (c == ']')
                    ++index;
                if (c != '_' && c != '[' && c != ']')
                    cells.at(size_t(index++)) = value;
            }
        }
    }
    world->installPreparedTerrain(std::move(cells), saved);
    return world;
}
} // namespace goatup2
