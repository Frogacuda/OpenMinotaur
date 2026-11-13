#pragma once
#include "climber.h"
#include "effects.h"
#include <memory>

namespace goatup {
// Original save counters used by this family. The enclosing core serializes
// them; the host achievement collection never supplies gameplay state.
struct CollectionRecord {
    std::array<uint32_t, 16> bonuses{};
    int uniqueBonuses = 0, billyGoats = 0, mostKids = 0, highestPlatform = 0;
};
struct DenizenContext {
    ClimbContext &game;
    Climber &goat;
    neon::DarwinRandom &random;
    Effects &effects;
    CollectionRecord &records;
    std::array<uint32_t, 16> runBonuses{};
    int uniqueRunBonuses = 0, runBillyGoats = 0, runBirths = 0, score = 0;
    int mode = 0, hazardGrace = 0, hazardWarning = 0, hazardFlash = 0, pendingKidLoss = 0,
        rewardKids = 0;
    // theGoat snapshots its kid count at the start of each active player run
    // (native 3c4); all later contact rewards in that tick use this value.
    uint32_t achievements = 0;
    bool paused = false, tablet = false;
    float flash = 0; // Original +434 request; no active read identified.
    // Original +3f4 is one shared value: poison timer, player gravity/jump
    // variation, terrain brightness, caption gate and feedback intensity.
    float &poison = game.tower.fade;
    neon::colour cyclingBonusColour{}, vulnerableColour{}, protectionColour{};
    // Required synchronous original feedback sites. Sound includes the native
    // gain shift (not final device gain). Popup does not itself credit score.
    std::function<void(int, int)> sound{};
    std::function<void(int)> achievement{}, bonusCaption{};
    std::function<void(int, int)> status{};
    std::function<void(int, Vec2, double, double, float)> popup{};
    std::function<Vec2(int)> spriteSize{};
    std::function<bool(Platform &)> checkBridge{};
    void award(int);
    void play(int sample, int gainShift = 12);
    void showScore(int points, Vec2, float vx, float vy, float radius);
};

// NPC list; births enqueue loveHeart children on Effects' original UI list.
// Births are synchronous in platform
// preparation; updates and draw-suppression counters retain their native order.
class Denizens {
  public:
    explicit Denizens(DenizenContext &);
    ~Denizens();
    Denizens(const Denizens &) = delete;
    Denizens &operator=(const Denizens &) = delete;
    void reset();
    void create(const PlatformBirth &);
    void tick();
    void tickBits();
    void homecoming();
    void drawBits(std::vector<ActorDraw> &);
    void prepareDraw(std::vector<ActorDraw> &);
    bool touch(Vec2 position, float radius, int type, bool &touching, int drawSkip = 0,
               bool dead = false);
    // Dispatch from the source contact traversal, after its overlap/latch test.
    // A kid supplies type=2 and its own position; type=1 is the main goat.
    void hit(int ordinal, int type, Vec2 sourcePosition);
    struct Contact {
        int ordinal;
        Vec2 position;
        float radius;
        int drawSkip;
        bool dead;
    };
    std::vector<Contact> contacts() const;
    int count() const;

  private:
    struct Actor;
    Actor *actors_ = nullptr;
    Actor *bits_ = nullptr;
    DenizenContext &context_;
    void heart(float surfaceAngle, double y, double xRange, double yRange);
    bool touchImpl(Vec2, float, int, bool &, int, bool, Actor *);
    void touchActor(Actor &);
};
} // namespace goatup
