// bge.h - Caverns of Minos background effects ("BGEs"): the waves theGame::launchBGEs
// (0x5c884) sends after the cavern goal, one kind per sector (cavern zone word low byte,
// BGESetup): 0 asteroid, 1 invader (+ invBull), 2 galag, 3 grunt, 4 slab, 5 ball, 6 tet.
// Translated from the decompilation with the instructions checked where it loses doubles,
// multipliers or call arguments. All go to list +0x2e8 (bounceList).
#pragma once
#include "enemies.h"

namespace minos {

struct Asteroid
    : PlayEntity { // vtable 0x1ece5c, 0x174 bytes: init 0x559cc, run 0x6f804, hitBy 0x6f044
    void init(PlayState &g);
    void run() override;
    void hitBy(Entity *attacker) override;
    float spin = 0;         // +0x16c added to rot
    int32_t generation = 0; // +0x170 splits left
};

struct Invader : Enemy { // vtable 0x1ece9c, 0x188 bytes: run 0x6fb70, hitBy 0x5f86c
    void run() override;
    void hitBy(Entity *attacker) override;
    // score +0x16c is also the step period (run reloads its timer from it; hitBy shortens it)
    float stepX = 0;        // +0x170
    int32_t group = 0;      // +0x174 BGEIntervalSetup count at launch (the wave)
    int32_t fireChance = 0; // +0x178 out of 4096 per frame
    int32_t dying = 0;      // +0x17c
    int32_t timer = 0;      // +0x180
    int32_t frames = 0;     // +0x184
    int32_t spriteBase = 0; // +0x100
};

struct InvBull : EnemyBull { // vtable 0x1ece7c, 0x1d4 bytes: run 0x70718
    void run() override;
    neon::colour saved; // +0x184 the invader's colour
};

struct Galag : Enemy { // vtable 0x1ece3c, 0x1b0 bytes: init 0x59768, run 0x6e554, hitBy xentity
    void start(PlayState &g); // galag::init
    void run() override;
    float angle = 0, angleRate = 0; // +0x178, +0x17c
    float phase = 0, phaseRate = 0; // +0x180, +0x184
    float fall = 0, speed = 0;      // +0x188 (added to vel.y), +0x18c (sin amplitude of vel.x)
    Vec2 base, wobble;              // +0x190, +0x1a0
};

struct Grunt : Enemy { // vtable 0x1ecebc, 0x18c bytes: run 0x6ffcc, hitBy 0x6efd4
    void run() override;
    void hitBy(Entity *attacker) override;
    int32_t period = 0; // +0x178 frames per step (hitBy of a wave mate shortens it)
    float speed = 0;    // +0x17c
    int32_t group = 0;  // +0x180
    int32_t timer = 0;  // +0x184
    int32_t frames = 0; // +0x188
};

struct Slab : PlayEntity { // vtable 0x1ecdfc, 0x16c bytes: run 0x6b38c, hitBy 0x5dfe0
    void run() override;
    void hitBy(Entity *attacker) override;
};

struct Ball : Enemy { // vtable 0x1ecddc, 0x178 bytes: run 0x6b0d8, hitBy xentity
    void run() override;
};

struct Tet : Enemy { // vtable 0x1ecdbc, 0x180 bytes: init 0x54a24, run 0x6ae3c, hitBy 0x6ef00
    void start(PlayState &g); // tet::init
    void run() override;
    void hitBy(Entity *attacker) override;
    int32_t hits = 0;   // +0x178 hits left
    float spinStep = 0; // +0x17c added to rot per hit
};

// theGame::launchBGEs 0x5c884 (set as PlayState::launchBGEs by the host).
void launchBGEs(PlayState &g);

} // namespace minos
