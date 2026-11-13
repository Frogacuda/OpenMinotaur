#include "neon/input.h"
#include "world.h"
#include "waves.h"
#include "original_tables.h"
#include "air_enemy.h"
#include "ground.h"
#include "pickups.h"
#include "terrain.h"
#include "effects.h"
#include "scene.h"
#include "controls.h"
#include "projectiles.h"
#include "ground_features.h"
#include "frontend.h"
#include "core_api.h"
#include <iostream>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <limits>
using namespace superoxwars;
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
void checkOmissionRepairs();
int main() {
    try {
        checkOmissionRepairs();
        {
            Frontend menu;
            for (bool motion : {false, true})
                for (bool controller : {false, true}) {
                    menu.capabilities(motion, controller);
                    check(menu.tiltChoiceVisible() == (motion && controller),
                          "Tilt toggle requires both device capabilities");
                    check(menu.pauseCount() == (motion && controller ? 4 : 3) &&
                              menu.pauseAction(menu.pauseCount() - 1) == 6,
                          "Exit remains last in both pause layouts");
                }
            menu.enter(Frontend::Screen::Paused);
            menu.selection = 3;
            menu.capabilities(true, false);
            check(menu.selection == 2 && menu.pauseAction(menu.selection) == 6,
                  "Hidden toggle preserves Exit focus");
            menu.capabilities(true, true);
            menu.selection = 2;
            menu.touchBlocked = false;
            menu.capabilities(true, false);
            check(menu.selection == 0 && menu.touchBlocked,
                  "Removing a selected toggle safely focuses Resume and gates held touches");
        }
        for (float x : {-1.f, -.5f, -.001f, 0.f, .001f, .5f, 1.f})
            check(AnalogControls::worldInfluence(x) == double(x),
                  "World input is linear with no second deadzone");
        for (float lt : {0.f, .35f, 1.f})
            for (float rt : {0.f, .35f, 1.f}) {
                neon::input::ControllerState p;
                p.connected = true;
                p.lt = lt;
                p.rt = rt;
                check(AnalogControls::worldInfluence(neon::input::worldInfluenceAxis(p)) ==
                          double(rt - lt),
                      "Triggers cancel in native world input units");
            }
        check(AnalogControls::worldInfluence(2) == 1 && AnalogControls::worldInfluence(-2) == -1,
              "Direct world command respects steady-gravity bounds");
        check(AnalogControls::worldInfluence(std::numeric_limits<float>::quiet_NaN()) == 0,
              "Invalid controller report is neutral");
        check(original::Rows.size() == 10 && original::Waves.size() == 10, "Original area tables");
        auto wStorage = std::make_unique<World>();
        World &w = *wStorage;
        auto *ship = new Ship(w);
        w.player = ship;
        w.add(Domain::Player, ship);
        for (int n = 0; n < 40; ++n)
            w.tick({});
        int count = 0;
        for (auto *e = w.lists[size_t(Domain::PlayerShots)]; e; e = e->next)
            ++count;
        check(count == 4, "Initial fire phase produces paired shots");
        check(w.coverageStop.empty(), "Isolated player controls/fire");
        auto moveStorage = std::make_unique<World>();
        World &move = *moveStorage;
        auto *player = new Ship(move);
        move.add(Domain::Player, player);
        move.tick({{.1, -.06}});
        check(player->position.x > 0 && player->position.y < .3,
              "Direct controller intent follows source movement path");
        player->grace = 0;
        Entity hazard(move);
        player->didHit(hazard);
        check(player->state == Ship::State::Hit, "Source contact enters hit state");
        PlayerBullet bullet(w, *ship, 0, 1, 2);
        Entity target(w);
        bullet.didHit(target);
        check(!bullet.dead && bullet.health == 1,
              "Source pierce callback decrements before retirement");
        bullet.didHit(target);
        bullet.didHit(target);
        check(bullet.dead, "Source projectile retires after penetration count");
        Entity moving(w);
        moving.position = {.49, -.49};
        moving.velocity = {.02, -.02};
        moving.run();
        check(moving.wrapped && moving.age == 1, "Shared original base motion");
        auto launchWorldStorage = std::make_unique<World>();
        World &launchWorld = *launchWorldStorage;
        WaveController wave(launchWorld);
        std::array<uint32_t, 1024> seeds{};
        for (size_t n = 0; n < seeds.size(); ++n)
            seeds[n] = uint32_t(n + 1);
        wave.init(0, seeds);
        for (int n = 0; n < 3; ++n)
            wave.rowCrossed();
        check(!wave.active, "Wave countdown advances on row crossings");
        const auto prior = launchWorld.random.state();
        wave.rowCrossed();
        check(wave.active && wave.remaining == 12 && wave.archetype == 0,
              "Original first wave tuple");
        check(launchWorld.random.state() != prior && launchWorld.random.draws() == 1,
              "Temporary wave setup retains one global random advance");
        for (int n = 0; n < 240; ++n)
            wave.tick();
        int pods = 0;
        for (auto *e = launchWorld.lists[size_t(Domain::LaunchPods)]; e; e = e->next)
            ++pods;
        check(pods == 12 && !wave.active, "Original wave interval produces twelve pods");
        int arrived = 0;
        launchWorld.launchEnemy = [&](const LaunchPod &p) {
            ++arrived;
            check(p.retained.x >= -.3 && p.retained.x <= .3, "First wave retained launch anchor");
        };
        for (int n = 0; n < 500; ++n)
            launchWorld.runDomain(Domain::LaunchPods);
        check(arrived == 12 && launchWorld.coverageStop.empty(),
              "Full pod travel, retirement and combat-birth boundary");
        auto combatStorage = std::make_unique<World>();
        World &combat = *combatStorage;
        Ship combatShip(combat);
        combat.player = &combatShip;
        combatShip.position = {.2, .3};
        PodLaunch launch;
        launch.archetype = 0;
        LaunchPod launchPod(combat, launch);
        launchPod.position = {-.3, -.7};
        launchPod.retained = launchPod.position;
        const auto beforeBirth = combat.random.draws();
        AirEnemy air(combat, launchPod);
        check(combat.random.draws() - beforeBirth == 51,
              "58194 constructor retains 3 + 4*12 source random draws");
        air.run();
        check(air.position.y > -.7 && air.previousStep.y > 0,
              "ABE first program advances along the source velocity ramp");
        for (int kind = 0; kind < 29; ++kind) {
            launchPod.launch.archetype = kind;
            AirEnemy enemy(combat, launchPod);
            check(combat.coverageStop.empty(), "Connected air program constructor coverage");
            for (int n = 0; n < 400; ++n)
                if (!enemy.dead)
                    enemy.run();
            check(std::isfinite(enemy.position.x) && std::isfinite(enemy.position.y),
                  "Phase interpreter stays finite");
        }
        auto groundWorldStorage = std::make_unique<World>();
        World &groundWorld = *groundWorldStorage;
        groundWorld.tiles.positions[10] = {.2, .1};
        GroundEnemy turret(groundWorld, 10, 2);
        std::vector<ActorDraw> drawing;
        turret.prepare(drawing);
        check(turret.active && turret.position.x == .2,
              "Ground draw publishes tile position before activation");
        Entity shot(groundWorld);
        shot.type = 0x2000;
        shot.frame = 28;
        turret.hitBy(shot);
        check(turret.dead && groundWorld.blueScore == 10 && groundWorld.coverageStop.empty(),
              "Ground neutral-colour kill retains source score and crater birth");
        check(dynamic_cast<Crater *>(groundWorld.lists[size_t(Domain::LaunchPods)]) != nullptr,
              "Ground crater uses original +368 domain");
        auto deathWorldStorage = std::make_unique<World>();
        World &deathWorld = *deathWorldStorage;
        auto *dying = new Ship(deathWorld);
        deathWorld.player = dying;
        deathWorld.add(Domain::Player, dying);
        dying->grace = 0;
        Entity hazard2(deathWorld);
        dying->didHit(hazard2);
        check(dynamic_cast<Explosion *>(deathWorld.lists[size_t(Domain::Decor)]) != nullptr,
              "Ship hit starts explosion before breakup");
        for (int n = 0; n < 10; ++n)
            deathWorld.tick({});
        int fragments = 0;
        for (auto *e = deathWorld.lists[size_t(Domain::PlayerShots)]; e; e = e->next)
            fragments += dynamic_cast<Fragment *>(e) != nullptr;
        check(fragments == 150 && dying->state == Ship::State::Waiting,
              "Source breakup timing and 150-fragment population");
        check(deathWorld.coverageStop.empty(), "Death family runs without placeholder callback");
        auto terrainWorldStorage = std::make_unique<World>();
        World &terrainWorld = *terrainWorldStorage;
        Terrain terrain(terrainWorld);
        terrain.init(0);
        int births = 0;
        terrain.createActor = [&](char c, int cell, int variant) {
            check(c == 'A' && cell >= 0 && variant == -1, "Original opening row actor decoding");
            ++births;
        };
        terrain.cursor = 13;
        terrain.worldRow = 13;
        terrain.generate(0);
        drawing.clear();
        terrain.prepare(drawing);
        check(births == 2, "Extracted first turret row produces two actors");
        check(!drawing.empty(), "Original terrain cells prepare into native artwork commands");
        check(terrainWorld.coverageStop.empty(), "Opening row glyph coverage");
        auto upgradesStorage = std::make_unique<World>();
        World &upgrades = *upgradesStorage;
        Ship upgradeShip(upgrades);
        upgrades.player = &upgradeShip;
        upgrades.polarity = -.5f;
        WeaponPickup pickup(upgrades, 0, 28);
        Entity collector(upgrades);
        pickup.hitBy(collector);
        check(upgrades.blueCollected == 1,
              "Weapon pickup contributes to the original cumulative star total");
        check(upgradeShip.power == 1, "Matching-colour weapon pickup grants a live follower");
        upgradeShip.shotThisTick = true;
        upgrades.runDomain(Domain::Decor);
        int curved = 0;
        float turns = 0;
        for (auto *e = upgrades.lists[size_t(Domain::PlayerShots)]; e; e = e->next)
            if (auto *b = dynamic_cast<PlayerBullet *>(e)) {
                ++curved;
                turns += b->arcStep;
            }
        check(curved == 2 && turns == 0, "Blue follower fires the source paired opposite arcs");
        upgradeShip.shotThisTick = false;
        upgrades.polarity = .5f;
        upgrades.runDomain(Domain::Decor);
        upgrades.runDomain(Domain::Decor);
        check(upgradeShip.power == 0,
              "Changing colour retires incompatible follower and returns weapon count");
        auto lifeStorage = std::make_unique<World>();
        World &life = *lifeStorage;
        LifePickup lifePickup(life, 0, 45);
        lifePickup.hitBy(collector);
        const int afterLife = life.lives;
        lifePickup.hitBy(collector);
        check(afterLife == 4 && life.lives == afterLife,
              "Oneup collection is single-use and reaches the source token reward");
        auto oxStorage = std::make_unique<World>();
        World &ox = *oxStorage;
        ox.polarity = .25f;
        OxBlock block(ox, 0);
        Entity red(ox);
        red.type = 0x2000;
        red.frame = 29;
        for (int n = 0; n < 4; ++n)
            block.hitBy(red);
        check(block.dead && ox.oxStreak == 1 && ox.redScore == 6 && ox.polarity > .25f,
              "Ox armour, colour reward and streak interaction");
        AnalogControls controls;
        const auto normal = controls.normal.apply({.5, 0}), focus = controls.focus.apply({.5, 0});
        check(focus.x > 0 && focus.x < normal.x,
              "A-held profile gives lower half-stick displacement");
        const auto horizontal = controls.normal.apply({1, 0}),
                   vertical = controls.normal.apply({0, 1}),
                   diagonal = controls.normal.apply({1, 1});
        check(std::abs(horizontal.x * 320 - vertical.y * 480) < 1e-6,
              "Analog axes use equal native pixel speed");
        check(diagonal.x < horizontal.x && std::abs(diagonal.x / diagonal.y - 1.5) < 1e-6,
              "Radial curve caps diagonal magnitude before native aspect conversion");
        auto profileWorldStorage = std::make_unique<World>();
        World &profileWorld = *profileWorldStorage;
        Ship profileShip(profileWorld);
        const auto initial = profileShip.position;
        profileWorld.input.analogMovement = controls.normal.apply({.5, 0});
        profileShip.run();
        const double fast = profileShip.position.x - initial.x;
        profileShip.init();
        profileWorld.input.analogMovement = controls.focus.apply({.5, 0});
        profileShip.run();
        check(profileShip.position.x > 0 && profileShip.position.x < fast,
              "Native ship path applies focus without original touch acceleration");
        auto cannonWorldStorage = std::make_unique<World>();
        World &cannonWorld = *cannonWorldStorage;
        Ship cannonShip(cannonWorld);
        cannonWorld.player = &cannonShip;
        cannonWorld.enemyShotChance = 256;
        GroundEnemy cannon(cannonWorld, 0, 3);
        cannon.muzzle = {0, -.2};
        cannon.position = cannon.muzzle;
        emitEnemyShots(cannonWorld, cannon, 2, .003f, 0, .3926991f);
        const double first = cannonWorld.lists[size_t(Domain::Pickups)]->position.x;
        emitEnemyShots(cannonWorld, cannon, 2, .003f, 0, .3926991f);
        const double second = cannonWorld.lists[size_t(Domain::Pickups)]->position.x;
        check(first * second < 0 && std::abs(first + second) < 1e-7,
              "Cannon alternates mirrored muzzle offsets instead of accumulating translation");
        check(controls.normal.gain == 1.2f && controls.normal.curve == 1 &&
                  controls.focus.gain == .65f && controls.focus.curve == 2,
              "Fixed direct-controller response profiles");
        auto chainMinesStorage = std::make_unique<World>();
        World &chainMines = *chainMinesStorage;
        chainMines.level = 1;
        auto *mine1 = new Mine(chainMines, 10, 30);
        auto *mine2 = new Mine(chainMines, 11, 31);
        auto *farMine = new Mine(chainMines, 12, 30);
        chainMines.add(Domain::Foreground, mine1);
        chainMines.add(Domain::Foreground, mine2);
        chainMines.add(Domain::Foreground, farMine);
        mine1->detonate();
        check(mine2->countdown == 10 && farMine->countdown == 0,
              "Mine chain uses adjacent logical columns/rows");
        int mineShots = 0;
        for (auto *e = chainMines.lists[size_t(Domain::Pickups)]; e; e = e->next)
            ++mineShots;
        check(mineShots == 5, "Level-two mine burst has level + power*4 + 4 shots");
        auto pyramidWorldStorage = std::make_unique<World>();
        World &pyramidWorld = *pyramidWorldStorage;
        pyramidWorld.level = 1;
        Pyramid pyramid(pyramidWorld, 0);
        pyramid.prepare(drawing);
        Entity activating(pyramidWorld);
        pyramid.hitBy(activating);
        for (int n = 0; n < 60; ++n)
            pyramid.run();
        int missiles = 0;
        for (auto *e = pyramidWorld.lists[size_t(Domain::Enemies)]; e; e = e->next)
            missiles += dynamic_cast<HomingMissile *>(e) != nullptr;
        check(missiles == 14 && pyramid.missiles == 0,
              "Activated pyramid emits its finite source missile budget");
        auto transitionStorage = std::make_unique<World>();
        World &transition = *transitionStorage;
        Scene transitionScene(transition);
        transitionScene.start();
        transition.score = 123;
        transition.blueScore = 100;
        transition.redScore = 23;
        transition.lives = 5;
        transition.tiles.poke(3, 4, 0x211);
        transition.tiles.rowOffset = 7;
        transition.tiles.scroll.y = .37;
        const auto *retainedShip = transition.player;
        transitionScene.nextLevel();
        check(transition.level == 1 && transition.lives == 5 && transition.score == 123 &&
                  transition.player == retainedShip,
              "Area transition retains lives, scores and live player");
        check(transition.tiles.peek(3, 4) == 0x211 && transition.tiles.rowOffset == 7 &&
                  transition.tiles.scroll.y == .37,
              "Area transition retains circular terrain contents and scroll phase");
        auto mappingStorage = std::make_unique<World>();
        World &mapping = *mappingStorage;
        Scene mappingScene(mapping);
        mappingScene.start();
        mappingScene.create('H', 4, -1);
        check(dynamic_cast<Ear *>(mapping.lists[size_t(Domain::Air)]) != nullptr,
              "Original factory H uses ear vtable 7b5a8");
        mappingScene.create('R', 4, -1);
        check(dynamic_cast<Horn *>(mapping.lists[size_t(Domain::Air)]) != nullptr,
              "Original factory R uses horn vtable 7b5d8");
        auto featuresStorage = std::make_unique<World>();
        World &features = *featuresStorage;
        Ship featureShip(features);
        features.player = &featureShip;
        featureShip.position = {0, .3};
        features.enemyShotChance = 256;
        Ear ear(features, 3);
        ear.position = ear.muzzle = {0, -.2};
        ear.active = true;
        ear.queued = true;
        ear.run();
        check(ear.hingeState == 1 && !ear.queued, "Ear consumes queued hit on its idle frame");
        for (int n = 0; n < 8; ++n)
            ear.run();
        check(ear.hingeState == 2 && std::abs(ear.angle) > 1.5f,
              "Ear opens in doubled hinge steps before its aimed fan");
        int earShots = 0;
        for (auto *e = features.lists[size_t(Domain::Pickups)]; e; e = e->next)
            ++earShots;
        check(earShots == 5, "Ear fan retains five emission slots");
        for (int n = 0; n < 20 && ear.hingeState == 2; ++n)
            ear.run();
        check(ear.hingeState == 0 && ear.angle == 0,
              "Ear returns to the closed hinge without drift");
        features.tiles.positions[3] = {-.2, -.2};
        drawing.clear();
        ear.prepare(drawing);
        check(drawing.size() == 1 && drawing[0].pivot.x < 0 && ear.muzzle.x == -.2,
              "Ear draw publishes its tile and signed edge pivot");
        Horn horn(features, 3);
        horn.position = horn.muzzle = {0, -.2};
        features.ticks = 95;
        horn.run();
        int hornShots = 0;
        for (auto *e = features.lists[size_t(Domain::Pickups)]; e; e = e->next)
            ++hornShots;
        check(hornShots == earShots, "Horn is quiet before the source tick-mask window");
        features.ticks = 96;
        horn.run();
        auto *hornBullet = dynamic_cast<EnemyBullet *>(features.lists[size_t(Domain::Pickups)]);
        check(hornBullet && hornBullet->frame == 57 && hornBullet->paletteSlot == 0 &&
                  !features.enemyBulletOverride,
              "Horn special projectile override is scoped to its timed shot");
        EnemyBullet ordinary(features, {}, {});
        check(ordinary.frame == 2 && ordinary.paletteSlot == 10 && ordinary.health == 1,
              "Later ordinary shots retain palette 10 and inherited entity contact field");
        Tooth tooth(features, 2, false), invertedTooth(features, 2, true);
        check(tooth.scale.y == -invertedTooth.scale.y,
              "U tooth inverts the original vertical scale");
        features.area = 5;
        Fence mazeFence(features, 0, -1);
        drawing.clear();
        mazeFence.prepare(drawing);
        check(mazeFence.frame == 61 && mazeFence.layerPalette == 0xc00040 && drawing.size() == 9,
              "Maze fence retains packed layer colour and eight armour layers");
        const auto sparkDraws = features.random.draws();
        Entity petals(features);
        flowerSparks(features, petals);
        check(features.random.draws() - sparkDraws == 40,
              "Ten flower petals consume four draws each");
        const auto butterflyDraws = features.random.draws();
        Butterfly butterfly(features, petals);
        check(features.random.draws() - butterflyDraws == 3 && butterfly.grace == 10 &&
                  butterfly.tracking >= 100 && butterfly.tracking <= 200,
              "Butterfly keeps the overwritten tracking draw and ten-frame grace");
        butterfly.hitBy(shot);
        check(!butterfly.dead, "Butterfly ignores contacts during birth grace");
        for (int n = 0; n < 10; ++n)
            butterfly.run();
        butterfly.hitBy(shot);
        check(butterfly.dead, "Butterfly accepts contacts after grace expires");
        auto terminalStorage = std::make_unique<World>();
        World &terminal = *terminalStorage;
        terminal.terminalTilesRemaining = 1;
        int foxAwards = 0;
        terminal.achievement = [&](int id) {
            check(id == 0, "Fox award source ID");
            ++foxAwards;
        };
        PlayerBullet terminalShot(terminal);
        terminalShot.position = {0, -.2};
        const auto foxCell = terminal.tiles.cellAt(terminalShot.position);
        terminal.tiles.poke(foxCell[0], foxCell[1], 0x72e);
        terminalShot.run();
        check(terminal.terminalTilesRemaining == 0 && terminal.foxFound && foxAwards == 1 &&
                  dynamic_cast<Fox *>(terminal.lists[size_t(Domain::Decor)]),
              "Maze terminal threshold reveals the fox through the source award path");
        terminal.tiles.poke(foxCell[0], foxCell[1], 0x72e);
        terminalShot.run();
        check(foxAwards == 1, "Fox reveal is single-use within an area");
        Records records;
        records.checkpoint(transition, 1);
        check(records.unlocked == 1 && records.starts[1].lives == 5 &&
                  records.starts[1].blue == 100,
              "Source checkpoint banks lives and start scores");
        transition.lives = 2;
        transition.blueScore = 500;
        records.checkpoint(transition, 1);
        check(records.starts[1].blue == 100,
              "Lower-life replay cannot replace a better checkpoint");
        Frontend ui;
        ui.records = records;
        ui.attach(transition, transitionScene);
        transition.tick({});
        ui.command(transition, transitionScene, mino::PAD_A);
        transition.tick({});
        std::vector<TextLine> enteredText;
        std::vector<ActorDraw> enteredDraw;
        ui.prepare(transition, transitionScene, enteredText, enteredDraw);
        check(std::none_of(enteredText.begin(), enteredText.end(),
                           [](const auto &l) { return l.y >= 4 && l.y <= 29; }),
              "Entering selector clears the title writes made earlier in the owner update");
        ui.selectedLevel = 1;
        transition.tick({});
        std::vector<TextLine> uiText;
        std::vector<ActorDraw> uiDraws;
        ui.prepare(transition, transitionScene, uiText, uiDraws);
        check(std::any_of(uiText.begin(), uiText.end(),
                          [](const auto &t) { return t.text == "'BETA-AREA'"; }),
              "Source area selector uses the saved absolute area");
        ui.activate(transition, 0);
        transition.tick({});
        for (int n = 0; n < 120; ++n)
            transition.tick({});
        check(ui.pauseOpacity < .1f, "Untouched pause entity fades away");
        ui.activate(transition, 5);
        for (int n = 0; n < 20; ++n)
            transition.tick({});
        check(ui.pauseOpacity == 1 && ui.exitOpacity == .6f,
              "Paused original UI entities become visible");
        ui.command(transition, transitionScene, mino::PAD_DOWN);
        check(ui.selection == 1, "Pause navigation selects Title between Resume and Exit");
        ui.command(transition, transitionScene, mino::PAD_A);
        transition.tick({});
        check(ui.phase == Frontend::Phase::Attract && transition.demo && !ui.quitRequested,
              "Title resets attract without exiting the module");
        ui.command(transition, transitionScene, mino::PAD_A);
        transition.tick({});
        ui.activate(transition, 0);
        transition.tick({});
        ui.activate(transition, 5);
        ui.command(transition, transitionScene, mino::PAD_DOWN);
        ui.command(transition, transitionScene, mino::PAD_DOWN);
        ui.command(transition, transitionScene, mino::PAD_A);
        check(ui.quitRequested, "Pause Exit requests iOX without a title reset");
        // The touch path uses these same original entities, including their held
        // latch across title/selector transitions and retirement on a live start.
        auto touchStorage = std::make_unique<World>();
        World &touchWorld = *touchStorage;
        Scene touchScene(touchWorld);
        Frontend touchUI;
        touchUI.attach(touchWorld, touchScene);
        touchWorld.tick({});
        touchWorld.tick({});
        touchWorld.pointerHeld[0] = true;
        touchWorld.pointers[0] = {0, .22};
        touchWorld.tick({});
        for (int n = 0; n < 30; ++n)
            touchWorld.tick({});
        check(touchUI.phase == Frontend::Phase::Select && touchWorld.demo,
              "Held title touch cannot press the new selector Start button");
        touchWorld.pointerHeld[0] = false;
        touchWorld.tick({});
        touchWorld.pointerHeld[0] = true;
        touchWorld.tick({});
        touchWorld.tick({});
        check(touchUI.phase == Frontend::Phase::Play && !touchWorld.demo,
              "Fresh touch starts through the original button entity");
        touchWorld.pointerHeld[0] = false;
        touchWorld.tick({});
        touchWorld.pointers[0] = {0, -.35};
        touchWorld.pointerHeld[0] = true;
        touchWorld.tick({});
        check(touchWorld.paused, "Original pause button responds through the native touch path");
        touchWorld.pointerHeld[0] = false;
        for (int n = 0; n < 20; ++n)
            touchWorld.tick({});
        touchWorld.pointers[0] = {0, -.06};
        touchWorld.pointerHeld[0] = true;
        touchWorld.tick({});
        touchWorld.tick({});
        check(touchUI.phase == Frontend::Phase::Attract && touchWorld.demo && !touchWorld.paused &&
                  !touchUI.quitRequested,
              "Title touch returns through the owner reset branch without quitting");
        auto goalsStorage = std::make_unique<World>();
        World &goals = *goalsStorage;
        int notifications = 0;
        goals.achievement = [&](int) { ++notifications; };
        goals.demo = true;
        goals.achieveGoal(0);
        check(notifications == 0 && goals.sessionAchievements == 0,
              "Demo cannot unlock achievements");
        goals.demo = false;
        goals.achieveGoal(0);
        goals.achieveGoal(0);
        goals.achieveGoal(4);
        check(notifications == 2 && goals.sessionAchievements == 17,
              "Original session gate retains simultaneous goals and suppresses repeats");
        goals.blueCollected = 10001;
        goals.redCollected = 1000;
        ui.finish(goals);
        check((goals.sessionAchievements & 0x102u) == 0x102u,
              "Source end-run allegiance awards use cumulative collected coins");
        goals.sessionAchievements = 0;
        ui.finish(goals);
        check((goals.sessionAchievements & 0x102u) == 0x102u,
              "Lifetime tracking does not suppress a fresh session's awards");
        goals.sessionAchievements = 0;
        goals.blueCollected = 20000;
        goals.redCollected = 2000;
        ui.finish(goals);
        check(!(goals.sessionAchievements & 0x102u),
              "Original float-to-double allegiance comparison rejects the rounded exact "
              "ten-percent boundary");
        goals.redCollected = 1999;
        ui.finish(goals);
        check((goals.sessionAchievements & 0x102u) == 0x102u,
              "Just below the original ten-percent boundary is eligible");
        auto demoStorage = std::make_unique<World>();
        World &demo = *demoStorage;
        demo.demo = true;
        Scene demoScene(demo);
        demoScene.start();
        demoScene.nextLevel();
        check(demo.level == 0 && demo.sessionAchievements == 0,
              "Original attract progression stays in the opening area without awards");
        auto endedStorage = std::make_unique<World>();
        World &ended = *endedStorage;
        Scene endedScene(ended);
        endedScene.start();
        const auto terrainColour = endedScene.terrain.colours[1];
        endedScene.endRun();
        check(endedScene.terrain.colours[1].a == terrainColour.a * .5,
              "Game over dims original terrain colour and alpha");
        ended.player->state = Ship::State::GameOver;
        ended.blueScore = ended.score = 44;
        endedScene.blueBonus.remaining = 400;
        endedScene.blueBonus.steps = 1;
        endedScene.blueBonus.step = 400;
        endedScene.blueBonus.finalScore = 444;
        endedScene.bonusState = 3;
        endedScene.bonusTimer = 1;
        endedScene.tallyBonus();
        check(ended.blueScore == 44 && endedScene.blueBonus.steps == 0,
              "Bonus animation cannot award points after game over");
        // The campaign route also runs moo's frontend owner, retained messages and
        // UI lists, rather than testing Scene in isolation from their lifecycle.
        auto openingStorage = std::make_unique<World>();
        World &opening = *openingStorage;
        Scene scene(opening);
        Frontend campaignUI;
        campaignUI.attach(opening, scene);
        opening.tick({});
        campaignUI.command(opening, scene, mino::PAD_A);
        opening.tick({});
        campaignUI.command(opening, scene, mino::PAD_A);
        opening.tick({});
        opening.player->grace = 1000000;
        std::vector<ActorDraw> prepared;
        int ticks = 0, completed = 0;
        std::array<int, 5> injected{};
        while (opening.level < 10 && ticks++ < 250000) {
            opening.tick({{std::sin(double(ticks) * .013) * .025, 0}});
            opening.prepare(prepared);
            opening.finishForeground();
            if (opening.level != completed) {
                completed = opening.level;
                std::cerr << "Completed area " << completed << " at tick " << ticks << '\n';
            }
            for (auto *e = opening.lists[size_t(Domain::Air)]; e; e = e->next)
                if (auto *g = dynamic_cast<GroundEnemy *>(e); g && !g->dead && scene.speed < .1f) {
                    const int family = dynamic_cast<Eyeball *>(g) ? 0
                                       : dynamic_cast<Nose *>(g)  ? 1
                                       : dynamic_cast<Ear *>(g)   ? 2
                                       : dynamic_cast<Horn *>(g)  ? 3
                                       : dynamic_cast<Tooth *>(g) ? 4
                                                                  : -1;
                    if (family >= 0) {
                        Entity hit(opening);
                        hit.type = 0x2000;
                        hit.frame = 28;
                        hit.position = g->position;
                        const int hits = g->armour + 1;
                        for (int n = 0; n < hits; ++n)
                            g->hitBy(hit);
                        ++injected[size_t(family)];
                    }
                }
            if (!opening.coverageStop.empty())
                throw std::runtime_error(opening.coverageStop + " in area " +
                                         std::to_string(opening.area) + " row " +
                                         std::to_string(scene.terrain.cursor));
        }
        check(opening.level == 10 && opening.area == 0 && !opening.paused,
              "All ten source areas connect into the repeat campaign");
        check(opening.score == opening.blueScore + opening.redScore,
              "Bonus accounting preserves total score");
        check(scene.shotChanceStep > 0 && opening.enemyShotChance == 40,
              "First repeat starts the original probability ramp");
        for (int n = 0; n < 500; ++n) {
            opening.tick({});
            opening.prepare(prepared);
            opening.finishForeground();
        }
        check(opening.level == 10 && opening.coverageStop.empty() && scene.speed > 0,
              "Repeat campaign resumes scrolling and spawning");
        transition.level = 10;
        transitionScene.nextLevel();
        check(transition.level == 11 && transition.enemyShotChance == 110,
              "Later loop uses source level-dependent baseline");
        auto bonusesStorage = std::make_unique<World>();
        World &bonuses = *bonusesStorage;
        Scene bonusScene(bonuses);
        bonuses.blueTotal = bonuses.redTotal = 10;
        bonuses.blueRemaining = 0;
        bonuses.redRemaining = 10;
        bonusScene.beginBonus();
        check(bonusScene.blueBonus.remaining == 20000 && bonusScene.redBonus.remaining == 0,
              "Neutral blue champion bonus doubles 10000 to 20000; all-missed red gets zero");
        bonuses.blueRemaining = 2;
        bonuses.redRemaining = 0;
        bonusScene.beginBonus();
        check(bonusScene.blueBonus.remaining == 3333 && bonusScene.redBonus.remaining == 10000,
              "Miss penalty divides by misses plus one without unconditional blue doubling");
        std::cout << "Ten areas and bonus tallies completed in " << ticks
                  << " ticks; bosses defeated by test-only hit injection.\n";
        std::cout << "Super Ox Wars model checks passed: analog profiles, terrain/air families, "
                     "mine chains, pyramid budget and connected progression. All source areas "
                     "connect; frontend/records and session achievement checks passed.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
