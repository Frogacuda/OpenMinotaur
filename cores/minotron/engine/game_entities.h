// game_entities.h - Minotron's entity classes (players, beasts, enemies,
// bullets, particles). Members are named by their original byte offset
// (fXXX) unless the meaning is clear; the class layouts are otherwise free.
//
// Constructors set what the original's constructors (often inlined into
// theGame::genLevel) set; everything else is zero-initialised.
#pragma once

#include "game.h"

namespace NEON_GAME {
namespace Neon_Internals {

// "gEntity" in the original: an entity with a virtual init(theGame*). In
// this port every entity has init() (entity vtable slot 6), so gEntity is
// just an alias.
using gEntity = entity;

// ================================================================ player ====

class playerMinotaur : public entity {
  public:
    playerMinotaur();
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void init(theGame *g) override;
    void shoot();
    void kill();

    entity *demoTarget = nullptr; // 0x168 beast the demo player walks to
    bool moveTouch = false;       // 0x16c a movement touch is down
    bool aimCursor = false;       // 0x16d show the aim-touch marker
    vec2d aimTouchPos;            // 0x170
    bool moveCursor = false;      // 0x180 show the move-touch marker
    vec2d moveTouchPos;           // 0x184
    vec2d lastMove;               // 0x194
    vec2d lastAim;                // 0x1a4
    int fireTimer = 15;           // 0x1b4
    int fireReload = 6;           // 0x1b8 (aiDroid: its fire rate)
    vec2d aimDir{1.0, 0.0};       // 0x1bc
    float trailDist = 0;          // 0x1cc
    vec2d trail[1024];            // 0x1d0 positions, for beasts to follow
    int trailIndex = 0;           // 0x41d0
    int rescued = 0;              // 0x41d4 beasts collected (herd waves)
    bool invincible = false;      // 0x41d8
    float invT = 0;               // 0x41dc invincibility pulse (0..10)
    float invDT = 0;              // 0x41e0
};

// The helper droid of "assisted" mode.
class aiDroid : public playerMinotaur {
  public:
    explicit aiDroid(theGame *g);
    void run() override;

    float aimAngle = 0;   // 0x41e4
    float bob = 0;        // 0x41e8
    int droidType = 0;    // 0x41ec (saveData + 0xc84)
    bool changed = false; // 0x41f0
};

// The player's shot.
class basicBull : public entity {
  public:
    basicBull() = default;
    void run() override;
    int life = 0; // 0x168 frames to live
};

class droidBull : public entity {
  public:
    droidBull() = default;
    void run() override;
};

// The "explosive shots" blast: grows, fades and hits what it touches.
class expandaBull : public entity {
  public:
    expandaBull() = default;
    void run() override;
};

// ============================================================== beasts =====

// A beastie to rescue: wanders until touched by the minotaur (or the droid),
// then follows its trail.
class beast : public entity {
  public:
    beast() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;
    void initRaw(theGame *g);

    int f164 = 0;
    int gfx = 0;           // 0x16c first sprite (2 frames)
    int timer = 0;         // 0x170 frames to the next wander direction
    int followingSlot = 0; // 0x174 place in the following line
    int rescuerType = 0;   // 0x178 3 the minotaur, 0x34 the droid
    bool rescued = false;  // 0x17c
};

// Powerup pickup (kinds: 0 beast love, 1 hot bullets, 2 bomb, 3 3-way shot,
// 4 reflective shots, 5 Floyd bonus, 6 extra life, 7 explosive shots,
// 8 invincibility).
class powerup : public entity {
  public:
    powerup() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    float phase = 0;   // 0x168
    float life = 1.0f; // 0x16c
    int kind = 0;      // 0x170
};

// Floating score/bonus sprite.
class littleBonus : public entity {
  public:
    littleBonus() = default;
    void run() override;
    void init(theGame *g) override;

    int framesRemaining = 0;  // 0x168 frames to live
    float growthPerFrame = 0; // 0x16c growth per frame (> 1 grows and fades)
    int paletteIndex = 0;     // 0x170 theGame::gcol index
};

class staticObstacle : public entity {
  public:
    staticObstacle() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    float f168 = 0, f16c = 0, f170 = 0;
};

class raindrop : public entity {
  public:
    raindrop() = default;
    void run() override;
    void hitBy(entity *by) override;
};

// Zombeast fragments.
class shrapnel : public entity {
  public:
    shrapnel() = default;
    void run() override;
    void hitBy(entity *by) override;
    int f168 = 0;
};

class turd : public entity {
  public:
    turd() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;
    int harmlessFrames = 0; // 0x168 frames before it can hurt
    float spin = 0;         // 0x16c
};

class bogroll : public entity {
  public:
    bogroll() = default;
    void run() override;
    void hitBy(entity *by) override;
};

// ============================================================== enemies =====
// Shared: `game` is at 0x164 for every gEntity-derived class; f-numbered
// members are at that byte offset in the original.

class gruntle : public entity {
  public:
    gruntle() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;
    // The common setup (no position); shared with the gruntles that
    // recognisers and sheepipedes drop.
    void setup(theGame *g);

    int gfx = 0;                         // 0x168 first animation frame
    int animType = 0;                    // 0x16c gruntAnimationType
    int timer = 0;                       // 0x170 frames to the next step
    int timerReload = 0;                 // 0x174 (spiny: fixed step time)
    unsigned animCount = 0;              // 0x178
    double baseScX = 1.0, baseScY = 1.0; // 0x17c, 0x184
    float speed = 0;                     // 0x18c step speed
    bool blob = false;                   // 0x190 hit on a "blob" level: bounces about
};

class spiny : public gruntle {
  public:
    spiny() = default;
    void hitBy(entity *by) override;
    void init(theGame *g) override;
    float f1a0 = 0; // 0x1a0
};

class mandy : public entity {
  public:
    mandy() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    double accelX = 0, accelY = 0; // 0x168, 0x170
    int facingLocked = 0;          // 0x178 (non-zero: keeps its facing)
};

class shootingMandy : public mandy {
  public:
    shootingMandy() = default;
    void run() override;

    int shotTimer = 0;       // 0x17c
    int shotReload = 0;      // 0x180 (frontFireRate; < 0 never)
    int rockTimer = 0;       // 0x184
    int rockReload = -1;     // 0x188 (< 0 never)
    float shotSpeed = 0.01f; // 0x18c
};

class sixteenTons : public entity {
  public:
    sixteenTons() = default;
    void run() override;
    void hitBy(entity *by) override;

    bool falling = false;       // 0x168
    float trackSpeed = 0.0004f; // 0x16c
    float riseSpeed = 0.004f;   // 0x170
    float fallAccel = 0.0004f;  // 0x174
};

class brolly : public entity {
  public:
    brolly() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    bool closed = false; // 0x168 touched by the minotaur
    int rainTimer = 0;   // 0x16c
    int rainReload = 30; // 0x170
};

// Horizontal laser emitter; vlaser is the vertical one (rot = pi/2).
class hlaser : public entity {
  public:
    hlaser() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;

    double accelX = 0, accelY = 0; // 0x168, 0x170
};

class vlaser : public hlaser {
  public:
    vlaser() = default;
    int draw(sprite *s, vec2d sz) override;
};

// Big blobby splits into numBlobs small ones; smallBlobby has type 0x13.
class bigBlobby : public entity {
  public:
    bigBlobby() = default;
    void run() override;
    void hitBy(entity *by) override;
};

class smallBlobby : public bigBlobby {
  public:
    smallBlobby() = default;
};

class spinner : public entity {
  public:
    spinner() = default;
    void run() override;
    void hitBy(entity *by) override;
};

class pointyArrow : public entity {
  public:
    pointyArrow() = default;
    void run() override;
    void hitBy(entity *by) override;

    int mode = 0;  // 0x168 0 aim, 1 fly, 2 stunned spin
    int timer = 0; // 0x16c
};

// Types 0x15 (robot), 0x2f (growing robot); subclasses runningMan (0x17,
// turns beasts into zombeasts) and killerRam (0x26).
class killerRobot : public entity {
  public:
    killerRobot() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    entity *prey = nullptr; // 0x168 beast being chased
    int brainChance = 0;    // 0x16c (out of 8192 per frame)
};

class runningMan : public killerRobot {
  public:
    runningMan() = default;
    void init(theGame *g) override;
};

class killerRam : public killerRobot {
  public:
    killerRam() = default;
    void init(theGame *g) override;
};

class zombeast : public entity {
  public:
    zombeast() = default;
    void run() override;
    void hitBy(entity *by) override;
};

class lavvy : public entity {
  public:
    lavvy() = default;
    void run() override;
    void hitBy(entity *by) override;

    int rollTimer = 0;  // 0x168
    int rollReload = 0; // 0x16c
};

class yinYang : public entity {
  public:
    yinYang() = default;
    void run() override;
    void hitBy(entity *by) override;

    int mineTimer = 0;  // 0x168
    int mineReload = 0; // 0x16c
};

class yyMine : public entity {
  public:
    yyMine() = default;
    void run() override;
    void hitBy(entity *by) override;
    int life = 0; // 0x168
};

class teapot : public entity {
  public:
    teapot() = default;
    void run() override;
    void hitBy(entity *by) override;
    int stun = 0; // 0x168
};

// Skier and its subclasses licker, biscuit (firingBiscuit, beachball).
class skier : public entity {
  public:
    skier() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int gfx = 0;        // 0x168
    int stun = 0;       // 0x16c frames knocked back
    int moveType = 0;   // 0x170 0..3
    float baseSize = 0; // 0x174
    float stunDrag = 0; // 0x178
};

class licker : public skier {
  public:
    licker() = default;
    void run() override;
    void init(theGame *g) override;
    float trail = 0; // 0x17c distance since the last mine
};

class biscuit : public skier {
  public:
    biscuit() = default;
};

class firingBiscuit : public biscuit {
  public:
    firingBiscuit() = default;
    void run() override;
    int shotTimer = 5;  // 0x17c
    int shotReload = 5; // 0x180
};

class beachball : public biscuit {
  public:
    beachball() = default;
};

// A fading trail of positions (fhg, falseTeeth, kevinToms, brainBull).
struct trailPoint {
    vec2d pos;
    double alpha = 0;
};

// Recogniser: enters from a side, moves in straight lines, drops gruntles.
class recogniser : public entity {
  public:
    recogniser() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int turnTimer = 0;    // 0x168
    int turnReload = 120; // 0x16c
    bool entered = false; // 0x170
    int dropTimer = 50;   // 0x174
    int dropReload = 50;  // 0x178
};

// Spawned by fhg: wanders and squirts at the minotaur.
class hydrant : public entity {
  public:
    hydrant() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int turnTimer = 0;    // 0x168
    int turnReload = 120; // 0x16c
    unsigned mode = 0;    // 0x170
    int shotTimer = 0;    // 0x174
    int shotReload = 90;  // 0x178
    float speed = 0.004f; // 0x17c
};

class sheepipede : public entity {
  public:
    sheepipede() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    vec2d trail[512];        // 0x168
    unsigned trailIndex = 0; // 0x2168
    int segments = 12;       // 0x216c
    int hitDelay = 0;        // 0x2170
    float wobble = 0;        // 0x2174
    int segHp = 5;           // 0x2178
    int dropTimer = 40;      // 0x217c
    int dropReload = 40;     // 0x2180
};

// Fixed cannons on an edge (types 0x1e, 0x2c, 0x2d, 0x2e) tracking the
// minotaur along it; type 0x1e also lobs splitting rocks.
class astroSmashCannon : public entity {
  public:
    astroSmashCannon() = default;
    void run() override;
    void hitBy(entity *by) override;

    double lockX = 0, lockY = 0; // 0x168, 0x170 non-zero: does not track that axis
    int shotTimer = 20;          // 0x178
    int shotReload = 20;         // 0x17c
    int rockTimer = 0;           // 0x180
    int rockReload = -1;         // 0x184
    vec2d shotVel;               // 0x188
    double f198 = 0, f1a0 = 0;   // 0x198, 0x1a0
    int shotSprite = 0x8b;       // 0x1a8
    int f1ac = 0;                // 0x1ac
};

class astroSmashRock : public entity {
  public:
    astroSmashRock() = default;
    void run() override;
    void hitBy(entity *by) override;
    void initFrom(astroSmashCannon *c);

    vec2d target;        // 0x168
    int generations = 2; // 0x178
    int pieces = 2;      // 0x17c
    int life = 600;      // 0x180
};

// The flying head thing: meanders, leaving a trail, and drops hydrants.
class fhg : public entity {
  public:
    fhg() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    float heading = 0;         // 0x168
    float turn = 0;            // 0x16c
    float speed = 0;           // 0x170
    int turnTimer = 0;         // 0x174
    int dropTimer = 0;         // 0x178
    int dropReload = 0;        // 0x17c
    int turnReload = 0;        // 0x180
    int drops = 0;             // 0x184 hydrants left
    trailPoint trail[1024];    // 0x188
    float trailRot[1024] = {}; // 0x6188
    unsigned trailIndex = 0;   // 0x7188
    float trailDist = 0;       // 0x718c
    bool dying = false;        // 0x7190
};

// Teeth on a Lissajous path; 0x24 shoots horizontally, 0x30 vertically.
class falseTeeth : public entity {
  public:
    falseTeeth() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int shotTimer = 30;              // 0x168
    int shotReload = 30;             // 0x16c
    trailPoint trail[1024];          // 0x170
    unsigned trailIndex = 0;         // 0x6170
    float trailDist = 0;             // 0x6174
    bool dying = false;              // 0x6178
    float r1 = 0;                    // 0x617c main radius
    float a1 = 0, a2 = 0;            // 0x6180, 0x6184
    float da1 = 0;                   // 0x6188
    float a3 = 0, a4 = 0;            // 0x618c, 0x6190
    float da2 = 0, da3 = 0, da4 = 0; // 0x6194, 0x6198, 0x619c
    float r2 = 0;                    // 0x61a0 wobble radius
};

// The boss: a Lissajous dance spraying shots, then (second life) chases.
class kevinToms : public entity {
  public:
    kevinToms() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    trailPoint trail[1024];          // 0x168
    unsigned trailIndex = 0;         // 0x6168
    float trailDist = 0;             // 0x616c
    bool dying = false;              // 0x6170
    float r1 = 0.3f;                 // 0x6174
    float a1 = 0, a2 = 0;            // 0x6178, 0x617c
    float da1 = 0;                   // 0x6180
    float a3 = 0, a4 = 0;            // 0x6184, 0x6188
    float da2 = 0, da3 = 0, da4 = 0; // 0x618c, 0x6190, 0x6194
    float a5 = 0, da5 = 0;           // 0x6198, 0x619c shot sweep
    int shotTimer = 120;             // 0x61a0
    int shotReload = 120;            // 0x61a4
    int burst = 5;                   // 0x61a8 frames of each volley
    float r2 = 0;                    // 0x61ac
    int phase = 0;                   // 0x61b0
    int stun = 0;                    // 0x61b4
};

// Thrown by running men: zig-zags at the minotaur with a trail.
class brainBull : public entity {
  public:
    brainBull() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int turnTimer = 0;       // 0x168
    int turnReload = 30;     // 0x16c
    unsigned axis = 0;       // 0x170
    trailPoint trail[1024];  // 0x174
    unsigned trailIndex = 0; // 0x6174
    float trailDist = 0;     // 0x6178
    bool dying = false;      // 0x617c
};

// ------------------------------------------------------------- bullets ---

class shootableEnemyBullet : public entity {
  public:
    shootableEnemyBullet() = default;
    void run() override;
    void hitBy(entity *by) override;
    void init(theGame *g) override;

    int edgeBehavior = 0;             // 0x168 edge behaviour (0 stop, 1 die, 2 bounce)
    int framesRemaining = 150;        // 0x16c frames to live
    int invulnerableFrames = 2;       // 0x170 frames it cannot be hit
    bool killingShotSurvives = false; // 0x174 the shot that kills it survives
    float spriteAngleOffset = 0;      // 0x178 sprite angle offset (mode 1)
    int flightMode = 0; // 0x18c 0 straight, 1 vectorSeek, 2 straight animated, 3 homing animated
    float spinPerFrame = 0.2f; // 0x190 spin
};

class genericEnemyBull : public entity {
  public:
    genericEnemyBull() = default;
    void run() override;
    void hitBy(entity *by) override;
    bool hittingShotSurvives = false; // 0x168
};

class skateboard : public entity {
  public:
    skateboard() = default;
    void run() override;
    void hitBy(entity *by) override;
    void launch(theGame *g, vec2d v);
};

// ============================================================ particles =====

class explosion : public _particle {
  public:
    explosion();
    int draw(sprite *s, vec2d sz) override;

    int count = 0x10;     // 0x21c
    float radius = 16.0f; // 0x220 spread (x size)
    unsigned seed = 0;    // 0x224
};

class droplet : public _particle {
  public:
    droplet() = default;
    void run() override;
};

// One piece of a shattered sprite (theGame::pixelShatter).
class shatterBit : public entity {
  public:
    shatterBit() = default;
    int draw(sprite *s, vec2d sz) override;
    void run() override;

    float rate = 0.02f;            // 0x168 life lost per frame
    double texX = 0, texY = 0;     // 0x16c, 0x174 piece origin in the sprite (pixels)
    double pieceW = 0, pieceH = 0; // 0x17c, 0x184 piece size (pixels)
    vec2d target;                  // 0x18c (type 1: fly to this point)
    float f19c = 1.0f;             // 0x19c
};

} // namespace Neon_Internals
} // namespace NEON_GAME
