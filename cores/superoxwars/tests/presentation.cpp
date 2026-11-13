#include "world.h"
#include "scene.h"
#include "ground.h"
#include "air_enemy.h"
#include "effects.h"
#include "projectiles.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace superoxwars;
namespace {
void check(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
}
struct Counter : Entity {
    int &runs;
    Counter(World &w, int &n) : Entity(w), runs(n) {}
    void run() override {
        ++runs;
    }
};
struct ContactTarget : Entity {
    int hits = 0;
    using Entity::Entity;
    void hitBy(Entity &) override {
        ++hits;
    }
};
struct SoundProducer : Entity {
    using Entity::Entity;
    void run() override {
        world.noise(8);
    }
};
} // namespace
int main() {
    try {
        auto contact = std::make_unique<World>();
        Entity source(*contact);
        auto *later = new ContactTarget(*contact);
        auto *delayed = new ContactTarget(*contact);
        delayed->skipDraw = 1;
        contact->add(Domain::Air, later);
        contact->add(Domain::Air, delayed);
        contact->contacts(source, Domain::Air);
        check(later->hits == 0, "9178 delayed overlap suppresses later contacts in the traversal");
        delayed->dead = true;
        later->position = {.01, 0};
        contact->contacts(source, Domain::Air);
        check(later->hits == 1 && contact->contactPosition.x == .01 &&
                  contact->contactNormal.x == 1,
              "9178 publishes contact state after callbacks");
        auto queued = std::make_unique<World>();
        int sounds = 0;
        queued->sound = [&](int, int) { ++sounds; };
        queued->add(Domain::Air, new SoundProducer(*queued));
        queued->tick({});
        check(sounds == 0, "418c4 queues entity sound for the next owner flush");
        queued->tick({});
        check(sounds == 1, "Next owner tick publishes the pending sample");
        auto campaign = std::make_unique<World>();
        Scene campaignScene(*campaign);
        campaignScene.start();
        campaign->lifeDropBudget = 0;
        campaignScene.nextLevel();
        check(campaign->lifeDropBudget == 1, "48b1c renews the area life-drop allowance");
        auto tilted = std::make_unique<World>();
        tilted->lateralTilt = .5;
        tilted->tick({});
        tilted->tiles.positions[10] = {.2, .1};
        GroundEnemy stack(*tilted, 10, 2);
        stack.armour = 2;
        std::vector<ActorDraw> layers;
        stack.prepare(layers);
        check(std::abs(stack.muzzle.x - .22) < 1e-12 && std::abs(stack.muzzle.y - .103) < 1e-12,
              "5301c includes the snapshotted lateral tilt in every armour layer and muzzle");
        auto direct = std::make_unique<World>();
        direct->lateralTilt = -.8;
        PlayerInput worldInput;
        worldInput.worldInfluence = .5;
        direct->tick(worldInput);
        direct->tiles.positions[10] = {.2, .1};
        GroundEnemy directStack(*direct, 10, 2);
        directStack.armour = 2;
        std::vector<ActorDraw> directLayers;
        directStack.prepare(directLayers);
        check(directStack.muzzle.x == stack.muzzle.x && directStack.muzzle.y == stack.muzzle.y &&
                  directLayers.size() == layers.size(),
              "Direct world input reaches the same layer and muzzle mechanism as physical tilt");
        for (size_t n = 0; n < layers.size(); ++n)
            check(directLayers[n].position.x == layers[n].position.x &&
                      directLayers[n].position.y == layers[n].position.y,
                  "Direct and physical deflection emit identical layer positions");
        EnemyBullet directBullet(*direct, {.2, 0}, {});
        directBullet.run();
        check(directBullet.position.x == .2 && directBullet.velocity.x == .00005,
              "Direct world input reaches the original projectile deflection after movement");
        check(direct->lateralTilt == -.8 && direct->layerTilt == .5,
              "Direct commands do not replace physical sensor state");
        direct->tick({});
        check(direct->layerTilt == -.8, "Neutral direct command returns to physical input");
        auto scoring = std::make_unique<World>();
        Ship ship(*scoring);
        scoring->player = &ship;
        scoring->polarity = float(1. / 999.);
        scoring->groupSize[0] = 1;
        PodLaunch launch;
        LaunchPod pod(*scoring, launch);
        AirEnemy victim(*scoring, pod);
        victim.position = {.2, .1};
        Entity hit(*scoring);
        hit.type = 0x2000;
        hit.frame = 28;
        scoring->scoreNotice = [&](int p, Vec2 at, int side, Vec2 motion, float size) {
            scoring->add(Domain::Decor, new FloatingScore(*scoring, p, at, side, motion, size));
        };
        victim.hitBy(hit);
        check(scoring->score == 11,
              "5b354 promotes polarity before its chain-score multiplication");
        FloatingScore *notice = nullptr;
        for (auto *e = scoring->lists[size_t(Domain::Decor)]; e; e = e->next)
            if (auto *f = dynamic_cast<FloatingScore *>(e))
                notice = f;
        check(notice && notice->radius == .06f && notice->velocity.x == double(float(-.006)) &&
                  notice->velocity.y == double(float(-.002)),
              "455f8 retains the group score caller's velocity and size");
        scoring->message(8, 8, 2, "EXTRA LIFE!");
        scoring->runMessages();
        check(scoring->messageLines.size() == 1 && scoring->messageLines[0].text == "EXTRA LIFE!",
              "40c9c/40dac keeps the row-keyed timed notice");
        scoring->runMessages();
        check(scoring->messageLines[0].text.empty(),
              "Timed message expiry explicitly clears its row");
        // Independent owner fields in 418c4/44a2c vs moo 48f94/playerBull 51004.
        for (int count : {4000, 0, -1})
            for (int mode : {1, 0, -1}) {
                auto w = std::make_unique<World>();
                int player = 0, shots = 0, decor = 0, ui = 0;
                w->terminalTilesRemaining = count;
                w->gameMode = mode;
                w->add(Domain::Player, new Counter(*w, player));
                w->add(Domain::PlayerShots, new Counter(*w, shots));
                w->add(Domain::Decor, new Counter(*w, decor));
                w->add(Domain::Always, new Counter(*w, ui));
                w->tick({});
                check(player == (mode > 0) && shots == (mode >= 0) && decor == 1 && ui == 1,
                      "Scheduler must read game mode, never the scene terrain counter");
                w->paused = true;
                w->tick({});
                check(ui == 2 && decor == 1, "UI list continues during gameplay pause");
            }
        PresentationState state;
        neon::DarwinRandom random(123);
        state.beginAttract();
        check(state.feedback.colour[0] == .2f && state.feedback.colour[3] == 0 &&
                  state.feedback.scaleX == .99f,
              "418c4 attract RGB/scale; alpha retains base initialization");
        state.beginPlay();
        check(state.feedback.scaleX == 1 && state.feedback.scaleY == 1,
              "421e0 live start restores unit feedback scale");
        state.pulse = 1;
        state.shakeTicks = 3;
        state.run(false, random);
        check(state.pulse == float(.95) && state.feedback.colour[3] == float(.95) &&
                  random.draws() == 2,
              "418c4 pulse and odd text-shake tick");
        check(state.feedback.x == 0 && state.feedback.y == 0 && state.textScroll.x != 0,
              "Shake belongs to cellGrid scrolling, not feedback transform");
        const auto scroll = state.textScroll;
        state.run(false, random);
        check(random.draws() == 2 && state.textScroll.x == scroll.x,
              "Even shake tick retains the prior text position without random draws");
        state.run(false, random);
        check(random.draws() == 4 && state.shakeTicks == 0 && state.textScroll.x == 0 &&
                  state.textScroll.y == 0,
              "Final odd shake tick consumes two draws then resets the grid");
        state.pulse = .041f;
        state.run(false, random);
        check(state.pulse == 0 && state.feedback.colour[0] == 0,
              "Pulse threshold is applied after decay");
        state.pulse = 1;
        state.shakeTicks = 3;
        state.run(true, random);
        check(state.pulse == 0 && state.shakeTicks == 0 && random.draws() == 4,
              "Attract clears event counters without consuming shake RNG");
        auto paused = std::make_unique<World>();
        paused->paused = true;
        paused->presentation.pulse = 1;
        paused->tick({});
        check(paused->ticks == 0 && paused->presentation.pulse == float(.95),
              "Feedback update is outside paused gameplay lists");
        auto drawing = std::make_unique<World>();
        auto *fg = new Entity(*drawing);
        fg->frame = 1;
        drawing->add(Domain::Always, fg);
        std::vector<ActorDraw> background, foreground;
        drawing->camera = {.25, 0};
        drawing->prepare(background);
        check(background.empty() && drawing->tiles.scroll.x == 0,
              "Background excludes UI and does not prematurely publish the camera");
        drawing->prepareForeground(foreground);
        check(foreground.size() == 1, "4124c draws the UI list separately");
        drawing->finishForeground();
        check(drawing->tiles.scroll.x == .25, "4124c mode 1 publishes camera after foreground");
        drawing->gameMode = 0;
        drawing->camera.x = .5;
        drawing->finishForeground();
        check(drawing->tiles.scroll.x == .25, "Mode 0 does not publish camera");
        drawing->backgroundMode = -1;
        drawing->prepare(background);
        check(background.empty(), "41300 background mode gate");
        std::cout << "SOW owner/presentation source-rule regressions passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
