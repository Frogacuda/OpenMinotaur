#include "stage_runtime.h"
#include "stage_presentation.h"
#include "stage_controls.h"
#include "stage_hud.h"
#include "name_generator.h"
#include "stage_session.h"
#include "stage_frontend.h"
#include "campaign_actors.h"
#include "neon/entity_list.h"
#ifdef GOATUP2_RESOURCE_CATALOG
#include "neon/resource_catalog.h"
#endif
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__func__) + ": " + #x);                           \
    } while (false)
using namespace goatup2;
namespace {
size_t count(StageActor *node) {
    size_t n = 0;
    for (; node; node = node->next)
        ++n;
    return n;
}
std::unique_ptr<StageRuntime> arena(bool water = false, bool floor = false) {
    TerrainBounds bounds;
    auto layout = calculateTerrainLayout(100, 100, 21, 40, {1, 1}, &bounds);
    std::vector<uint32_t> cells(10000, water ? 43U : 0U);
    if (floor)
        for (int column = 0; column < 100; ++column)
            cells[54 * 100 + column] = 0x30001U;
    auto runtime = std::make_unique<StageRuntime>(
        layout, bounds, cells, cells, StageAllocationInputs{0, 0, 0, false, 0},
        MediumSharedState{0, 0, layout.cellSize.y, 8, false}, 1);
    // Source sprite 107: supplied NeonResources.xml entry 109 is 30x26.
    runtime->spriteScales.resize(188, {1, 1});
    runtime->spriteScales[19] = {1, 20. / 29.};
    runtime->spriteScales[107] = {1, 26. / 30.};
    runtime->spriteScales[110] = {1, 40. / 117.};
    runtime->spriteScales[118] = {1, 114. / 127.};
    runtime->spriteScales[140] = {1, 34. / 22.};
    runtime->spriteScales[166] = {1, 20. / 85.}; // Original Reset atlas entry.
    runtime->spawnGoat({0, 0}, false, {1, 26. / 30.});
    return runtime;
}
void groundJumpAndEffects() {
    auto w = arena();
    for (int column = 0; column < 100; ++column)
        w->working[54 * 100 + column] = 0x30001U;
    // Binary32 half-width / widened reciprocal puts X=0 in physical column 49,
    // not the idealized mathematical column 50 (same source terrain rule).
    w->working[53 * 100 + 49] = 4;
    bool collected = false, jumped = false;
    size_t peak = 0;
    for (uint32_t tick = 0; tick < 160; ++tick) {
        const bool launch = tick == 80;
        w->advanceLocomotion(0, launch ? 1 : 0, tick, uint8_t(tick));
        CHECK(std::isfinite(w->goat->stagePosition.y) && !w->goat->hazard);
        if (launch) {
            CHECK(!w->goat->resting && w->goat->velocity.y < 0);
            jumped = true;
        }
        if (w->score > 0)
            collected = true;
        peak = std::max(peak, count(w->foregroundEffects));
    }
    if (!(collected && jumped && w->score == 1 && peak >= 5 && w->goat->resting))
        std::cerr << "ground run: score=" << w->score << " peak=" << peak
                  << " resting=" << w->goat->resting << " support=" << w->goat->supportColumn << ','
                  << w->goat->supportRow << " y=" << w->goat->stagePosition.y << '\n';
    CHECK(collected && jumped && w->score == 1 && peak >= 5 && w->goat->resting);
    CHECK(w->working[53 * 100 + 49] == 0x800006U && !w->foregroundEffects);
    CHECK(w->random.draws() == 36); // factory 2, one reward 34; no hidden adapter RNG
}
void headbuttBirthsRunAndRetire() {
    auto w = arena();
    w->goat->resting = false;
    w->sharedGrace = 5;
    const auto oldWorld = w->goat->worldPosition;
    // Inspect the source world-position copy before logical draw preparation
    // replaces it; the full frame API now performs that preparation too.
    w->jumpInput = 1;
    w->goat->run();
    w->advanceEffects();
    CHECK(w->headbuttEvent && count(w->ordinaryEffects) == 20 && w->random.draws() == 163);
    for (auto *p = w->ordinaryEffects; p; p = p->next) {
        CHECK(p->worldPosition.x == oldWorld.x && p->worldPosition.y == oldWorld.y);
        CHECK(p->colour.a < 1 && p->colour.a > .9); // newborns actually run in this frame
        CHECK(p->velocity.x != 0 && p->policy.solidCollisions);
    }
    w->preparePresentation();
    for (auto *p = w->ordinaryEffects; p; p = p->next) {
        const auto prepared = stageToWorld(p->stagePosition, w->gridOffset, w->layout, w->bounds);
        CHECK(p->worldPosition.x == prepared.x && p->worldPosition.y == prepared.y);
    }
    for (unsigned i = 0; i < 100; ++i)
        w->advanceEffects();
    CHECK(!w->ordinaryEffects && !w->foregroundEffects);
}
void waterBirthsAndListScheduling() {
    auto w = arena(true);
    w->goat->resting = false;
    w->goat->mediumEntryAge = 100;
    w->goat->bubbleThreshold = 4098; // force both original medium-visit bubble branches
    w->advanceLocomotion(0, 0, 0, 0);
    CHECK((w->goat->mediumFlags & 1U) && w->goat->mediumEntryAge == 1);
    CHECK(count(w->ordinaryEffects) == 50 && count(w->foregroundEffects) >= 2);
    unsigned ranBubbles = 0;
    for (auto *p = w->foregroundEffects; p; p = p->next) {
        CHECK(p->bubbleThreshold == -1);
        if (p->mediumEntryAge == 2)
            ++ranBubbles;
        else
            CHECK(p->mediumEntryAge == 0); // born from the later +3ac pass: deferred
    }
    CHECK(ranBubbles == 2);
    std::fill(w->working.begin(), w->working.end(), 0);
    w->advanceEffects();
    CHECK(!w->foregroundEffects);
    for (unsigned i = 0; i < 100; ++i)
        w->advanceEffects();
    CHECK(!w->ordinaryEffects);
}
void mirroredGravityAndInput() {
    auto w = arena();
    w->goat.reset();
    w->spawnGoat({0, 0}, true, {1, 26. / 30.});
    for (int column = 0; column < 100; ++column)
        w->working[46 * 100 + column] = 0x30001U;
    for (uint32_t tick = 0; tick < 80; ++tick)
        w->advanceLocomotion(0, 0, tick, uint8_t(tick));
    CHECK(w->goat->resting && !w->goat->hazard);
    w->advanceLocomotion(.1, 1, 80, 80);
    CHECK(w->goat->velocity.y > 0 && !w->goat->resting && w->goat->drawScale.x == -1);
    const auto last = w->goat->stagePosition.x;
    w->advanceLocomotion(.1, 2, 81, 81);
    CHECK(w->goat->stagePosition.x > last && w->goat->heldJumpTicks == 2);
}
void kidContactsHistoryAndHeadbutt() {
    auto w = arena();
    w->spriteScales.resize(188, {1, 1});
    w->spriteScales[19] = {1, 20. / 29.};
    w->spriteScales[140] = {1, 34. / 22.};
    for (int column = 0; column < 100; ++column)
        w->working[54 * 100 + column] = 0x30001U;
    for (uint32_t tick = 0; tick < 80; ++tick)
        w->advanceLocomotion(0, 0, tick, uint8_t(tick));
    w->spawnKid(w->goat->stagePosition, false);
    auto *older = w->contactObjects;
    w->spawnKid(w->goat->stagePosition, false);
    auto *newer = w->contactObjects;
    w->preparePresentation(); // source draw-ready flag precedes contact eligibility
    w->advanceLocomotion(0, 0, 80, 80);
    CHECK(w->goat->followingCount == 2 && w->multiplier == 3 && w->activeFollowers == 2);
    CHECK(older->headbuttState == 1 && older->serial == 2 && newer->serial == 1);
    CHECK(static_cast<KidActor *>(newer)->followDelay == 2);
    CHECK(w->random.draws() == 6); // two goat frequency and four kid damping draws
    CHECK(w->attemptHeadbutt());
    CHECK(older->dead && !newer->dead && w->multiplier == 2 && w->goat->followingCount == 1);
    CHECK(w->random.draws() == 74); // flash 3, ten radial children*6, fragment 5
    w->advanceEffects();
    CHECK(w->contactObjects == newer && newer->next == nullptr);
    CHECK(w->activeFollowers == 2); // source runs the already-dead kid before culling
    w->advanceEffects();
    CHECK(w->activeFollowers == 1);
    w->beginDeath(*w->goat);
    w->advanceLocomotion(0, 0, 81, 81);
    CHECK(w->goat->state == 1); // previous follower count gates restart
    for (uint32_t tick = 82; tick < 102 && w->goat->state == 1; ++tick)
        w->advanceLocomotion(0, 0, tick, uint8_t(tick));
    CHECK(!w->contactObjects && w->goat->state == 3 && w->lives == 2);
}
void deathRestartAndExitStates() {
    auto w = arena(false, true);
    auto &goat = *w->goat;
    goat.activeTicks = 25;
    goat.mediumFlags = 0x20;
    goat.changedMediumFlags = 0x40;
    goat.mediumEntryAge = 37;
    goat.forcedJumpSpeed = .777f;
    goat.emissionRemainder = .75f;
    w->multiplier = 7;
    goat.checkpoints.record({.125, .25}, -.0002f);
    w->working[5000] = 123; // mutation of working storage must not change reset baseline
    w->beginDeath(goat);
    CHECK(goat.state == 1 && count(w->foregroundEffects) == 202 && count(w->overlayEffects) == 2);
    CHECK(w->random.draws() == 1211 && w->textRowsToClear == std::vector<int32_t>({6, 8}));
    CHECK(w->sounds.back().kind == RuntimeSound::Kind::Stop && w->sounds.back().sample == 63);
    w->advanceLocomotion(0, 0, 0, 0);
    CHECK(goat.state == 3 && goat.invulnerability == 50 && w->lives == 2 && w->working[5000] == 0);
    for (uint32_t tick = 1; tick <= 50; ++tick)
        w->advanceLocomotion(0, 0, tick, uint8_t(tick));
    CHECK(goat.state == 0 && goat.invulnerability == 150 && w->multiplier == 1 &&
          w->random.draws() == 1213);
    CHECK(goat.stagePosition.x == .125 && goat.stagePosition.y == .25 && goat.gravity == -.0002f);
    CHECK(goat.mediumFlags == 0x20 && goat.changedMediumFlags == 0x40 && goat.mediumEntryAge == 37);
    CHECK(goat.activeTicks == 25 && goat.forcedJumpSpeed == .777f && goat.emissionRemainder == 0);
    goat.state = 2;
    goat.colour.a = .006;
    const auto oldPosition = goat.stagePosition;
    w->advanceLocomotion(.1, 1, 51, 51);
    w->advanceLocomotion(.1, 1, 52, 52);
    CHECK(goat.state == 4 && goat.colour.a == 0 && w->exitFadeTicks == 2);
    CHECK(goat.stagePosition.x == oldPosition.x && goat.stagePosition.y == oldPosition.y);
    w->advanceLocomotion(0, 0, 53, 53);
    CHECK(!w->playerVisible && w->lives == 2);
}
void pickupVariantsAndExit() {
    const int32_t awards[] = {200,  2000, 1000,  10000, 1000, 1000,
                              1000, 1000, 10000, 1000,  1000, 200};
    for (uint32_t variant = 0; variant < 12; ++variant) {
        auto w = arena();
        w->spawnPickup({.1, .1}, variant);
        auto *item = static_cast<PickupActor *>(w->contactObjects);
        item->hitBy(w->goat.get());
        CHECK(w->score == awards[variant] && item->headbuttState == 1);
        CHECK(w->goat->followingCount == (variant == 10 ? 0 : 1) && w->requiredFruit == 0);
        CHECK(w->lives == 3); // variant 10 grants energy/shield, not an extra life
        if (variant == 3)
            CHECK(w->sharedGrace == 1000);
        if (variant == 8)
            CHECK(w->headbuttDisabled && w->messages[0].ticks == 250);
        if (variant == 10)
            CHECK(item->dead && w->goat->invulnerability == 100);
    }
    auto gift = arena();
    gift->spawnPickup({0, 0}, 8);
    gift->contactObjects->hitBy(gift->goat.get());
    gift->spawnPickup({0, 0}, 0);
    gift->contactObjects->hitBy(gift->goat.get());
    CHECK(gift->score == 10600);
    gift->spawnPickup({0, 0}, 3);
    gift->contactObjects->hitBy(gift->goat.get());
    CHECK(!gift->headbuttDisabled && gift->sharedGrace == 0 && gift->score == 40600);
    CHECK(gift->messages[0].ticks == 150 && gift->sounds.back().sample == 74);
    auto reversible = arena();
    reversible->spawnPickup({.25, .5}, 9);
    auto *cart = reversible->contactObjects;
    cart->hitBy(reversible->goat.get());
    CHECK(reversible->attemptHeadbutt() && !cart->dead && cart->headbuttState == 2 &&
          cart->serial == -1);
    reversible->goat->state = 1;
    cart->run();
    CHECK(cart->headbuttState == 0 && cart->stagePosition.x == .25 && cart->stagePosition.y == .5);

    auto w = arena();
    w->spawnPickup({.2, 0}, 0);
    auto *fruit = w->contactObjects;
    w->spawnExit({0, 0});
    auto *exit = w->contactObjects;
    exit->run();
    CHECK(exit->headbuttState == 0);
    fruit->hitBy(w->goat.get());
    exit->run();
    CHECK(exit->headbuttState == 1);
    exit->hitBy(w->goat.get());
    CHECK(w->goat->state == 2 && w->exitBonus == 100 && w->score == 200);
    CHECK(count(w->overlayEffects) == 1 && !w->placementOverlays); // 0x5f480 inserts in +3b4
    for (uint32_t tick = 0; tick < 20; ++tick)
        w->advanceLocomotion(0, 0, tick, uint8_t(tick));
    CHECK(w->score == 1200 && w->exitBonus == 1100 && !w->terminal);
}
#ifdef GOATUP2_RESOURCE_CATALOG
void importedTerrain(const std::filesystem::path &assets, int bankIndex, int level,
                     bool includeKids, bool fullFamily) {
    CHECK(bankIndex >= 0 && bankIndex < 4 && level >= 0 && level < 20);
    std::ifstream input(assets / ("DEFAULT_" + std::to_string(bankIndex)), std::ios::binary);
    CHECK(input.good());
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    Bank bank;
    std::string error;
    CHECK(readBank(bytes, bank, error));
    std::ifstream xml(assets / "NeonResources.xml");
    CHECK(xml.good());
    neon::ResourceCatalog resources;
    CHECK(resources.load({std::istreambuf_iterator<char>(xml), {}}, &error));
    const auto *sprite = resources.sprite(109);
    CHECK(sprite && sprite->w > 0);
    auto &stage = bank.stages[level];
    PlacementTable placements;
    placements.load(stage);
    const Placement *spawn = nullptr;
    unsigned unsupported = 0;
    for (const auto &p : placements.entries())
        if (p.type == 0)
            spawn = &p;
        else if (p.type >= 0 && !(includeKids && p.type == 1) &&
                 !(fullFamily &&
                   (p.type == 2 || p.type == 3 || p.type == 4 || p.type == 8 || p.type == 9)))
            ++unsupported;
    CHECK(spawn);
    TerrainBounds bounds;
    auto layout = calculateTerrainLayout(stage.declaredColumns, stage.declaredRows,
                                         stage.displayWidth, stage.displayHeight, {1, 1}, &bounds);
    // Diagnostic terrain run, NOT full stage setup/RNG order or object factory.
    auto saved = stage.cells;
    StageRuntime runtime(layout, bounds, stage.cells, std::move(saved), {0, 0, 0, false, 0},
                         {0, 0, layout.cellSize.y, 8, true}, uint32_t(stage.seed));
    for (int i = 0; i < resources.spriteCount() - 2; ++i) {
        const auto *entry = resources.sprite(i + 2);
        runtime.spriteScales.push_back({1, entry->w > 0 ? double(entry->h) / entry->w : 1.});
    }
    runtime.stageIndex = bankIndex * 20 + level;
    if (fullFamily)
        runtime.spawnOriginalObjects(placements);
    else
        runtime.spawnGoat({spawn->x, spawn->y}, spawn->flags != 0,
                          {1, double(sprite->h) / sprite->w});
    if (includeKids && !fullFamily)
        for (const auto &p : placements.entries())
            if (p.type == 1)
                runtime.spawnKid({p.x, p.y}, p.flags != 0);
    StagePresentation presentation;
    presentation.attach(runtime, stage.displayWidth, stage.displayHeight);
    runtime.preparePresentation();
    unsigned steps = 0;
    for (; steps < 240; ++steps) {
        presentation.requestCamera(runtime);
        updateStagePalette(runtime, steps);
        runtime.advanceLocomotion(0, 0, steps, uint8_t(steps));
        presentation.prepare(runtime);
        CHECK(std::isfinite(runtime.goat->stagePosition.x) &&
              std::isfinite(runtime.goat->stagePosition.y));
        for (const auto &d : presentation.draws())
            CHECK(std::isfinite(d.position.x) && std::isfinite(d.position.y));
        if (runtime.goat->hazard || runtime.exitContact || runtime.goat->state != 0)
            break;
    }
    std::cout << (fullFamily    ? "STAGE OBJECT FAMILY DIAGNOSTIC: bank "
                  : includeKids ? "GOAT/KID FAMILY DIAGNOSTIC: bank "
                                : "TERRAIN-ONLY DIAGNOSTIC: bank ")
              << bankIndex << " level " << level
              << ", other placements not instantiated: " << unsupported << ", frames " << steps
              << ", position " << runtime.goat->stagePosition.x << ','
              << runtime.goat->stagePosition.y << ", state " << runtime.goat->state << ", hazard "
              << runtime.goat->hazard << ", exit contact " << runtime.exitContact << '\n';
}
#endif
void scrollingHistoryAndPresentation() {
    auto w = arena(false, true);
    w->gridOffset = {-2, -3};
    w->goat->stagePosition = {.1, .2};
    w->goat->sinkOffset = .03f;
    const auto before = w->goat->stagePosition;
    w->goat->preparePresentation();
    const auto sample = w->goat->followerHistory[0];
    // Source vector subtraction/addition, independent of the screen transform.
    CHECK(sample.position.x == before.x - (-2 * w->layout.displayReciprocal.x));
    CHECK(sample.position.y == before.y - (-3 * w->layout.displayReciprocal.y) + double(.03f));
    w->spawnKid({0, 0}, false);
    auto *kid = w->contactObjects;
    kid->followDelay = 1;
    kid->targetDelay = 1;
    kid->followGoat();
    CHECK(kid->stagePosition.x == sample.position.x + (-2 * w->layout.displayReciprocal.x));
    StagePresentation presentation;
    presentation.attach(*w, 21, 40);
    presentation.requestCamera(*w);
    w->goat->flashTicks = 4;
    kid->freezeTicks = 2;
    const auto cursor = w->goat->followerCursor;
    const auto random = w->random.draws();
    presentation.prepare(*w);
    CHECK(w->goat->followerCursor == cursor && w->random.draws() == random &&
          kid->freezeTicks == 1);
    CHECK(!presentation.draws().empty() && presentation.terrainFrames == 1);
    CHECK(presentation.draws().size() > 50); // fifty history calls, live goat, terrain
    // Re-reading prepared draw packets must not repeat any logical mutations.
    const auto count = presentation.draws().size();
    CHECK(presentation.draws().size() == count && presentation.terrainFrames == 1);
}
void nativeAndTouchControls() {
    using namespace neon::input;
    CHECK(StageControls::gentle(.5f) == .25f && StageControls::gentle(-.5f) == -.25f);
    auto w = arena();
    StageControls controls;
    ControllerState pad;
    pad.connected = true;
    pad.lx = .5f;
    controls.pads(&pad, 1);
    auto intent = controls.tick(*w);
    CHECK(intent.movement == .0225 && intent.jump == 0);
    pad.lx = 0;
    pad.buttons = Right | South;
    controls.pads(&pad, 1);
    intent = controls.tick(*w);
    CHECK(intent.jump == 1);
    pad.buttons = Left;
    controls.pads(&pad, 1);
    intent = controls.tick(*w);
    CHECK(intent.jump == 0); // reversed d-pad resets its X ramp before stepping
    controls.cancel();
    controls.pads(&pad, 1);
    CHECK(controls.tick(*w).movement == 0);
    pad = {};
    controls.pads(&pad, 1);
    CHECK(controls.tick(*w).movement == 0);
    controls.touch(TouchPhase::Down, 11, {-.3, .1});
    controls.touch(TouchPhase::Move, 11, {-.2, .1});
    controls.touch(TouchPhase::Down, 12, {.4, .1});
    controls.touch(TouchPhase::Down, 13, {.35, .2});
    intent = controls.tick(*w);
    CHECK(intent.movement > 0 && intent.movement < .091 && intent.jump == 1);
    CHECK(controls.tick(*w).jump == 2); // two jump fingers still advance once
    controls.touch(TouchPhase::Up, 12, {.4, .1});
    CHECK(controls.tick(*w).jump == 3);
    controls.touch(TouchPhase::Cancel, 13, {.35, .2});
    CHECK(controls.tick(*w).jump == 0);
    controls.touch(TouchPhase::Up, 11, {-.2, .1});
    controls.cancel();
    controls.tick(*w);
    controls.hand = TouchHand::MovementRight;
    controls.touch(TouchPhase::Down, 21, {.3, .1});
    controls.touch(TouchPhase::Move, 21, {.2, .1});
    controls.touch(TouchPhase::Down, 22, {-.4, .1});
    intent = controls.tick(*w);
    CHECK(intent.movement < 0 && intent.jump == 1);
    StageControls tilt;
    tilt.motion({true, .4, -.9, 0}, true);
    const double expected = double(float(double(float(-.4 * .5)) * .5)) * .7;
    CHECK(tilt.tick(*w).movement == expected);
    tilt.touch(TouchPhase::Down, 31, {-.4, 0});
    CHECK(tilt.tick(*w).jump == 1);
    tilt.motion({}, true);
    tilt.touch(TouchPhase::Up, 31, {-.4, 0});
    CHECK(std::isfinite(tilt.tick(*w).movement)); // Missing sensor safely restores touch producer.
}
void touchGestureOwnership() {
    using namespace neon::input;
    for (auto hand : {TouchHand::MovementLeft, TouchHand::MovementRight}) {
        auto w = arena();
        StageControls controls;
        controls.hand = hand;
        const bool left = hand == TouchHand::MovementLeft;
        controls.touch(TouchPhase::Down, 41, left ? neon::vec2d{-.3, .1} : neon::vec2d{.3, .1});
        controls.touch(TouchPhase::Move, 41, left ? neon::vec2d{.49, .1} : neon::vec2d{.45, -.49});
        CHECK(controls.tick(*w).movement > 0);
        // A slight reversal starts the source floating-anchor drift. Holding
        // here moves that anchor across X=.2 or Y=-.2 without another event.
        controls.touch(TouchPhase::Move, 41, left ? neon::vec2d{.48, .1} : neon::vec2d{.44, -.48});
        for (int tick = 0; tick < 16; ++tick) {
            const auto intent = controls.tick(*w);
            CHECK(intent.jump == 0 && intent.movement > .001);
        }
        controls.touch(TouchPhase::Down, 42, left ? neon::vec2d{.4, .1} : neon::vec2d{-.4, .1});
        CHECK(controls.tick(*w).jump == 1);
        controls.touch(TouchPhase::Move, 42, left ? neon::vec2d{-.4, .1} : neon::vec2d{.4, -.4});
        CHECK(controls.tick(*w).jump == 2); // Jump role survives crossing regions too.
        controls.touch(TouchPhase::Cancel, 41, {.43, .41});
        CHECK(!controls.ownsTouch(41) && controls.ownsTouch(42));
        CHECK(controls.tick(*w).jump == 3);
        controls.touch(TouchPhase::Up, 42, {0, 0});
        CHECK(controls.tick(*w).jump == 0);
        controls.touch(TouchPhase::Down, 41, left ? neon::vec2d{.4, .1} : neon::vec2d{-.4, .1});
        CHECK(controls.tick(*w).jump == 1); // Reused ID/slot gets its new role.
        controls.cancel();
        CHECK(!controls.anyReportInput() && !controls.ownsTouch(41));
        CHECK(controls.tick(*w).movement == 0);
    }
    auto w = arena();
    StageControls controls;
    controls.touch(TouchPhase::Down, 1, {-.3, .1});
    controls.touch(TouchPhase::Down, 2, {-.2, .1});
    controls.touch(TouchPhase::Move, 1, {-.1, .1});
    controls.touch(TouchPhase::Move, 2, {0, .1});
    controls.touch(TouchPhase::Down, 3, {.4, .1});
    controls.touch(TouchPhase::Down, 4, {.35, .2});
    controls.touch(TouchPhase::Down, 5, {.3, .1});
    CHECK(!controls.ownsTouch(5));
    CHECK(controls.tick(*w).movement > 0);
    controls.touch(TouchPhase::Up, 1, {-.1, .1});
    controls.touch(TouchPhase::Down, 2,
                   {.4, .1}); // Duplicate Down cannot steal an active ID's role.
    auto intent = controls.tick(*w);
    CHECK(intent.movement > 0 && intent.jump == 2);
    controls.touch(TouchPhase::Cancel, 3, {0, 0});
    controls.touch(TouchPhase::Up, 4, {0, 0});
    intent = controls.tick(*w);
    CHECK(intent.movement > 0 && intent.jump == 0);
    controls.touch(TouchPhase::Up, 2, {0, .1});
    CHECK(!controls.anyReportInput());
}
void touchGestureSteadyHold() {
    using namespace neon::input;
    for (auto hand : {TouchHand::MovementLeft, TouchHand::MovementRight})
        for (int direction : {-1, 1})
            for (double jitter : {0., .001, .004, .01}) {
                auto w = arena();
                StageControls controls;
                controls.hand = hand;
                const double start = hand == TouchHand::MovementLeft ? -.3 : .3;
                const double end = start + direction * .15, retreat = end - direction * .01;
                controls.touch(TouchPhase::Down, 71, {start, .1});
                controls.touch(TouchPhase::Move, 71, {end, .1});
                CHECK(controls.tick(*w).movement * direction > .01);
                controls.touch(TouchPhase::Move, 71, {retreat, .1});
                controls.touch(TouchPhase::Down, 72,
                               {hand == TouchHand::MovementLeft ? .4 : -.4, .1});
                for (int tick = 0; tick < 600; ++tick) {
                    if (jitter)
                        controls.touch(TouchPhase::Move, 71,
                                       {retreat + (tick % 2 ? jitter : -jitter),
                                        .1 + (tick % 2 ? jitter : -jitter)});
                    const auto intent = controls.tick(*w);
                    CHECK(controls.ownsTouch(71) && intent.movement * direction > .01 &&
                          intent.jump > 0);
                }
                controls.touch(TouchPhase::Up, 71, {retreat, .1});
                StageIntent released;
                for (int tick = 0; tick < 20; ++tick)
                    released = controls.tick(*w);
                CHECK(std::fabs(released.movement) < 1e-10 && released.jump > 0);
            }
}
void originalHudPhases() {
    CHECK(StageHud::timeText("TIME", 59) == "TIME: 00:00:98");
    CHECK(StageHud::timeText("TIME", 3601) == "TIME: 01:00:01");
    auto w = arena();
    w->score = 123;
    w->stageTicks = 60;
    w->messages[0] = {"SOURCE MESSAGE", 2, 8, 3};
    StageHud hud;
    const auto draws = w->random.draws();
    hud.prepare(*w, 108000, 32);
    CHECK(w->random.draws() == draws && w->messages[0].ticks == 1);
    bool score = false, message = false;
    for (const auto &line : hud.text) {
        score = score || (line.row == 2 && line.column == 1 && line.text == "123     " &&
                          line.colour.r == 1 && line.colour.g == 1);
        message = message || line.text == "SOURCE MESSAGE";
    }
    CHECK(score && message);
    w->score = 999; // prepared text retains the pre-update score, not a live reference
    for (const auto &line : hud.text)
        if (line.row == 2 && line.column == 1)
            CHECK(line.text == "123     ");
}
void originalNamesAndStageEntry() {
    std::ifstream input(std::filesystem::path(GU2_TEST_FIXTURE_DIR) / "frontend-names.tsv");
    CHECK(input.good());
    uint32_t seed, state;
    int mode, count = 0;
    while (input >> seed >> mode >> state) {
        std::string expected;
        std::getline(input, expected);
        if (!expected.empty() && expected.back() == '\r')
            expected.pop_back();
        CHECK(!expected.empty());
        expected.erase(0, 1);
        neon::DarwinRandom random(77);
        CHECK(generateOriginalName(random, seed, mode ? NameMode::Bank : NameMode::Level) ==
              expected);
        CHECK(random.state() == state);
        ++count;
    }
    CHECK(count == 84); // existing independent original-word witnesses, not original execution
    neon::DarwinRandom random(5);
    CHECK(generateOriginalName(random, 0, NameMode::Level) == "* NAMELESS THING *");
    CHECK(random.state() == 5 && random.draws() == 0);
    CHECK(generateOriginalName(random, 1508545802, NameMode::Level) == "Tiny Fursuit of Wales");
    CHECK(random.state() == 214459759U);
}
void sourceStarfieldIntegration() {
    neon::DarwinRandom random(17);
    StageStarfield stars;
    stars.reset(random);
    CHECK(random.draws() == 300 && stars.points().size() == 100);
    CHECK(std::count_if(stars.points().begin(), stars.points().end(),
                        [](const auto &p) { return p.sprite == 179; }) == 1);
    CHECK(std::count_if(stars.points().begin(), stars.points().end(),
                        [](const auto &p) { return p.sprite == 178; }) == 1);
    auto w = arena();
    w->terrainPalette[2] = {.1, .2, .3, .4};
    StagePresentation presentation;
    presentation.stars = stars;
    const auto before = random.state();
    presentation.prepare(*w);
    CHECK(random.state() == before && stars.cameraDepth() == 0);
    std::vector<StageDraw> quads;
    stars.append(quads, w->terrainPalette);
    CHECK(!quads.empty());
    bool whitePoint = false, edgeFade = false;
    for (const auto &d : quads) {
        CHECK(d.position.x > -.6 && d.position.x < .6 && d.position.y > -.6 && d.position.y < .6);
        if (d.frame == 4) {
            CHECK(d.colour.r == 1 && d.colour.g == 1 && d.colour.b == 1);
            whitePoint = true;
            edgeFade |= d.colour.a < 1.;
        }
    }
    CHECK(whitePoint && edgeFade);
    stars.setCamera({.1, -.2});
    std::vector<StageDraw> shifted;
    stars.append(shifted, w->terrainPalette);
    CHECK(!shifted.empty() && (shifted.front().position.x != quads.front().position.x ||
                               shifted.front().position.y != quads.front().position.y));
    // A camera request changes the persistent star owner, not star coordinates
    // or RNG. Preview/title keep that owner until the next camera request.
    presentation.stars = stars;
    w->goat->stagePosition = {.2, -.4};
    presentation.requestCamera(*w);
    std::vector<StageDraw> requested;
    presentation.stars.append(requested, w->terrainPalette);
    CHECK(requested.size() == shifted.size() &&
          requested.front().position.x == shifted.front().position.x &&
          requested.front().position.y == shifted.front().position.y);
    stars.advance(true);
    CHECK(stars.cameraDepth() == 0);
    for (int i = 0; i < 1000; ++i)
        stars.advance(false);
    CHECK(stars.cameraDepth() < 0 &&
          stars.cameraDepth() > -1.); // source strict >1 wrap, f64 accumulation
    std::ifstream original(std::filesystem::path(GU2_TEST_FIXTURE_DIR) / "stars-original.tsv");
    CHECK(original.good());
    int steps, n, scenarios = 0;
    double cx, cy;
    while (original >> steps >> cx >> cy >> n) {
        neon::DarwinRandom seed(17);
        StageStarfield observed;
        observed.reset(seed);
        observed.setCamera({cx, cy});
        for (int i = 0; i < steps; ++i)
            observed.advance(false);
        std::vector<StageDraw> actual;
        observed.append(actual, w->terrainPalette);
        CHECK(int(actual.size()) == n);
        for (const auto &d : actual) {
            int frame;
            std::array<double, 8> expected;
            CHECK(bool(original >> frame));
            for (auto &v : expected)
                CHECK(bool(original >> v));
            CHECK(frame == d.frame);
            const std::array<double, 8> values = {d.position.x, d.position.y, d.size.x,
                                                  d.size.y,     d.colour.r,   d.colour.g,
                                                  d.colour.b,   d.colour.a};
            for (size_t i = 0; i < 8; ++i)
                CHECK(std::abs(values[i] - expected[i]) < 1e-12);
        }
        ++scenarios;
    }
    CHECK(original.eof() && scenarios == 4);
}
void currentOwnerAndDrawing() {
    auto w = arena();
    std::ifstream input(std::filesystem::path(GU2_TEST_FIXTURE_DIR) / "currents-original.tsv");
    CHECK(input.good());
    auto store = [](float &f, uint32_t bits) { std::memcpy(&f, &bits, 4); };
    auto bits = [](float f) {
        uint32_t b;
        std::memcpy(&b, &f, 4);
        return b;
    };
    std::array<uint32_t, 8> row;
    int count = 0;
    while (input >> row[0]) {
        for (int i = 1; i < 8; ++i)
            CHECK(bool(input >> row[size_t(i)]));
        store(w->downwardCurrentPhase, row[0]);
        store(w->medium.downwardRamp, row[1]);
        store(w->upwardCurrentPhase, row[2]);
        store(w->medium.upwardRamp, row[3]);
        w->advanceCurrents();
        CHECK(bits(w->downwardCurrentPhase) == row[4] && bits(w->medium.downwardRamp) == row[5]);
        CHECK(bits(w->upwardCurrentPhase) == row[6] && bits(w->medium.upwardRamp) == row[7]);
        ++count;
    }
    CHECK(input.eof() && count == 80);
    w->resetCurrents();
    CHECK(w->medium.downwardRamp == .0625f && w->medium.upwardRamp == .0625f);
    w->working[10 * 100 + 10] = 46;
    w->working[10 * 100 + 11] = 47;
    w->working[10 * 100 + 12] = 0x20000c;
    w->working[10 * 100 + 13] = 0x20000d;
    StagePresentation p;
    // Preview covers the complete template. A retained redraw must not advance
    // the session clock; a paused logical tick still advances these UVs.
    p.prepareMenu(*w, {}, true);
    auto initial = p.draws();
    w->originalPauseFlag = true;
    const auto random = w->random.state();
    w->advanceCurrents();
    p.prepareMenu(*w, {}, true);
    const auto &advanced = p.draws();
    int changed = 0;
    for (size_t i = 0; i < initial.size(); ++i)
        if (initial[i].tile)
            changed += initial[i].u != advanced[i].u || initial[i].v != advanced[i].v;
    CHECK(changed == 4 && w->random.state() == random);
    p.prepareMenu(*w, {}, true);
    CHECK(w->downwardCurrentPhase == .0625f);
    w->originalPauseFlag = false;
    w->gridOffset.y = -10;
    w->working[50] = 0x01840002;            // above view: source outer loop skips it
    w->working[90 * 100 + 50] = 0x1e840002; // below view: expires through 0x506c0
    p.prepare(*w);
    CHECK(w->working[50] == 0x01840002 && w->working[90 * 100 + 50] == 0);
    w->gridOffset = {0, 0};
    w->originalPauseFlag = true;
    w->working[10 * 100 + 10] = w->working[90 * 100 + 50] = 0x20840002;
    p.prepare(*w);
    CHECK(w->working[10 * 100 + 10] == 0 && w->working[90 * 100 + 50] == 0x20840002);
    std::fill(w->working.begin(), w->working.end(), 0);
    w->gridOffset = {0, 0};
    w->working[10 * 100 + 10] = 0x400001;
    w->terrainPalette[0] = {.2, .4, .6, .8};
    p.prepareEditor(*w, {}, true);
    CHECK(p.draws().empty()); // overview omits editor guides
    p.prepareEditor(*w, {}, false);
    bool hiddenGuide = false;
    for (const auto &d : p.draws())
        if (d.tile && d.colour.r == .2 && d.colour.g == .4) {
            CHECK(d.u == 1. / 512. && d.v == 420. / 512.);
            hiddenGuide = true;
        }
    CHECK(hiddenGuide); // hidden wall gets glyph zero and the cycling ink
}
void devicesEnemiesAndRewards() {
    // Existing independent table/numeric witnesses, not expectations copied
    // from this implementation. Include constructor RNG draws for zero speeds.
    std::ifstream input(std::filesystem::path(GU2_TEST_FIXTURE_DIR) / "enemy-constructors.tsv");
    CHECK(input.good());
    int kind, flags, n = 0;
    double vx, vy, mx, my;
    uint32_t gravity, state;
    uint64_t draws;
    while (input >> kind >> flags >> vx >> vy >> mx >> my >> gravity >> state >> draws) {
        auto w = arena();
        EnemyActor enemy(*w, {0, 0}, kind, uint32_t(flags));
        uint32_t bits;
        std::memcpy(&bits, &enemy.gravity, 4);
        CHECK(enemy.velocity.x == vx && enemy.velocity.y == vy);
        CHECK(enemy.maximumSpeed.x == mx && enemy.maximumSpeed.y == my);
        CHECK(bits == gravity && w->random.state() == state && w->random.draws() == draws);
        CHECK(enemy.contactHealth == 1);
        ++n;
        CHECK(enemy.policy.automaticVerticalHandling == (kind == 4 || (kind == 8 && flags == 0)));
    }
    CHECK(n == 15);
    auto w = arena(false, true);
    auto &goat = *w->goat;
    DeviceActor checkpoint(*w, {.1, .2}, 4, 17);
    goat.gravity = -.0002f;
    checkpoint.hitBy(&goat);
    CHECK(goat.checkpoints.cursor == 0);
    goat.gravity = .0002f;
    checkpoint.hitBy(&goat);
    CHECK(goat.checkpoints.cursor == 1 && goat.lastCheckpointDevice == 17);
    checkpoint.hitBy(&goat);
    CHECK(goat.checkpoints.cursor == 1);
    goat.resetFromCheckpoint();
    CHECK(goat.stagePosition.x == .1 && goat.stagePosition.y == .2);
    CHECK(goat.lastCheckpointDevice == -1);
    DeviceActor flip(*w, {0, 0}, 1, 18);
    flip.hitBy(&goat);
    CHECK(goat.gravity < 0 && flip.stageDrawScale == 2.5f && flip.radius == .055f);
    const float g = goat.gravity;
    flip.hitBy(&goat);
    CHECK(goat.gravity == g);
    flip.run();
    CHECK(flip.headbuttState == -1);
    flip.run();
    CHECK(flip.headbuttState == 0);
    EnemyActor enemy(*w, {0, 0}, 4, 0);
    goat.invulnerability = 30;
    goat.hazard = false;
    enemy.hitBy(&goat);
    CHECK(enemy.dead && !goat.hazard && w->score == 1000);
    auto *fruit = dynamic_cast<DroppedFruitActor *>(w->contactObjects);
    CHECK(fruit && fruit->category == 6);
    const float beforeHunger = goat.hunger;
    fruit->hitBy(&goat);
    CHECK(w->score == 1500 && fruit->headbuttState == 2 && goat.hunger > beforeHunger &&
          goat.followingCount == 0);
    fruit->hitBy(&goat);
    CHECK(w->score == 1500);
    EnemyActor machine(*w, {0, 0}, 8, 2);
    goat.stagePosition = {.2, 0};
    machine.hitBy(&goat);
    CHECK(!machine.dead);
    goat.stagePosition = {0, 0};
    machine.hitBy(&goat);
    CHECK(machine.dead && w->score == 2500);
    EnemyActor launcher(*w, {0, 0}, 8, 4);
    const auto random = w->random.draws();
    launcher.hitBy(&goat);
    CHECK(goat.forcedJumpTicks == 30 && goat.forcedJumpSpeed < 0);
    CHECK(launcher.launchCooldown == 40 && w->random.draws() == random + 4 && !launcher.dead);
    launcher.hitBy(&goat);
    CHECK(w->random.draws() == random + 4);
    launcher.run();
    CHECK(launcher.launchCooldown == 38); // source double decrement
    auto portals = arena();
    auto &p = *portals->goat;
    p.worldPosition = {0, 0};
    auto *source = new DeviceActor(*portals, {0, 0}, 2, 0);
    auto *first = new DeviceActor(*portals, {1, 0}, 3, 1);
    auto *second = new DeviceActor(*portals, {-1, 0}, 3, 2);
    for (auto *d : {source, first, second})
        neon::entities::prepend<StageActor>(portals->contactObjects, *d);
    source->worldPosition = {0, 0};
    first->worldPosition = {1, 0};
    second->worldPosition = {-1, 0};
    p.gravity = -.0002f;
    p.velocity = {.003, .004};
    source->hitBy(&p);
    CHECK(p.stagePosition.x == -1 && p.gravity == -.0002f && p.velocity.x == .003);
    CHECK(second->headbuttState == -1 && first->headbuttState == 0 && p.invulnerability == 30);
    CHECK(portals->cameraSettleFrames == 10);
}
void platformRidingAndEnemyProjectiles() {
    auto w = arena();
    auto &goat = *w->goat;
    EnemyActor platform(*w, {0, 0}, 8, 9);
    platform.worldPosition = {0, 0};
    goat.stagePosition = {0, -.02};
    goat.worldPosition = {0, -.02};
    goat.resting = false;
    platform.hitBy(&goat);
    CHECK(!goat.riding && !platform.riderAttached);
    goat.resting = true;
    platform.hitBy(&goat);
    CHECK(goat.riding == &platform && platform.riderAttached && goat.stagePosition.y == -.04 &&
          goat.velocity.y == 0);
    // Carry uses the previous platform velocity, without replacing the goat's
    // own velocity. A held value 2 must not execute the first-press detach.
    goat.zoom = 0;
    goat.gravity = 0;
    goat.velocity = {0, .001};
    w->jumpInput = 2;
    const auto before = goat.stagePosition;
    goat.run();
    CHECK(goat.riding == &platform &&
          std::abs(goat.stagePosition.y - before.y - platform.velocity.y) < 1.e-12);
    w->jumpInput = 1;
    goat.run();
    CHECK(!goat.riding && !platform.riderAttached);
    goat.resting = true;
    goat.stagePosition = {0, 0};
    goat.worldPosition = {0, 0};
    platform.hitBy(&goat);
    CHECK(goat.riding == &platform);
    goat.stagePosition.x =
        platform.stagePosition.x + platform.drawScale.x * double(platform.radius) * 1.5 + .001;
    w->jumpInput = 0;
    goat.run();
    CHECK(!goat.riding && !platform.riderAttached);
    const auto original = platform.stagePosition;
    platform.run();
    CHECK(platform.stagePosition.y == original.y + platform.velocity.y);
    platform.stagePosition.y = double(w->bounds.halfHeight) + .1;
    platform.velocity.y = .003;
    platform.run();
    CHECK(platform.velocity.y < 0); // reflect, do not wrap or retire

    auto firing = arena();
    EnemyActor gun(*firing, {0, 0}, 4, 6);
    gun.gravity = .00035f;
    firing->goat->stagePosition = {0, -.2};
    firing->animationFrameFlags = 6;
    gun.run();
    CHECK(!firing->enemies);
    firing->animationFrameFlags = 7;
    const auto beforeDraws = firing->random.draws();
    gun.run();
    auto *shot = dynamic_cast<EnemyShotActor *>(firing->enemies);
    CHECK(shot);
    CHECK(firing->random.draws() == beforeDraws + 3 && shot->lifetime == 1000);
    uint32_t bits;
    std::memcpy(&bits, &shot->gravity, 4);
    CHECK(bits == 0x3892ccf6U);
    const auto start = shot->stagePosition;
    shot->lifetime = 0;
    shot->run();
    CHECK(shot->dead && shot->lifetime == -1 && shot->stagePosition.y != start.y);
    auto cannonWorld = arena();
    auto *cannon = new EnemyActor(*cannonWorld, {0, 0}, 8, 0);
    neon::entities::prepend<StageActor>(cannonWorld->enemies, *cannon);
    const auto cannonDraws = cannonWorld->random.draws();
    cannonWorld->advanceEffects();
    auto *ball = dynamic_cast<CannonballActor *>(cannonWorld->enemies);
    CHECK(ball);
    CHECK(cannon->attackCooldown == 500 && ball->bounces == 10 && !ball->hasLeftWater);
    CHECK(cannonWorld->random.draws() == cannonDraws + 3); // prepend birth waits for next list pass
    std::memcpy(&bits, &ball->gravity, 4);
    CHECK(bits == 0x3892ccf6U);
    cannonWorld->advanceEffects();
    CHECK(cannon->attackCooldown == 499 && ball->hasLeftWater);
    CHECK(ball->lifetime == 500); // no arbitrary dry-air timeout
    // A landed ball retains the pre-fall narrowed speed for the source bounce.
    ball->velocity.y = .01;
    ball->landed = true;
    const int bounces = ball->bounces;
    ball->run();
    CHECK(ball->bounces == bounces - 1 && ball->velocity.y == double(-.005f) && !ball->resting);
    cannonWorld->goat->invulnerability = 0;
    cannonWorld->goat->hazard = false;
    ball->hitBy(cannonWorld->goat.get());
    CHECK(ball->dead && cannonWorld->goat->hazard && cannonWorld->score == 0);
}
void laterMachineFamilies() {
    CHECK(originalSteeringStep({0, 0}, {1, 0}, .01f, 0) == -.01f);
    CHECK(originalSteeringStep({0, 0}, {-1, 0}, .01f, 0) == .01f);
    CHECK(originalSteeringStep({0, 0}, {0, -1}, .01f, 0) == 0);
    CHECK(originalSteeringStep({0, 0}, {1, 0}, .01f, 3.1f) == .01f); // wrap across pi
    auto w = arena();
    auto &goat = *w->goat;
    EnemyActor charger(*w, {0, 0}, 4, 4);
    charger.velocity.x = 0;
    charger.maximumSpeed.x = .01;
    charger.freezeTicks = 1;
    goat.stagePosition = {.02, .01};
    charger.run();
    CHECK(charger.charging && charger.velocity.x == .001);
    int sounds = 0;
    for (const auto &event : w->sounds)
        sounds += event.sample == 60;
    CHECK(sounds == 1);
    goat.stagePosition.y = 1;
    charger.run();
    CHECK(!charger.charging && charger.velocity.x == .001 * .95);
    EnemyActor tracker(*w, {0, 0}, 7, 3);
    tracker.originalVelocity = {0, -.005};
    goat.stagePosition = {1, 0};
    tracker.run();
    CHECK(tracker.steeringAngle == -.01f && tracker.velocity.x > 0);
    EnemyActor shooter(*w, {0, 0}, 7, 4);
    shooter.attackCooldown = 1;
    goat.stagePosition = {0, -1};
    const auto before = w->random.draws();
    shooter.run();
    auto *aimed = dynamic_cast<EnemyShotActor *>(w->enemies);
    CHECK(aimed && shooter.attackCooldown == 60);
    CHECK(w->random.draws() == before && aimed->gravity == 0 &&
          !aimed->policy.automaticVerticalHandling);
    CHECK(aimed->savedDrawScale.x == aimed->drawScale.x * .5);
    auto bombWorld = arena();
    EnemyActor bomber(*bombWorld, {0, 0}, 5, 0);
    bombWorld->goat->stagePosition = {0, -1};
    const auto bombDraws = bombWorld->random.draws();
    bomber.run();
    auto *bomb = dynamic_cast<EnemyBombActor *>(bombWorld->enemies);
    CHECK(bomb && bomber.attackCooldown == 120);
    CHECK(bombWorld->random.draws() == bombDraws && bomb->velocity.y == 0 &&
          bomb->lifetime == 1000);
    EnemyActor laser(*w, {0, 0}, 8, 3);
    laser.stagePosition.y = .2;
    goat.stagePosition = {0, .1};
    goat.hazard = false;
    laser.run();
    CHECK(goat.hazard); // tether is independently lethal
    laser.cycleState = 2;
    laser.stagePosition.y = .001;
    goat.stagePosition.x = 1;
    laser.run();
    CHECK(laser.cycleState == 0 && laser.velocity.y == 0);
    EnemyActor falling(*w, {0, 0}, 8, 8);
    falling.velocity.y = .01;
    falling.gravity = 0;
    falling.verticalContact = true;
    const auto burstDraws = w->random.draws();
    const auto particles = count(w->ordinaryEffects);
    falling.run();
    CHECK(falling.dead && count(w->ordinaryEffects) == particles + 30 &&
          w->random.draws() == burstDraws + 180);
    EnemyActor wrapper(*w, {0, 0}, 6, 7);
    wrapper.stagePosition.y = double(w->bounds.halfHeight) + .1;
    wrapper.run();
    CHECK(wrapper.stagePosition.y == -double(w->bounds.halfHeight));
}
void secondCampaignFamilies() {
    // +17c/+184 feed both steering limits and water hopping in 0x58680.
    auto w = arena();
    EnemyActor swimmer(*w, {0, 0}, 4, 1);
    CHECK(&swimmer.maximumSpeed.x == &swimmer.hopSpeedX &&
          &swimmer.maximumSpeed.y == &swimmer.hopSpeedY);
    swimmer.mediumFlags = 1;
    swimmer.hopCounter = swimmer.hopPeriod;
    swimmer.policy.solidCollisions = false;
    const auto before = w->random.draws();
    swimmer.moveHorizontal();
    CHECK(swimmer.velocity.x == -swimmer.maximumSpeed.x &&
          swimmer.velocity.y == -swimmer.maximumSpeed.y);
    CHECK(swimmer.velocity.y < 0 && swimmer.hopCounter == 0 && w->random.draws() == before);
    swimmer.gravity = -swimmer.gravity;
    swimmer.drawScale.x = -1;
    swimmer.hopCounter = swimmer.hopPeriod;
    swimmer.moveHorizontal();
    CHECK(swimmer.velocity.x == swimmer.maximumSpeed.x &&
          swimmer.velocity.y == swimmer.maximumSpeed.y);

    EnemyActor approach(*w, {0, 0}, 7, 6);
    CHECK(approach.behavior == 15 && approach.steeringRange == 2.f);
    approach.hopCounter = -1;
    approach.policy.solidCollisions = false;
    w->goat->stagePosition = {0, -3};
    approach.run();
    CHECK(approach.velocity.x == 0 && approach.velocity.y == 0);
    w->goat->stagePosition = {0, -1};
    approach.run();
    CHECK(approach.steeringAngle == 0 && approach.velocity.y == approach.originalVelocity.y * .5);
    const float unchanged = approach.heading;
    w->goat->stagePosition = {1, -1};
    approach.run();
    CHECK(approach.steeringAngle < 0 &&
          approach.heading == unchanged); // sprite 167 keeps its animation's angle

    auto turretWorld = arena();
    EnemyActor turret(*turretWorld, {0, 0}, 8, 11);
    turretWorld->goat->stagePosition = {-1, 0};
    turret.attackCooldown = 1;
    const auto draws = turretWorld->random.draws();
    turret.run();
    auto *shot = dynamic_cast<EnemyShotActor *>(turretWorld->enemies);
    CHECK(shot && turret.attackCooldown == 200 && turretWorld->random.draws() == draws + 1);
    CHECK(shot->sprite == 129 && shot->radius == .04f && shot->velocity.x == -.01 &&
          shot->velocity.y == 0);
    CHECK(shot->drawScale.x == shot->savedDrawScale.x && shot->gravity == 0 && shot->spin == 0);
    CHECK(turretWorld->sounds.back().sample == 19);
    const auto x = shot->stagePosition.x;
    shot->run();
    CHECK(shot->stagePosition.x < x && shot->lifetime == 999);

    auto fireWorld = arena();
    fireWorld->sourceTime = 0;
    auto fire = std::make_unique<EnemyActor>(*fireWorld, neon::vec2d{0, 0}, 8, 1);
    auto *rotor = fire.get();
    neon::entities::prepend<StageActor>(fireWorld->enemies, *fire);
    fire.release();
    fireWorld->goat->stagePosition = {0, .03};
    fireWorld->preparePresentation();
    const auto centre = rotor->worldPosition, scale = rotor->drawScale;
    const float radius = rotor->radius;
    rotor->flashTicks = 6;
    rotor->freezeTicks = 2;
    StagePresentation presentation;
    presentation.prepare(*fireWorld);
    CHECK(fireWorld->goat->hazard && rotor->flashTicks == 1 && rotor->freezeTicks == 1);
    int fireballs = 0;
    for (const auto &d : presentation.draws())
        if (d.frame == 108)
            ++fireballs;
    CHECK(fireballs == 4 && rotor->sprite == 105 && rotor->radius == radius);
    CHECK(rotor->worldPosition.x == centre.x && rotor->worldPosition.y == centre.y &&
          rotor->drawScale.x == scale.x);
    // Placement bit 2 reverses rotation; low two bits choose four..seven balls.
    rotor->placementTransform = 7;
    fireWorld->sourceTime = 1.5707964f;
    fireWorld->goat->stagePosition = {-.03, 0};
    fireWorld->goat->hazard = false;
    fireWorld->preparePresentation();
    presentation.prepare(*fireWorld);
    CHECK(fireWorld->goat->hazard);
    fireballs = 0;
    for (const auto &d : presentation.draws())
        if (d.frame == 108)
            ++fireballs;
    CHECK(fireballs == 7);
}

void thirdCampaignFamilies() {
    auto w = arena();
    w->random.seed(1);
    auto parent = std::make_unique<EnemyActor>(*w, neon::vec2d{0, 0}, 7, 5);
    auto *root = parent.get();
    neon::entities::prepend<StageActor>(w->enemies, *root);
    parent.release();
    CHECK(w->random.draws() == 3 && w->random.state() == 1622650073U);
    CHECK(root->heading == -2.3151164054870605f && root->steeringRate == .005112106446176767f);
    CHECK(root->splitDepth == 2 && root->colour.r == 1 &&
          std::abs(root->colour.g - 128. / 255.) < 1e-7 && root->colour.b == 0);
    root->policy.solidCollisions = false;
    root->contactHealth = 100;
    for (int i = 0; i < 105; ++i)
        root->run();
    CHECK(root->contactHealth == 1 && !root->dead); // omitted IT predicate in decompiler C
    const auto constructorDraws = w->random.draws();
    {
        EnemyActor probe(*root, EnemyActor::SplitChild{});
        CHECK(w->random.draws() == constructorDraws + 3 && probe.splitDepth == 1);
    }
    const float parentRadius = root->radius;
    const auto draws = w->random.draws();
    w->goat->stagePosition = root->stagePosition;
    root->hitBy(w->goat.get());
    CHECK(root->dead && count(w->enemies) == 3 && w->random.draws() > draws + 6);
    auto *second = dynamic_cast<EnemyActor *>(w->enemies);
    auto *first = dynamic_cast<EnemyActor *>(second->next);
    CHECK(first && second && first->next == root && first->splitDepth == 1 &&
          second->splitDepth == 1);
    CHECK(first->radius == float(double(parentRadius) * .6) && first->contactHealth == 100);
    CHECK(first->gravity == w->allocation.splitChildGravity && first->buoyancy == -.7f &&
          first->motionFlags == 3 && !first->policy.automaticVerticalHandling);
    CHECK(std::abs(first->originalVelocity.length() - root->velocity.length() * 1.2) < 1e-15);
    CHECK(w->score == 1000 && count(w->contactObjects) == 1);
    // A child has its own creation path and three fresh draws. Both generations
    // split twice; depth zero still rewards but creates no further descendants.
    first->hitBy(w->goat.get());
    CHECK(count(w->enemies) == 5 && w->score == 2000);
    auto *leaf = dynamic_cast<EnemyActor *>(w->enemies);
    CHECK(leaf && leaf->splitDepth == 0);
    const auto leafDraws = w->random.draws();
    leaf->hitBy(w->goat.get());
    CHECK(count(w->enemies) == 5 && w->score == 3000 && w->random.draws() > leafDraws);
    // Death bursts consume RNG; there must be no additional split children.
    CHECK(StageFrontend::campaignPlayable(2));
}
void fullCampaignCompletionAndGameOver() {
    Bank bank;
    bank.pur = 1000;
    bank.tim = 20000;
    for (auto &level : bank.stages) {
        level.seed = 1;
        level.bestTime = 108000;
    }
    auto w = arena();
    StageSession session;
    session.setBank(bank);
    session.beginRun(*w);
    StageHud hud;
    for (int level = 0; level < 20; ++level) {
        w->stageIndex = level;
        w->score = w->highScore = 5;
        w->stageTotal += 5;
        w->stageTicks = 100;
        w->bestStageTime = 100;
        w->exitFadeTicks = 51;
        w->sceneFade = .09f;
        w->activeFollowers = 0;
        session.prepareOuterFrame(*w, hud, 0);
        if (level < 19) {
            CHECK(session.phase() == StageSession::Phase::BetweenLevels);
            while (session.phase() != StageSession::Phase::NextLevel)
                session.prepareOuterFrame(*w, hud, 0);
            auto next = arena();
            session.continueRun(*next, *w);
            w = std::move(next);
        }
    }
    CHECK(session.phase() == StageSession::Phase::GameComplete && session.nextOrdinal() == 19);
    CHECK(session.livesBonus() == 180000 && w->stageTotal == 180100 && w->lives == 9);
    CHECK(bank.pur == 180100 && bank.tim == 2000 && bank.fur == 19 &&
          bank.stages[19].highScore == 180005);
    CHECK(session.elapsedRunTicks() == 2000);
    bool fastest = false;
    while (session.phase() != StageSession::Phase::Done) {
        session.prepareOuterFrame(*w, hud, 0, true);
        for (const auto &line : hud.text)
            fastest |= line.text == "** NEW FASTEST GAME TIME! **";
    }
    CHECK(fastest && session.reportTicks() == 352);
    auto terminal = arena();
    StageSession over;
    over.setBank(bank);
    over.beginRun(*terminal);
    terminal->terminal = true;
    terminal->stageTicks = 60;
    terminal->sceneFade = 1;
    over.prepareOuterFrame(*terminal, hud, 0);
    CHECK(over.phase() == StageSession::Phase::GameOver);
    CHECK(over.elapsedRunTicks() == 60 && over.reportTicks() == 0);
    for (int i = 0; i < 351; ++i)
        over.prepareOuterFrame(*terminal, hud, 0);
    CHECK(over.phase() == StageSession::Phase::GameOver && over.elapsedRunTicks() == 60);
    over.prepareOuterFrame(*terminal, hud, 0, true);
    CHECK(over.phase() == StageSession::Phase::Done);
}
void editorCatalogueAndSparseCampaign() {
    // Editor descriptors include records absent from the original campaigns.
    const int counts[] = {14, 4, 10, 12, 12};
    for (int family = 4; family <= 8; ++family)
        for (int variant = 0; variant < counts[family - 4]; ++variant) {
            auto w = arena();
            EnemyActor actor(*w, {.1, 0}, family, uint32_t(variant));
            actor.run();
            actor.preparePresentation();
        }
    auto w = arena();
    EnemyActor returning(*w, {0, 0}, 6, 4);
    CHECK(returning.behavior == 8 && returning.contactHealth == 50);
    w->requiredFruit = 1;
    returning.fireMachine();
    CHECK(returning.behavior == 8);
    w->requiredFruit = 0;
    const auto draws = w->random.draws();
    returning.fireMachine();
    CHECK(returning.behavior == 88 && returning.paletteIndex == 2 &&
          returning.returnRadius == returning.radius * 2.f);
    auto *exit = new ExitActor(*w, {0, 0, 0, false, 0}, {.1, .1});
    neon::entities::prepend<StageActor>(w->contactObjects, *exit);
    returning.stagePosition = {.1, .1};
    returning.velocity = {.001, .002};
    returning.heading = 1;
    returning.steeringAngle = 2;
    returning.runMachine();
    CHECK(returning.stagePosition.x == .1 + .001 && returning.stagePosition.y == .1 + .002);
    CHECK(returning.heading == float(.95) && returning.steeringAngle == float(1.9) &&
          w->random.draws() == draws);
    Bank bank;
    bank.stages[2].seed = 1;
    bank.stages[17].seed = 1;
    StageSession session;
    session.setBank(bank);
    session.beginRun(*w, 2);
    StageHud hud;
    w->stageIndex = 2;
    w->exitFadeTicks = 51;
    w->sceneFade = .09f;
    w->activeFollowers = 0;
    session.prepareOuterFrame(*w, hud, 0);
    CHECK(session.phase() == StageSession::Phase::BetweenLevels && session.nextOrdinal() == 17);
    while (session.phase() != StageSession::Phase::NextLevel)
        session.prepareOuterFrame(*w, hud, 0);
    auto next = arena();
    session.continueRun(*next, *w);
    next->stageIndex = 17;
    next->exitFadeTicks = 51;
    next->sceneFade = .09f;
    next->activeFollowers = 0;
    session.prepareOuterFrame(*next, hud, 0);
    CHECK(session.phase() == StageSession::Phase::GameComplete && session.nextOrdinal() == 17);
}
void originalAttractBranch() {
    auto w = arena(false, true);
    w->attractTicks = 1500;
    w->medium.splashSoundSuppressed = true;
    auto &goat = *w->goat;
    const auto hunger = goat.hunger;
    const int active = goat.activeTicks;
    for (int i = 0; i < 10; ++i) {
        goat.run();
        CHECK(goat.attractCameraTarget.x == goat.stagePosition.x &&
              goat.attractCameraTarget.y == goat.stagePosition.y);
    }
    CHECK(goat.attractCountdown == 1 && goat.hunger == hunger && goat.activeTicks == active &&
          w->stageTicks == 10);
    const auto position = goat.stagePosition;
    const auto draws = w->random.draws();
    w->sourceTime = 12.5f;
    goat.run();
    CHECK(goat.attractCountdown == 100 && w->random.draws() == draws + 1);
    CHECK(goat.stagePosition.x == position.x && goat.stagePosition.y == position.y);
    CHECK(goat.attractCameraTarget.x == double(std::sin(float(12.5 * goat.attractFrequencyX))));
    CHECK(goat.attractCameraTarget.y == double(std::sin(float(12.5 * goat.attractFrequencyY))));
    StageFrontend ui;
    ui.enter(StageFrontend::Page::Title, *w);
    for (int i = 0; i < 500; ++i)
        CHECK(ui.tick(*w, uint32_t(i)) == StageFrontend::None);
    CHECK(ui.tick(*w, 501) == StageFrontend::IdleCycle);
    ui.enter(StageFrontend::Page::Attract, *w);
    for (int i = 0; i < 1499; ++i)
        CHECK(ui.tick(*w, uint32_t(i)) == StageFrontend::None);
    CHECK(ui.tick(*w, 1500) == StageFrontend::Return && ui.attractRemaining() == 0);
    ui.enter(StageFrontend::Page::Attract, *w);
    ui.touch(neon::input::TouchPhase::Down, 1, {0, 0});
    CHECK(ui.tick(*w, 1) == StageFrontend::Return);
}
void exitRecordsAndBetweenLevelReport() {
    auto w = arena();
    w->stageTicks = 123;
    w->bestStageTime = 200;
    w->spawnExit({0, 0});
    auto *exit = w->contactObjects;
    exit->headbuttState = 1;
    exit->hitBy(w->goat.get());
    CHECK(w->bestStageTime == 123 && w->newBestStageTime && w->timeRecordCounter == 1);
    CHECK(w->overlayEffects && !w->placementOverlays);
    StageSession session;
    session.beginRun(*w);
    CHECK(w->lives == 2);
    // Transitions sample PRE-simulation state, including both strict thresholds.
    w->goat->state = 4;
    w->sceneFade = .09f;
    w->exitFadeTicks = 50;
    w->activeFollowers = 0;
    StageHud hud;
    session.prepareOuterFrame(*w, hud, 1);
    CHECK(session.phase() == StageSession::Phase::Playing);
    w->exitFadeTicks = 51;
    w->activeFollowers = 1;
    session.prepareOuterFrame(*w, hud, 2);
    CHECK(session.phase() == StageSession::Phase::Playing);
    w->activeFollowers = 0;
    w->beginLogicalFrame();
    session.prepareOuterFrame(*w, hud, 3);
    CHECK(session.phase() == StageSession::Phase::BetweenLevels && session.reportTicks() == 0);
    CHECK(w->lives == 3 && session.nextOrdinal() == 1 && session.elapsedRunTicks() == 123);
    CHECK(w->sounds.back().kind == RuntimeSound::Kind::Stop && w->sounds.back().sample == 63);
    w->runLocomotion(0, 0, 3, 3);
    CHECK(w->sounds.front().sample == 63); // outer UI events survive actor update
    w->newRecord = true;
    w->score = 321;
    w->stageTotal = 432;
    int highScoreSounds = 0, timeSounds = 0, spareSounds = 0;
    bool sawTime = false;
    while (session.phase() != StageSession::Phase::NextLevel) {
        const int before = session.reportTicks();
        w->beginLogicalFrame();
        session.prepareOuterFrame(*w, hud, 4);
        if (before == 30)
            CHECK(session.reportTicks() == 32); // source deliberate skip after record sound
        if (before == 90)
            CHECK(session.reportTicks() == 92);
        for (const auto &sound : w->sounds) {
            highScoreSounds += sound.sample == 70;
            timeSounds += sound.sample == 71;
            spareSounds += sound.sample == 81;
        }
        for (const auto &line : hud.text)
            if (line.row == 16) {
                CHECK(line.text == "LEVEL TIME: 00:02:05");
                sawTime = true;
            }
    }
    CHECK(highScoreSounds == 1 && timeSounds == 1 && spareSounds == 1 && sawTime);
    CHECK(session.reportTicks() == 251 && w->lives == 3);
    auto &survivor = w->showScore(*w->goat, 17);
    survivor.fade = .001f;
    StageParticleBirth particle{};
    particle.sprite = 2;
    particle.fadePerUpdate = .001f;
    w->adoptImpact(particle);
    auto *ordinary = w->ordinaryEffects;
    auto next = arena();
    session.continueRun(*next, *w);
    CHECK(session.phase() == StageSession::Phase::Playing && next->lives == 3 &&
          next->stageTotal == 432);
    CHECK(next->score == 0 && next->stageTicks == 0 && session.elapsedRunTicks() == 123);
    CHECK(!w->foregroundEffects && !w->ordinaryEffects);
    CHECK(next->foregroundEffects == &survivor && next->ordinaryEffects == ordinary);
    CHECK(&survivor.owner() == next.get() && &ordinary->owner() == next.get());
    w.reset();
    next->terrainPalette[3] = {.25, .5, .75, 1};
    next->advanceEffects();
    CHECK(survivor.colour.r == .25 && survivor.colour.g == .5);
    next->clearActors();
    CHECK(!next->goat && !next->foregroundEffects && !next->ordinaryEffects &&
          !next->contactObjects);
    auto capped = arena();
    session.beginRun(*capped);
    capped->lives = 9;
    capped->exitFadeTicks = 51;
    capped->sceneFade = .09f;
    session.prepareOuterFrame(*capped, hud, 0);
    CHECK(!session.extraGoatAwarded() && capped->lives == 9);
    auto equal = arena();
    equal->stageTicks = 100;
    equal->bestStageTime = 100;
    equal->spawnExit({0, 0});
    equal->contactObjects->headbuttState = 1;
    equal->contactObjects->hitBy(equal->goat.get());
    CHECK(!equal->newBestStageTime && equal->timeRecordCounter == 0);
}
void frontendLifecycle() {
    {
        auto w = arena();
        StageFrontend ui;
        ui.initialize(*w);
        ui.enter(StageFrontend::Page::Banks, *w);
        w->terrainPalette[0] = {.23, .37, .61, 1};
        ui.focusAction(StageFrontend::Choose0);
        const auto lit = [&]() {
            ui.prepareDraws(*w);
            return std::any_of(ui.draws().begin(), ui.draws().end(), [&](const auto &d) {
                return d.colour.r == .23 && d.colour.g == .37 && d.colour.b == .61;
            });
        };
        neon::input::ControllerState pad;
        pad.connected = true;
        pad.pressed = neon::input::Right;
        ui.pads(&pad, 1);
        CHECK(lit());
        ui.pads(nullptr, 0);
        CHECK(!lit());
        ui.keyboardNavigation = true;
        ui.pads(nullptr, 0);
        CHECK(lit());
        ui.keyboardNavigation = false;
        ui.pads(nullptr, 0);
        CHECK(!lit());
    }

    {
        auto w = arena();
        StageFrontend ui;
        ui.enter(StageFrontend::Page::Banks, *w);
        ui.focusAction(StageFrontend::ToggleEdit);
        neon::input::ControllerState p;
        p.connected = true;
        ui.pads(&p, 1);
        ui.tick(*w, 0);
        p.buttons = p.pressed = neon::input::South;
        ui.pads(&p, 1);
        p.pressed = 0;
        for (int n = 0; n < 4; ++n)
            ui.pads(&p, 1); // host frames with no logical tick
        CHECK(ui.tick(*w, 1) == StageFrontend::ToggleEdit);
        for (int n = 0; n < 30; ++n) {
            ui.pads(&p, 1);
            CHECK(ui.tick(*w, n + 2) == StageFrontend::None);
        }
        ui.editing = true;
        ui.enter(StageFrontend::Page::Banks, *w);
        ui.focusAction(StageFrontend::ToggleEdit);
        p.buttons = 0;
        ui.pads(&p, 1);
        ui.tick(*w, 40);
        p.buttons = p.pressed = neon::input::South;
        ui.pads(&p, 1);
        p.buttons = p.pressed = 0;
        ui.pads(&p, 1);
        CHECK(ui.tick(*w, 41) == StageFrontend::ToggleEdit &&
              ui.focus() == StageFrontend::ToggleEdit);
        StageHud hud;
        ui.prepare(*w, hud);
        CHECK(std::any_of(hud.text.begin(), hud.text.end(),
                          [](const auto &t) { return t.row == 10 && t.text == "GAME 1:"; }));
        CHECK(std::none_of(hud.text.begin(), hud.text.end(), [](const auto &t) {
            return t.text.find("TOTAL SCORE:") != std::string::npos ||
                   t.text.find("A GAME OF") != std::string::npos;
        }));
    }
    auto w = arena();
    StageFrontend ui;
    StageHud hud;
    ui.capabilities(true, false, *w);
    for (auto &bank : ui.banks) {
        bank.gid = 1298753211;
        for (auto &stage : bank.stages)
            stage.seed = 1508545802;
    }
    ui.initialize(*w);
    ui.enter(StageFrontend::Page::Title, *w);
    const auto initialRandom = w->random.state();
    ui.tick(*w, 1);
    for (int i = 1; i < 13; ++i)
        ui.tick(*w, uint32_t(i + 1));
    ui.prepare(*w, hud);
    CHECK(ui.draws().size() == 2);
    CHECK(ui.draws()[0].position.y == double(.32f)); // .08 source interpolation saturates
    CHECK(w->random.state() == initialRandom);       // no portrait/banner update draws
    neon::input::ControllerState pad;
    pad.connected = true;
    pad.buttons = pad.pressed = neon::input::South;
    ui.pads(&pad, 1);
    CHECK(ui.tick(*w, 14) == StageFrontend::Begin);
    ui.enter(StageFrontend::Page::Banks, *w);
    ui.pads(&pad, 1);
    CHECK(ui.tick(*w, 15) == StageFrontend::None); // held acceptance blocked
    pad.buttons = pad.pressed = 0;
    ui.pads(&pad, 1);
    ui.tick(*w, 16);
    pad.buttons = pad.pressed = neon::input::South;
    ui.pads(&pad, 1);
    CHECK(ui.tick(*w, 17) == StageFrontend::Choose0);
    ui.enter(StageFrontend::Page::Playing, *w);
    pad.buttons = pad.pressed = 0;
    ui.pads(&pad, 1);
    ui.tick(*w, 18);
    pad.buttons = pad.pressed = neon::input::South;
    ui.pads(&pad, 1);
    CHECK(ui.tick(*w, 19) == StageFrontend::None); // goat jump stays gameplay input
    ui.setPaused(true, *w);
    pad.buttons = pad.pressed = 0;
    ui.pads(&pad, 1);
    ui.tick(*w, 20);
    for (int tick = 0; tick < 13; ++tick)
        ui.tick(*w, 21 + tick);
    ui.prepare(*w, hud);
    CHECK(ui.foregroundDraws().size() == 5 && ui.draws().empty());
    auto reset = std::find_if(ui.buttons().begin(), ui.buttons().end(),
                              [](const auto &b) { return b.action == StageFrontend::Reset; });
    CHECK(reset != ui.buttons().end() && reset->alpha == 1); // packed colour writes alpha too
    ui.touch(neon::input::TouchPhase::Down, 42, {0, -.17});
    CHECK(ui.tick(*w, 35) == StageFrontend::Reset);
    ui.armReset();
    ui.touch(neon::input::TouchPhase::Up, 42, {0, -.17});
    ui.tick(*w, 36);
    CHECK(ui.resetTicks() == 59);
    ui.touch(neon::input::TouchPhase::Down, 42, {0, -.17});
    CHECK(ui.tick(*w, 37) == StageFrontend::Reset && ui.resetTicks() == 58);
    ui.capabilities(false, true, *w);
    const auto has = [&](int action) {
        return std::any_of(ui.buttons().begin(), ui.buttons().end(),
                           [&](const auto &b) { return b.action == action; });
    };
    CHECK(!has(StageFrontend::Hand) && has(StageFrontend::Tilt));
    ui.capabilities(true, true, *w);
    CHECK(has(StageFrontend::Hand) && has(StageFrontend::Tilt));
    ui.capabilities(false, false, *w);
    CHECK(!has(StageFrontend::Hand) && !has(StageFrontend::Tilt));
    ui.enter(StageFrontend::Page::Title, *w);
    pad.buttons = pad.pressed = 0;
    ui.pads(&pad, 1);
    ui.tick(*w, 40);
    pad.buttons = pad.pressed = neon::input::East;
    ui.pads(&pad, 1);
    CHECK(ui.tick(*w, 41) == StageFrontend::Exit);
    ui.enter(StageFrontend::Page::Credits, *w);
    for (int tick = 0; tick < 301; ++tick)
        ui.tick(*w, uint32_t(tick));
    ui.prepare(*w, hud);
    CHECK(std::any_of(hud.text.begin(), hud.text.end(), [](const auto &t) {
        return t.row == 32 && t.text.find("2013 LLAMASOFT") != std::string::npos;
    }));
}

} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--touch-hold-only") {
            touchGestureSteadyHold();
            std::cout << "Touch holds: both layouts/signs, ten-second holds, jitter, concurrent "
                         "jump and release passed\n";
            return 0;
        }
        currentOwnerAndDrawing();
        editorCatalogueAndSparseCampaign();
        touchGestureOwnership();
        touchGestureSteadyHold();
        originalAttractBranch();
        frontendLifecycle();
        groundJumpAndEffects();
        headbuttBirthsRunAndRetire();
        waterBirthsAndListScheduling();
        mirroredGravityAndInput();
        kidContactsHistoryAndHeadbutt();
        deathRestartAndExitStates();
        pickupVariantsAndExit();
        scrollingHistoryAndPresentation();
        nativeAndTouchControls();
        originalHudPhases();
        originalNamesAndStageEntry();
        sourceStarfieldIntegration();
        devicesEnemiesAndRewards();
        platformRidingAndEnemyProjectiles();
        laterMachineFamilies();
        secondCampaignFamilies();
        thirdCampaignFamilies();
        exitRecordsAndBetweenLevelReport();
        fullCampaignCompletionAndGameOver();
        std::cout << "Owned goat/kid family: contacts, history, movement, effects and checkpoint "
                     "lifecycle passed\n";
        if (argc > 1) {
#ifdef GOATUP2_RESOURCE_CATALOG
            const std::string mode = argv[1];
            CHECK(argc == 5 && (mode == "--terrain-only" || mode == "--player-family" ||
                                mode == "--stage-family"));
            importedTerrain(std::filesystem::u8path(argv[2]), std::stoi(argv[3]),
                            std::stoi(argv[4]), mode != "--terrain-only", mode == "--stage-family");
#else
            throw std::runtime_error(
                "asset diagnostics use the root build's shared ResourceCatalog");
#endif
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
