#include "stage_hud.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
namespace goatup2 {
std::string StageHud::timeText(const char *label, int ticks) {
    if (ticks < 0)
        throw std::invalid_argument("negative source HUD time");
    // Original divides remainder by 60 in float, then multiplies by 100 in
    // float before truncation. Avoid substituting integer hundredths.
    const int hundredths = int((float(ticks % 60) / 60.f) * 100.f);
    char line[96];
    std::snprintf(line, sizeof line, "%s: %02d:%02d:%02d", label, ticks / 3600, (ticks / 60) % 60,
                  hundredths);
    return line;
}
void StageHud::prepare(StageRuntime &w, int bestTime, uint32_t frame) {
    text.clear();
    clearRows = {42, 6, 8};
    const auto print = [&](int x, int y, std::string value, const neon::colour &c) {
        text.push_back({x, y, std::move(value), c});
    };
    for (auto &message : w.messages)
        if (message.ticks > 0 && !message.text.empty()) {
            clearRows.push_back(message.row);
            --message.ticks;
            if (message.ticks > 0)
                print(0, message.row, message.text, w.terrainPalette.at(size_t(message.style)));
        }
    // The source plain print calls specify explicit RGBA, unlike the following
    // full-record calls. Retain that distinction when the palette cycles.
    if (w.playerVisible)
        print(1, 42, "Status:", {1, 1, 1, 1});
    if (!w.goat)
        return;
    if (w.sharedGrace > 0) {
        print(0, 6, " *** C H I C K E N   V I N D A L O O ***", w.terrainPalette[9]);
        print(0, 8, "*** U N L I M I T E D    F A R T I N G ***", w.terrainPalette[8]);
    }
    if (w.playerVisible) {
        static constexpr const char *hunger[] = {"DIRE",           "RAVENOUS", "FAMISHED",
                                                 "RUMBLING",       "PECKISH",  "CONTENT",
                                                 "FINE AND DANDY", "REPLETE",  "STUFFED"};
        const int index = std::min(8, int(w.goat->hunger * 8.f));
        if (index < 0)
            throw std::out_of_range("negative original hunger index");
        print(9, 42, hunger[index], w.terrainPalette[3]);
    }
    clearRows.push_back(1);
    clearRows.push_back(2);
    char line[96];
    std::snprintf(line, sizeof line, "%-8d", w.score);
    print(1, 2, line, {1, 1, 0, 1});
    print(0, 1, "   SCORE                          BEST", {1, 1, 1, 1});
    std::snprintf(line, sizeof line, "%8d", w.highScore);
    print(31, 2, line, {1, 0, 1, 1});
    if (w.newRecord && (frame & 32U))
        print(31, 2, "HOORAY! ", w.terrainPalette[1]);
    std::snprintf(line, sizeof line, "% 2d/%02d", w.initialFruit - w.requiredFruit, w.initialFruit);
    print(35, 44, line, w.terrainPalette[w.requiredFruit == 0 ? 5 : 0]);
    print(14, 1, timeText("TIME", w.stageTicks), w.terrainPalette[4]);
    print(14, 3, timeText("BEST", bestTime), w.terrainPalette[5]);
}
} // namespace goatup2
