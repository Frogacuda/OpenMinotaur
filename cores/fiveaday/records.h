#pragma once
#include <array>
#include <string>
namespace fiveaday {
// Original savedata: score[day+16], lives[day+216]. The host file names
// those arrays rather than retaining the Objective-C save buffer's layout.
struct Records {
    struct Day {
        int lives = 0, score = 0;
    };
    std::array<Day, 200> days{};
    int highScore = 0, fruit = 0, minotaurs = 0, planes = 0;
    bool tilt = false, writable = true, dirty = false;
    Records() {
        days[0].lives = 3;
    }
    int unlocked() const;
    bool checkpoint(int day, int lives, int score);
    bool load(const std::string &);
    bool save(const std::string &);
};
} // namespace fiveaday
