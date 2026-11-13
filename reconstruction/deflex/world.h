#pragma once
#include "main_ball.h"
#include "particles.h"
#include "grid_view.h"
#include <memory>
namespace deflex {
// The complete original standard sequence, including WHAT A PONG's startup.
inline constexpr int kPlayableLevelCount = level::kLevelCount;
struct Sound {
    int sample, volume;
};
enum class WorldMode { Standard, Attract, Scenery };
struct Progress {
    std::array<int, 52> bestMoves;
    std::array<int, 52> bestScores{};
    std::array<int, 52> lastScores{};
    std::array<bool, 16> achievements{};
    int volume = 8;
    Progress() {
        bestMoves.fill(1000);
    }
};
class World final : public LevelBirths,
                    public ActorServices,
                    public BallFlightEvents,
                    public DeflectorServices,
                    public BallRecords {
    std::list<std::unique_ptr<Entity>> targets_, hazards_;
    std::unique_ptr<MainGameBall> ball_;
    Entity *hazardHead_ = nullptr;
    int messageIndex_ = 0, messageDelay_ = 0, musicPair_ = 0, musicScale_ = 0, musicOffset_ = 0;
    std::array<bool, 64> notePlayed_{};
    std::vector<neon::vec2d> spriteSizes_;
    neon::colour cyclingColour_;
    WorldMode mode_;
    void updateList(std::list<std::unique_ptr<Entity>> &, Entity *&);
    void contacts(BallFlight &, Entity *);
    void prependHazard(std::unique_ptr<Entity>);

  public:
    neon::DarwinRandom random;
    LevelSetup level;
    ActorContext context;
    DeflectorInput input;
    BallFlightContext flightContext;
    Particles particles;
    GridView gridView{level.grid};
    Progress &progress;
    std::vector<Sound> sounds;
    bool noteGate = false;
    bool showHUD = true;              // Frontend results reuse this world after score settlement.
    uint32_t sessionAchievements = 0; // source +2d4; separate from persisted awards
    uint32_t particleColour = 0x400080f0;
    World(int index, Progress &, const std::vector<neon::vec2d> &spriteSizes, uint32_t seed = 1,
          WorldMode mode = WorldMode::Standard);
    void beginTick(bool music = true, bool advanceClock = true);
    void resultCue();
    void tick(bool paused);
    void drawGrid(GridRenderer &, ActorRenderer &, neon::vec2d view, bool paused);
    void drawActors(ActorRenderer &, neon::vec2d view, bool overlay, bool paused);
    MainGameBall &ball() {
        return *ball_;
    }
    bool scoreSettled() const {
        return targets_.empty();
    }
    void deflector(const DeflectorButton &) override;
    void target(const TargetBirth &) override;
    void hazard(neon::vec2d, int) override;
    void weight(neon::vec2d) override;
    void bomb(neon::vec2d) override;
    void restoreBallParticles(BallFlight &, int) override;
    void sample(int id, int volume) override;
    void sample(int id) override {
        sample(id, context.volume);
    }
    void note(int) override;
    void spark(int, const Entity &) override;
    void spark(int n, const BallFlight &) override {
        spark(n, *ball_);
    }
    void derez(const Entity &, int, float, int) override;
    void plinger(int n) override {
        particles.plinger(n);
    }
    void message(const ScoreNotice &) override;
    void bonusGlow(neon::vec2d p) override {
        particles.bonusGlow(p);
    }
    void achievement(int n) override;
    void hazardParticle(const HazardParticleBirth &p) override {
        particles.hazardParticle(p);
    }
    void explosion(neon::vec2d p) override {
        particles.explosion(p);
    }
    void trail(int, const BallFlight &) override;
    void blow(int, int) override;
    void contactTargets(BallFlight &b) override {
        contacts(b, context.targets);
    }
    void contactHazards(BallFlight &b) override {
        contacts(b, hazardHead_);
    }
    void derezAllBalls(int n, float gain, float decay) override {
        ball_->mesh.derez(*ball_, n, gain, decay, random);
    }
    int bestMoves() const override {
        return progress.bestMoves[size_t(context.levelIndex)];
    }
    void setBestMoves(int n) override {
        progress.bestMoves[size_t(context.levelIndex)] = n;
    }
    bool firstFourInOneMove() const override {
        return progress.bestMoves[0] + progress.bestMoves[1] + progress.bestMoves[2] +
                   progress.bestMoves[3] ==
               4;
    }
};
} // namespace deflex
