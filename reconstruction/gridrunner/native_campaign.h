#pragma once
#include "native_laser.h"
#include "native_pickup.h"
#include "native_pod.h"
#include "native_enemies.h"
#include "native_cannons.h"
#include <string>
#include <functional>
namespace gr::reconstruction::ios {
// Source-led implementation of ac's actual script language. It does
// not call the former Gameplay/WaveScript implementation.
class LevelProgram {
  public:
    std::array<int32_t, 26> variables{};
    int32_t wait = 0, value = 0, launchType = 0;
    explicit LevelProgram(int32_t level = 0, bool survival = false);
    void step(int32_t priorLive, const std::function<void(int32_t, int32_t)> &launch);

  private:
    size_t pc_ = 0;
    struct Loop {
        size_t pc;
        int32_t remaining;
    };
    std::vector<Loop> loops_;
    int32_t number();
    std::string script_;
};
class Campaign final : public HeadWorld {
  public:
    enum class State { Playing, Completing, Complete, GameOver };
    static constexpr int32_t normalScriptCount = 23, survivalScriptCount = 6;
    explicit Campaign(std::function<Vec2(int32_t)> textureSize, uint32_t seed = 1,
                      uint32_t startClock = 0, int lives = 3, bool demo = false, int32_t level = 0,
                      bool survivalMode = false, int32_t score = 0,
                      DarwinRandom *sharedRandom = nullptr);
    ~Campaign();
    void tick(Vec2 directDisplacement);
    void updatePalette(const std::array<Rgba, 33> &);
    State state() const {
        return state_;
    }
    int points() const {
        return points_;
    }
    int launches() const {
        return launches_;
    }
    bool advanceLevel();
    int32_t level() const {
        return ac.level;
    }
    uint32_t gridColour() const;
    int32_t soundVolume(int32_t sample) const {
        return sample >= 0 && sample < 65 ? soundGains_[size_t(sample)] : 7 << 12;
    }
    void soundAt(int32_t sample, int32_t volume);
    float feedback() const {
        return feedback_;
    }
    float presentedFeedback() const {
        return feedbackColour_;
    }
    void clearPausedFeedback() {
        feedback_ = 0;
    }
    EntityLists lists;
    TouchCollider collision;
    DarwinRandom ownedRandom;
    DarwinRandom &random; // original global stream; isolated ownership for standalone checks
    std::array<Rgba, 33> palette{};
    std::array<Rgba, 8> pickupTints{};
    Effects effects;
    std::vector<int32_t> cells = std::vector<int32_t>(1200, 0);
    std::vector<int32_t> sounds;
    std::vector<int32_t> achievements; // original local goal events, once per run
    void achievement(int32_t) override;
    LevelProgram program;
    Ship *currentShip() const;
    void makePod(Vec2, bool) override;
    void score(int32_t) override;
    void sound(int32_t) override;
    void touchEnemyList(Head &) override;
    void explode(const HeadExplosion &s) override {
        effects.explode(s);
    }
    void bonus(const HeadBonus &) override;
    uint32_t globalRandom() override {
        return random.next();
    }
    double headRandomRange(Head &, double lo, double hi) override {
        return random.range(lo, hi);
    }
    void pickup(Vec2, int32_t) override;
    void outline(const HeadOutline &s) override {
        effects.outline(s);
    }

  private:
    // theGame +c6c is shared by all producers; +d0..dc publish it before actors.
    float feedback_ = 0, feedbackColour_ = 0;
    struct Ships final : ShipWorld {
        Campaign &game;
        explicit Ships(Campaign &);
        ShipInput readInput() override;
        void sound(int32_t) override;
        void stopSound(int32_t) override;
        void achievement(int32_t n) override {
            game.achievement(n);
        }
        void livesChanged(int32_t) override {} // frontend reads original count
        void finishGame() override;
        void unbindShip(Ship &) override;
    } ships_;
    struct Pickups final : PickupWorld {
        Campaign &game;
        explicit Pickups(Campaign &);
        void sound(int32_t) override;
        void score(int32_t) override;
    } pickups_;
    struct Pods final : PodWorld {
        Campaign &game;
        explicit Pods(Campaign &);
        void sound(int32_t) override;
        void score(int32_t) override;
        void fireRing(const Entity &, int32_t) override;
        void spawnSwarmers(const Entity &, int32_t) override;
    } pods_;
    struct Lasers final : LaserWorld {
        Campaign &game;
        explicit Lasers(Campaign &);
        void sound(int32_t) override;
        void makePod(Vec2, bool) override;
        void barrel(Vec2) override;
        void zakato(Vec2, Vec2) override;
    } lasers_;
    struct Enemies final : EnemyWorld {
        Campaign &game;
        explicit Enemies(Campaign &);
        void sound(int32_t, int32_t) override;
    } enemies_;
    Vec2 input_{};
    State state_ = State::Playing;
    int points_ = 0, launches_ = 0, completionTicks_ = 0;
    int32_t laserTimer_ = 270, laserPeriod_ = 270, podTimer_ = 135, podPeriod_ = 135,
            podQuota_ = 9999;
    bool madePod_ = false;
    bool demo_ = false;
    uint32_t goalsDone_ = 0;
    std::array<int32_t, 65> soundGains_{};
    void initLevel();
    void launch(int32_t count, int32_t type);
    void runController();
};
} // namespace gr::reconstruction::ios
