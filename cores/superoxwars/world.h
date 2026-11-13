#pragma once
#include "neon/math.h"
#include "neon/entity_motion.h"
#include "neon/entity_list.h"
#include "neon/source_random.h"
#include "neon/game_presentation.h"
#include <array>
#include <functional>
#include <string>
#include <vector>
namespace superoxwars {
using Vec2 = neon::vec2d;
struct Colour {
    double r = 1, g = 1, b = 1, a = 1;
};
Colour unpack(uint32_t);
Colour sequenceColour(float, const uint32_t *);
Colour mixColour(Colour, Colour, float); // a0a4: interpolate and truncate RGBA bytes
struct ActorDraw {
    Vec2 position, scale{1, 1};
    float radius = .02f, angle = 0;
    int frame = 0;
    Colour colour;
    float z = 0;
    bool explicitUv = false;
    Vec2 uv{}, uvSize{}, pivot{};
};
// theGame's presentation state, distinct from moo's campaign/terrain counters.
// Original fields +14..34, +3a8 (pulse), +7c0 (shake countdown).
struct PresentationState {
    neon::FeedbackState feedback;
    Vec2 textScroll; // cellGrid +2b20/+2b28: shake moves text, NOT feedback UVs
    // Explicit inactive host defaults: no nonzero producer was identified in
    // the reviewed game paths. Do not invent hit/death triggers for these helpers.
    float pulse = 0;
    uint32_t shakeTicks = 0;
    void beginAttract();
    void beginPlay();
    void run(bool demo, neon::DarwinRandom &);
};
class World;
struct LaunchPod;
// entity constructor 31284, motion 3136c, draw 31488. Defaults here name
// constructor stores; game-specific allocation-dependent fields stay separate.
struct Entity {
    World &world;
    Entity *prev = nullptr;
    Entity *next = nullptr;
    Vec2 position, velocity, damping{.98, .98}, extent{1, 1}, scale{1, 1};
    Colour colour;
    int32_t age = 0;
    int grace = 0, type = 0, frame = 140, health = 1, group = 0, hitFlash = 0, skipDraw = 0;
    float radius = .02f, angle = 0, mass = 1, z = 0;
    bool dead = false, wrapped = false, noWrap = false, hidden = false, touching = false,
         hit = false, collidable = true;
    int collisionFlags = 0;
    explicit Entity(World &w) : world(w) {}
    virtual ~Entity() = default;
    virtual void run();
    virtual void prepare(std::vector<ActorDraw> &);
    virtual void hitBy(Entity &);
    virtual void didHit(Entity &) {}
};
// gentity adds retained game-space position and world/scene references; it
// does not replace entity's virtual run with a universal projected update.
struct GameEntity : Entity {
    Vec2 retained;
    explicit GameEntity(World &);
};
enum class Domain : size_t {
    Player,
    Decor,
    LaunchPods,
    Ambient,
    Scenery,
    PlayerShots,
    Ground,
    Air,
    Enemies,
    Auxiliary,
    Pickups,
    Foreground,
    Always,
    Count
};
struct TileScreen {
    static constexpr int Columns = 20, Rows = 16, BufferRows = 22, Stride = 21;
    std::array<int32_t, 2500> cells{};
    std::array<Vec2, 2500> positions{};
    Vec2 origin, scroll, scale{1, 1};
    int rowOffset = 0;
    int peek(int x, int y) const;
    void poke(int x, int y, int value);
    std::array<int, 2> cellAt(Vec2) const;
};
struct PlayerInput {
    Vec2 movement;
    bool alternate = false;
    Vec2 analogMovement;
    // Direct world deflection in the steady-gravity range [-1,1]. Nonzero
    // input takes priority over physical tilt; zero leaves the sensor active.
    double worldInfluence = 0;
};
// 40c9c/40dac: ten independently timed, row-keyed messages. Dynamic colours
// follow the game palette; a negative slot retains the supplied literal colour.
struct TimedMessage {
    std::string text;
    int row = 0, x = -1, ticks = 0, palette = -1;
    Colour colour;
};
struct MessageLine {
    int x = -1, row = 0;
    std::string text;
    Colour colour;
};
struct Ship : GameEntity {
    enum class State { Flying, Hit, Waiting, GameOver };
    State state = State::Flying;
    float shotSpeed = .009f, fireRate = .05f, firePhase = 1, trailDistance = 0;
    int stateTicks = 0, trailCursor = 0, power = 0, shotCycle = 0;
    bool shotThisTick = false;
    std::array<Vec2, 512> history{};
    explicit Ship(World &);
    void init(bool respawning = false);
    void run() override;
    void shoot();
    void didHit(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct PlayerBullet : GameEntity {
    int palette = 0;
    bool impacted = false;
    float arcStep = 0, arcRemaining = 0, repeatStep = 0, repeatInterval = 0, repeatClock = 0,
          repeatAngle = 0;
    int repeatCount = 0;
    explicit PlayerBullet(World &w) : GameEntity(w) {}
    PlayerBullet(World &, const Ship &, double offset, int colour, int pierces);
    // 517d0 uses entity +108, including its inherited constructor value of 1.
    void run() override;
    void didHit(Entity &) override {
        if (health > 0)
            --health;
        else
            dead = true;
    }
};
// Each original list retains its own pause gate and position in the scheduler.
// Names for +368/+36c/+30c/+308 await their family callers; domain identity is
// explicit instead of silently merging those lists with ordinary enemies.
class World {
  public:
    neon::DarwinRandom random;
    std::array<Entity *, size_t(Domain::Count)> lists{};
    PlayerInput input;
    TileScreen tiles;
    Ship *player = nullptr;
    uint64_t ticks = 0;
    bool paused = false, demo = false;
    int gameMode = 1;                  // theGame +0xc: scheduler/foreground gate
    int backgroundMode = 0;            // theGame +0x10: background gate
    int terminalTilesRemaining = 4000; // moo +0xc, NOT theGame +0xc
    PresentationState presentation;
    int lives = 3, area = 0, level = 0, score = 0, redScore = 0, blueScore = 0,
        particleBudget = 100;
    int blueRemaining = 0, redRemaining = 0, blueTotal = 0, redTotal = 0;
    bool bossPresent = false;
    uint32_t sessionAchievements = 0;
    int blueCollected = 0, redCollected = 0;
    Colour flowerColour;
    bool foxFound = false;
    bool enemyBulletOverride = false;
    int enemyBulletFrame = 2, enemyBulletPalette = 10;
    int weaponFollowers = 0; // ship::run publishes Scene +b4 before later contacts.
    std::array<int, 40> soundWanted{};
    std::array<int, 8> groupSize{}, groupKills{};
    int chain = 0, oxStreak = 0, oxTimer = 60, lifeTokens[2] = {0, 0}, lifeDropBudget = 1;
    float enemyShotChance = 40, polarityPulse = 0, polarity = 0, weaponPower = 0, weaponThreat = 0,
          scrollSpeed = .015f, healthFade = 1;
    bool relativeControls = true, tabletTouch = false, holdPolarity = false;
    Vec2 camera, scrollDelta;
    std::array<Colour, 16> palette{};
    // EAGLView 4660 publishes -(previousAccelX+accelX)/2 as a float promoted
    // to double. 418c4 snapshots it before scene work; it is not the camera.
    // layerTilt is the resolved world input, shared by the original consumers;
    // direct controller input never modifies the physical sensor producer.
    double lateralTilt = 0, layerTilt = 0;
    Vec2 contactPosition, contactNormal; // 9178, published after target callbacks
    std::array<TimedMessage, 10> messages{};
    std::vector<MessageLine> messageLines;
    std::array<Vec2, 4> pointers{};
    std::array<bool, 4> pointerHeld{};
    // controlBlock 8bf0/8ce4 and game 487e8: event deltas and drag anchors
    // have distinct lifetimes. Only the first active pointer drives the ship.
    std::array<Vec2, 4> touchAnchor{}, touchPrevious{}, touchDelta{};
    std::array<int, 4> touchMoves{};
    Vec2 touchSlope, touchStick, touchRelative;
    bool freshPress = false, pressLatched = false;
    void pointerDown(size_t, Vec2);
    void pointerMove(size_t, Vec2);
    void pointerUp(size_t);
    void updateTouch();
    std::string coverageStop;
    std::function<void(int, int)> sound;
    std::function<void(Entity &, Domain)> contacts;
    std::function<void(PlayerBullet &)> bulletTiles;
    std::function<void(Ship &)> shipTiles;
    std::function<void(Vec2)> deathBurst;
    std::function<void(Vec2, int)> groupReward;
    std::function<void()> runEnded, bankRun;
    std::function<void(const LaunchPod &)> launchEnemy;
    std::function<void(int, Vec2, int, Vec2, float)> scoreNotice;
    std::function<void()> ownerTick; // complete moo owner; Scene-only harnesses use sceneTick
    std::function<void()> sceneTick;
    std::function<void(std::vector<ActorDraw> &)> terrainDraw;
    std::function<void(int)> achievement;
    explicit World(uint32_t seed = 1);
    ~World();
    void add(Domain, Entity *);
    void runDomain(Domain);
    void tick(const PlayerInput &);
    void updatePalette();
    void prepare(std::vector<ActorDraw> &);
    void prepareForeground(std::vector<ActorDraw> &);
    void finishForeground();
    void award(int points, int side, Vec2 position);
    void resetChain();
    void achieveGoal(int id);
    std::function<void(int)> checkpoint;
    void noise(int sample, int gain = 0) {
        if (!demo && sample >= 0 && sample < 40)
            soundWanted[size_t(sample)] = gain;
    }
    // 421e0/45540 call the sample player directly, independently of demo audio.
    void uiSound(int sample) {
        if (sound)
            sound(sample, 8 << 12);
    }
    void flushSound();
    void clearGameplay();
    void message(int row, uint32_t rgba, int ticks, std::string text, int x = -1, int dynamic = -1);
    void runMessages();
    void clearMessages();
    void unsupported(const std::string &message) {
        if (coverageStop.empty())
            coverageStop = message;
    }
};
} // namespace superoxwars
