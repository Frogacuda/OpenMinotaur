#pragma once
#include "animation.h"
#include "ground.h"
#include "jump.h"
#include "stage_bank.h"
#include "vertical.h"
#include <memory>
#include <string>
#include <vector>

namespace goatup2 {
class StageRuntime;
class EnemyActor;
int32_t fruitSpriteForStage(int32_t stageIndex);
int32_t pickupSpriteForStage(int32_t stageIndex, int variant);

// Values not written by the source constructor are supplied by allocation/
// session policy. Fresh-start observations are not universal constructor defaults.
// Each field below documents its first consumer and initialization responsibility.
struct StageAllocationInputs {
    // Initial wet state affects the goat's first hunger tick and water entry.
    // changedMediumFlags is overwritten by each medium query before use.
    uint32_t mediumFlags, changedMediumFlags;
    int32_t mediumEntryAge;
    // Live goat updates clear this before movement. Bubbles also read it, so
    // their first-tick allocation input remains explicit.
    bool oppositeContact25a;
    // Stage constructor does not write +1a4. Ordinary particles run horizontal
    // before falling first assigns it; do not silently initialize it from g.
    float effectiveGravity;
    // Patroller factory writes animation weight X=80, but not Y. Explicit
    // host allocation default for that cosmetic input, not an original constructor store.
    double enemyAnimationVertical = 0;
    // 0x5ced8 leaves its child's +72c8 spin unwritten; explicit fresh host
    // allocation input for 0x62018, not an inferred original constructor store.
    float cannonballSpin = 0;
    // Split-child creation does not assign +21c; explicit host allocation input,
    // separate from the placed parent factory's .00035f gravity.
    float splitChildGravity = 0;
};

// Owned state shared by movement, contacts and effects. The object
// is a native actor rather than a binary-compatible guest layout.
class StageActor {
    friend class StageRuntime;
    StageRuntime *world_;

  public:
    StageActor(StageRuntime &owner, StageAllocationInputs allocation, float gravity, bool player);
    virtual ~StageActor() = default;
    virtual void run() = 0;
    virtual void onContact(StageActor *other); // original slot +18; base is empty
    virtual void hitBy(StageActor *other);     // original stageEntity slot +14
    virtual void wasShot(StageActor *other);
    StageActor *prev = nullptr;
    StageActor *next = nullptr;
    bool dead = false;
    const bool player;
    StageRuntime &owner() const {
        return *world_;
    }
    neon::vec2d worldPosition{0, 0}, stagePosition{0, 0}, velocity{0, 0};
    neon::vec2d drawScale{1, 1}, savedDrawScale{1, 1}, travelled{0, 0};
    neon::vec2d damping{.99, .99};
    float renderOffset = 0; // host allocation storage; goat factory overrides +1ac
    neon::colour colour;
    // Aliases bind the established engine contact API to this same owned state.
    // They are not copied positions, collision bodies or a second target list.
    neon::vec2d &pos = worldPosition;
    neon::vec2d &vel = velocity;
    uint32_t mediumFlags, changedMediumFlags, motionFlags = 3, tileInteractionMask = 0;
    int32_t mediumEntryAge, bubbleThreshold = 32, freezeTicks = 0, hopCounter = -1, hopPeriod = -1;
    // Dormant host storage, not claimed source initialization. Source never
    // reads these while hopCounter=-1; factories enabling hopping must set them.
    neon::vec2d hopSpeed{0, 0};
    double &hopSpeedX = hopSpeed.x;
    double &hopSpeedY = hopSpeed.y;
    float gravity, effectiveGravity, buoyancy = -.7f, submergedDamping = .99f;
    float radius = .02f, elasticity = 1, heading = 0, sinkOffset = 0;
    float &size = radius;
    float mass = 1, contactHealth = 1, attackPower = .3f;
    int32_t group = 0, flags = 0, drawDelay = 0, category = 5, age = 0, flashTicks = 0;
    float stageDrawScale = 1, stretch = 1; // stage constructor +210/+170
    int32_t serial = -1, rewardValue = 100, headbuttState = 0;
    int32_t paletteIndex = -1, followDelay = 0, targetDelay = 0, followedState = 0;
    neon::vec2d savedStagePosition{0, 0};
    bool active = true, touching = false, hit = false;
    bool directWorldDraw = false;  // actual base-entity overlays bypass stage draw
    bool readyForContacts = false; // source +255: set by logical draw preparation
    double horizontalImpulse = 0, verticalImpulse = 0;
    StageContactPolicy policy;
    bool resting = true, landed = false, verticalContact = false, horizontalContact = false;
    bool oppositeContact259 = false, oppositeContact25a, hazard = false, turnSignal = false;
    int32_t ledgeGrace = 0;
    // Host storage until the first source contact writes the cached cursor.
    int32_t supportColumn = 0, supportRow = 0;
    // Player-only writes in vertical contact; unused for other subclasses.
    int32_t forcedJumpTicks = 0;
    int32_t sprite = 140;

    MediumActorView mediumView();
    HorizontalActorView horizontalView();
    VerticalActorView verticalView();
    void moveHorizontal();
    void moveVertical();
    void fall();
    void prepareWorldPosition();
    void followGoat(); // shared source operation 0x58d00
    virtual void preparePresentation();
    StageActor(const StageActor &) = delete;
    StageActor &operator=(const StageActor &) = delete;
};

class GoatActor final : public StageActor {
  public:
    GoatActor(StageRuntime &, StageAllocationInputs, neon::vec2d spawn, bool inverted,
              neon::vec2d spriteScale);
    void run() override;
    void onContact(StageActor *) override;
    void preparePresentation() override;
    GoatCheckpointHistory checkpoints;
    float hunger = .5f, zoom = 4, forcedJumpSpeed = 0, emissionRemainder = 0;
    bool heldJumpFinished = true, jumpConsumed = false, headbuttFlag = false;
    int32_t heldJumpTicks = 0, invulnerability = 150, activeTicks = 0;
    int32_t state = 0, followingCount = 0;
    int32_t attractCountdown = -10;    // +72d4, settle then camera-sweep counter
    int32_t lastCheckpointDevice = -1; // +72c8, case-3 placement slot identity
    bool deathSoundSuppressed = false;
    int32_t animationMode = 1, baseSprite = 107;
    neon::vec2d animationWeights{20, 0}; // type-0 descriptor 0x7e498
    // Factory draws exist even when this actor is controlled interactively.
    double attractFrequencyX, attractFrequencyY;
    neon::vec2d attractCameraTarget; // goat +72d8/+72e0, not player intent
    EnemyActor *riding = nullptr;    // source machine behavior 13, not a second body
    void animate();
    void resetFromCheckpoint();
    struct FollowerSample {
        neon::vec2d position;
        float gravity, speed;
        int32_t state;
    };
    std::array<FollowerSample, 1024> followerHistory;
    uint32_t followerCursor = 0;
};

class KidActor final : public StageActor {
  public:
    KidActor(StageRuntime &, StageAllocationInputs, neon::vec2d spawn, bool inverted,
             neon::vec2d spriteScale);
    void run() override;
    void hitBy(StageActor *) override;
    void onContact(StageActor *) override;
    float originalRadius = .02f;
};

class PickupActor final : public StageActor {
  public:
    PickupActor(StageRuntime &, StageAllocationInputs, neon::vec2d spawn, uint32_t flags);
    void run() override;
    void hitBy(StageActor *) override;
    void preparePresentation() override;
    bool drawMarker = false, drawMain = true;
    neon::vec2d markerWorldPosition;
};
class GrassBonusActor final : public StageActor {
  public:
    GrassBonusActor(StageRuntime &, StageAllocationInputs, int32_t column, int32_t row,
                    uint8_t grass);
    void run() override;
    void hitBy(StageActor *) override;
    void award();
    int32_t bonusState = 0;
};
class ExitActor final : public StageActor {
  public:
    ExitActor(StageRuntime &, StageAllocationInputs, neon::vec2d spawn);
    void run() override;
    void hitBy(StageActor *) override;
};

struct RuntimeSound {
    int32_t sample;
    uint32_t gain;
    int32_t sourceOption;
    enum class Kind { Play, Stop };
    Kind kind = Kind::Play;
};

class FloatingScoreActor : public StageActor {
  public:
    FloatingScoreActor(StageActor &target, int32_t value);
    void run() override;
    void preparePresentation() override;
    void setNumber(int32_t number);
    int32_t value, firstDigit = 14, lastDigit = 15, firstGlyph = 29;
    int32_t iconSprite = 72, iconPaletteIndex = 3;
    // Unused slots are host storage. The source writes the selected digit range.
    std::array<int32_t, 16> digits{};
    float life = 1, fade = .06f, growth = 1.001f, layoutScale = 1;
    bool screenSpace = false;
    neon::vec2d glyphScale;
};
class ExitTotalizer final : public FloatingScoreActor {
  public:
    explicit ExitTotalizer(StageActor &);
    void run() override;
    int32_t observedTotal = 100;
};
struct StatusMessage {
    std::string text;
    int32_t ticks = 0, row = 0, style = 0;
};

// Owned stage runtime. Remaining placed classes and the complete session/backend
// binding are tracked in the current reconstruction record.
class StageRuntime final : public MediumOutput,
                           public ContactParticleOutput,
                           public GroundOutput,
                           public HeadbuttParticleOutput,
                           public JumpActions {
  public:
    StageRuntime(TerrainLayout layout, TerrainBounds bounds, std::vector<uint32_t> working,
                 std::vector<uint32_t> backup, StageAllocationInputs allocation,
                 MediumSharedState shared, uint32_t seed);
    ~StageRuntime();
    StageRuntime(const StageRuntime &) = delete;
    StageRuntime &operator=(const StageRuntime &) = delete;

    TerrainLayout layout;
    TerrainBounds bounds;
    std::vector<uint32_t> working, backup;
    // Original mutable 0x7cf90 table; timed-tile drawing updates three words
    // also read by collision queries. Session transitions must retain it.
    std::array<uint32_t, 98> glyphLookup;
    std::vector<uint32_t> initialWorking, initialBackup;
    neon::vec2d gridOffset{0, 0};
    TerrainCursor cursor{0, 0};
    uint32_t clock = 0;
    // Explicit owner state. Callers supply source palette/rainbow when rendering
    // or source palette setup is connected; no claim of original game defaults.
    std::array<neon::colour, 32> terrainPalette;
    neon::colour rainbow;
    MediumSharedState medium;
    // Session +44/+4c, paired with medium's shared negative/positive ramps.
    float downwardCurrentPhase = 0, upwardCurrentPhase = 0;
    void resetCurrents();   // 0x4ca24 stage entry and 0x45d38 editor test
    void advanceCurrents(); // 0x4310a, including while paused
    StageAllocationInputs allocation;
    neon::DarwinRandom random;
    std::unique_ptr<GoatActor> goat;
    StageActor *ordinaryEffects = nullptr;   // source +3ac
    StageActor *foregroundEffects = nullptr; // source +3bc
    StageActor *contactObjects = nullptr;    // source +3c0
    StageActor *enemies = nullptr;           // source +3c4
    StageActor *grassBonuses = nullptr;      // source +3c8
    StageActor *overlayEffects = nullptr;    // source +3b4, direct screen-space actors
    StageActor *placementOverlays =
        nullptr; // source +3c908; native placement binding is data-owned
    std::vector<RuntimeSound> sounds;
    int32_t score = 0, stageTotal = 0, highScore = 0, recordCounter = 0, multiplier = 1;
    bool newRecord = false, exitContact = false, headbuttEvent = false;
    bool headbuttDisabled = false;
    int32_t sharedGrace = 0;
    uint8_t animationFrameFlags = 0; // source game +37c, separate from terrain clock
    int32_t jumpInput = 0;
    int32_t attractTicks = 0;  // owner +c44, positive selects camera demo branch
    double movementIntent = 0; // source +3c8e8, not a normalized controller axis
    int32_t gravityDirection = 1, hazardContactCount = 0;
    int32_t activeFollowers = 0, exitOrdinal = 1, exitBonus = 0;
    int32_t requiredFruit = 0, initialFruit = 0, fruitPaletteIndex = 0, stageIndex = 0;
    int32_t grassSequence = 1, bonusPickups = 0, bestStageTime = 0, timeRecordCounter = 0;
    bool unlockExit = false, newBestStageTime = false;
    std::array<StatusMessage, 4> messages;
    float exitBonusStep = 0, exitBonusProgress = 0, sourceTime = 0;
    // Kid's +1b8 copies the placement-factory receiver's base entity radius
    // (+134), not the kid descriptor. Session construction must supply its value.
    float placementReceiverRadius = .02f;
    bool alternateBurstSprite = false;
    int32_t cameraSettleFrames = 0, dropFruitSequence = 0;
    bool originalPauseFlag = false;
    int32_t lives = 3, mode = 0, modeDetail = 0, stageTicks = 0, exitFadeTicks = 0;
    bool deathDisabled = false, terminal = false, playerVisible = true;
    float sceneFade = 1, displayFade = 1, inputFriction = .99f, savedInputFriction = .02f;
    std::vector<int32_t> textRowsToClear; // source character-row clearing 0x35280
    // Asset-derived 0xa8f0 ratios; populated by session resource setup. Missing
    // sprite metadata is rejected when a source factory actually requires it.
    std::vector<neon::vec2d> spriteScales;

    void spawnGoat(neon::vec2d position, bool inverted, neon::vec2d spriteScale);
    void spawnKid(neon::vec2d position, bool inverted);
    void spawnPickup(neon::vec2d position, uint32_t flags);
    void spawnExit(neon::vec2d position);
    void spawnOriginalObjects(const PlacementTable &);
    void spawnGrassBonuses();
    // Source stage setup reparses terrain after live construction, takes the
    // backup, then writes tutorial labels. Seal that prepared restart image.
    void installPreparedTerrain(std::vector<uint32_t> cells, std::vector<uint32_t> saved);
    // Explicit source input and clock at this boundary; does not fake touches.
    // Runs real births in +3bc before +3ac after the goat, as in 0x4567c.
    void advanceLocomotion(double sourceMovementIntent, int32_t sourceJumpInput,
                           uint32_t sourceClock, uint8_t sourceAnimationFrameFlags);
    void beginLogicalFrame();
    void runLocomotion(double sourceMovementIntent, int32_t sourceJumpInput, uint32_t sourceClock,
                       uint8_t sourceAnimationFrameFlags);
    void advanceEffects();
    void preparePresentation();
    void retainEffectsFrom(StageRuntime &previous);
    void clearActors(); // 0x40958 full game teardown, unlike stage entry
    HorizontalTerrainView horizontalTerrain();
    VerticalTerrainView verticalTerrain();
    GroundScoreView scoreView();
    neon::vec2d spriteScale(int32_t sprite) const;
    int collide(StageActor &actor, StageActor *targets);
    void editorRemovalFlash(neon::vec2d); // same 0x80e78 class used by 0x52990
    void explode(StageActor &actor, int32_t fragments);
    void beginDeath(GoatActor &actor);
    void emitRadialBurst(StageActor &actor, int32_t count, float speed, float spread,
                         float strength);
    void emitCollectionBurst(StageActor &actor);
    FloatingScoreActor &showScore(StageActor &, int32_t number, float scale = 5.f,
                                  int32_t palette = 3, int32_t icon = 0);
    void addScore(int32_t amount);
    void explodeHeadbuttTarget(StageActor &, int32_t count);
    void createHeadbuttMultiplierLabel(StageActor &, int32_t value);

    void adoptBubble(const BubbleBirth &) override;
    void adoptSplash(const SplashBirth &) override;
    void adoptTileFragment(const TileFragmentBirth &) override;
    void adoptImpact(const StageParticleBirth &) override;
    void adoptGroundParticle(const GroundParticleBirth &) override;
    void adoptHeadbuttParticle(const StageParticleBirth &) override;
    void playSample(int32_t, uint32_t, int32_t) override;
    void updateMovementAnimation() override;
    bool attemptHeadbutt() override;
    void emitHeadbuttParticles(float, float, neon::DarwinRandom &) override;
};
} // namespace goatup2
