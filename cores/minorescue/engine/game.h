// game.h - Minotaur Rescue's game layer (spaceWars.cpp and friends in the
// original source tree): the controlBlock (touch input + entity lists), the
// entity base classes and spaceWars, the game itself. The entity subclasses
// live in game_entities.h.
//
// Field names: where the meaning is clear the member is named; otherwise it
// is called fXXX after its byte offset in the 32-bit original, so the port can
// be checked against the disassembly. Offsets in comments are original
// offsets; the C++ layout is unrelated.
//
// Coordinates: entities live in -0.5..0.5 screen units (y down); sizes are
// radii as a fraction of the render target, drawn at (2 * size) * target.
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
class spaceWars;
class entity;
class bull;

// ------------------------------------------------------------- helpers -----

// Splits `value` into decimal digits right-aligned in buf[0..n-1] (zeros to
// the left), with an optional trailing marker `suffix` (-2 stores 0).
// Returns a pointer to the first written digit.
int *extractScore(int *buf, int n, int value, int suffix);

// Float lerp used by particles: a + (b - a) * t.
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// vec2d::lerp: a + (b - a) * t with t clamped to -1..1.
vec2d vlerp(const vec2d &a, const vec2d &b, float t);
// vec2d::normalize: unit vector, or (0, 0) for a zero vector.
vec2d normalized(const vec2d &v);

// Team colours (RGBA) and the end-of-match messages.
extern const uint32_t teamColours[4];
extern const char *const teamWinMessages[5];

// Scroller texts (rescue_texts.cpp): pages of lines, nullptr = blank line,
// a line starting with '^' ends the page.
extern const char *const *const kInfoPages[6]; // About, solar, deep space, tanks, jets, Llamasoft
extern const int kInfoPageLines[6];
extern const char *const *const kStoryPages[3]; // story, title roll, how to play
extern const int kStoryPageLines[3];

// Shell mailbox commands (controlBlock::shellCommand).
enum ShellCommand : int8_t {
    SHELL_PURGE_TOUCHES = -1,
    SHELL_CHECK_ONLINE = 1,
    SHELL_POST_SCORE = 2,  // postScore = value, postBoard = category
    SHELL_SHOW_BOARD = 3,  // postBoard = category
    SHELL_ACHIEVEMENT = 4, // 4 + n: achievement n (0..9)
    SHELL_SHOW_DASHBOARD = 14,
    SHELL_SCREEN_ON = 15, // first title music sting: the splash may end
    SHELL_EXIT = 32,      // (port) EXIT on the title screen: leave the game
};

// --------------------------------------------------------- controlBlock ----

class controlBlock : public neon::input::TouchState<vec2d> {
  public:
    controlBlock();
    virtual ~controlBlock() = default;
    virtual void run(); // vtable[0]

    minorescue::module::ControllerRouting controllers;
    neon::input::PlayerMotion playerControl(int player) const;
    neon::input::MenuActions menuActions;
    bool infoControllerMode = false;
    float infoScrollAxis = 0;
    void purgeTouches();
    void processJoyUp(unsigned id);
    void processJoyDown(unsigned id, vec2d p);
    void processJoyMove(unsigned id, vec2d p);
    void addEntity(entity *e, entity **list);
    int collide(entity *a, entity *list, bool bounce);
    int touch(entity *a, entity *list) {
        return collide(a, list, false);
    }
    int bounceOff(entity *a, entity *list) {
        return collide(a, list, true);
    }
    // Gravity on `a` from every entity of `list`: sum of mass / distance^2
    // (distances in 1/512 units) along the unit direction, times a->mass
    // * 1e-6 * strength, or each entity's own mass when strength <= 0.
    // Also leaves the nearest distance in a->nearest.
    vec2d attract(entity *a, entity *list, float strength);
    vec2d feelTheForce(entity *a, entity *list) {
        return attract(a, list, 0.0f);
    }

    float fbColour[4] = {0, 0, 0, 0}; // 0x04 feedback tint (Mixer shader 1)
    float fbRot = 0;                  // 0x14 feedback rotation (x pi)
    float fbScaleX = 1, fbScaleY = 1; // 0x18, 0x1c
    float fbX = 0, fbY = 0;           // 0x20, 0x24
    float flash = 0;                  // 0x28 centre flash (showJoy)
    bool paused = false;              // 0x2c
    bool pauseArmed = false;          // 0x2d double-tap in the middle pauses
    bool onlineOK = false;            // 0x2e
    int8_t shellCommand = 0;          // 0x2f
    int postScore = 0;                // 0x30
    int postBoard = 0;                // 0x34
    float pulse = 0;                  // 0x38 planet-hit pulse (feedback colour)
    bool inputFlipped = false;        // 0x3c device upside down
    bool resetRequest = false;        // 0x3d (back to the title)
    int joyZones = 1;                 // 0x40 touch zones / players
    oxRNG rng;                        // 0x44
    // Original touch/joystick fields are inherited from shared TouchState.
    uint32_t bgColour = 0;    // 0x134 full-screen backdrop (RGBA, 0 = none)
    bool titleMode = false;   // 0x138 showJoy draws the title logo
    int frameCount = 0;       // 0x13c
    cellGrid *grid = nullptr; // 0x140
    bool running = false;     // 0x144
    bool demo = true;         // 0x145 attract mode / one shared joystick
    // entity lists (singly linked through entity::next)
    entity *bullets = nullptr;    // 0x148 player shots
    entity *players = nullptr;    // 0x14c ships, tanks, jets
    entity *planets = nullptr;    // 0x150 planets and the sun
    entity *list154 = nullptr;    // 0x154 (unused)
    entity *particles = nullptr;  // 0x158
    entity *minotaurs = nullptr;  // 0x15c
    entity *saucers = nullptr;    // 0x160 saucers and mines
    entity *enemyShots = nullptr; // 0x164
    int pauseTapTimer = 0;        // 0x168
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
    virtual void wasShot(entity *by);              // [3] (a bull)
    virtual void blowUp(unsigned seed, int count); // [4]
    virtual void hitBy(entity *by) {}              // [5]

    bool dead = false;       // 0x04
    bool active = true;      // 0x05
    bool wrapped = false;    // 0x06
    bool noWrap = false;     // 0x07
    bool touching = false;   // 0x08 overlapping something (collide)
    bool hit = false;        // 0x09 collide() reported a hit
    int fc = 0;              // 0x0c (craft: invulnerable frames)
    int age = 0;             // 0x10
    int type = 0;            // 0x14 1 player, 2 planet, 3 player shot,
                             //      4 minotaur, 5 saucer/mine, 6 saucer shot, 7 CPU
    int spriteIndex = 0x8c;  // 0x18 (frame - 1)
    oxRNG rng;               // 0x1c
    colour col;              // 0x20
    int group = 0;           // 0x40 player number + 1; same group doesn't collide
    unsigned flags = 0;      // 0x48 bits 0-1: hits call wasShot
    float rot = 0;           // 0x4c
    float elasticity = 1.0f; // 0x50
    vec2d pos;               // 0x54
    vec2d vel;               // 0x64
    vec2d drag{0.98, 0.98};  // 0x74
    double wrapWidth = 1.0, wrapHeight = 1.0; // 0x84, 0x8c
    double scaleX = 1.0, scaleY = 1.0;        // 0x94, 0x9c
    int hp = 1;                               // 0xa4
    float mass = 1.0f;                        // 0xa8
    float nearest = 0;                        // 0xac (attract)
    float size = 0.02f;                       // 0xb0
    entity *prev = nullptr;                   // 0xb4
    entity *next = nullptr;                   // 0xb8
    entity *owner = nullptr;                  // 0xbc (trails)
    spaceWars *game = nullptr;                // 0xc0
};

// Particles: fade from colour `from` to `to` while the size lerps from
// sizeStart to sizeEnd as `life` runs from 1 down to 0.
class _particle : public entity {
  public:
    _particle();
    int draw(sprite *spr, vec2d size) override;
    void run() override;

    float life = 1.0f;      // 0xc4
    float fade = 0.01f;     // 0xc8 life lost per frame
    colour from;            // 0xcc
    colour to;              // 0xec
    int fromDynamic = -1;   // 0x10c getDynamicColour type for `from` (-1 none)
    int toDynamic = -1;     // 0x110
    float sizeStart = 1.0f; // 0x114
    float sizeEnd = 4.0f;   // 0x118
};

// A burst of `count` dots scattered (deterministically, from `seed`) within
// `radius` sizes of the centre. spark is the same class.
class explosion : public _particle {
  public:
    explosion();
    int draw(sprite *spr, vec2d size) override;

    int count = 16;       // 0x11c
    float radius = 16.0f; // 0x120
    unsigned seed = 0;    // 0x124
};

// An explosion that also shows a number rising from where it happened.
class scoreExplosion : public explosion {
  public:
    int draw(sprite *spr, vec2d size) override;
    // Sets the digits (extractScore with a -1 terminator) and where they show.
    void setScore(int value, vec2d at);

    int digits[8] = {};         // 0x128
    int *digitPtr = digits + 7; // 0x148
    int colourType = 5;         // 0x14c getDynamicColour type
    float digitScale = 0.5f;    // 0x150
    vec2d digitPos;             // 0x154
};

// Player and saucer shots.
class bull : public _particle {
  public:
    bull();
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void hitBy(entity *by) override {
        dead = true;
    }

    int damage = 1;           // 0x11c
    bool bounceShots = false; // 0x120 also bounces off other shots
    int power = 0;            // 0x124 shooter's weapon level
    class gun *gun = nullptr; // 0x128
};

// Bullets that bounce off players and planets.
class enemyBull : public bull {
  public:
    void run() override;
};

// A gun fires `shoot()` every `reload` frames while enabled.
class gun {
  public:
    gun() = default;
    virtual ~gun() = default;
    virtual void run();   // vtable[0]
    virtual void shoot(); // vtable[1]

    entity *owner = nullptr;  // 0x04
    bool disabled = true;     // 0x08
    int shotSprite = 0x8c;    // 0x0c
    int reload = 15;          // 0x10
    float angle = 0;          // 0x18
    float speed = 0.6f;       // 0x1c
    float shotFade = 0.01f;   // 0x20
    float shotDrag = 0.99f;   // 0x34
    float shotSize = 0.01f;   // 0x38
    bool bounceShots = false; // 0x3c
    int timer = 1;            // 0x40
};

// ------------------------------------------------------------ spaceWars ----

class spaceWars : public controlBlock {
  public:
    spaceWars();
    ~spaceWars() override;
    void run() override;

    void reset(int keep);
    void newGame();
    void newLevel();
    void makeMaze();
    void runStarField();
    void randomSaucer();
    void randomMinotaur();
    void score(int points, int player);
    void achieveGoal(int n);
    int doStartBonus();
    void showScore(sprite *spr, vec2d size);
    int showTankLives(sprite *spr, vec2d size);
    int drawScoreDigits(int alignRight, sprite *spr, float width, int player);
    int drawKillsDigits(sprite *spr, float width, int player);

    void infoScreen();
    void pauseScreen();
    void printTitleScreen();
    void startGameScreen();
    int minoInfoScreen(bool back = false);
    int storyTextScroll(bool advance = false);

    void playSample(int n) const;
    void playGrunt() const; // one of the minotaur grunts at random
    void releaseSaucerSound(int big);
    void saucerSound(int big);

    bool inited = false;     // 0x16c
    bool isPad = false;      // 0x16d (4 players)
    bool planetsOn = true;   // 0x16e
    bool minotaursOn = true; // 0x16f
    bool sunOn = true;       // 0x170
    bool bouncyShots = true; // 0x171 tank shots bounce off walls

    bool shields3 = true;      // 0x173 ships start with 3 shields (else 9)
    bool levelEnding = false;  // 0x174 last planet wave spawned
    bool levelDone = false;    // 0x175
    bool showHud = false;      // 0x176
    bool waitRelease = false;  // 0x177
    bool survival = false;     // 0x178
    bool dingSent = false;     // 0x179
    int level = 0;             // 0x17c
    int volume = 4;            // 0x180 0..7
    unsigned goalsDone = 0;    // 0x184 achievement bits this run
    int maxSaucers = 0;        // 0x188
    int saucerCount = 0;       // 0x18c
    int startLevel = 0;        // 0x190
    int startBonus[2][8] = {}; // 0x194
    int saucerTimer = 0;       // 0x308
    int maze = 1;              // 0x30c tank maze 0..2
    int msgTimer = 0;          // 0x310 frames until the message clears
    int state = 1;             // 0x314
    int maxPlayers = 2;        // 0x318
    int lastMode = 0;          // 0x31c
    int lastPlayers = 1;       // 0x320
    int lastStartLevel = 0;    // 0x324
    int minesHit = 0;          // 0x328 mines detonated by players
    int timeLeft = 0;          // 0x32c tank battle frames left
    int planetsSpawned = 0;    // 0x330 (bit 0 also picks the orbit direction)
    float warp = 0;            // 0x334 feedback zoom in / out
    bool blackHole = false;    // 0x338

    bool startBonusDue = false; // 0x33a
    entity *sun = nullptr;      // 0x33c
    float massScale = 1.0f;     // 0x340
    float massGrowth = 1.002f;  // 0x344
    int mode = 0;               // 0x348 0 solar, 1 deep space, 2 tanks, 3 jets
    int subMode = 0;            // 0x34c 0 ships, 1 tanks, 2 jets
    int nPlayers = 1;           // 0x350
    int shields[4] = {};        // 0x354
    int scores[4] = {};         // 0x364
    int lives[4] = {};          // 0x374 (tanks / jets: kills)
    int rescued = 0;            // 0x384 minotaurs collected this game

    int rescuedThisLevel = 0; // 0x38c
    int levelsNoRescue = 0;   // 0x390 consecutive levels without a rescue
    int levelsCleared = 0;    // 0x394
    int infoPage = 0;         // 0x398

    int pausedState = 0; // 0x3a4

    // ---- port additions: local high scores (replacing OpenFeint) ----
    // Boards as the game numbered them for OpenFeint (postBoard): 0 solar,
    // 1 deep space, 2 solar survival, 3 deep space survival.
    static constexpr int kHiBoards = 4;
    mino::HighScores hs; // loaded / saved by the shell
    int hiBoard = 0;     // board on show
    int hiNew = -1;      // row just entered (highlighted)
    int hiReturn = 0;    // where BACK goes: 1 title sequence, 2 title menu, 3 info menu
    int hiTimer = 0;
    int entryScore = 0, entryLevel = 0, entryBoard = 0;
    std::string entryName;

    void defaultHighScores();
    void highScoreScreen(); // state 0x71
    void nameEntryScreen(); // state 0x72
    void leaveHighScores();
    void finishNameEntry();
};

} // namespace Neon_Internals
} // namespace NEON_GAME
