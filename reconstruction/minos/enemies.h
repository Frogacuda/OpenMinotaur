// enemies.h - Minos map entity families (created by theGame::launch from map characters):
// the shared xentity contact response and each class's init/run/hitBy. Translated from the
// decompilation. Offsets refer to the original image.
#pragma once
#include "ship.h"

namespace minos {

extern const uint32_t kRcols[12]; // rcols (0x231ba0): count 11, then RGBA words
// Shared creation helpers (enemies.cpp): explo into +0x2e0 (rot rand(-pi, pi)); a bspark as
// xentity::hitBy makes them (vel y, vel x, age rate draws); the ship's position minus p.
void addExplo(PlayState &g, Vec2 pos, float from, float to, float rate);
void addSpark(PlayState &g, Vec2 pos, double ylo, double yhi, double xlo, double xhi, double rlo,
              double rhi);
Vec2 toShip(const PlayState &g, Vec2 p);

// centity subclasses: MapEntity (centity/xentity fields) + theGame.
struct Enemy : MapEntity {
    PlayState *g = nullptr;                // +0x168
    virtual void init() {}                 // Class::init(theGame*) where the class has one
    void hitBy(Entity *attacker) override; // xentity::hitBy 0x6e9e8
    void mapRun();                         // centity::run with the explo it may create
    int32_t visibleTile() const; // the visible tile under pos (-1 outside), as faller/thrust
};

struct Turret : Enemy { // 'T' etc., init 0x55888, run 0x6a024
    void init() override;
    void run() override;
    Vec2 aim; // +0x194 bullet velocity (0, -0.004) rotated by rot
};

struct EnemyBull : PlayEntity { // _enemyBull, vtable 0x1ecedc: run 0x7035c, hitBy 0x5fca8
    void run() override;
    void hitBy(Entity *attacker) override;
    double aimX = 0;      // +0x16c (scrambleRocket: float(ship.x - x) * -0.008; not read by run)
    double fall = 0;      // +0x174 added to vel.y (binary64: vldr d)
    bool noDrift = false; // +0x17c
};

struct CavernGoal : Enemy { // 'G': run 0x69f1c, hitBy 0x758d4
    void run() override;
    void hitBy(Entity *attacker) override;
};

struct OneUp : Enemy { // run 0x69e1c, hitBy 0x5e2a0
    void run() override;
    void hitBy(Entity *attacker) override;
};

struct Faller : Enemy { // init 0x56b94, run 0x6a8e8, hitBy 0x5957c
    void init() override;
    void run() override;
    void hitBy(Entity *attacker) override;
    Vec2 v184;                 // +0x184 (zeroed; not read here)
    double accel = 0;          // +0x18c added to vel.y while falling
    float spin = 0;            // +0x194
    float spinWhenFalling = 0; // +0x198
    bool falling = false;      // +0x19c
    bool fading = false;       // +0x19d
};

struct Saucer : Enemy { // 's': init 0x54828, run 0x69458
    void init() override;
    void run() override;
    bool awake = false;        // +0x182
    float triggerDistance = 0; // +0x184
    float phase = 0;           // +0x188
    float phaseStep = 0;       // +0x18c
    float maxVy = 0;           // +0x190
    float xSpeed = 0;          // +0x194
};

struct ScrambleRocket : Enemy { // init 0x570f4, run 0x6a1ec, hitBy 0x73ca0
    void init() override;       // kind by sprite: 5 fuel pod, 0x22 gun, 0x37 rocket
    void run() override;
    void hitBy(Entity *attacker) override;
    Vec2 dir;              // +0x184 (0, -size * 0.001) rotated by rot
    bool launched = false; // +0x194 (0x37)
    float trigger = 0;     // +0x198 (0x37) launch when |ship.y - y| < this
};

struct Rocket : Enemy {   // init 0x56930, run 0x725ec, foom 0x71ae4, hitBy 0x71ebc
    void init() override; // kind by sprite: 2 missile, 5 fuel pod, 6 rocket,
    void run() override;  // 0x21/0x41 bomber/mine, 0x22/0x40 gun, 0x23 homing
    void hitBy(Entity *attacker) override;
    void foom();
    int32_t timer = 10000;  // +0x184 frames until launch (< 1: flying)
    int32_t burstAfter = 0; // +0x188 (6, 0x23) frames of flight before bursting
    float mineRange = 0;    // +0x198 (0x41)
};

struct Goat : Enemy { // init 0x55b78, run 0x7078c (hitBy: xentity), vtable 0x1ecf34
    void init() override;
    void run() override;
    int32_t state = 0; // +0x184 0 walking, 1 jumping, 2 thrown (set elsewhere)
    float lift = 0;    // +0x188 state 2: explo trail fades as this reaches 1
    float spin = 0;    // +0x18c state 2: added to rot
};

struct MHeart : PlayEntity { // mheart, vtable 0x1ecefc (400 bytes): init 0x55728, run 0x55094
    void init(PlayState &g); // entity::draw; drag (+0xac/+0xb4) is the entity's
    void run() override;
    float age = 0;           // +0x16c
    float ageRate = 0;       // +0x170
    float spin = 0;          // +0x174
    Vec2 accel;              // +0x178
    float alphaScale = 1.0f; // +0x188 colour alpha = (1 - age) * this
    float sizeRate = 0;      // +0x18c
};

struct WavingMinotaur
    : Enemy { // 'M' (0x1c0 bytes, no init): run 0x76b68, hitBy 0x7521c, vtable 0x1ecf14
    void run() override;
    void hitBy(Entity *attacker) override;
    void jumpIntoShip();  // 0x750d8
    float phase = 0;      // +0x184 frame phase 0..8 (launch: float(rand(0, 7)))
    int32_t state = 0;    // +0x188 0 waving, 1 jumping into the ship, 2 falling
    float hop = 0;        // +0x18c y offset of the jump
    float hopVel = 0;     // +0x190
    float hopGravity = 0; // +0x194
    Vec2 from;            // +0x198 chases the ship by t each frame
    float t = 0;          // +0x1a8
    float spinRate = 0;   // +0x1ac rot = hop * this
    Vec2 home;            // +0x1b0 launch position copy (not read by run)
};

struct MotherSheep : Enemy { // init 0x5674c, run 0x69620, hitBy 0x75234, vtable 0x1ecf94
    void init() override;
    void run() override;
    void hitBy(Entity *attacker) override;
    int32_t state = 0;    // +0x184 0 waiting, 1 leaving (after the ship reached it)
    Vec2 wobble;          // +0x188 (sin(frame / 511) * 0.3, 0)
    Vec2 rest;            // +0x198 position without the wobble
    float heartAngle = 0; // +0x1a8
    float heartStep = 0;  // +0x1ac
    int32_t pulse = 0;    // +0x1b0 counts 22..0
    int32_t hum = 0;      // +0x1b4 sample volume factor
};

struct ScRocket : Enemy { // created by theGame::doLauncher 0x5d138; run 0x6ab4c, hitBy 0x742ec
    void run() override;  // vtable 0x1ed0f4 (0x184 bytes, no fields of its own)
    void hitBy(Entity *attacker) override;
};

struct FloatingScore : PlayEntity { // theGame::_score's popup, run 0x54278, draw 0x55478
    void run() override;
    int draw(EntityRenderer &r, Vec2 screen) override;
    int32_t digits[16] = {};   // +0x16c
    int32_t first = 15;        // +0x1ac
    int32_t spriteBase = 0x12; // +0x1b0
    float alpha = 1.0f;        // +0x1b4
    float fade = -0.01f;       // +0x1b8
};

// Creates the class of a factory request (theGame::launch's allocation, init, post-init writes).
Enemy *makeEnemy(cavern::EntityClass cls);

} // namespace minos
