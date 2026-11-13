#pragma once
#include "stage_runtime.h"
namespace goatup2 {
// Source factory case 3 and six device records; not a pickup subclass.
class DeviceActor final : public StageActor {
  public:
    DeviceActor(StageRuntime &, neon::vec2d, int variant, int placementSlot);
    void run() override;
    void hitBy(StageActor *) override;
    int variant, placementSlot;
    bool contactLatch = false;
};
// Original patroller/floater/machine records for the campaigns and editor.
class EnemyActor final : public StageActor {
  public:
    EnemyActor(StageRuntime &, neon::vec2d, int kind, uint32_t flags);
    // Contact-born child follows 0x5d090, not the table/placement factory.
    struct SplitChild {};
    EnemyActor(EnemyActor &parent, SplitChild);
    void run() override;
    void hitBy(StageActor *) override;
    void preparePresentation() override;
    bool patroller;
    int variant, behavior, animationMode, baseSprite, launchCooldown = 0;
    int attackCooldown = 0;     // +1bc, distinct from +1ec motion/colour freeze
    bool riderAttached = false; // machine +72c8, behavior 13's 0/1 occupancy
    bool charging = false;
    int kind, cycleState = 0, fireInterval = 0;
    int splitDepth = 0; // +72c0, distinct from +1c0 launch cooldown
    unsigned placementTransform = 0;
    float steeringRange = 0, returnRadius = 0; // latter is read only after mode 8 -> 88
    neon::vec2d animationWeights;
    // The source +17c/+184 pair is BOTH the steering speed limit and the
    // horizontal routine's water-hop velocity. Never keep a second copy.
    neon::vec2d &maximumSpeed = hopSpeed;
    neon::vec2d originalPosition, originalVelocity;
    float steeringAngle = 0, steeringRate = 0, spriteAngleOffset = 0;
    float verticalRebound = 0;
    void initializeMachine();
    void runMachine();
    void fireMachine();
};
class EnemyShotActor final : public StageActor {
  public:
    explicit EnemyShotActor(StageActor &source); // patroller 0x5dfe0 case 6
    void run() override;                         // 0x61f58
    int lifetime = 1000;
    float spin;
    // Distinct constructor for 0x5bb70's aimed, non-gravitating machine shot.
    EnemyShotActor(EnemyActor &source, bool aimed);
    struct Sideways {};
    EnemyShotActor(EnemyActor &source, Sideways); // 0x5bb70 mode 16
};
class EnemyBombActor final : public StageActor {
  public:
    explicit EnemyBombActor(StageActor &source); // 0x5bb70 case 2
    void run() override;                         // 0x61edc
    int lifetime = 1000;
};
// Source angle operations 0x9aa0 / 0x9b28; coordinates narrow before atan2.
float originalAimAngle(neon::vec2d from, neon::vec2d to);
float originalSteeringStep(neon::vec2d from, neon::vec2d to, float step, float current);
class CannonballActor final : public StageActor {
  public:
    explicit CannonballActor(StageActor &source); // 0x5ced8
    void run() override;                          // 0x62018
    void hitBy(StageActor *) override;            // 0x621a8
    int bounces = 10, lifetime = 500;
    bool hasLeftWater = false;
    float spin, previousVerticalSpeed = 0;
};
class DroppedFruitActor final : public StageActor {
  public:
    explicit DroppedFruitActor(StageActor &);
    void run() override;
    void hitBy(StageActor *) override;
};
} // namespace goatup2
