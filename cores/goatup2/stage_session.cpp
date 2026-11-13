#include "stage_session.h"
#include "neon/integer.h"
#include <cstdio>
#include <algorithm>
#include <stdexcept>
namespace goatup2 {
void StageSession::beginRun(StageRuntime &w, int startingOrdinal) {
    if (startingOrdinal < 0 || startingOrdinal >= 20)
        throw std::out_of_range("original run start ordinal");
    phase_ = Phase::Playing;
    reportTicks_ = 0;
    ordinal_ = startingOrdinal;
    elapsedRunTicks_ = 0;
    extraGoatAwarded_ = false;
    if (bank_) {
        const auto found = std::find(activeSlots_.begin(), activeSlots_.end(), startingOrdinal);
        if (found == activeSlots_.end())
            throw std::invalid_argument("empty campaign level");
        ordinal_ = int(found - activeSlots_.begin());
    }
    startingOrdinal_ = ordinal_;
    livesBonus_ = 0;
    lifetimeImproved_ = pureScoreBeaten_ = fastestTimeBeaten_ = false;
    // Play callback 0x4aec8: two SPARE goats, zero run totals/record counts.
    w.lives = 2;
    w.stageTotal = 0;
    w.recordCounter = 0;
    w.timeRecordCounter = 0;
}
void StageSession::prepareOuterFrame(StageRuntime &w, StageHud &hud, uint32_t frame,
                                     bool reportInput) {
    if (phase_ == Phase::Playing) {
        hud.prepare(w, w.bestStageTime, frame);
        observeCompletion(w);
        observeGameOver(w);
    } else if (phase_ == Phase::BetweenLevels)
        report(w, hud);
    else if (phase_ == Phase::GameComplete || phase_ == Phase::GameOver)
        endReport(w, hud, reportInput);
}
void StageSession::continueRun(StageRuntime &next, StageRuntime &previous) {
    if (phase_ != Phase::NextLevel)
        throw std::logic_error("next stage before report completion");
    next.lives = previous.lives;
    next.stageTotal = previous.stageTotal;
    next.recordCounter = previous.recordCounter;
    next.timeRecordCounter = previous.timeRecordCounter;
    next.bonusPickups = previous.bonusPickups;
    next.hazardContactCount = previous.hazardContactCount;
    next.retainEffectsFrom(previous);
    phase_ = Phase::Playing;
    reportTicks_ = 0;
    extraGoatAwarded_ = false;
}
void StageSession::observeCompletion(StageRuntime &w) {
    // Success is NOT state==4 or full black. Wait for the source exit timer,
    // carried-object count and float fade threshold, in that order.
    if (w.exitFadeTicks <= 50 || w.activeFollowers >= 1 || !(double(w.sceneFade) < .1) ||
        w.originalPauseFlag)
        return;
    w.sharedGrace = 0;
    w.sounds.push_back({63, 0, 0, RuntimeSound::Kind::Stop});
    w.textRowsToClear.push_back(6);
    w.textRowsToClear.push_back(8);
    ++ordinal_;
    const int last = activeSlots_.empty() ? 19 : int(activeSlots_.size()) - 1;
    const bool complete = ordinal_ > last;
    if (complete) {
        ordinal_ = last; // original clamps the stored ordinal, but uses 20000 for bonus
        livesBonus_ = neon::integers::signedBits(uint32_t(w.lives) * 20000U);
        w.addScore(livesBonus_); // BEFORE awarding the spare goat
    }
    extraGoatAwarded_ = w.lives < 9;
    if (extraGoatAwarded_)
        w.lives = neon::integers::add32(w.lives, 1);
    elapsedRunTicks_ = neon::integers::add32(elapsedRunTicks_, w.stageTicks);
    storeLevelRecord(w);
    if (bank_)
        bank_->fur = std::max(bank_->fur, ordinal_);
    phase_ = complete ? Phase::GameComplete : Phase::BetweenLevels;
    reportTicks_ = 0;
    if (complete) {
        w.sounds.push_back({66, uint32_t(w.medium.sampleVolume) << 9, 0});
        finishRecords(w, true);
    }
}
void StageSession::report(StageRuntime &w, StageHud &hud) {
    hud.clearRows.clear();
    hud.text.clear();
    const auto print = [&](int x, int row, std::string value, int colour) {
        hud.text.push_back({x, row, std::move(value), w.terrainPalette.at(size_t(colour))});
    };
    const auto centred = [&](int row, const char *value, int colour) {
        print((42 - int(std::char_traits<char>::length(value))) / 2, row, value, colour);
    };
    const auto sound = [&](int sample) {
        if (!w.medium.splashSoundSuppressed)
            w.sounds.push_back({sample, uint32_t(w.medium.sampleVolume) << 12, 0});
    };
    centred(6, "*** BETWEEN LEVELS STATUS REPORT ***", 9);
    if (!w.originalPauseFlag)
        reportTicks_ = neon::integers::add32(reportTicks_, 1);
    if (reportTicks_ < 31)
        return;
    char line[96];
    std::snprintf(line, sizeof line, "LEVEL SCORE: %d", w.score);
    print(5, 9, line, 0);
    if (!w.newRecord) {
        std::snprintf(line, sizeof line, "LEVEL HISCORE: %d", w.highScore);
        print(5, 11, line, 3);
    } else {
        print(5, 11, "*** NEW LEVEL HISCORE! ***", 2);
        if (reportTicks_ == 31) {
            reportTicks_ = 32;
            if (w.medium.splashSoundSuppressed)
                return; // source breaks this case
            sound(70);
        }
    }
    if (reportTicks_ < 61)
        return;
    std::snprintf(line, sizeof line, "TOTAL SCORE: %d", w.stageTotal);
    print(5, 14, line, 0);
    if (reportTicks_ < 91)
        return;
    if (w.stageTicks < 108000) {
        print(5, 16, StageHud::timeText("LEVEL TIME", w.stageTicks), 0);
        print(5, 18, StageHud::timeText("BEST TIME", w.bestStageTime), 5);
        if (w.newBestStageTime) {
            print(5, 20, "*** BEST TIME BEATEN ***", 1);
            if (reportTicks_ == 91) {
                reportTicks_ = 92;
                if (w.medium.splashSoundSuppressed)
                    return;
                sound(71);
            }
        }
    }
    if (reportTicks_ <= 100)
        return;
    if (extraGoatAwarded_) {
        print(5, 23, "*** EXTRA GOAT AWARDED ***", 1);
        if (reportTicks_ == 101) {
            if (w.medium.splashSoundSuppressed)
                return;
            sound(81);
        }
    }
    if (reportTicks_ <= 120)
        return;
    centred(26, "***     KEEP ON GOATING UP!      ***", 2);
    if (reportTicks_ > 250)
        phase_ = Phase::NextLevel;
}
} // namespace goatup2
