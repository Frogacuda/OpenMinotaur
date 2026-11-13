#include "stage_session.h"
#include "neon/integer.h"
#include <cstdio>
#include <algorithm>
namespace goatup2 {
void StageSession::setBank(Bank &bank) {
    activeSlots_.clear();
    for (int slot = 0; slot < 20; ++slot)
        if (bank.stages[size_t(slot)].seed)
            activeSlots_.push_back(slot);
    bank_ = &bank;
    oldPureScore_ = bank.pur;
    oldFastestTime_ = bank.tim;
    oldLifetimeScore_ = 0;
    for (const auto &level : bank.stages)
        if (level.seed)
            oldLifetimeScore_ = neon::integers::add32(oldLifetimeScore_, level.highScore);
}
void StageSession::storeLevelRecord(StageRuntime &w) {
    if (!bank_)
        return;
    auto &level = bank_->stages.at(size_t(w.stageIndex));
    level.highScore = w.highScore;
    level.bestTime = w.bestStageTime;
    // BankStorage persists these source record tags at report/lifecycle
    // boundaries; imported geometry and original archives remain unchanged.
}
void StageSession::finishRecords(StageRuntime &w, bool complete) {
    // 0x4f92c for main-bank normal play; the pure-run records require start 0.
    if (!bank_)
        return;
    if (startingOrdinal_ == 0) {
        pureScoreBeaten_ = w.stageTotal > oldPureScore_;
        if (pureScoreBeaten_)
            bank_->pur = w.stageTotal;
        if (complete && elapsedRunTicks_ < oldFastestTime_) {
            bank_->tim = elapsedRunTicks_;
            fastestTimeBeaten_ = true;
        }
    }
    int sum = 0;
    for (const auto &level : bank_->stages)
        if (level.seed)
            sum = neon::integers::add32(sum, level.highScore);
    lifetimeImproved_ = sum > oldLifetimeScore_;
}
void StageSession::observeGameOver(StageRuntime &w) {
    if (!w.terminal)
        return;
    // 0x42f28 case 0 checks the frontend terminal flag, before full fade-out.
    w.sharedGrace = 0;
    w.sounds.push_back({63, 0, 0, RuntimeSound::Kind::Stop});
    w.textRowsToClear.push_back(6);
    w.textRowsToClear.push_back(8);
    elapsedRunTicks_ = neon::integers::add32(elapsedRunTicks_, w.stageTicks);
    storeLevelRecord(w);
    finishRecords(w, false);
    phase_ = Phase::GameOver;
    reportTicks_ = 0;
    w.sounds.push_back({66, uint32_t(w.medium.sampleVolume) << 9, 0});
}
void StageSession::endReport(StageRuntime &w, StageHud &hud, bool input) {
    // 0x42f28 cases 2/3 and 0x4faa0. Unlike BETWEEN LEVELS, this report's
    // counter is not gated by the original pause byte; input adds ten ticks.
    hud.clearRows.clear();
    hud.text.clear();
    const auto print = [&](int x, int row, std::string value, int colour) {
        hud.text.push_back({x, row, std::move(value), w.terrainPalette.at(size_t(colour))});
    };
    const char *title =
        phase_ == Phase::GameComplete ? " *** GAME COMPLETE ***" : " *** GAME OVER ***";
    print((42 - int(std::char_traits<char>::length(title))) / 2, 6, title, 9);
    char line[96];
    if (phase_ == Phase::GameComplete) {
        std::snprintf(line, sizeof line, "LIVES REMAINING BONUS: %d", livesBonus_);
        print(5, 8, line, 7);
    }
    reportTicks_ = neon::integers::add32(reportTicks_, input ? 11 : 1);
    if (reportTicks_ > 130) {
        std::snprintf(line, sizeof line, "TOTAL SCORE: %d", w.stageTotal);
        print(5, 11, line, 0);
        int row = 13;
        if (lifetimeImproved_) {
            print(5, row, "** LIFETIME SCORE IMPROVED! **", 1);
            row = 15;
        }
        if (pureScoreBeaten_)
            print(5, row, "** PURE SCORE BEATEN! **", 4);
    }
    if (reportTicks_ > 160) {
        print(5, 18, StageHud::timeText("PLAYING TIME", elapsedRunTicks_), 5);
        if (fastestTimeBeaten_)
            print(5, 20, "** NEW FASTEST GAME TIME! **", 1);
        if (reportTicks_ > 190) {
            if (w.recordCounter < 1)
                print(5, 23, "NO LEVEL SCORES BEATEN", 0);
            else if (w.recordCounter == 1)
                print(5, 23, "ONE LEVEL SCORE BEATEN", 0);
            else {
                std::snprintf(line, sizeof line, "%d LEVEL SCORES BEATEN!", w.recordCounter);
                print(5, 23, line, 0);
            }
            if (w.timeRecordCounter < 1)
                print(5, 25, "NO LEVEL TIMES BEATEN", 0);
            else if (w.timeRecordCounter == 1)
                print(5, 25, "ONE LEVEL TIME BEATEN", 5);
            else {
                std::snprintf(line, sizeof line, "%d LEVEL TIMES BEATEN!", w.timeRecordCounter);
                print(5, 25, line, 5);
            }
            if (reportTicks_ > 250) {
                if (w.bonusPickups == 1)
                    print(5, 29, "ONE MINOTAUR COLLECTED", 4);
                else if (w.bonusPickups > 1) {
                    std::snprintf(line, sizeof line, "%d MINOTAURS COLLECTED", w.bonusPickups);
                    print(5, 29, line, 4);
                }
                if (reportTicks_ > 300 && w.hazardContactCount > 0) {
                    if (reportTicks_ == 301)
                        w.sounds.push_back({69, uint32_t(w.medium.sampleVolume) << 12,
                                            0}); // unconditional source call
                    if (w.hazardContactCount == 1)
                        print(5, 31, "ONE OX-RELATED DEMISE", 3);
                    else {
                        std::snprintf(line, sizeof line, "%d OX-RELATED DEMISES",
                                      w.hazardContactCount);
                        print(5, 31, line, 3);
                    }
                }
            }
        }
    }
    if (reportTicks_ >= 351 && (input || reportTicks_ >= 2601))
        phase_ = Phase::Done;
}
} // namespace goatup2
