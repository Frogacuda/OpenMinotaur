// game_entities.h - Minotaur Rescue's entity classes: the player craft
// (ships, tanks, jets) with their guns, shots and trails, and the world
// (planets, the sun, saucers, mines, minotaurs) plus the particle effects.
#pragma once

#include "game.h"

namespace NEON_GAME {
namespace Neon_Internals {

// A small white-to-blue spark burst (an "explosion" with no velocity) at p.
void makeSpark(spaceWars *g, vec2d p);

// ------------------------------------------------------------------ craft ---
// Shared player-vehicle physics: thrust from the player's joystick (or the
// autopilot), gravity from the planets, a speed limit, a position history.

class craft : public entity {
  public:
    craft();
    void run() override;
    virtual void doAutoPilot() {} // [6]
    virtual void damage() {}      // [7] (spaceCraft and up)

    bool thrusting = false;    // 0xc4
    class gun *gun = nullptr;  // 0xc8
    vec2d thrust;              // 0xcc
    vec2d force;               // 0xdc acceleration this frame
    float drawAngle = 0;       // 0xec rot, smoothed
    float turnRate = 0.08f;    // 0xf0
    int joy = 0;               // 0xf4 player / joystick zone
    float power = 6.0f;        // 0xf8 thrust
    bool autopilot = false;    // 0xfc
    vec2d history[128];        // 0x100
    unsigned historyCount = 0; // 0x900
    float maxSpeed = 0.01f;    // 0x904
};

class spaceCraft : public craft {
  public:
    ~spaceCraft() override;
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void hitBy(entity *by) override;
    void doAutoPilot() override;
    void damage() override;
    void init(spaceWars *g, int player);
    void reSpawn(); // inlined in the original

    int weapon = 0;                             // 0x908 weapon level
    float spin = 0;                             // 0x90c (> 0: spinning after a hit)
    float spinDecay = 0.1f;                     // 0x910
    float phase[3] = {};                        // 0x914
    float phaseSpeed[3] = {0.1f, 0.13f, 0.11f}; // 0x920
    float trailDist = 0;                        // 0x92c
    int apTimer = 0;                            // 0x930
    int apThrustTimer = 0;                      // 0x934
    float apThrust = 0;                         // 0x938
    bool apReturning = false;                   // 0x93c
    vec2d apDir;                                // 0x940
};

class jet : public spaceCraft {
  public:
    void run() override;
    void damage() override;
    void init(spaceWars *g, int player, int slot);
    void reSpawn();

    int life = 0;             // 0x950 times this jet has been rebuilt
    float distance = 0;       // 0x954 total distance flown
    float trailCountdown = 0; // 0x958
    int slot = 0;             // 0x95c place in the formation
    float cruise = 0.8f;      // 0x960
    float aiPhase = 0;        // 0x964
    float aiPhaseSpeed = 0;   // 0x968
    float aiTurn = 0;         // 0x96c
};

class tank : public craft {
  public:
    ~tank() override;
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void doAutoPilot() override;
    void init(spaceWars *g, int player);
    // Walks from `from` to `to` in 40 steps; pathResult 1 wall, 2 / 3 off the
    // side / top, 0 clear; pathLength is how far it got.
    void checkPath(vec2d from, vec2d to);

    bool bumped = false;      // 0x908
    float distance = 0;       // 0x90c
    float trailCountdown = 0; // 0x910
    int powerLevel = 0;       // 0x914 minotaurs collected (faster fire)
    float spin = 0;           // 0x918 (> 0: spinning after a hit)
    float spinDecay = 0.02f;  // 0x91c
    int apTimer = 0;          // 0x920
    int pathResult = 0;       // 0x924
    vec2d apDir;              // 0x928
    float pathLength = 0;     // 0x938
};

// A fading dot left behind a jet (`smoke` once it is damaged).
class jetTrail : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;

    float startDist = 0; // 0xc4 owner's distance when laid
    float dist = 0;      // 0xc8 distance behind the owner
    float baseSize = 0;  // 0xcc
    int life = 0;        // 0xd0 owner's life when laid
    bool smoke = false;  // 0xd4
};

// Track marks behind a tank.
class tankTrail : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;

    float startDist = 0; // 0xc4
    float dist = 0;      // 0xc8
};

class spaceCraftGun : public gun {
  public:
    void run() override;
    void shoot() override;
};

class tankGun : public gun {
  public:
    void run() override;
    void shoot() override;

    int burst = 0; // 0x50 rapid-fire shots left
};

class spaceCraftBull : public bull {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void wasShot(entity *) override {}
};

class tankBull : public bull {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void wasShot(entity *) override {}
    void hitBy(entity *by) override;
};

class saucerShot : public enemyBull {
  public:
    void run() override;
};

// ------------------------------------------------------------------ world ---

class saucer : public entity {
  public:
    void run() override;
    void wasShot(entity *by) override;

    int big = 0;            // 0xc4
    int kind = 0;           // 0xc8 (level / 4) & 3
    int moveTimer = 0;      // 0xcc
    int shotTimer = 0;      // 0xd0
    int shotReload = 0;     // 0xd4
    int aimedCount = 0;     // 0xd8 random shots since the last aimed one
    int burst = 0;          // 0xdc
    vec2d burstVel;         // 0xe0
    int colourType = 0;     // 0xf0
    unsigned moveFlags = 0; // 0xf4 1/2 chase in x/y, 4 change course,
                            //      8/16 ... towards the player,
    vec2d maxVel;           // 0xf8
    float accel = 0;        // 0x108
};

class mine : public entity {
  public:
    void run() override;
    void wasShot(entity *by) override;
    void hitBy(entity *by) override;
    void detonate(int byGroup);

    float fuse = 1.0f;       // 0xc4
    float fuseRate = 0.002f; // 0xc8
    int count = 0;           // 0xcc
    int kind = 0;            // 0xd0
};

// A free-floating minotaur (to be rescued).
class collectible : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void hitBy(entity *by) override;
    void init(entity *from);

    float spin = 0; // 0xc4
};

// Tanks mode: a minotaur that appears after a countdown on the grid.
class tankMinotaur : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void hitBy(entity *by) override;

    colour f0cc;              // 0xcc (unused)
    int countdown = 0x240;    // 0xec
    int cellX = 0, cellY = 0; // 0xf0, 0xf4
};

class thePlanet : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void wasShot(entity *by) override;
    void hitBy(entity *by) override;
    void reSpawn();
    void split(vec2d v);

    float spin = 0;          // 0xc4
    int orbitDir = 0;        // 0xc8
    int age2 = 0;            // 0xcc
    int generation = 2;      // 0xd0 2 whole, 1 half, 0 quarter
    colour flashColour;      // 0xd4
    float flashT = 0;        // 0xf4
    float flashDecay = 0.4f; // 0xf8
};

class theSun : public entity {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
    void hitBy(entity *by) override;

    bool collapsed = false; // 0xc4 black hole
    // Pulse phases 0xc8 (yellow), 0xcc (red), 0xd8, 0xdc (unused) and their
    // speeds 0xd0, 0xd4, 0xe0, 0xe4.
    float pulse[4] = {};
    float pulseSpeed[4] = {0.1f, 0.11f, 0.01f, 0.002f};
};

// ---------------------------------------------------------------- effects ---

class gravParticle : public _particle {
  public:
    int draw(sprite *spr, vec2d size) override;
    void run() override;
};

class solarFlare : public _particle {
  public:
    int draw(sprite *spr, vec2d size) override;
};

class starFieldStar : public _particle {
  public:
    int draw(sprite *spr, vec2d size) override;
};

// The heart (with the points) a rescued minotaur leaves.
class minotaurLoveHeart : public _particle {
  public:
    int draw(sprite *spr, vec2d size) override;

    int digits[8] = {};         // 0x11c
    int *digitPtr = digits + 7; // 0x13c
    vec2d digitPos;             // 0x140
};

} // namespace Neon_Internals
} // namespace NEON_GAME
