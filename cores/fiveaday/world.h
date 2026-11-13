#pragma once
#include "neon/source_random.h"
#include "neon/foundation_rules.h"
#include "neon/entity_list.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fiveaday {
constexpr double Pi = 3.141592654, Tau = 6.2831853072;
struct Vec2 {
    double x = 0, y = 0;
    Vec2 operator+(Vec2 b) const {
        return {x + b.x, y + b.y};
    }
    Vec2 operator-(Vec2 b) const {
        return {x - b.x, y - b.y};
    }
    Vec2 operator*(double k) const {
        return {x * k, y * k};
    }
    Vec2 &operator+=(Vec2 b) {
        x += b.x;
        y += b.y;
        return *this;
    }
    double length() const {
        return std::sqrt(x * x + y * y);
    }
    void rotate(double angle) {
        neon::rotateClockwise(x, y, angle, neon::ZeroAngleRule::PreserveCoordinates);
    }
};
struct Colour {
    double r = 1, g = 1, b = 1, a = 1;
};
Colour unpack(uint32_t);
uint32_t interpolatePacked(uint32_t low, uint32_t high, float progress);
Colour dynamicColour(int, uint64_t, float alpha = 1);
struct ActorDraw {
    Vec2 position, scale{1, 1};
    float radius = .01f, angle = 0;
    int frame = 0;
    Colour colour;
};
struct Input {
    Vec2 direction;
    bool held = false;
    // Touch and steering use downward-positive screen coordinates.
    static Vec2 directionFromGravity(Vec2 gravity) {
        return {gravity.x, -gravity.y};
    }
};
class World;
struct Entity {
    World &world;
    Entity *prev = nullptr;
    Entity *next = nullptr;
    Vec2 position, velocity, scale{1, 1};
    Colour colour;
    // 999 is the game factory's post-constructor type; frame/radius are the
    // original entity constructor defaults, before each subclass initializes.
    int type = 999, frame = 140, health = 1, age = 0;
    float radius = .02f, angle = 0;
    bool dead = false, culled = false, collidable = true;
    explicit Entity(World &w) : world(w) {}
    virtual ~Entity() = default;
    virtual void run() = 0;
    virtual void hitBy(Entity &) {}
    virtual void didHit(Entity &) {}
    virtual void prepare(std::vector<ActorDraw> &);
};
// Five a Day's wentity: separate retained world position and projected position.
// The camera translation is applied before the original one-span wrap tests.
struct Wrapped : Entity {
    Vec2 retained;
    float targetRadius = .01f, cullMargin = .5f, spinOffset = 0, heading = 0, cameraCoupling = 1;
    float wrapX = .8f, wrapY = .8f;
    unsigned wrapped = 0;
    bool floating = false;
    using Entity::Entity;
    void run() override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct Launch {
    Vec2 position;
    float heading = 0, turnScale = 0, speedScale = 0, orbitRadius = 0, orbitGain = 0, sideSign = 0;
    int formation = 0;
    bool horizontal = false;
};
struct Ship : Entity {
    enum class State { Flying, Dying, Waiting, Entering, Leaving, Bonus, GameOver = 999 };
    State state = State::Entering;
    Vec2 impulse, cameraOrigin;
    float targetHeading = 0, turnRate = 0, cameraTurn = 0, flame = 0, displayHealth = 0;
    float speedMultiplier = 1, integrity = 1, weaponEnergy = 0, travel = 0, damageTravel = 0,
          hitFlash = 0, exitAcceleration = 0;
    int damageSounds_ = 0, burstCount = 1;
    float burstTimeout = 0;
    int fireTicks = 10, fireInterval = 10, waitTicks = 0, collected = 1, shieldTicks = 0;
    bool chasing = false;
    std::array<Vec2, 512> trail{};
    uint32_t trailCursor = 0;
    explicit Ship(World &);
    void init();
    void steer(const Input &);
    void fire();
    void run() override;
    void didHit(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
// scoreDisplay and annunciator share glyph layout but retain different updates.
struct ScoreText : Entity {
    std::string text = "      00";
    Vec2 offset;
    float advance = 0;
    int columns = 8, lastScore = 0, remaining = 50;
    bool popup = false;
    explicit ScoreText(World &);
    void run() override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct Bullet : Entity {
    bool burst = false;
    Vec2 initialVelocity;
    using Entity::Entity;
    void run() override;
    void didHit(Entity &) override;
};
struct Attacker : Wrapped {
    bool suppressDeathBlast = false;
    float damageRange = .6f;
    bool formationMember = false, carrier = false, enraged = false, formationLocked = false,
         armed = true, killedNow = false, wasHit = false;
    int reward = 10, weight = 1, deathTicks = 0, missileTicks = 200, missileInterval = 200,
        missileChance = 0;
    int shotTicks = 20, shotInterval = 20, shotChance = 64, shotMode = 0, formationColour = 0;
    int shotCount = 0, missileFrame = 0;
    float shotSpread = 0, gunAngle = 0, gunAngleStep = 0, missileHeading = 0, missileTurn = 0,
          missileRadius = 0;
    Vec2 missileScale{1, 1};
    float turnStep = 0, speed = 0, orbitRadius = 0, orbitGain = 0, distance = 0, tailDecay = 0,
          tailLength = 60, aimThreshold = .5f, shotSpeed = 1.2f;
    Vec2 course;
    std::array<Vec2, 256> history{};
    unsigned historyCursor = 0;
    explicit Attacker(World &);
    void init(const Launch &);
    void run() override;
    void hitBy(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct HomingMissile : Attacker {
    int lifetime = 500;
    bool retireWhenCulled = true;
    HomingMissile(World &, const Attacker &, bool special = false);
    void run() override;
};
// This bomb-only actor leaves its inherited gun gate unstored. Mode -1 cannot
// emit bullets, but an open gate still consumes RNG in Attacker::run. The factory
// supplies zero, matching the reference allocator witness; this is allocation
// policy, not a recovered constructor store.
struct BomberGunAllocationInputs {
    float aimThreshold;
    int shotTicks, shotInterval, shotChance;
};
struct Bomber : Attacker {
    int bombTicks = 60, bombInterval = 60, soundTicks = 0, soundInterval = 480;
    Bomber(World &, const Launch &, BomberGunAllocationInputs);
    void run() override;
    void hitBy(Entity &) override;
    void didHit(Entity &) override;
};
struct Bomb : Entity {
    int state = 0, fuse = 0, shotCount = 5;
    float spin = 0, triggerRadius = 0;
    Bomb(World &, const Bomber &);
    void run() override;
    void hitBy(Entity &) override;
    void explode();
};
struct MinotaurKiller : Attacker {
    MinotaurKiller(World &, const Launch &);
    void run() override;
};
struct Miner : Attacker {
    float mineTravel = 0, mineSpacing = .1f, spawnRange = .4f, baseTurn = 0;
    int mineStyle = 0;
    Miner(World &, const Launch &);
    void run() override;
};
struct Mine : Wrapped {
    int remaining = 200;
    float spin = 0;
    Mine(World &, const Entity &);
    void run() override;
    void hitBy(Entity &) override;
};
struct Jellyfish : Wrapped {
    int phase = 0, phaseTicks = 0, restTicks = 100, diveTicks = 200, shotTicks = 150,
        shotInterval = 150, shotCount = 8;
    float topSpeed = .006f, speed = .006f, targetHeading = 0, travel = 0;
    double baseWidth = 1;
    bool reflectionLatch = false;
    Jellyfish(World &, const Launch &);
    void run() override;
    void hitBy(Entity &) override;
    void didHit(Entity &) override;
};
struct PinkPlane : Wrapped {
    uint32_t shotPattern = 0, soundPattern = 0, patternCursor = 0;
    int phase = 0, phaseTicks = 0, restTicks = 100, diveTicks = 100, pulses = 0, pulseCount = 0,
        beatTicks = 7, beatInterval = 7, shotCount = 1;
    float targetHeading = 0, speed = .007f;
    PinkPlane(World &, const Launch &);
    void run() override;
    void hitBy(Entity &) override;
};
struct DecoyCloud : Wrapped {
    int phase = 0, phaseTicks = 0, restTicks = 100, diveTicks = 500, bursts = 2, burstCount = 2,
        shotTicks = 50, shotInterval = 50;
    float targetHeading = 0, speed = .001f;
    DecoyCloud(World &, const Launch &);
    void run() override;
    void hitBy(Entity &) override;
};
// Original init does not store the reflection latch. Keep the reconstruction's
// allocation policy explicit until original allocator reuse has been established.
struct BossAllocationInputs {
    bool reflectionLatch;
};
struct Boss : Attacker {
    int maximumHealth = 1;
    float healthFraction = 1, displayHealth = 1, hitFlash = 0;
    bool reflectionLatch;
    Boss(World &, const Launch &, BossAllocationInputs);
    void run() override;
    void hitBy(Entity &) override;
    void didHit(Entity &) override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct Fruit : Wrapped {
    enum class State { Loose, Collected, Fading, Carried };
    State state = State::Carried;
    Attacker *parent = nullptr;
    Vec2 drift;
    float spin = .2f, energy = 1;
    int index = 0;
    bool minotaur = false;
    explicit Fruit(World &, Attacker &);
    void run() override;
    void hitBy(Entity &) override;
};
struct Cloud : Wrapped {
    float phase = 0, phaseStep = 0, spin = 0, minAlpha = 0, targetAlpha = 0, alphaScale = 1,
          flash = 0;
    Colour flashColour;
    explicit Cloud(World &);
    void details(int);
    void run() override;
};
struct Explosion : Entity {
    float progress = 0, progressStep = .04f, growth = .005f;
    using Entity::Entity;
    void run() override;
};
struct Glow : Entity {
    float fade = .97f;
    using Entity::Entity;
    void run() override;
};
struct ProgressFruit : Entity {
    int state = 0, slideTicks = 0;
    float slideStep = -.005f, spin = 0;
    explicit ProgressFruit(World &);
    void run() override;
};
struct BonusFruit : Entity {
    float spin = 0, travel = 0;
    explicit BonusFruit(World &);
    void run() override;
};
struct StageSun : Entity {
    uint32_t flashColour = 0;
    float flashRadius = 0;
    bool sounded = false, released = false;
    explicit StageSun(World &);
    void run() override;
};
struct Message {
    std::string text;
    int row = 0, ticks = 0;
    Colour colour;
};
struct GameOverFruit : Entity {
    Vec2 target;
    float spin = 0;
    int delay = 0, state = 0;
    GameOverFruit(World &, Vec2, int frame, int delay);
    void run() override;
};

class World {
  public:
    neon::DarwinRandom random;
    Ship ship;
    ScoreText scoreDisplay;
    Input input;
    Launch currentLaunch;
    Vec2 camera, bossPosition;
    float cameraAngle = 0, progress = 0, progressStep = 0, week = 0, month = 0, year = 0,
          scoreMultiplier = 1;
    int level = -1, score = 0, lives = 3, fruitCount = 0, fruitSets = 0, formationKills = 0,
        formations = 0, formationBonusMultiplier = 1;
    uint32_t sky = 0, cloudColour = 0, achievements = 0;
    bool sequencePaused = false, gameOver = false, paused = false, demo = false, tilt = false;
    float gameOverFade = 0;
    uint32_t finalSky = 0, finalCloud = 0;
    int gameOverTicks = 0;
    // goFruit signals a landing/retirement; the scene rate-limits its plop sound.
    bool letterFeedbackPending = false;
    int letterFeedbackTicks = 0;
    bool bossLaunched = false, bossDefeated = false, cleanStage = true;
    bool perfectDay = true;
    // tPi 3d/3e/3f: first-stage tutorial and its two one-shot prompts.
    bool openingTutorial = false, repairHintPending = true, bossHintPending = true;
    int bonusRemaining = 0, bonusTicks = 30, bonusInterval = 30, ordinaryFruitTotal = 0,
        powerFruitTotal = 0;
    int lifetimePlanes = 0, lifetimeFruit = 0, lifetimeMinotaurs = 0;
    std::vector<int> bonusFrames;
    size_t bonusCursor = 0;
    std::array<Message, 10> messages{};
    int goal = -1, levelEndedTicks = 0;
    std::vector<int> awardMessages;
    int population = 0, spawnTicks = 10, spawnInterval = 120, populationLimit = 10;
    size_t scriptCursor = 0;
    std::string script;
    // Unsupported campaign commands stop execution with a diagnostic;
    // they are never skipped or replaced with another enemy family.
    std::string checkpoint;
    uint64_t ticks = 0;
    std::function<void(int, int)> sound;
    std::function<Vec2(int)> spriteSize;
    std::function<void(int, int, int)> dayCheckpoint;
    Entity *bullets = nullptr, *effects = nullptr, *fruits = nullptr, *enemies = nullptr,
           *shots = nullptr, *clouds = nullptr, *scenery = nullptr, *hud = nullptr;
    explicit World(uint32_t seed = 1);
    ~World();
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    void start(int day = 0, int startingLives = 3, int startingScore = 0,
               bool demonstration = false);
    void newLevel();
    void beginBonus();
    void deliverFruitSet();
    void tick(const Input &);
    void goGameOver();
    void checkpointDay();
    void checkLifetimeGoals();
    void message(int row, const std::string &, int ticks, uint32_t colour = 0xffffff00u);
    void advanceMessages();
    Launch launch();
    void obey(char);
    void sequence();
    void touch(Entity &, Entity *);
    Vec2 attractShip(Vec2 position, float mass, float strength) const;
    void add(Entity *&list, Entity *entity) {
        neon::entities::prepend(list, *entity);
    }
    void noise(int sample, int shift = 12) {
        if (!demo && sound)
            sound(sample, 8 << shift);
    }
    void noiseGain(int sample, int gain) {
        if (!demo && sound)
            sound(sample, gain);
    }
    void addScore(int points) {
        if (!demo)
            score += points;
    }
    Colour palette(int) const;
    Vec2 dimensions(int frame) const {
        return spriteSize ? spriteSize(frame) : Vec2{1, 1};
    }
    Explosion &explode(Vec2, float radius, float step, float growth, int frame = -1);
    void flash(Vec2, int colour, float amount);
    void popupScore(Vec2, int);
    void radialShoot(const Entity &, int, float, float, float);
    void achieveGoal(int);
    bool playerCanFinishBoss() const {
        return ship.state == Ship::State::Flying && ship.integrity > 0;
    }
    void prepareForeground(std::vector<ActorDraw> &, bool drawHud = true);
};
} // namespace fiveaday
