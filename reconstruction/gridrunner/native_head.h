#pragma once
#include "native_entity.h"
namespace gr::reconstruction::ios {
class Head;
// The subset of the original ac used by Head. Game and ac remain distinct.
struct HeadAcState {
    float timeScale = 1, laserX = 0;
    int32_t deathRepel = 0, liveThreats = 0;
    bool laserFrozen = false;
    bool bottomTrigger = false, leftTrigger = false; // +26a4,+26a5
    int32_t laserCharge = 50;                        // +f0
    Rgba headTint, bodyTint;
    std::array<int32_t, 8> powers{};                                     // +26b8
    float hitFlash = 0;                                                  // +8
    int32_t level = 0, dropChance = 0, dropIncrement = 0, dropReset = 0; // +cc,e0,2750,276c
    bool warpUsed = false;                                               // +20
    bool completed = false, shipActive = false, gameOver = false;        // +104,+105,+1
    int32_t lives = 3, phase = 0, phaseTimer = 0;                        // +d0,+d8,+d4
    int32_t deflectionCount = 0, gridsWithoutDeath = 0;                  // +2694,+2698
    int32_t swarmerKills = 0; // +2690, original achievement counter
    // liveThreats is original +2798, shared by Heads AND pickups.
};
struct HeadExplosion {
    Vec2 position;
    int32_t sprite;
    float minSpeed, maxSpeed;
    uint32_t colour;
    int32_t count;
    float radius;
};
struct HeadBonus {
    Vec2 position;
    int32_t sprite;
    float radius;
    uint32_t colour;
};
struct HeadOutline {
    Vec2 position, velocity;
    float angle, radius, growth;
    int32_t sprite = 33, colour = 4;
    float fade = .05f;
};
// Module-local services for the original game calls.
struct HeadWorld {
    HeadAcState ac;
    uint32_t clock = 0;
    const Vec2 *shipPosition = nullptr;
    Ship *ship = nullptr;
    bool survival = false;
    virtual ~HeadWorld() = default;
    virtual void touchEnemyList(Head &) = 0; // original controlBlock::touch
    virtual void checkSweep(Head &);         // native default: original helper 0x4f0d0
    // Original game/ac call boundaries. Until the concrete routines exist,
    // fail visibly rather than substitute approximate spawns or skip RNG.
    virtual void makePod(Vec2, bool) {
        pending();
    }
    virtual void score(int32_t) {
        pending();
    }
    virtual void sound(int32_t) {
        pending();
    }
    // Local notification hook; Campaign binds this separately
    // from any obsolete online service. Isolated mechanics hosts may omit it.
    virtual void achievement(int32_t) {}
    virtual void explode(const HeadExplosion &) {
        pending();
    }
    virtual void bonus(const HeadBonus &) {
        pending();
    }
    virtual uint32_t globalRandom() {
        pending();
    } // iOS _rand
    // Receiver is head+24, but iOS 0x99f8 ignores it and consumes global rand.
    virtual double headRandomRange(Head &, double, double) {
        pending();
    }
    virtual void pickup(Vec2, int32_t) {
        pending();
    }
    virtual void outline(const HeadOutline &) {
        pending();
    }

  private:
    [[noreturn]] static void pending() {
        throw std::logic_error("Original Head game/ac service not bound");
    }
};
struct HeadSample {
    Vec2 position;
    float opacity = 0;
    int32_t mode = -1, returnMode = 0;
    float target = 0, turnStep = 0;
    int32_t turning = 0;
    float angle = 0;
};
struct HeadSpawn {
    float x = 0, y = 0, baseSpeed = .004f;
    int32_t cycle = 0, headArmour = 0, bodyArmour = 0, separation = 10, turnTicks = 16;
    // Corresponds to the already-produced random bit when init needs it.
    uint32_t randomBit = 0;
};
class Head final : public Entity {
  public:
    explicit Head(HeadWorld &sourceWorld) : world(sourceWorld), ac(sourceWorld.ac) {}
    HeadWorld &world;
    HeadAcState &ac;                              // original +168 game / +16c ac references
    Head *parent = nullptr;                       // +174; retained on promotion as in the original
    float baseRadius = .03f, damageRadius = .08f; // +178,+17c
    int32_t armour = 0, originalArmour = 0;       // +180,+184
    uint32_t frame = 0;
    int32_t separation = 10; // +188,+18c
    bool bump = false;
    int32_t headCooldown = 0, podCooldown = 0, deathCountdown = 0; // +190..19c
    int32_t baseSprite = 20;                                       // +1a0
    std::array<Vec2, 32> positions{};                              // +1a4
    std::array<float, 32> opacities{};                             // +3a4
    std::array<int32_t, 32> modes{}, returnModes{};                // +428,+4ac
    std::array<float, 32> targets{}, turnSteps{};                  // +534,+5b8
    std::array<int32_t, 32> turnCounts{};                          // +63c
    std::array<float, 32> angles{};                                // +6c0
    float opacity = 0;
    int32_t mode = 0, returnMode = 0, turnTicks = 16;
    float target = 0, turnStep = 0;
    int32_t turning = 0;
    // Entity::angle is the source +84 draw angle. This is source +740.
    float movementAngle = 0, speed = .004f;
    void initialize(const HeadSpawn &spawn, Head *parentHead, uint32_t flags);
    void run() override;
    void hitBy(Entity &other) override;
    void didHit(Entity &other) override;
    void normalTurn();       // iOS 0x4e6a8
    void diagonalBoundary(); // iOS 0x4e810
    void checkSweep();       // iOS 0x4f0d0; power 7, distinct from the laser row sweep
    HeadSample historyAt(uint32_t slot) const;

  private:
    void startTurn(float increment, float destination);
    void writeHistory();
};
} // namespace gr::reconstruction::ios
