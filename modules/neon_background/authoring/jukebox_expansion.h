#pragma once
#include <array>
namespace neon::pc {
// Append-only IDs preserve existing scene and layer documents.
inline constexpr std::array<const char *, 80> addedJukeboxPresets = {
    "r1-c13", "r2-c5",  "r2-c6",  "r2-c12", "r2-c15", "r3-c5",  "r3-c8",  "r4-c3",  "r6-c7",
    "r7-c3",  "r1-c14", "r2-c7",  "r2-c9",  "r2-c11", "r3-c2",  "r3-c10", "r3-c15", "r4-c6",
    "r4-c8",  "r4-c9",  "r4-c12", "r4-c14", "r5-c4",  "r5-c6",  "r5-c9",  "r5-c13", "r6-c0",
    "r6-c10", "r6-c12", "r7-c1",  "r7-c2",  "r1-c1",  "r1-c2",  "r1-c3",  "r1-c5",  "r1-c7",
    "r1-c9",  "r1-c11", "r2-c0",  "r2-c1",  "r2-c2",  "r2-c3",  "r3-c3",  "r3-c6",  "r3-c9",
    "r3-c11", "r3-c13", "r3-c14", "r4-c0",  "r4-c1",  "r4-c2",  "r4-c4",  "r4-c5",  "r4-c7",
    "r4-c11", "r4-c15", "r5-c2",  "r5-c3",  "r5-c5",  "r5-c11", "r5-c14", "r5-c15", "r6-c2",
    "r6-c3",  "r6-c4",  "r6-c5",  "r6-c6",  "r6-c8",  "r6-c9",  "r6-c13", "r6-c14", "r7-c0",
    "r3-c12", "r6-c15", "r1-c15", "r3-c0",  "r3-c4",  "r4-c13", "r5-c1",  "r5-c10"};
// Keep the established study IDs 100..104 outside the preset ordinal space.
inline constexpr bool isStudy(int id) { return id >= 100 && id <= 104; }
inline constexpr int presetOrdinal(int id) { return id >= 105 ? id - 5 : id; }
inline constexpr int presetId(int ordinal) { return ordinal >= 100 ? ordinal + 5 : ordinal; }
inline constexpr int addedIndex(int id) { return presetOrdinal(id) - 22; }
inline constexpr int editIndex(int id) { return id == 1 ? 19 : presetOrdinal(id) - 2; }
inline constexpr int modeForGroup(int group) { return group == 19 ? 1 : presetId(group + 2); }
inline constexpr int lastJukeboxPreset = presetId(21 + int(addedJukeboxPresets.size()));
inline bool editablePreset(int id) {
    return id >= 1 && id <= lastJukeboxPreset && id != 18 && id != 21 && !isStudy(id);
}
} // namespace neon::pc
