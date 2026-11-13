#pragma once
#include "highscores.h"
#include <cstdint>
#include <cstdio>
#include <string>

namespace minos {
// Host lifetime records, separate from the original per-game goalBits and prefs.
// Original EAGLView::drawView consumes +0x54 events; the port replaces its online
// consumers, not the simulation's criteria, score bookkeeping or notifications.
struct LocalRecords {
    mino::HighScores scores;
    uint32_t achievements = 0;
    bool achievementsDirty = false;
    bool load(const std::string &dir) {
        scores = {};
        scores.init(4, {"MINOTAUR"}, 0, 0);
        for (int b = 0; b < 4; ++b)
            for (int r = 0; r < mino::HighScores::kRows; ++r)
                scores.at(b, r).level = 0; // Empty local slots are not completed caverns.
        const bool haveScores = scores.load(dir + "highscores.txt");
        achievements = 0;
        achievementsDirty = false;
        if (FILE *f = std::fopen((dir + "achievements.txt").c_str(), "r")) {
            unsigned version = 0, bits = 0;
            if (std::fscanf(f, "%u %u", &version, &bits) == 2 && version == 1)
                achievements = bits & 0x3ffu;
            std::fclose(f);
        }
        return haveScores;
    }
    void importPersonalBests(const int32_t *prefs) {
        // Older previews saved the score, but no player name or ending cavern.
        // Import once when the new leaderboard file is absent; cavern 0 is unknown.
        for (int board = 0; board < 4; ++board)
            if (scores.qualifies(board, prefs[board * 100]))
                scores.insert(board, "MINOTAUR", prefs[board * 100], 0);
    }
    // Capture the entire source mask: two goals in one tick can overwrite the
    // single platform event byte. Never seed the simulation mask from this file.
    bool observe(uint32_t bits) {
        const auto prior = achievements;
        achievements |= bits & 0x3ffu;
        achievementsDirty |= prior != achievements;
        return prior != achievements;
    }
    bool save(const std::string &dir) {
        bool ok = true;
        if (scores.dirty) {
            if (!scores.save(dir + "highscores.txt")) {
                scores.dirty = true;
                ok = false;
            }
        }
        if (achievementsDirty) {
            const std::string path = dir + "achievements.txt", tmp = path + ".tmp";
            bool wrote = false;
            if (FILE *f = std::fopen(tmp.c_str(), "w")) {
                wrote = std::fprintf(f, "1 %u\n", unsigned(achievements)) > 0;
                wrote = std::fclose(f) == 0 && wrote;
                if (wrote) {
                    std::remove(path.c_str());
                    wrote = std::rename(tmp.c_str(), path.c_str()) == 0;
                }
            }
            if (wrote)
                achievementsDirty = false;
            else
                ok = false;
        }
        return ok;
    }
};
} // namespace minos
