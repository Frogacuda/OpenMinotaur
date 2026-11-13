// ship.h - the Minos player ship family (Minos_armv7): shentity (doControls 0x65134,
// rateLanding 0x5e374, draw 0x55194), ship (init 0x62b70, run 0x68118), its children
// sensor (run 0x5eec0, draw 0x53f08 = nothing) and gnu (run 0x59ad8, fire 0x598f0,
// draw 0x5405c), what they create: shot (run 0x71544), thrustParticle (init 0x56cd8,
// run 0x5a5ac), bspark (run 0x71194), explo (run 0x541b8), and theGame::spark 0x5f5cc /
// theGame::checkKlaxons 0x5e05c. Translated from the decompilation with explicit
// original numerical boundaries.
//
// PlayState is the part of theGame these classes read and write, with the original offsets.
// Fields are named by their effect where it is established, otherwise by offset.
#pragma once
#include "entity.h"
#include "cavern_launch.h"
#include "neon/math.h"
#include "neon/input.h"
#include <cstdint>
#include <functional>

namespace minos {

struct Ship;

// goMsg (0x23b140): the HUD message texts newLevel, cavernGoal, ship, motherSheep and oneUp use.
extern const char *const kGoMsg[9];

// theGame as the ship family sees it. Lists are entity list heads (push-front, next +0x15c).
struct PlayState {
    PlayState() {
        stmPalette[0].unpackRGBA(0x0080ffffu);
    } // stm::stm 0x5637c: palette [0]
    virtual ~PlayState() = default;
    // ---- services ----
    virtual void playSample(int id, int sampleVolume) {
        (void)id;
        (void)sampleVolume;
    } // AudioSystemPlaySample(id, vol, 0)
    virtual void dispose(Entity *e) {
        delete e;
    } // operator delete
    neon::DarwinRandom *rand = nullptr;     // libc rand() stream (oxRNG::rand uses it too)
    cavern::StripMap *map = nullptr;        // stm at +0xa18
    cavern::FlashChannels *flash = nullptr; // +0x207908 lit / +0x20790c watched / +0x207910 seen
    const cavern::CavernDefinition *cavern = nullptr; // caverns[+0x450]
    int32_t cavernIndex = 0;                          // +0x450

    // ---- input (controlBlock touch block and prefs) ----
    bool touchDown[4] = {};    // +0x16c..+0x16f touch 0..3 down
    bool touchPressed[4] = {}; // +0xf8..+0xfb held joystick bindings (not one-frame edges)
    int32_t touchTimer[4] =
        {};                 // +0x15c..+0x168 frames held (joyWatchdog; the touch layer zeroes them)
    bool touchFlag[8] = {}; // +0x170 + i (read for the rotate slot)
    double touchStartX[8] = {};     // +0x10c + 16 i
    double touchDeltaX[8] = {};     // +0x1b4 + 16 i
    double touchNowX[8] = {};       // +0x204 + 16 i
    double touchPos[4][2] = {};     // +0x244 + 16 i: (x, y) of touch i
    double touchStartY[8] = {};     // +0x114 + 16 i
    double controllerInfluence = 0; // direct world drift, separate from physical tilt
    double lateralTilt = 0;         // +0x38 smoothed accelerometer X, in g
    double analog = 0;              // +0x284 hardware-button rotation ramp
    uint32_t prefsButtons = 0;      // prefs +0xc84 (& 0xff0: thrust)
    uint32_t prefsControl = 0;      // prefs +0xc88 (bit 0: iPad side flip; 0x16 tilt steering mode)
    int touchLayout = 0;            // phone, tablet, mirrored tablet
    bool touchGameplayEnabled = true, tiltEnabled = true;
    bool touchAvailable = false, tiltAvailable = false, controllerAvailable = false;
    bool physicalTiltEnabled() const {
        return tiltEnabled || (updatedMenus && !controllerAvailable);
    }
    bool iPad = false; // +0x441

    // ---- state ----
    bool paused = false;       // +0x50
    bool updatedMenus = false; // project UX conventions; original-layout replay remains selectable
    bool exitToLauncherRequested = false; // host action, separate from original platform events
    int32_t frameCount = 0;               // +0x2a0
    bool muted = false;                   // +0x2ad
    int32_t volume = 8;                   // +0x454
    Vec2 bounceNormal;                    // +0x2b0 (ship bounce: spark velocity = -this)
    Entity *shots = nullptr;              // +0x2d0
    Entity *shipList = nullptr;           // +0x2d4
    Entity *debris = nullptr;             // +0x2e0 sensors, guns, particles (run by theGame::run)
    Entity *touchList = nullptr;          // +0x2e4 controlBlock::touch(ship, ...)
    Entity *bounceList = nullptr;         // +0x2e8 controlBlock::bounceOff(ship, ...)
    Entity *list2ec = nullptr;            // +0x2ec deleted when the ship dies
    bool reset310 = false;                // +0x310
    float fuel = 1.0f;                    // +0x314
    float shield = 1.0f;   // +0x318 (drained by impacts and by an empty tank; 0 kills)
    bool cleanRun = false; // +0x31c newLevel true; impacts, poor landings and death clear it
    bool respawnedThisCavern =
        false;               // +0x31d newLevel false; respawn true; disables the fuel bonus
    bool byte31e = false;    // +0x31e (gun fire gate, see Gun::run)
    double cameraY = 0;      // +0x328 (camera x +0x320 is not written here)
    double cameraStep = 0;   // +0x338
    Vec2 scrollDelta;        // +0x340 (+0x348 = camera step / 25)
    double scrollLimit = 0;  // +0x36c sectorLength * sectors - 24 (newLevel)
    double screenY = 0;      // +0x37c ship screen y (the camera keeps the ship there)
    double scrollSpeed = 0;  // +0x38c (-0.25 climbing, 0.15 falling)
    bool flag203808 = false; // +0x203808 (gun fire gate)
    bool byte442 = false;    // +0x442
    bool byte443 = false,
         pauseGlowRequested = false;  // +0x443, +0x444 (newLevel 1: the pause button glows)
    bool pauseRequested = false;      // +0x52 (set: the pause button presses itself)
    bool gameOver = false;            // +0x445
    int32_t perfectLandingStreak = 0; // +0x458 reset on imperfect landing, contact and newLevel
    int32_t word478 = 0;              // +0x478
    int32_t lives = 3;                // +0x4b4
    int32_t shipKind = 0;             // +0x9e4
    int32_t clearAll = 0;             // +0x9fc
    bool landed = false;              // +0xa0c
    int32_t padCell[2] = {0, 0};      // +0xa10 / +0xa14 cell of the pad landed on
    float shotDrift = 0; // +0xa08 (added to shot x per frame * 0.03; run: dead-zoned +0x38)
    uint32_t rocketFlags =
        0; // +0x203804 bit 0: rockets ignite near the ship; bit 1: burst into bullets
    int32_t scoreTotal = 0;    // +0x4a4 (theGame::_score)
    int32_t launcherTimer = 0; // +0x9e0 doLauncher countdown
    int32_t sector = 0;        // +0x207914 _bgdraw: max(0, int(camera.y) / sectorLength)
    // HUD message slots: count +0x3a0 + 4k, cellGrid row +0x3c8 + 4k, colour index +0x3f0 + 4k
    // (colours[]), text +0x418 + 4k. theGame::run prints each slot centred while its count runs
    // down (game_states.cpp). Written by newLevel, cavernGoal, oneUp, ship, wavingMinotaur,
    // motherSheep, awardLanding and the goal block.
    struct MessageSlot {
        int32_t count = 0, row = 0, colour = 0;
        const char *text = nullptr;
    };
    MessageSlot slots[10];
    void setMessage(int k, int32_t count, int32_t row, int32_t colour, const char *text) {
        slots[k].count = count;
        slots[k].row = row;
        slots[k].colour = colour;
        slots[k].text = text;
    }
    void awardLanding(); // theGame::awardLanding 0x74798 (game_states.cpp)
    // goals (achievements): +0x45c bits, +0x54 = goal state, +0x488 < 2 required
    uint32_t goalBits = 0; // +0x45c
    int8_t goalState = 0;  // +0x54 platform event byte (goals 3..13, 2 high score, 0xe..0x11 menus;
                           // < 0: purge touches)
    int32_t awardEligibilityGate =
        0;                       // +0x488 values below 2 permit records/goals; owner initializes 1
    int32_t minosSaved = 0;      // +0x48c (goal 6 at > 24)
    int32_t minosSavedTotal = 0; // +0x490 (goals 7 / 8 at > 100 / > 200)
    bool byte398 = false;        // +0x398 set with a minotaur voice sample
    void achieve(uint32_t bit, uint8_t state, const char *name); // the goal block's writes
    // theGame state motherSheep reads/writes (the core mirrors +0x4 and +0x470 each frame)
    int32_t drawMode = 2;          // +0x4 (motherSheep does nothing while it is 5)
    int32_t runState = 4;          // +0x470 run() state (5: motherSheep dies)
    float bgBright = 0;            // +0x18 background brightness (motherSheep raises it)
    float bgColour[3] = {0, 0, 0}; // +0xc/+0x10/+0x14 = +0x18 * colours[4] rgb
    float bgFade[2] = {0, 0};      // +0x20/+0x24 (motherSheep: 0.99)
    int32_t approachTimer =
        0; // +0x20380c (motherSheep::hitBy sets 1; _bgdraw draws the map while < 200)
    bool buttonLatch = false; // +0x446 set on a press, cleared when nothing is held (theGame::run);
                              // motherSheep::hitBy sets it
    bool bossPresent =
        false; // +0x2078b4 (motherSheep::run sets it every frame; launchBGEs clears it)
    // sprite::getSize(frame) -> {w, h} (resource catalog); getScale(i) = {1, h / w} of frame i + 2.
    std::function<Vec2(int frame)> spriteSize;
    Vec2 getScale(int spriteIndex) const {
        const Vec2 s = spriteSize ? spriteSize(spriteIndex + 2) : Vec2{1.0, 1.0};
        return {1.0, s.y / s.x};
    }
    // klaxons (+0x207840..)
    bool fuelWarn = false, fuelCritical = false;
    int32_t fuelTimer = 0; // +0x207840/1/4
    bool shieldWarn = false;
    int32_t shieldTimer = 0; // +0x207848/c
    // landing / respawn record (+0x207868..)
    int32_t landingBonus = 0;   // +0x207868
    int32_t rescueBonus = 0;    // +0x20786c rescued-minotaur landing rewards; cleared on respawn
    bool bonusPending = false;  // +0x207870
    Vec2 bonusPos;              // +0x207874
    Vec2 respawnPos;            // +0x207884 map position (cells, lines)
    float respawnRot = 0;       // +0x207894
    bool respawnLanded = false; // +0x207898
    // theGame::run's colour table (colour, 0x50 bytes each), refreshed every frame
    // (game_states.cpp): [0] +0x4d8 a8cols cycle, [1..10] +0x528.. getDynamicColour(8, 5, 0, 4, 2,
    // 3, 1, 6, 7, 9).
    neon::colour colours[11];

    // ---- theGame methods ----
    void addEntity(Entity *&list, Entity *e);     // controlBlock::addEntity (push-front)
    void spark(int n, Vec2 pos, Vec2 vel);        // theGame::spark 0x5f5cc
    void checkKlaxons();                          // theGame::checkKlaxons 0x5e05c
    void purgeTouches();                          // controlBlock::purgeTouches
    void runDebris();                             // theGame::run: the +0x2e0 loop
    void runShots();                              // runUnpausedEntities: the +0x2d0 list
    void score(int points, Vec2 pos, Vec2 scale); // theGame::_score 0x5a318
    // theGame::launch's entity creation from a factory request (cavern_launch.h): the class,
    // its init, the post-init writes, into list +0x2e4 / +0x2e8.
    void launch(const cavern::SpawnRequest &r);
    void doLauncher(int arg); // theGame::doLauncher 0x5d138 (scRocket every 10 frames on 0x100)
    void deleteList(Entity *&list);
    void addMapExplosion(
        const ExplosionBirth &birth); // shared centity/Enemy construction -> actual explo
    double rnd(double lo, double hi) {
        return rand->range(lo, hi);
    } // oxRNG::rand(lo, hi)
    uint32_t libcRand() {
        return rand->next();
    }
    void sample(int id) {
        if (!muted)
            playSample(id, volume << 12);
    }
    // controlBlock::collide 0xedc8 (touch: no bounce, bounceOff: bounce). Records the last
    // hit's target position (+0x2c0) and unit normal (+0x2b0).
    int collide(Entity &a, Entity *list, bool bounce);
    int touch(Entity &a, Entity *list) {
        return collide(a, list, false);
    }
    int bounceOff(Entity &a, Entity *list) {
        return collide(a, list, true);
    }
    Vec2 contactPos; // +0x2c0

    // ---- theGame run/newLevel/bonus state (game_states.h / game_states.cpp) ----
    struct Hud *hud = nullptr;            // +0x2a4 cellGrid (HUD text); null: no HUD output
    bool hudReady = false;                // +0x440
    bool running = true;                  // +0x2ac (run stops after its state switch while clear)
    int32_t bgLayer = 2;                  // +0x8 _bgdraw's mode (+0x4 is drawMode)
    int32_t selCavern[4] = {};            // +0x460 + 4 k selected start cavern per ship kind
    char textBuf[64] = {};                // txtbuf
    std::function<Entity *()> createShip; // newLevel: the ship for +0x9e4, added to +0x2d4
    void initStars(float w, float h, uint32_t rgba, float size, float scroll); // 0x55d28 (taurus)
    int32_t prefs[0x320] =
        {}; // prefs (+0x34) words: per ship kind k, [100k] high score,
            // [100k+1] furthest cavern, [100k+0x10+c] best score, [100k+0x30+c] best lives
    bool inputFlipped = false;                        // +0x6c
    bool byte6d = false, byte6e = false;              // +0x6d, +0x6e
    float phase = 0;                                  // +0xa00 0..1 in steps of 0.02
    float phaseSin = 0;                               // +0xa04 sinf(phase * 2pi)
    bool firePressed = false;                         // +0x207864 a press this frame
    bool fireHeld = false;                            // +0x207865 held since the press
    int32_t keyRepeat = 0, keyCode = 0, keyCount = 0; // +0x9e8, +0x9ec, +0x9f0 (menu key repeat)
    neon::input::MenuActions menuActions;    // native host actions; never writes source touch slots
    bool gameFinished = false;               // +0x9f4
    int32_t endScreenCountdown = 0;          // +0x9f8 (goGameOver: 1000)
    int32_t shake = 0;                       // +0x4c4 HUD shake frames
    int32_t word39c = 0;                     // +0x39c
    bool tutorial = false;                   // +0x47d osj hints showing (the HUD score lines wait)
    bool tutorialShown = false;              // +0x449
    int32_t startCavern = 0;                 // +0x44c (attract)
    bool byte447 = false;                    // +0x447
    int32_t reportScore = 0, reportKind = 0; // +0x5c, +0x60 (new high score report)
    int32_t missionTime = 0;                 // +0x207850 frames in the cavern
    int32_t parTime = 0, parTimeBase = 0;    // +0x207854, +0x207858
    float rating = 0, ratingShown = 0;       // +0x20785c, +0x207860
    float bgeRate = 0.3f;                    // +0x20791c (newLevel 0.3)
    // sequenceBonuses (+0x20792c..)
    int32_t scoreBase = 0;                          // +0x20792c
    float bonusT = 0;                               // +0x207930
    int32_t bonusTotal = 0;                         // +0x207934
    int32_t bonusPhase = 0, bonusTimer = 0;         // +0x207948, +0x20794c
    int32_t fuelBonus = 0, timeBonus = 0;           // +0x207954, +0x207958
    float bonusX = 0, bonusStep = 0, fuelShown = 0; // +0x207960, +0x207964, +0x207968
    // flash channel timers per channel (cavern words +0x244 + 12 k: on, count, off)
    int32_t flashOn[4] = {}, flashCountReload[4] = {},
            flashOff[4] = {};                       // +0x2078b8, +0x2078c8, +0x2078d8
    int32_t flashTimer[4] = {}, flashCount[4] = {}; // +0x2078e8, +0x2078f8
    // background effects (BGESetup/BGEIntervalSetup/launchBGEs)
    int32_t bgeKind = 0;      // +0x20381c
    int32_t bgeCurrent = 0;   // +0x207918 BGESetup's last argument (theGame starts zeroed)
    int32_t bgeIntervals = 0; // +0x2078b0
    int32_t bgePeriod = 0, bgeWaves = 0; // +0x203810, +0x203814
    int32_t bgeGapReload = 0; // +0x203818 frames between waves (250 / 450 / 150; written as bytes)
    int32_t bgeTimer = 0,
            bgeLeft = 0;    // +0x207938 frames to the next spawn, +0x20793c spawns left in the wave
    int32_t bgeGap = 0;     // +0x207940 frames to the next wave
    Vec2 bgeOrigin;         // +0x20789c
    uint32_t bgeColour = 0; // +0x2078ac
    float bgeDx = 0;        // +0x203830
    float bgeAngle = 0, bgeSpin = 0, bgeSpeed = 0; // +0x203834, +0x203838, +0x20383c
    int32_t bgeSide = 0;                           // +0x203820
    // Host callbacks for game transitions and pause presentation
    std::function<void()> goGameOverHook; // after goGameOver's state writes (QuickMsg)
    std::function<void()> newLevelHook;   // theGame::newLevel (state 6 -> 4)
    std::function<void()> pausePressed;   // pauseButton::onPressed (pause menu)
    void refreshPauseOptions();
    void openPause();             // pauseButton::onPressed 0x59e3c
    void runFrame();              // theGame::run 0x6c020 (play states; game_states.cpp)
    void runUnpausedEntities();   // 0x54310
    void joyWatchdog();           // controlBlock::joyWatchdog
    void bgeSetup(int kind);      // theGame::BGESetup 0x54e7c
    void bgeIntervalSetup();      // theGame::BGEIntervalSetup 0x54bbc
    void sequenceBonuses();       // theGame::sequenceBonuses 0x5d380
    void goGameOver();            // theGame::goGameOver 0x602dc
    void newLevelState();         // theGame::newLevel 0x636f0 after the map part (game_states.cpp)
    void clearMessages(int upTo); // cll + clear the slots' texts 0..upTo-1
    void announceCavern();        // theGame::announceCavern 0x60740 (state 6)
    void
    resetLists(int keep); // theGame::reset 0x5654c: delete each list unless its keep bit is set
    // ---- menus: title 1-3, select 7, game over / finished 8, info 9 (menu_states.cpp) ----
    bool byte51 = false;          // +0x51
    float fbRot = 0;              // +0x1c (title: 0)
    float fbPos[2] = {0, 0};      // +0x28 / +0x2c (title: 0)
    bool byte70 = false;          // +0x70 (a game started from the select screen)
    int32_t word298 = 0;          // +0x298
    int32_t tipIndex = -1;        // +0x394 the game-over TOP TIPS entry (constructor -1)
    bool startupReported = false; // +0x448 title seen once (goal state 0xf)
    bool byte47c = false;         // +0x47c
    int32_t maxCavern = 0;        // +0x480 last selectable cavern (furthest + 3, at most 20)
    int32_t word484 = 0;          // +0x484
    int32_t word9d8 = 0;          // +0x9d8 (title: 6000)
    bool restartBest = false; // +0x207842 BEST GAME: start with the cavern's best lives and score
    int32_t infoPage = 0;     // +0x20795c
    neon::colour
        stmPalette[16]; // stm+4 (newLevel: the cavern's; the scenery writes [0], _bgdraw [1] / [2])
    // Test intercept for run's states 1-3 and 7-9 (the play witness intercepts state 8): true
    // skips the original handler.
    std::function<bool()> stateHook;
    void menuState(); // run's cases 1, 2, 3, 7, 8, 9
    // _bgdraw's state writes (mode 1: stm palette [1]; mode 2: +0x31e, the visible lines and
    // sector for the camera, stm palette [1] / [2]); the core draws after it.
    void bgdrawState(Vec2 camera);
    void titleScenery();        // announceCavern's / state 1's stm layout
    void addTitleStars();       // 50 titleStars into +0x2dc (after reset(0))
    void goSelect();            // 0x5e548
    void updateSelect();        // 0x579b4
    void dynamicSelect();       // 0x57b34
    void startFromSelect();     // run case 7, keys 3 / 9 (NEW GAME / BEST GAME)
    void starReset();           // 0x5e818
    void dynamicInfo();         // 0x58268
    void dynamicGameOver();     // 0x58f3c
    void dynamicGameFinished(); // 0x6bac8
    void fireWorkExplosion();   // 0x6b648
    Entity *list2d8 = nullptr;  // +0x2d8 [2]
    Entity *list2dc = nullptr;  // +0x2dc [4] stars (_bgdraw draws them)
    Entity *list2f0 = nullptr;  // +0x2f0 [0x100]
};

// Common: every class here keeps theGame at +0x168.
struct PlayEntity : Entity {
    PlayState *g = nullptr;
};

struct Sensor : PlayEntity { // vtable 0x1ed13c, 0x1bc bytes
    void run() override;
    int draw(EntityRenderer &, Vec2) override {
        return 0;
    }
    bool touching = false;    // +0x16c
    bool hazard = false;      // +0x16d tile code negative (sign bit)
    bool rated = false;       // +0x16e counts for landing (the two feet)
    Ship *ship = nullptr;     // +0x170
    float sparkDecay = 1.0f;  // +0x174
    float weight = 0.2f;      // +0x178 impact weight (0.2 feet, 3.0 sides/top)
    Vec2 offset;              // +0x17c offset from the ship centre (unrotated)
    Vec2 rotated;             // +0x18c
    Vec2 normal;              // +0x19c contact normal (unrotated)
    int32_t cell[2] = {0, 0}; // +0x1ac / +0x1b0 column, line of the visible tile
    bool shipLanded = false;  // +0x1b4 copied from the ship each frame
    int32_t impact = 0;       // +0x1b8
};

struct Gun : PlayEntity { // gnu, vtable 0x1ed05c, 0x1c4 bytes
    void run() override;
    int draw(EntityRenderer &r, Vec2 screen) override;
    void fire();
    Ship *ship = nullptr;   // +0x16c
    bool disabled = false;  // +0x170 (1 with the gates closed: no fire)
    Vec2 offset;            // +0x174
    Vec2 rotated;           // +0x184
    float spread = 0;       // +0x194 (fire: x speed factor) ; constructor 0
    float jitter = 0;       // +0x198 rand(-j, j) added to x speed
    float dirY = 1.0f;      // +0x19c y direction (-1 for the downward gun)
    float speed = 0.01f;    // +0x1a0
    int32_t word1a4 = 0;    // +0x1a4 (< 0 opens the fire gate)
    int32_t burst = 7;      // +0x1a8
    int32_t interval = 4;   // +0x1ac
    int32_t reload = 8;     // +0x1b0
    int32_t left = 7;       // +0x1b4 shots left in the burst
    int32_t wait = 0;       // +0x1b8
    int32_t reloadLeft = 0; // +0x1bc
    float flash = 0;        // +0x1c0 muzzle flash
};

struct Shot : PlayEntity { // vtable 0x1ed03c, 0x170 bytes
    void run() override;
    int32_t grow = 1; // +0x16c
};

struct ThrustParticle : PlayEntity { // vtable 0x1ed15c, 0x18c bytes
    void run() override;
    void init(PlayState &game); // 0x56cd8
    bool collides = true;       // +0x16c
    float spin = 0;             // +0x170
    float growth = 1.0f;        // +0x174 scale factor per frame while < 4
    float life = 1.0f;          // +0x178
    float fade = 0;             // +0x17c
    float fall = 0;             // +0x180 added to vy
    int64_t lastTile = 0x3039;  // +0x184 tile word last bounced off (12345 initially)
    bool hot = false;           // +0x188 coloured by the thrust colour after a bounce
};

struct Bspark : PlayEntity { // vtable 0x1ecffc, 0x180 bytes
    void run() override;
    float age = 0;           // +0x16c
    float ageRate = 0;       // +0x170
    float exploChance = 0;   // +0x174
    int32_t colourIndex = 0; // +0x178
    float maxScale = 0;      // +0x17c
};

struct Explo : PlayEntity { // explo, vtable 0x1ed01c, 0x17c bytes
    void run() override;
    float from = 0; // +0x16c start scale
    float to = 0;   // +0x170 end scale
    float t = 0;    // +0x174
    float rate = 0; // +0x178
};

struct Ship : PlayEntity { // ship : shentity, vtable ship, size per newLevel
    virtual void
    init(PlayState &game); // ship::init 0x62b70; other ship classes have distinct init/run
    void run() override;   // ship::run 0x68118
    int draw(EntityRenderer &r, Vec2 screen) override; // shentity::draw 0x55194
    void doControls();                                 // shentity::doControls 0x65134
    int rateLanding();                                 // shentity::rateLanding 0x5e374

    float f164 = 0.006f;           // +0x164
    Vec2 shake;                    // +0x168 draw offset while the shield is low
    float fuelPerThrust = 0.0004f; // +0x178
    float damage = 0.0003125f;     // +0x17c shield lost per impact unit
    float hitDamage = 0;           // +0x180 damRate[kind] * 0.0003125
    float thrust = 0;              // +0x184
    float thrustTarget = 0;   // +0x188 primary run assigns before reading; init leaves it unwritten
    bool launchHold = false;  // +0x18c
    int32_t rotateHeld = 0;   // +0x190
    int32_t thrustHeld = 0;   // +0x194
    int32_t rotateSlot = -1;  // +0x198 touch slot driving rotation (3: analog)
    int32_t thrustSlot = -1;  // +0x19c
    int32_t touches = 0;      // +0x1a0
    int32_t secondTouch = -1; // +0x1a4
    int32_t age = 0;          // +0x1a8 frames
    int32_t sinceContact = 100;       // +0x1ac
    bool analogDrive = false;         // +0x1b4
    bool dying = false;               // +0x1b5
    bool touchedDown = false;         // +0x1b6 landed this frame
    int32_t impact = 0;               // +0x1b8 sensor sum; run clears before collecting contacts
    int32_t airborne = 0;             // +0x1bc
    Vec2 lastLanding{-599.0, -599.0}; // +0x1c0
    bool isLanded = false;            // +0x1d0
    Vec2 mapPos{9.5, 10.0};           // +0x1d4 / +0x1dc (cells, lines)
    float idleFuel = 0.0001f;         // +0x1e4
    int32_t deathTimer = 0;           // +0x1e8
    int32_t landingTimer = 0;         // +0x1ec
    Entity *children[16] = {};        // +0x1f0..+0x22c (sensors in 0..4)
    float gunFacing = 1.0f;           // +0x230 mirrors weapon mount X when the taxi turns
    Vec2 prevMapPos;                  // +0x234 / +0x23c
    float gravity = 0.003f;           // +0x244
    Sensor *sensor(int i) const {
        return static_cast<Sensor *>(children[i]);
    }

  protected:
    // Repeated source initialization/child construction, not a shared flight model.
    void initCommon(PlayState &game);
    void addSensor(int i, Vec2 normal, float weight, Vec2 offset);
    Gun *addGun(Vec2 offset, float aimX, float aimY);
};

struct VicLander final : Ship {
    void init(PlayState &game) override; // 0x61614
    void run() override;                 // 0x65f08
};

struct Taxi final : Ship {
    void init(PlayState &game) override; // 0x62184
    void run() override;                 // 0x66fa8
};

struct AsteroidsSpaceship final : Ship {
    void init(PlayState &game) override; // 0x60cf8
    void run() override;                 // 0x75b64
};

} // namespace minos
