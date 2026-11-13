// game.h - Minotron's game layer (theGame.cpp in the original source tree):
// the controlBlock (input + entity-list owner), the entity base class and
// theGame itself. The entity subclasses live in game_entities.h.
//
// Field names: where the meaning is clear the member is named; otherwise it
// is called fXXX after its byte offset in the 32-bit original, so the port can
// be checked against the disassembly. The offsets in comments are original
// offsets; the C++ layout is unrelated.
#pragma once

#include <cstdint>
#include <cstring>

#include "neon_base.h"
#include "neon/touch_slots.h"
#include "../controller_input.h"
#include "neon_sprite.h"
#include "highscores.h"

#include <string>

namespace NEON_GAME {
namespace Neon_Internals {

class cellGrid;
class theGame;
class entity;
class bull; // only ever an entity in practice (the parameter type of wasShot)

// ------------------------------------------------------------- helpers -----

// Splits `value` into decimal digits right-aligned in buf[0..n-1] (zeros to
// the left), with an optional trailing marker `suffix` (-2 stores 0).
// Returns a pointer to the first written digit.
int *extractScore(int *buf, int n, int value, int suffix);

// Float lerp used by particles: a + (b - a) * t.
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// --------------------------------------------------------- controlBlock ----

class controlBlock : public neon::input::TouchState<vec2d> {
  public:
    controlBlock();
    virtual ~controlBlock() = default;

    // vtable
    virtual void _draw(sprite *drawSprite, vec2d size) {}   // [0]
    virtual void _bgdraw(sprite *drawSprite, vec2d size) {} // [1]
    virtual void run();                                     // [2]
    virtual void processJoyUp(unsigned id);                 // [3]
    virtual void processJoyDown(unsigned id, vec2d p);      // [4]
    virtual void processJoyMove(unsigned id, vec2d p);      // [5]

    void drawList(entity *e);
    void drawList(entity *e, float alpha);
    int findOldestTouch(int exclude);
    int findLeftTouch();
    int findRightTouch();
    void purgeTouches();
    void addEntity(entity *e, entity **list);
    void joyWatchdog();
    int collide(entity *a, entity *list, bool bounce);
    int touch(entity *a, entity *list) {
        return collide(a, list, false);
    }

    // save data (the shell owns the buffer; see MinotronSave)
    int &sd(int off) {
        return *reinterpret_cast<int *>(saveData + off);
    }

    int bgLayer = -1;                 // 0x08 cellGrid background layer (-1 none)
    float fbColour[4] = {0, 0, 0, 0}; // 0x0c feedback tint (Mixer shader 1)
    float fbRot = 0;                  // 0x1c feedback rotation (x pi)
    float fbScaleX = 1, fbScaleY = 1; // 0x20, 0x24
    float fbX = 0, fbY = 0;           // 0x28, 0x2c
    float flash = 0;                  // 0x30
    uint8_t *saveData = nullptr;      // 0x34
    bool paused = false;              // 0x50
    bool pauseArmed = false;          // 0x51 a touch in the score bar pauses
    bool onlineOK = false;            // 0x53
    int8_t shellCommand = 0;          // 0x54 mailbox to the module host
    int gameMode = 0;                 // 0x58 0 normal, 1 assisted, 2 simplified, 3 hard
    int postScoreValue = 0;           // 0x5c
    int postScoreCategory = 0;        // 0x60
    int msgTimer = 0;                 // 0x64 frames left of the current message
    bool inputFlipped = false;        // 0x6c
    bool resumeResetRequested = false, savedVolumeLoaded = false;
    oxRNG rng; // 0x74
    // Original touch/joystick fields are inherited from shared TouchState.
    uint32_t bgColour = 0;     // 0x298 full-screen backdrop (RGBA, 0 = none)
    int frontEndTouchMode = 0; // 0x29c touch handling mode of the front-end screens
    int frameCount = 0;        // 0x2a0
    cellGrid *grid = nullptr;  // 0x2a4
    bool running = false;      // 0x2a8
    bool demo = false;         // 0x2a9 attract-mode demo: silent, no text
    // entity lists (singly linked through entity::next)
    entity *projectiles = nullptr; // 0x2ac shots
    entity *players = nullptr;     // 0x2b0
    entity *obstacles = nullptr;   // 0x2b4
    entity *particles = nullptr;   // 0x2bc (also bonuses, the helper droid)
    entity *beasts = nullptr;      // 0x2c0 (also powerups)
    entity *enemies = nullptr;     // 0x2c4
    entity *enemyShots = nullptr;  // 0x2c8
    int f2d4 = 0;
    sprite *spr = nullptr; // 0x2d8 sprite used by the draw calls
    vec2d drawSize;        // 0x2dc render-target size
};

// --------------------------------------------------------------- entity ----

class entity {
  public:
    entity();
    virtual ~entity() = default;

    // vtable
    virtual void told(int msg) {}                  // [0]
    virtual int draw(sprite *spr, vec2d size);     // [1]
    virtual void run();                            // [2]
    virtual void wasShot(entity *by);              // [3]
    virtual void blowUp(unsigned seed, int count); // [4]
    virtual void hitBy(entity *by) {}              // [5]
    virtual void init(theGame *g) {}               // [6] (gEntity)

    bool dead = false;                        // 0x04
    bool active = true;                       // 0x05
    bool wrapped = false;                     // 0x06
    bool noWrap = false;                      // 0x07
    bool touching = false;                    // 0x08 overlapping something (collide)
    bool hit = false;                         // 0x09 collide() reported a hit
    int fc = 0;                               // 0x0c
    int age = 0;                              // 0x10
    int type = 0;                             // 0x14 (3 player, 1 beast, ...)
    float zOffset = 0;                        // 0x18 -> sprite zOffset
    int spriteIndex = 0x8c;                   // 0x1c (sprite number - LITBASE)
    int drawDelay = 0;                        // 0x20 frames before it is drawn / collides
    colour col;                               // 0x28
    int group = 0;                            // 0x78 entities of one group do not collide
    unsigned flags = 0;                       // 0x80 bits 0-1: hits call wasShot
    float rot = 0;                            // 0x84 -> sprite angle
    float elasticity = 1.0f;                  // 0x88 bounce response (collide)
    vec2d pos;                                // 0x8c
    vec2d vel;                                // 0x9c
    vec2d drag{0.98, 0.98};                   // 0xac inherited entity damping
    double speedLimitX = 0, speedLimitY = 0;  // 0xbc, 0xc4 (per-class speeds / limits)
    double wrapWidth = 1.0, wrapHeight = 1.0; // 0xcc, 0xd4
    double scaleX = 1.0, scaleY = 1.0;        // 0xdc, 0xe4
    double baseScaleX = 0, baseScaleY = 0;    // 0xec, 0xf4
    int baseSprite = 0;                       // 0xfc
    int health = 1;                           // 0x100 (players: lives)
    int hp = 1;                               // 0x104 hit points (theGame::damage)
    int flashFrames = 0;                      // 0x108
    float seekSpeed = 0, seekTurnRate = 0, seekHeading = 0; // 0x10c..0x114
    double seekAccelX = 0, seekAccelY = 0;                  // 0x118, 0x120
    float mass = 1.0f;                                      // 0x128
    float size = 0.02f;                                     // 0x130 radius (fraction of the target)
    float targetDistance = 0;
    entity *prev = nullptr;  // 0x154
    entity *next = nullptr;  // 0x158
    theGame *game = nullptr; // 0x160 / 0x164
};

// Particles: fade from colour `from` to `to` while the size lerps from
// sizeEnd to sizeStart as `life` runs from 1 down to 0.
class _particle : public entity {
  public:
    _particle();
    int draw(sprite *spr, vec2d size) override;
    void run() override;

    float life = 1.0f;      // 0x164
    float fade = 0.01f;     // 0x168 life lost per frame
    colour from;            // 0x16c
    colour to;              // 0x1bc
    int fromDynamic = -1;   // 0x20c getDynamicColour type for `from` (-1 none)
    int toDynamic = -1;     // 0x210
    float sizeStart = 1.0f; // 0x214
    float sizeEnd = 4.0f;   // 0x218
};

// ------------------------------------------------------------- theGame -----

class theGame : public controlBlock {
  public:
    theGame();
    ~theGame() override;

    void _draw(sprite *spr, vec2d size) override;
    void run() override;
    void processJoyUp(unsigned id) override;
    void processJoyDown(unsigned id, vec2d p) override;
    void processJoyMove(unsigned id, vec2d p) override;

    void drawDeferredSprites();
    vec2d newPosition(float minDx, float minDy, float margin);
    vec2d newPosition(float minDist);
    void reset(int keep);
    void drawMessage();
    void eraseMessage();
    void pauseScreen();
    void infoScreen();
    void printTitleScreen();
    void startGameScreen();
    void updateStats();
    void makeArena();
    void shootFrom(entity *from, vec2d v, int kind, int sprite, int flightMode, float size,
                   uint32_t rgba);
    void swarmerSplit(entity *e);
    void pixelShatter(entity *src, entity *e, vec2d v, int frames, int type, int cols, int rows,
                      int mode);
    void bootScreen();
    void newLevel();
    void newGame();
    int minoInfoScreen();
    void drawInfoBack();
    int storyTextScroll();
    void rez(entity *e, int mode, float speed, int frames);
    void derez(entity *e, int mode, float speed, int frames);
    void restartLevel();
    void spinySplit(entity *e);
    void zombeastSplit(entity *e);
    void splash(int n, vec2d p);
    void checkPowerup(vec2d p);
    int explodeLastBeast();
    void genLevel();
    void damage(entity *by, entity *e, int amount, int points, int shatter);
    // Steering; `edge` is what happens at the arena edge: 0 stop, 1 die, 2 bounce.
    void inertialSeek(entity *e, entity *target, int edge);
    void vectorSeek(entity *e, entity *target, int edge);
    void moveTowards(entity *e, entity *target, int edge);

    void addScore(int points);
    bool goalAllowed(unsigned bit) const;
    void goal(int ach, unsigned bit, int cmd);
    void menuInput(int activationLabel = 0);
    void levelComplete();
    void playSample(int n) const; // AudioSystemPlaySample(n, volume << 12, 0)
    int &best(int mode, int levelIndex, int k) {
        return sd(((mode * 100 + levelIndex) * 8) + k * 4);
    }

    bool gridConfigured = false;
    bool isPad = false; // 0x2ed iPad (4 players)
    bool spawnQueueDrained = false;
    bool levelEnding = false;
    bool waitRelease = false;
    bool practice = false; // 0x2f2 (one life, level 1)
    bool endurance = false;
    bool bootNotificationSent = false;
    bool forcePowerup = false; // 0x2f5
    bool resumeBest = false;   // 0x2f6
    bool newLevelBest = false;

    unsigned levelFlags = 0; // 0x2fc gameFlags[level]
    int demoLevel = 0;       // 0x300

    int level = 0;  // 0x308
    int volume = 4; // 0x30c 0..7
    int goalsDone = 0;
    int selLevel = 0;   // 0x314 level chosen on the start screen
    int message = 0;    // 0x358 drawMessage selector
    int state = 1;      // 0x35c front-end / game state
    int maxPlayers = 2; // 0x360
    int lastMode = 0, lastPlayers = 1, lastStartLevel = 0;
    int deathPauseFrames = 0; // Positive counts down; -1 marks an exhausted run.
    int furthest = 9;         // 0x378 furthest level reached (selectable)
    int banner = 0;           // 0x37c
    int spawnCount = 0;       // 0x380
    int spawnQueue[500] = {}; // 0x384
    int nBeasts = 0;          // 0xb54
    int nObstacles = 0;       // 0xb58
    int nPlayers = 1;         // 0xb5c
    int score[4] = {};        // 0xb70
    int lives[4] = {};        // 0xb80
    float fb90[24] = {};      // 0xb90..0xbec (fb90[i] at 0xb90 + 4i)
    float shotSpeed = 0;
    int shotDelay = 0;
    bool fiveWayShots = false;
    int gruntDelay = 0;      // 0xc00
    float gruntSpeed = 0;    // 0xc04
    float gruntSpeedMul = 0; // 0xc08
    float stepSpeedAdd = 0;  // 0xc0c
    int liveEnemyCount = 0;  // Recounted by entity runs; completion reads the previous tick.
    int hotShotFrames = 0;
    bool hotShotsPermanent = false;
    int powerupTimer = 0; // 0xc1c
    int forcedPowerupKind = 0;
    int beastLoveFrames = 0;       // Attraction with the visible heart effect.
    int beastAttractionFrames = 0; // Attraction alone, including restart assistance.
    bool spreadShots = false, reflectiveShots = false, spreadPowerupDue = false;
    int rescueChain = 0;
    int levelBonusStep = 0, levelBonusPoints = 0;
    int shakeFrames = 0;
    int extraLove = 0;       // 0xc40
    float maxGruntSpeed = 0; // 0xc44
    float driftAngle = 0, driftSpeed = 0, driftPhaseX = 0, driftPhaseY = 0, driftPhaseStepX = 0,
          driftPhaseStepY = 0;
    vec2d worldDrift; // 0xc60 drift vector (rotated by driftAngle each frame)
    vec2d gravity;    // 0xc70 (0, 0.0007) for droplets and turds
    int beamCycleFrames = 0, beamCycleLength = 0, beamFireFrames = 0;
    // 0xc8c: [0] a colour cycler, [1..10] getDynamicColour(8,5,0,4,2,3,1,6,7,9)
    // refreshed every frame, [11..15] unused.
    colour gcol[16];
    int restarts = 0; // 0x118c restarts this level
    int bombsCollected = 0, pickupBonusIndex = 0;
    int f1198[4] = {}; // 0x1198
    int deathsThisRun = 0, rescuedAllBeforeKills = 0, bonusPickupChain = 0;
    bool movementTouchStarted = false; // 0x11b4 a movement touch went down (aiDroid)
    bool confirmResumeReset = false;
    vec2d movementTouchOrigin; // 0x11b8 where it went down
    int achievement = 0;       // 0x11c8 last achievement index
    int infoPage = 0;          // 0x11cc minoInfoScreen page
    // Port input: module supplies a vertical axis separately from dismiss buttons.
    minotron::module::ControllerInput controller;
    neon::input::MenuActions menuActions;
    float infoScrollAxis = 0;
    bool infoControllerMode = false;
    int pausedState = 0; // 0x11d4 state that was paused

    // ---- port additions: local high scores (replacing OpenFeint) ----
    // Boards as the game numbered them for OpenFeint (postScoreCategory):
    // game mode 0-3, +4 for endurance.
    static constexpr int kHiBoards = 8;
    mino::HighScores hs; // loaded / saved by the shell
    int hiBoard = 0, hiNew = -1;
    int hiReturn = 0; // where BACK goes: 1 title sequence, 2 title menu, 3 info menu
    int hiTimer = 0;
    int entryScore = 0, entryLevel = 0, entryBoard = 0;
    std::string entryName;
    void defaultHighScores();
    void highScoreScreen(); // state 0x71
    void nameEntryScreen(); // state 0x72
    void finishNameEntry();
    void leaveHighScores();
    void scoreScreensInput(int activationLabel = 0); // states 0x71 / 0x72
};

// Game-wide options set by newGame from the game mode.
extern bool useRobot, autoAim, reducedPowerups, hard;

} // namespace Neon_Internals
} // namespace NEON_GAME
