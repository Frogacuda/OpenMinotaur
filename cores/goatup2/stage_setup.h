#pragma once
#include "stage_runtime.h"
#include "neon/resource_catalog.h"
namespace goatup2 {
// Banks have twenty physical slots; campaign order skips slots whose NSE is zero.
inline constexpr int BankStageSlots = 20;
// Original placement construction followed by 0x49f44 terrain reparse and
// 0x46f70 backup/tutorial labels. This is stage setup, not campaign state.
std::unique_ptr<StageRuntime>
createOriginalStage(const Stage &, int stageIndex, const neon::ResourceCatalog &,
                    StageAllocationInputs, MediumSharedState, uint32_t seed, bool tutorialLabels,
                    const StageRuntime *previous = nullptr, bool actors = true);
struct LoadedStage {
    std::string name;
    std::unique_ptr<StageRuntime> world;
};
// 0x48ffc -> 0xa230 -> 0x4cf50. Do not use NSE directly as actor rand state.
LoadedStage loadOriginalStage(const Stage &, int stageIndex, const neon::ResourceCatalog &,
                              StageAllocationInputs, MediumSharedState, bool tutorialLabels,
                              const StageRuntime *previous = nullptr);
} // namespace goatup2
