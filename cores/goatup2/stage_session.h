#pragma once
#include "stage_hud.h"
namespace goatup2 {
// Normal-play entry/completion and the BETWEEN LEVELS branch of 0x42f28.
// Title/editor/game-complete/game-over reports remain separate mechanisms;
// this class does not replace them with a common guessed report algorithm.
class StageSession {
  public:
    enum class Phase { Playing, BetweenLevels, NextLevel, GameComplete, GameOver, Done };
    void beginRun(StageRuntime &, int startingOrdinal = 0);
    void setBank(Bank &);
    void continueRun(StageRuntime &next, StageRuntime &previous);
    // Original outer update reads PRE-simulation state. A transition on this
    // call leaves that frame's active HUD intact; report drawing starts next tick.
    void prepareOuterFrame(StageRuntime &, StageHud &, uint32_t frame, bool reportInput = false);
    Phase phase() const {
        return phase_;
    }
    int reportTicks() const {
        return reportTicks_;
    }
    int nextOrdinal() const {
        return activeSlots_.empty() ? ordinal_ : activeSlots_.at(size_t(ordinal_));
    }
    int elapsedRunTicks() const {
        return elapsedRunTicks_;
    }
    bool extraGoatAwarded() const {
        return extraGoatAwarded_;
    }
    int livesBonus() const {
        return livesBonus_;
    }

  private:
    Phase phase_ = Phase::Playing;
    int reportTicks_ = 0, ordinal_ = 0, elapsedRunTicks_ = 0;
    bool extraGoatAwarded_ = false;
    Bank *bank_ = nullptr;
    std::vector<int> activeSlots_;
    int startingOrdinal_ = 0, livesBonus_ = 0, oldPureScore_ = 0, oldLifetimeScore_ = 0,
        oldFastestTime_ = 0;
    bool lifetimeImproved_ = false, pureScoreBeaten_ = false, fastestTimeBeaten_ = false;
    void observeCompletion(StageRuntime &);
    void observeGameOver(StageRuntime &);
    void storeLevelRecord(StageRuntime &);
    void finishRecords(StageRuntime &, bool complete);
    void endReport(StageRuntime &, StageHud &, bool input);
    void report(StageRuntime &, StageHud &);
};
} // namespace goatup2
