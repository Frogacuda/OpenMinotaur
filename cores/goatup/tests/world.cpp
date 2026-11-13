#include "platform.h"
#include "climber.h"
#include "generation.h"
#include "effects.h"
#include "denizens.h"
#include "kids.h"
#include "world.h"
#include "original_tables.h"
#include "neon/source_random.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool pass, const char *why) {
    if (!pass)
        throw std::runtime_error(why);
}
void layouts() {
    goatup::Platform p;
    p.level = 1;
    p.earlyLayout(false);
    for (int tile : p.tiles)
        check(tile == 4, "First source platform is unbroken grass");
    p.level = 2;
    p.earlyLayout(false);
    for (int n = 0; n < 40; ++n)
        check(p.tiles[size_t(n)] == ((n >= 5 && n < 10) || (n >= 25 && n < 30) ? 0 : 4),
              "Two source gaps on platform 2");
    p.level = 2;
    p.earlyLayout(true);
    for (int n = 0; n < 40; ++n)
        check(p.tiles[size_t(n)] == ((n >= 5 && n < 12) || (n >= 25 && n < 32) ? 0 : 4),
              "Returning route widens gaps");
    for (int level = 1; level < 64; ++level)
        for (bool returning : {false, true}) {
            p.level = level;
            p.earlyLayout(returning);
            for (int n = 0; n < 20; ++n)
                check(p.spawnable[size_t(n)] ==
                          (p.tiles[size_t(n * 2)] == 4 && p.tiles[size_t(n * 2 + 1)] == 4),
                      "Birth eligibility uses adjacent tile pairs");
        }
    int draws = 0;
    p.spawnable.fill(false);
    auto random = [&](double, double) {
        ++draws;
        return 1.;
    };
    check(p.selectBirthSlot(random) == -1 && draws == 0,
          "No available site consumes no random value");
    p.spawnable[9] = true;
    check(p.selectBirthSlot(random) == 18 && draws == 1, "Source slot is the even tile index");
}
void poolAndProjection() {
    goatup::TowerSurface tower;
    int births = 0;
    tower.generate = [&](auto &p) {
        ++births;
        p.earlyLayout(false);
    };
    tower.prepare();
    check(births > 0 && tower.find(1), "Initial original view activates the starting platform");
    auto *first = tower.find(1);
    const int priorBirths = births;
    tower.prepare();
    check(births == priorBirths && tower.find(1) == first,
          "Retained platform is not generated again");
    check(std::fabs(first->y - .4f) < .00001,
          "Initial platform y follows negative truncation and source row walk");
    const auto count = tower.quads.size();
    check(count > 100, "Cylinder and platform faces are prepared");
    check(tower.quads.front().frame == 5 && tower.quads.front().corners[0].y == .5,
          "Initial tower texture phase matches the original draw");
    tower.scroll = .1f;
    tower.prepare();
    check(tower.quads.front().corners[0].y == double(.65f),
          "Tower texture advances at the original rate");
    tower.scroll = -.2f;
    tower.prepare();
    for (const auto &q : tower.quads)
        for (auto v : q.corners)
            check(std::isfinite(v.x) && std::isfinite(v.y), "Finite projection");
    // Moving to a new visible range retains old slots for 320 preparation ticks.
    tower.scroll = 4.f;
    tower.prepare();
    check(first->active && first->offscreenTicks == 1, "First offscreen tick retains old platform");
    for (int n = 0; n < 319; ++n)
        tower.prepare();
    check(first->active && first->offscreenTicks == 320, "Native expiry threshold is strict");
    tower.prepare();
    check(!first->active && first->y == -.49f, "321st offscreen tick retires slot");
    tower.scroll = -.2f;
    tower.prepare();
    check(tower.find(1) && births > priorBirths, "Retired source content can be generated again");
    goatup::TowerSurface negative;
    negative.scroll = -.21f;
    negative.generate = [](auto &p) { p.earlyLayout(false); };
    negative.prepare();
    check(negative.find(1) && std::fabs(negative.find(1)->y - .395f) < .00001,
          "Negative fractional scroll uses truncation, not floor");
}
void climbing() {
    goatup::TowerSurface tower;
    auto &platform = tower.platforms[0];
    platform.level = 1;
    platform.earlyLayout(false);
    platform.active = true;
    platform.y = .4f;
    goatup::ClimbContext context{tower};
    int treads = 0, landings = 0, contacts = 0;
    context.sound = [](int) {};
    context.stopSound = [](int) {};
    context.trailStar = [] {};
    context.smash = [](auto &, int) {};
    context.tread = [&](auto &, int) { ++treads; };
    context.contacts = [&] { ++contacts; };
    context.splash = [&](int count, float, float, auto, uint32_t, uint32_t) {
        check(count == 15, "Landing splash birth count");
        ++landings;
    };
    goatup::Climber goat;
    goat.step(context, {});
    check(goat.velocity.y == double(.001f) && goat.position.y == .35 + double(.001f),
          "Native single-precision gravity widens before integration");
    for (int n = 0; n < 20 && goat.platform < 0; ++n)
        goat.step(context, {});
    check(goat.platform == 1 && goat.velocity.y == 0 && landings == 1 && treads == 1,
          "First landing and immediate tread are source ordered");
    check(goat.position.y == double(float(.4f + float(.03f * -.5f))),
          "Standing foot offset is rounded to float");
    goat.pregnancy = 1.5f;
    goat.step(context, {goatup::ClimbInput::Method::None, 0, true});
    check(goat.platform == -1 && goat.velocity.y == -.005 && goat.jumpTicks == 24,
          "Jump duration truncates pregnancy before subtraction");
    const double before = goat.position.y;
    goat.step(context, {goatup::ClimbInput::Method::None, 0, true});
    check(goat.velocity.y == -.005 - .0014 + double(.001f) && goat.position.y < before,
          "Held jump extends ascent");
    check(goat.jumpTicks == 23, "Held jump duration advances once");
    goat.step(context, {});
    check(goat.jumpTicks == 0, "Releasing jump ends extra lift");
    goatup::Climber axis;
    axis.position.y = 0;
    axis.step(context, {goatup::ClimbInput::Method::DirectAxis, 1, false});
    check(axis.heading > 0 && axis.velocity.x > 0 && !axis.touchRebaseRequested,
          "Direct native control has its own source response");
    // Source scale is signed: rightward intent mirrors the goat atlas frame.
    axis.prepareDraw();
    check(axis.position.x > 0 && axis.projectedScale.x < 0,
          "Cylindrical player projection and source facing");
    goatup::Climber tilt;
    tilt.position.y = 0;
    tilt.step(context, {goatup::ClimbInput::Method::Accelerometer, .2, false});
    const double intent = double(float(double(float(.2 * .5)) * .19));
    check(tilt.heading == float(intent * -.1) && tilt.velocity.x == intent * -.1 * .85,
          "Tilt preserves the original half/sample drive and sign");
    const auto facing = tilt.lastDrive;
    tilt.step(context, {goatup::ClimbInput::Method::Accelerometer, .01, false});
    check(tilt.lastDrive == facing,
          "Tilt dead band coasts without replacing the source facing intent");
    goatup::Climber ceiling;
    ceiling.position.y = -.479;
    ceiling.velocity.y = -.01;
    ceiling.step(context, {});
    check(ceiling.position.y == -.48 && ceiling.velocity.y == 0, "Upward ceiling clamp");
    goatup::Climber wall;
    wall.position.y = 0;
    wall.heading = .4999f;
    wall.velocity.x = .009;
    wall.step(context, {goatup::ClimbInput::Method::RelativeTouch, 0, false});
    check(wall.heading == .4999f && wall.velocity.x < 0 && wall.touchRebaseRequested,
          "Wall bounce rebases real touch origin");
    goatup::Climber death;
    death.position.y = .56;
    death.step(context, {});
    check(context.gameOver && context.ticks == 0, "Fall-out enters source terminal state");
    check(contacts > 0, "Contact service called by climb integration");
}
void generation() {
    neon::DarwinRandom random(1);
    goatup::GenerationState state;
    goatup::PlatformGenerator generator(random, state);
    int zones = 0;
    std::vector<goatup::PlatformBirth> births;
    generator.create = [&](const auto &b) { births.push_back(b); };
    generator.enterZone = [&] { ++zones; };
    goatup::Platform p;
    p.level = 1;
    generator.generate(p);
    check(zones == 1 && births.size() == 3 && random.draws() == 3,
          "Starting grass platform creates three bonuses with one shared draw apiece");
    // Source-derived values: Park–Miller seed 1 => 16807,282475249,1622650073;
    // truncating [1,20] ordinal chooses the first, third and fifteenth pairs.
    check(births[0].angle == -.5f && births[1].angle == float(4.f / 40.f - .5f) &&
              births[2].angle == float(28.f / 40.f - .5f),
          "Original seed-one birth sequence");
    check(state.targetClimbSpeed == float(double(.003f) * 1.015),
          "Native per-platform speed target growth");
    births.clear();
    p.level = 8;
    generator.generate(p);
    check(births.size() == 4 && births[0].kind == goatup::PlatformBirth::Kind::BillyGoat,
          "Every eighth grass platform creates Billy before bonuses");
    check(zones == 1, "Retained zone does not replay zone notification");
    births.clear();
    p.level = 53;
    generator.generate(p);
    check(births.size() == 4 && births[0].kind == goatup::PlatformBirth::Kind::Sheep,
          "First-lap late-grass sheep birth rule");
    check(births[0].horizontalSpeed > .0015 && births[0].horizontalSpeed < .002,
          "Source sheep speed range");
    state.pregnant = true;
    births.clear();
    p.level = 16;
    generator.generate(p);
    check(births.size() == 3, "Pregnancy suppresses another Billy but preserves bonuses");
    state.returning = true;
    births.clear();
    p.level = 53;
    generator.generate(p);
    check(births.size() == 3, "Return journey suppresses grass sheeps");
}
void effects() {
    neon::DarwinRandom random(1);
    goatup::Effects effects(random);
    effects.initializeStars();
    check(effects.stars() == 50 && random.draws() == 450,
          "Original newLevel initializes fifty stars with nine draws each");
    std::vector<goatup::ActorDraw> draws;
    effects.drawStars(draws);
    check(draws.size() == 50 && random.draws() == 450,
          "Preparing star sprites consumes no additional random values");
    effects.tickStars();
    check(effects.stars() == 50 && random.draws() == 450,
          "First star tick does not recycle initial positions");
    const auto before = random.draws();
    effects.splash(3, -.009f, 1.8f, {0, 0}, 0x00ff40ffu, 0x008000ffu);
    check(effects.particles() == 3 && random.draws() == before + 6,
          "Landing splash consumes speed and rotation draws at birth");
    for (int tick = 0; tick < 33; ++tick)
        effects.tickParticles();
    check(effects.particles() == 3, "Particle lifetime remains positive through tick 33");
    effects.tickParticles();
    check(effects.particles() == 0, "Particle source decay retires at tick 34");
    for (int n = 0; n < 100; ++n)
        effects.playerTrail({0, 0}, 0);
    check(effects.trails() > 0, "Conditional source player trail creates actors");
    for (int n = 0; n < 230; ++n)
        effects.tickUi(false, 0, {});
    check(effects.trails() == 0, "Source alpha threshold retires player trails");
    effects.reset();
    check(effects.stars() == 0 && effects.particles() == 0,
          "Source list release clears its actors");
    effects.loveHeart(0, .2, .002, .002, 0);
    effects.floatingScore(123, {0, .1}, 0, -.001f, .03f);
    neon::colour white;
    white.white(1);
    effects.tickUi(false, 0, white);
    draws.clear();
    effects.prepareUiDraw(draws);
    check(draws.size() == 4 && draws[0].frame == 33 && draws[2].frame == 35 && draws[3].frame == 7,
          "Score and heart retain shared newest-first UI drawing order");
    const auto at = draws[0].position;
    effects.tickUi(true, 0, white);
    draws.clear();
    effects.prepareUiDraw(draws);
    check(draws[0].position.x == at.x && draws[0].position.y == at.y,
          "Paused UI traversal preserves score motion");
}
void denizens() {
    neon::DarwinRandom random(1);
    goatup::TowerSurface tower;
    goatup::ClimbContext game(tower);
    goatup::Climber goat;
    goatup::Effects effects(random);
    goatup::CollectionRecord records;
    goatup::DenizenContext context{game, goat, random, effects, records};
    std::vector<int> awards;
    int popups = 0, sounds = 0;
    context.sound = [&](int, int) { ++sounds; };
    context.achievement = [&](int n) { awards.push_back(n); };
    context.bonusCaption = [](int) {};
    context.popup = [&](int, goatup::Vec2, double, double, float) { ++popups; };
    context.spriteSize = [](int) { return goatup::Vec2{64, 32}; };
    goatup::Denizens actors(context);
    goatup::Platform p;
    p.level = 8;
    p.y = .2f;
    p.earlyLayout(false);
    actors.create({goatup::PlatformBirth::Kind::BillyGoat, &p, 0, 6});
    bool touching = false;
    check(!actors.touch(actors.contacts()[0].position, .03f, 1, touching),
          "Original contact rejects the newborn actor's draw delay");
    std::vector<goatup::ActorDraw> draws;
    actors.prepareDraw(draws);
    actors.prepareDraw(draws);
    check(draws.empty(), "Native Billy birth suppresses two actual draws");
    actors.prepareDraw(draws);
    check(draws.size() == 1 && draws[0].scale.x == 1.7,
          "Billy uses original scale and atlas frame");
    records.billyGoats = 99;
    goat.kids = 2;
    const auto before = random.draws();
    check(actors.touch(actors.contacts()[0].position, .03f, 1, touching),
          "Prepared Billy enters the native overlap/contact path");
    check(game.pregnant && effects.hearts() == 20 && random.draws() == before + 60,
          "Billy pickup creates twenty hearts with three draws each");
    check(context.score == 300 && awards.size() == 1 && awards[0] == 5,
          "Billy score and original lifetime-goal boundary");
    actors.tick();
    check(actors.count() == 0,
          "Marked-dead Billy runs then retires in the original list traversal");
    // Its last run keeps its original timer >0, so it does not add a heart.
    effects.tickUi(false, tower.angle, {});
    check(effects.hearts() == 20, "UI newborn hearts run in the later list on the same frame");
    actors.reset();
    records.uniqueBonuses = 99;
    actors.create({goatup::PlatformBirth::Kind::Bonus, &p, 0, 90, 0});
    actors.hit(0, 1, goat.position);
    check(context.score == 300, "First bonus contact only bumps it");
    actors.hit(0, 1, goat.position);
    check(context.uniqueRunBonuses == 0, "Early repeated contact cannot collect a rising bonus");
    for (int n = 0; n < 12; ++n)
        actors.tick();
    actors.hit(0, 2, goat.position);
    check(context.uniqueRunBonuses == 1 && records.uniqueBonuses == 100 && context.score == 324,
          "Later kid contact collects with kids/platform score multiplier");
    check((context.achievements & (1u << 7)) && goat.pregnancy == .2f,
          "Collection preserves original award and pregnancy increment");
    const auto after = random.draws();
    actors.hit(0, 1, goat.position);
    check(random.draws() == after && context.score == 324,
          "Collected bonus cannot repeat its collection effects");
    actors.reset();
    actors.create({goatup::PlatformBirth::Kind::Sheep, &p, 0, 596, -1, .0017});
    const auto damage = random.draws();
    actors.hit(0, 1, goat.position);
    check(goat.rotationLock == 40 && goat.platform == -1 && context.pendingKidLoss == 2,
          "Sheep hit queues source kid loss and player lock");
    check(random.draws() == damage + 4 && effects.particles() == 0,
          "Player-hit sheep consumes two recoil draws and two sound draws");
    actors.reset();
    context.hazardGrace = 1;
    actors.create({goatup::PlatformBirth::Kind::Sheep, &p, 0, 596, -1, .0017});
    const auto bop = random.draws();
    actors.hit(0, 2, goat.position);
    check(context.score == 1324 && (context.achievements & (1u << 4)),
          "Kid sheep hit credits score and Bop the Ox goal");
    check(effects.particles() == 30 && random.draws() == bop + 62,
          "Bop particles precede the two hurt-noise draws");
    context.award(4);
    check(awards.size() == 3, "Run achievement mask suppresses repeated source notification");
    check(popups == 3 && sounds > 0,
          "Actor consequences reach synchronous original feedback sites");
}
void kids() {
    neon::DarwinRandom random(3);
    goatup::TowerSurface tower;
    goatup::ClimbContext game(tower);
    goatup::Climber goat;
    goatup::Effects effects(random);
    goatup::CollectionRecord records;
    goatup::DenizenContext context{game, goat, random, effects, records};
    goatup::KidState state;
    context.sound = [](int, int) {};
    context.achievement = [](int) {};
    context.popup = [&](int points, auto at, double vx, double vy, float radius) {
        effects.floatingScore(points, at, vx, vy, radius);
    };
    goatup::Kids kids(context, state);
    int touches = 0;
    kids.touch = [&](auto, float, int, bool &) { ++touches; };
    goat.historyCount = goat.historyCursor = 128;
    for (auto &h : goat.history) {
        h.position = {.1, .4};
        h.scale = {-1.5, 1};
        h.frame = 12;
        h.platform = 8;
        h.platformOffset = -.015f;
    }
    auto &p = tower.platforms[0];
    p.level = 8;
    p.y = .2f;
    p.active = true;
    p.earlyLayout(false);
    const auto start = random.draws();
    game.pregnant = true;
    goat.pregnancy = 3.99f;
    goatup::treadOnTile(p, 0, context, kids);
    check(kids.count() == 1 && goat.kids == 1 && !game.pregnant && goat.pregnancy == 0,
          "Fresh grass completes native pregnancy and births a live kid");
    check(p.tiles[0] == 2 && context.score == 1 && context.runBirths == 1,
          "Grass tread changes the native tile and credits once");
    check(random.draws() == start + 68 && effects.hearts() == 20,
          "Tread splash precedes newborn kid hearts in the shared stream");
    const auto after = random.draws();
    goatup::treadOnTile(p, 0, context, kids);
    check(random.draws() == after, "Grazed tile does not replay birth/tread consequences");
    std::vector<goatup::ActorDraw> images;
    kids.draw(images);
    check(images.size() == 1 && images[0].position.y == .4 + .02 && images[0].frame == 12,
          "Birth takes the delayed history pose before first kid update");
    kids.tick();
    images.clear();
    kids.draw(images);
    check(touches == 1 && images[0].frame == 1 && images[0].scale.x == -1.125,
          "Kid update strips pregnancy animation and applies original three-quarter scale");
    check(images[0].position.y == double(float(.2f - .015f)),
          "Kid follows retained platform offset with float addition");
    context.pendingKidLoss = 1;
    kids.tick();
    check(kids.count() == 0 && goat.kids == 0 && context.pendingKidLoss == 0,
          "Queued sheep loss retires the matching kid and clears the source mailbox");
    check(effects.particles() == 154 && state.statusKind == 4 && state.statusTicks == 60,
          "Kid loss preserves original particle and status feedback");
    kids.create();
    state.tally = true;
    state.releaseOrdinal = 1;
    state.tallyMultiplier = 1;
    kids.tick();
    for (int n = 0; n < 180 && kids.count(); ++n)
        kids.tick();
    check(kids.count() == 0 && state.tallyMultiplier == 2 && context.score == 10001,
          "Released kid finishes native ballistic tally and credits its multiplier");
    effects.reset();
    goat.kids = 9;
    state.tally = false;
    kids.create();
    check((context.achievements & 1) && records.mostKids == 10 && state.mostInRun == 10,
          "Original ten-kid achievement and lifetime maximum remain game-owned");
}
void openingLoop() {
    goatup::CollectionRecord records;
    goatup::World world(records, 1);
    world.actors.sound = [](int, int) {};
    world.game.stopSound = [](int) {};
    world.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
    std::vector<goatup::ActorDraw> draw;
    int highest = 0, simulated = 0;
    for (; simulated < 6000 && !world.game.gameOver; ++simulated) {
        // Synthetic controller route through real simulation, not a reference
        // witness: aim at the closest solid cell on the next visible platform.
        double target = world.goat.heading, best = 10;
        if (auto *next = world.tower.find(std::max(1, world.game.highestPlatform + 1))) {
            for (int tile = 0; tile < 40; ++tile)
                if (next->tiles[size_t(tile)] > 0) {
                    const double angle = (tile + .5) / 40. - .5,
                                 distance = std::abs(angle - world.goat.heading);
                    if (distance < best) {
                        best = distance;
                        target = angle;
                    }
                }
        }
        const double drive = std::clamp((target - world.goat.heading) * 15., -.65, .65);
        world.tick({goatup::ClimbInput::Method::DirectAxis, drive, true});
        draw.clear();
        world.prepareBackground(draw);
        draw.clear();
        world.prepareForeground(draw);
        highest = std::max(highest, world.game.highestPlatform);
    }
    check(highest >= 8, "Integrated held-jump route cannot reach the first Billy platforms");
    std::cout << "Opening controller route: " << simulated << " ticks, height " << highest
              << ", generated " << world.game.highestGenerated << '\n';
    world.game.highestGenerated = 127;
    world.game.gameOver = false;
    const auto cursor = world.goat.historyCursor;
    world.tick({});
    check(world.goat.historyCursor != cursor,
          "Full course continues beyond the former checkpoint boundary");
}
void donkeyZone() {
    goatup::Platform p;
    p.level = 64;
    p.earlyLayout(false);
    for (int n = 0; n < 40; ++n)
        check(p.tiles[size_t(n)] == ((n >= 27 && n <= 31) ? 0 : 14),
              "Second-zone first pattern reverses the original five-cell gap");
    for (int level = 64; level < 128; ++level) {
        p.level = level;
        p.earlyLayout(false);
        for (int n : p.tiles)
            check(n == 0 || n == 14, "Donkey zone uses its own primary tile");
        for (int n = 0; n < 20; ++n)
            check(p.spawnable[size_t(n)] ==
                      (p.tiles[size_t(n * 2)] == 14 && p.tiles[size_t(n * 2 + 1)] == 14),
                  "Reversed donkey layouts retain paired eligibility");
    }
    neon::DarwinRandom random(1);
    goatup::GenerationState state;
    goatup::PlatformGenerator generation(random, state);
    std::vector<goatup::PlatformBirth> births;
    generation.create = [&](const auto &b) { births.push_back(b); };
    generation.enterZone = [] {};
    p.level = 64;
    generation.generate(p);
    check(births.size() == 4 && births[0].kind == goatup::PlatformBirth::Kind::Pickup &&
              births[0].frame == 82 && random.draws() == 7,
          "Zone entry forces speed pickup after discarded choices and the empty NPC-site draw");
    births.clear();
    p.level = 65;
    const auto first = random.draws();
    generation.generate(p);
    check(births.size() == 4 && births[0].kind == goatup::PlatformBirth::Kind::Baddy2049 &&
              random.draws() == first + 5,
          "Odd donkey levels consume outer and inner baddy-site draws before bonuses");
    births.clear();
    p.level = 70;
    const auto second = random.draws();
    generation.generate(p);
    check(births.size() == 4 && births[0].kind == goatup::PlatformBirth::Kind::Goody2049 &&
              random.draws() == second + 6,
          "Protective goody uses site, reversed offset range and frame draws");
    check(births[0].verticalOffset <= -.02f && births[0].verticalOffset >= -.08f,
          "Goody preserves reversed native offset range");
    goatup::CollectionRecord records;
    goatup::World world(records, 1);
    std::vector<int> sounds;
    world.actors.sound = [&](int sample, int) { sounds.push_back(sample); };
    world.game.stopSound = [](int) {};
    world.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
    world.game.gameOver = true;
    p.level = 70;
    p.earlyLayout(false);
    p.y = .2f;
    world.denizens.create({goatup::PlatformBirth::Kind::Pickup, &p, 0, 82});
    const auto speed = world.game.climbSpeed;
    world.denizens.hit(0, 1, {});
    check(world.game.climbSpeed == speed * .5f && world.kidState.statusKind == 0,
          "Speed pickup applies native response and local status");
    world.denizens.reset();
    world.denizens.create({goatup::PlatformBirth::Kind::Goody2049, &p, 0, 89, -1, 0, -.05f});
    world.denizens.tick();
    check(world.denizens.contacts()[0].position.y ==
              double(.2f) - double(.035f) * .5 + double(-.05f),
          "Goody uses platform projection plus its birth offset");
    world.denizens.hit(0, 2, {});
    check(world.actors.hazardGrace == 350 && world.actors.hazardWarning == 262,
          "Goody protects the player with native warning timing");
    world.denizens.reset();
    world.game.gameOver = false;
    world.denizens.create({goatup::PlatformBirth::Kind::Baddy2049, &p, 0, 85, -1, .001});
    world.denizens.hit(0, 2, {});
    check(world.actors.score == 100 && world.actors.achievements == 0,
          "Baddy reward is distinct from sheep scoring/achievements");
    world.denizens.reset();
    world.actors.hazardGrace = 0;
    world.goat.kids = 1;
    world.denizens.create({goatup::PlatformBirth::Kind::Baddy2049, &p, 0, 85, -1, .001});
    const auto recoil = world.random.draws();
    world.denizens.hit(0, 1, {});
    check(world.random.draws() == recoil + 2 && world.actors.pendingKidLoss == 1,
          "Baddy recoil has two draws, without sheep's extra hurt noises");
    world.denizens.reset();
    world.actors.pendingKidLoss = 0;
    world.goat.kids = 0;
    world.game.gameOver = true;
    world.denizens.create({goatup::PlatformBirth::Kind::Pickup, &p, 0, 83});
    world.denizens.hit(0, 1, {});
    check(world.game.flightTicks == 1200 && world.game.flightWarning == 900,
          "Flight pickup initializes the source duration");
    world.paused = true;
    world.tick({});
    check(world.game.flightTicks == 1200, "Pause freezes flight countdown");
    world.paused = false;
    world.game.flightTicks = 226;
    world.game.flightWarning = 225;
    world.tick({});
    check(world.game.flightFlash == 5 && sounds.back() == 45,
          "Late flight warning enters the native five-tick flash");
    world.denizens.reset();
    world.denizens.create({goatup::PlatformBirth::Kind::Pickup, &p, 0, 84});
    world.denizens.hit(0, 1, {});
    check(world.actors.poison == 1 && world.actors.hazardGrace == 500 &&
              world.kidState.statusKind == 2,
          "Poison pickup retains protection and status effects");
    world.paused = true;
    world.tick({});
    check(world.actors.poison == 1 && world.feedback.scaleX == float(.79),
          "Poison presentation continues in pause without draining duration");
    world.paused = false;
    for (int i = 0; i < 501; ++i)
        world.tick({});
    check(world.actors.poison <= 0 && world.feedback.scaleX == .99f &&
              world.feedback.colour[3] == 0,
          "Poison expiration restores neutral feedback");
}
void titlePresentation() {
    neon::DarwinRandom random(1);
    goatup::Effects title(random);
    std::array<uint32_t, 16> collection{};
    title.initializeTitleStars(collection, 0, [](int) { return goatup::Vec2{2, 1}; });
    std::vector<goatup::ActorDraw> draw;
    title.drawStars(draw);
    check(draw.size() == 50, "Title constructs fifty native titleStars");
    check(std::any_of(draw.begin(), draw.end(),
                      [](const auto &d) { return std::abs(d.position.x) < .1; }),
          "Title uses its full-width star distribution, not the gameplay edge bands");
    int icons = 0;
    for (int n = 0; n < 600; ++n) {
        title.tickStars();
        draw.clear();
        title.drawStars(draw);
        for (const auto &d : draw)
            if (d.radius == .05f) {
                ++icons;
                check(d.frame == 586, "Empty collection uses the native fallback icon");
                check(d.scale.y == .5, "Title icon uses the original sprite aspect ratio");
            }
    }
    check(icons > 0, "Title's rare bonus-icon branch executed");
    title.reset();
    collection[0] = 1;
    title.initializeTitleStars(collection, 1, [](int) { return goatup::Vec2{1, 1}; });
    icons = 0;
    for (int n = 0; n < 600; ++n) {
        title.tickStars();
        draw.clear();
        title.drawStars(draw);
        for (const auto &d : draw)
            if (d.radius == .05f) {
                ++icons;
                check(d.frame == goatup::original::Bonuses[0],
                      "Title icons are sampled from the actual collected flags");
            }
    }
    check(icons > 0, "Collected-item title presentation executed");
}

void campaign() {
    // Controlled course traversal exercises every layout and synchronous birth,
    // in both directions. It is an integration fixture, not a played victory.
    for (bool returning : {false, true})
        for (int lap : {0, 1, 3}) {
            goatup::CollectionRecord record;
            goatup::World world(record, 1);
            world.game.returning = returning;
            world.game.laps = lap;
            world.game.gameOver = true;
            world.actors.sound = [](int, int) {};
            world.game.stopSound = [](int) {};
            world.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
            std::vector<goatup::ActorDraw> draw;
            for (int step = 1; step <= 519; ++step) {
                const int level = returning ? 520 - step : step;
                // Reset the retained pool between controlled starts; normal running
                // uses its original 320-frame offscreen retirement instead.
                for (auto &p : world.tower.platforms)
                    p.reset();
                world.denizens.reset();
                world.effects.reset();
                world.tower.scroll = float(double(level) * .4 - .6);
                world.prepareBackground(draw);
                for (int tick = 0; tick < 12; ++tick) {
                    world.tick({});
                    draw.clear();
                    world.prepareForeground(draw);
                }
                for (const auto &item : draw)
                    check(std::isfinite(item.position.x) && std::isfinite(item.position.y),
                          "Nonfinite campaign actor");
                check(world.game.lastTheme == (level >> 6) % 9 ||
                          world.game.lastTheme == ((level + 6) >> 6) % 9,
                      "Course factory theme");
            }
        }
    goatup::CollectionRecord record;
    goatup::World w(record, 1);
    w.actors.sound = [](int, int) {};
    w.game.stopSound = [](int) {};
    w.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
    auto &p = w.tower.platforms[0];
    p.active = true;
    p.level = 192;
    p.theme = 3;
    p.y = .4f;
    p.tiles.fill(98);
    p.tiles[10] = p.tiles[11] = 101;
    p.tiles[16] = p.tiles[17] = 105;
    const auto before = w.random.draws();
    w.game.tread(p, 10);
    check(p.tiles[10] == 106 && p.tiles[16] == 105, "First bridge switch retains bridge");
    w.game.tread(p, 16);
    for (int n = 10; n <= 17; ++n)
        check(p.tiles[n] == 0, "Both switches remove the complete paired bridge");
    check(w.game.bridgeJustRemoved && w.random.draws() == before + 24,
          "Eight bridge bits consume three ordered draws each");
    p.level = 384;
    p.theme = 6;
    p.tiles.fill(188);
    const auto smash = w.random.draws();
    w.game.smash(p, 20);
    check(p.tiles[19] == 0 && p.tiles[20] == 0 && p.tiles[21] == 0 && w.random.draws() == smash + 6,
          "Smash removes three cells, noise and four spins");
    w.denizens.reset();
    w.effects.reset();
    p.level = 512;
    p.theme = 8;
    p.tiles.fill(204);
    p.y = .4f;
    w.goat.jumped = true;
    w.goat.platform = w.goat.lastPlatform = 512;
    w.goat.position.y = .385;
    w.game.tread(p, 20);
    check(w.game.summitVisited && (w.actors.achievements & 4),
          "Summit tile enters native award and tally state");
    std::vector<goatup::ActorDraw> draw;
    for (int tick = 0; tick < 1000 && !w.game.returning; ++tick) {
        w.tick({});
        draw.clear();
        w.prepareForeground(draw);
    }
    check(w.game.tallyBegun && w.game.tallyComplete && w.game.summitBirdSent && w.game.returning,
          "Summit bird initiates descent after tally and delay");
    // Repeat the summit through actual preparation/pool movement, rather than
    // just the state fixture above. No input is required during source tally.
    goatup::CollectionRecord liveRecord;
    goatup::World live(liveRecord, 1);
    live.actors.sound = [](int, int) {};
    live.game.stopSound = [](int) {};
    live.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
    live.tower.scroll = float(512. * .4 - .6);
    live.goat.jumped = true;
    live.game.ticks = 100;
    for (int tick = 0; tick < 1200 && !live.game.returning; ++tick) {
        draw.clear();
        live.prepareBackground(draw);
        draw.clear();
        live.prepareForeground(draw);
        live.tick({});
    }
    check(live.game.returning && !live.game.gameOver,
          "Actual summit preparation and bird contact reach descent");
    for (int tick = 0; tick < 180 && !live.game.gameOver; ++tick) {
        live.tick({});
        draw.clear();
        live.prepareBackground(draw);
        draw.clear();
        live.prepareForeground(draw);
    }
    check(live.game.diveSpacing > .05f,
          "Return platform spacing moves toward the recovered dive target");
    // Homecoming resets the pool and advances difficulty, not a fabricated win.
    w.game.summitVisited = w.game.diveRequested = false;
    w.game.returning = true;
    w.goat.platform = w.goat.lastPlatform = 1;
    p.level = 1;
    p.active = true;
    p.y = .4f;
    p.tiles.fill(4);
    w.goat.position.y = .385;
    w.tick({});
    check(w.game.laps == 1 && !w.game.returning && !w.game.atTop && (w.actors.achievements & 8),
          "Homecoming awards and begins the next ascent lap");
    // Landing at home may occur while jump remains held: retain the landing's
    // platform pointer through pool invalidation, as the native local does.
    w.game.returning = true;
    w.goat.platform = -1;
    w.goat.position.y = .39;
    w.goat.velocity.y = 0;
    p.active = true;
    p.level = 1;
    p.theme = 0;
    p.y = .4f;
    p.tiles.fill(4);
    w.tick({goatup::ClimbInput::Method::None, 0, true});
    check(w.game.laps == 2 && w.goat.velocity.y == -.005,
          "Held jump survives same-frame homecoming pool reset");
}

} // namespace
void reviewRegressions() {
    goatup::Platform retained;
    retained.spawnable.fill(false);
    retained.spawnable[17] = true;
    retained.reset();
    check(retained.spawnable[0] && retained.spawnable[9] && !retained.spawnable[10] &&
              retained.spawnable[17],
          "Reset initializes the original ten eligibility entries and retains the upper half");
    retained.level = 512;
    retained.layout(false, nullptr, {});
    check(retained.spawnable[17] && !retained.spawnable[10] && retained.tiles[0] == 204,
          "Summit layout retains eligibility while replacing its floor");
    retained.layout(true, nullptr, {});
    check(std::all_of(retained.tiles.begin(), retained.tiles.end(),
                      [](int tile) { return tile == 0; }),
          "Summit platforms disappear during descent");
    // Independent original-execution cases: late-zone scan 5c6f0, seed 17.
    goatup::Platform p;
    p.level = 256;
    p.layout(false, nullptr, {});
    check(p.spawnable[2] && !p.spawnable[9] && p.spawnable[12] && !p.spawnable[19],
          "Later zones retain odd-pair eligibility and final centre-word read");
    uint64_t word = 177;
    std::memcpy(&p.centres[0].x, &word, sizeof word);
    p.layout(false, nullptr, {});
    check(p.spawnable[19], "Retained centre alias is modeled without an out-of-range tile access");
    p.level = 257;
    p.layout(false, nullptr, {});
    check(p.tiles[1] == 0 && p.tiles[16] == 177 && p.tiles[19] == 0,
          "City phase zero does not reverse odd rows");
    goatup::CollectionRecord records;
    goatup::World w(records, 17, false);
    w.actors.sound = [](int, int) {};
    w.game.stopSound = [](int) {};
    w.actors.spriteSize = [](int) { return goatup::Vec2{1, 1}; };
    auto &support = w.tower.platforms[0];
    support.level = 449;
    support.active = true;
    support.y = -.1f;
    support.tiles.fill(197);
    w.game.laps = 5;
    w.denizens.create({goatup::PlatformBirth::Kind::Egg, &support, 0, 201});
    std::vector<goatup::ActorDraw> draw;
    for (int i = 0; i < 3; ++i) {
        w.denizens.tick();
        w.denizens.prepareDraw(draw);
    }
    check(!draw.empty() && draw.back().frame == 201 && draw.back().scale.y == 1,
          "A resting egg's saved swelling does not change its projected Y scale");
    w.denizens.reset();
    w.denizens.create({goatup::PlatformBirth::Kind::Bonus, &support, 0, 554, 0});
    w.denizens.create({goatup::PlatformBirth::Kind::CeDude});
    w.denizens.homecoming();
    auto contacts = w.denizens.contacts();
    check(!contacts[0].dead && contacts[1].dead,
          "Homecoming removes bonuses, not wandering Chuckie actors");
    w.denizens.reset();
    w.paused = true;
    w.game.lastTheme = 0;
    w.zoneTicks = w.captionTicks = w.summitTicks = w.goalTicks = 2;
    w.goal = 0;
    w.caption = "BONUS";
    w.summitCaption = "SUMMIT";
    w.tick({});
    bool bonus = false, summit = false;
    for (const auto &row : w.hudRows) {
        bonus |= row.row == 16 && row.text == "BONUS";
        summit |= row.row == 12 && row.text == "SUMMIT";
    }
    check(bonus && summit && w.zoneTicks == 1 && w.goalTicks == 1,
          "Original message slots coexist and age during pause");
    w.paused = false;
    w.game.laps = 0;
    support.level = 1;
    support.y = .4f;
    support.tiles.fill(4);
    w.goat.platform = w.goat.lastPlatform = 1;
    w.goat.position.y = .385;
    w.tick({});
    check(w.actors.score == 1 && w.hudRows.front().text == "Score: 0",
          "HUD is retained before same-tick grazing credits");
    records.highestPlatform = 88;
    w.game.gameOver = true;
    w.tick({});
    bool lifetime = false;
    for (const auto &row : w.hudRows)
        lifetime |= row.row == 28 && row.text == "Highest platform: 88";
    check(lifetime, "GOAT OVER includes the independent lifetime record panel");
}
void summitBirdBoundary() {
    goatup::CollectionRecord records;
    goatup::World world(records, 17, false);
    world.game.sound = [](int) {};
    world.goat.position = {0, .4};
    world.effects.summitBird(world.game, world.goat);
    for (int tick = 0; tick < 268; ++tick)
        world.effects.tickUi(false, 0, {1, 1, 1, 1});
    check(!world.game.diveRequested, "Summit bird made contact before the source threshold");
    world.effects.tickUi(false, 0, {1, 1, 1, 1});
    check(world.game.diveRequested && world.goat.platform == -1 && world.goat.velocity.y == -.016,
          "Summit bird uses the source float-distance/double-threshold comparison");
}
void ownerFeedbackRegression() {
    goatup::CollectionRecord records;
    goatup::World w(records, 17, false);
    w.game.gameOver = true;
    w.game.ticks = 100;
    w.actors.poison = 1;
    for (int n = 0; n < 499; ++n)
        w.tick({});
    // Original ARM owner run, observed at tick 499; an early f32 blue product differs.
    check(w.feedback.colour[2] == float(.004319710191339254),
          "Poison blue feedback retains the original double intermediate");
}

int main() {
    try {
        layouts();
        poolAndProjection();
        climbing();
        generation();
        effects();
        denizens();
        kids();
        openingLoop();
        donkeyZone();
        campaign();
        titlePresentation();
        reviewRegressions();
        ownerFeedbackRegression();
        summitBirdBoundary();
        std::cout << "GoatUp full-course generation, actor integration, terrain interactions, "
                     "summit/descent/homecoming and opening regressions passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
