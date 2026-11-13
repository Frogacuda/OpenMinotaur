// Cell movement/contact block of mainGameBall::run (6364c), independent of its
// particle mesh, health/completion prepass and touch-ripple presentation.
#pragma once
#include "level_setup.h"
#include "neon/source_random.h"

namespace deflex {
struct BallFlight;
struct GameplayFeedback {
    bool voicePlayed = false;
    int pendingParticleLoss = 0, reactionFrames = 0;
};
struct BallFlightEvents {
    virtual ~BallFlightEvents() = default;
    virtual void note(int index) = 0;
    virtual void trail(int count, const BallFlight &ball) = 0;
    virtual void spark(int count, const BallFlight &ball) = 0;
    virtual void blow(int x, int y) = 0;
    virtual void sample(int index) = 0;
    virtual void contactTargets(BallFlight &ball) = 0;
    virtual void contactHazards(BallFlight &ball) = 0;
};
struct BallFlightContext {
  private:
    GameplayFeedback standaloneFeedback_;

  public:
    // World-owned persistent state, shared by all balls. In particular voice
    // eligibility and the static note cue must not reset on each frame/call.
    LevelSetup &level;
    DeflectorInput &input;
    neon::DarwinRandom &random;
    BallFlightEvents &events;
    bool demo = false, secondaryTouch = false;
    bool &voicePlayed;
    int &pendingParticleLoss;
    int &demoReactionFrames;
    // The source note cue is static/shared between balls, not per-instance.
    std::optional<CellCoords> cuedNote;
    BallFlightContext(LevelSetup &board, DeflectorInput &controls, neon::DarwinRandom &rng,
                      BallFlightEvents &sink, GameplayFeedback *shared = nullptr)
        : level(board), input(controls), random(rng), events(sink),
          voicePlayed((shared ? *shared : standaloneFeedback_).voicePlayed),
          pendingParticleLoss((shared ? *shared : standaloneFeedback_).pendingParticleLoss),
          demoReactionFrames((shared ? *shared : standaloneFeedback_).reactionFrames) {}
    BallFlightContext(const BallFlightContext &) = delete;
    BallFlightContext &operator=(const BallFlightContext &) = delete;
};
struct BallFlight {
    neon::vec2d position, logical, velocity;
    CellCoords previousCell;
    float radius = 0.03125f, turnSpeed = 0;
    int left = 0, right = 0, top = 0, bottom = 0;
    bool turnedInCell = false;
    void init(const LevelSetup &level, bool demo = false);
    struct Prediction {
        neon::vec2d next;
        CellCoords boundary;
        bool queuedAtEntry;
    };
    // Call before the original completion/touch/particle prepass. Its result is
    // kept while those callbacks run, then consumed by advance in source order.
    Prediction predict(const CellGrid &grid, bool placementQueued) const;
    void advance(const Prediction &prediction, BallFlightContext &context);
};
} // namespace deflex
