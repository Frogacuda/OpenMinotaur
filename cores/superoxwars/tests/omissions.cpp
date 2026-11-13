// Source-rule checks for presentation, lifecycle and the enemy-bullet palette
// binding. These exercise the production model; they are not guest execution.
#include "world.h"
#include "scene.h"
#include "frontend.h"
#include "pickups.h"
#include "projectiles.h"
#include "air_enemy.h"
#include "effects.h"
#include "ground_features.h"
#include "original_tables.h"
#include "core_api.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace superoxwars;
namespace {
void expect(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
bool near(double a, double b) {
    return std::abs(a - b) < 1e-8;
}
bool same(Colour a, Colour b) {
    return near(a.r, b.r) && near(a.g, b.g) && near(a.b, b.b) && near(a.a, b.a);
}
} // namespace
void checkOmissionRepairs() {
    auto p = std::make_unique<World>();
    auto &w = *p;
    Scene scene(w);
    Frontend ui;
    ui.attach(w, scene);
    std::vector<int> sounds;
    w.sound = [&](int sample, int volume) {
        if (sample == 7 || sample == 10)
            expect(volume == 32768, "Direct UI samples use the source playback volume units");
        sounds.push_back(sample);
    };
    w.tick({});
    w.tick({});
    std::vector<TextLine> text;
    std::vector<ActorDraw> draw;
    ui.prepare(w, scene, text, draw);
    auto heading =
        std::find_if(text.begin(), text.end(), [](auto &l) { return l.text == "CAPRICHOSO"; });
    expect(heading != text.end() && heading->colour == 0x0000ffff,
           "F11: HUD heading retains scalar RGBA");
    w.blueScore = 10;
    w.redScore = 2;
    w.score = 12;
    w.tick({});
    ui.prepare(w, scene, text, draw);
    auto total =
        std::find_if(text.begin(), text.end(), [](auto &l) { return l.x == 10 && l.y == 2; });
    expect(total != text.end() && total->colour == 0x4040ffff,
           "F11: HUD total indicates blue lead");
    ui.command(w, scene, mino::PAD_A);
    w.tick({});
    expect(sounds.size() == 1 && sounds[0] == 10,
           "F10: selector plays one direct UI sound over silent demo");
    ui.activate(w, 3);
    w.tick({});
    expect(sounds.back() == 7, "F10: selector activation plays direct UI sound");
    w.pointerDown(0, {0, 33.5 / 38. - .5});
    w.tick({});
    expect(ui.screen == Frontend::Screen::Select && !w.paused,
           "F09: fresh BACK touch dismisses local scores");
    w.pointerUp(0);
    w.tick({});
    ui.activate(w, 4);
    w.tick({});
    w.pointerDown(0, {0, 33.5 / 38. - .5});
    w.tick({});
    expect(ui.screen == Frontend::Screen::Select && !w.paused,
           "F09: fresh BACK touch dismisses local goals");
    w.pointerUp(0);
    w.tick({});
    ui.activate(w, 0);
    w.tick({});
    for (auto *e = w.lists[size_t(Domain::Always)]; e; e = e->next)
        expect(e->type != 0x1001, "F12: selector type retires on start UI update");
    for (int n = 0; n < 120; ++n)
        w.tick({});
    draw.clear();
    w.prepareForeground(draw);
    expect(ui.pauseOpacity < .1 &&
               std::none_of(
                   draw.begin(), draw.end(), [](auto &d) { return d.frame == 1; }),
           "F13: faded pause button emits no sprite");
    w.pointerDown(0, {0, .3});
    w.tick({});
    w.player->state = Ship::State::GameOver;
    w.tick({});
    for (int n = 0; n < 123; ++n)
        w.tick({});
    expect(ui.screen == Frontend::Screen::GameOver,
           "F08: prior gameplay hold cannot dismiss game over");
    w.pointerUp(0);
    w.tick({});
    w.pointerDown(0, {0, .3});
    w.tick({});
    w.tick({});
    expect(ui.screen == Frontend::Screen::Title,
           "F08: a fresh touch after delay dismisses game over");

    auto q = std::make_unique<World>();
    Ship ship(*q);
    q->player = &ship;
    ship.mass = 0;
    EnemyBullet bullet(*q, {.2, 0}, {});
    q->ticks = 4;
    q->updatePalette();
    bullet.run();
    expect(bullet.paletteSlot == 10 && same(bullet.colour, q->palette[10]),
           "Bullet palette: factory enables slot 10, consumed by update");
    const auto prior = bullet.colour;
    q->ticks = 8;
    q->updatePalette();
    bullet.run();
    expect(!same(prior, bullet.colour),
           "Bullet palette: live bullet changes colour with the source clock");
    q->enemyBulletOverride = true;
    q->enemyBulletPalette = 0;
    q->enemyBulletFrame = 57;
    EnemyBullet special(*q, {.2, 0}, {});
    special.run();
    expect(special.frame == 57 && same(special.colour, Colour{}),
           "Bullet palette: tooth/horn override zero keeps literal colour");
    q->layerTilt = .5;
    EnemyBullet tilted(*q, {.2, 0}, {});
    tilted.run();
    expect(near(tilted.position.x, .2) && near(tilted.velocity.x, .00005),
           "F06: bullet tilt changes velocity after displacement");
    q->camera.x = -1.38;
    q->weaponThreat = 7;
    q->weaponFollowers = 4;
    ship.position.x = .3;
    ship.state = Ship::State::Waiting;
    ship.stateTicks = 0;
    ship.run();
    expect(near(q->camera.x, -1.38) && q->weaponThreat == 0 && q->weaponFollowers == 0,
           "F03: respawn preserves camera and resets published weapon state");
    Fragment fragment(*q);
    Entity target(*q);
    fragment.didHit(target);
    expect(!fragment.dead && fragment.health == 0, "F02: fragment inherits one contact");
    fragment.didHit(target);
    expect(fragment.dead, "F02: fragment retires on following contact");
    ship.state = Ship::State::Flying;
    ship.shotThisTick = true;
    Follower follower(*q, 28);
    follower.run();
    for (auto *e = q->lists[size_t(Domain::PlayerShots)]; e; e = e->next)
        if (auto *b = dynamic_cast<PlayerBullet *>(e))
            expect(b->health == 1, "F02: blue follower factory preserves inherited contact budget");
    ColourOrb orb(*q, target);
    orb.frame = 43;
    orb.hitBy(target);
    auto *burst = dynamic_cast<PlayerBullet *>(q->lists[size_t(Domain::PlayerShots)]);
    expect(burst && burst->health == 1,
           "F02: colour-orb shot factory preserves inherited contact budget");
    GroundEnemy turret(*q, 0, 3);
    draw.clear();
    turret.prepare(draw);
    expect(turret.mass == 1 && turret.baseAngle == 3.1415927f &&
               near(draw.back().angle, double(3.1415927f + 3.1415927f)),
           "F04: coloured turret body uses base orientation, not mass");
    Tooth tooth(*q, 0, false);
    expect(tooth.mass == 1 && tooth.baseAngle == 3.1415927f,
           "F04: tooth uses drawing-angle offset");

    auto boss = std::make_unique<World>();
    Scene bossScene(*boss);
    bossScene.start();
    bossScene.bossPhase = true;
    boss->polarity = -.5f;
    boss->groupSize[0] = 1;
    PodLaunch spec;
    LaunchPod pod(*boss, spec);
    AirEnemy last(*boss, pod);
    last.position = {.2, .1};
    Entity hit(*boss);
    hit.type = 0x2000;
    hit.frame = 28;
    last.hitBy(hit);
    int count = 0;
    for (auto *e = boss->lists[size_t(Domain::PlayerShots)]; e; e = e->next)
        if (dynamic_cast<Fragment *>(e))
            ++count;
    expect(count == 200, "F01: boss-phase group completion creates 200 live combat fragments");
    auto r = std::make_unique<World>();
    r->level = 19;
    r->area = 9;
    Ship player(*r);
    r->player = &player;
    BonusTile tile(*r, 1, 3);
    PlayerBullet shot(*r, player, 0, 8, 1);
    tile.hitBy(shot);
    for (auto *e = r->lists[size_t(Domain::Ground)]; e; e = e->next)
        expect(!dynamic_cast<WeaponPickup *>(e),
               "F05: absolute level 19 does not repeat level 9 exception");
    auto demo = std::make_unique<World>();
    demo->demo = true;
    Scene demoScene(*demo);
    demoScene.start();
    const auto raw = unpack(original::AreaColours[0]);
    expect(near(demoScene.terrain.colours[1].r, raw.r * double(.9f)),
           "F14: attract terrain applies float .9 dimming");

    auto input = std::make_unique<World>();
    input->pointerDown(0, {});
    input->pointerDown(1, {.4, .4});
    input->pointerMove(0, {.1, .1});
    input->pointerMove(0, {.11, .12});
    input->updateTouch();
    expect(near(input->touchRelative.x, .01) && near(input->touchRelative.y, .02),
           "F07: relative input uses first finger's event delta");
    expect(near(input->touchStick.length(), double(.009f) * 10.),
           "F07: anchor vector uses original radial cap");
    input->pointerUp(1);
    input->pointerMove(0, {.105, .115});
    const auto anchor = input->touchAnchor[0];
    input->updateTouch();
    expect(input->touchAnchor[0].x > anchor.x && input->touchAnchor[0].y > anchor.y,
           "F07: reversing drag advances anchor on both axes");
    input->pointerUp(0);
    input->updateTouch();
    expect(input->touchRelative.length() == 0, "F07: releasing touch clears relative movement");
    Ship touchShip(*input);
    input->player = &touchShip;
    input->relativeControls = true;
    input->input.movement = {0, .01};
    touchShip.run();
    expect(near(touchShip.retained.y, .31),
           "F07: relative vertical movement does not inherit horizontal 1.8 gain");
    input->input.movement = {};
    input->relativeControls = false;
    input->pointerDown(0, {});
    input->pointerMove(0, {.1, 0});
    const auto before = touchShip.retained;
    touchShip.run();
    expect(near(touchShip.retained.x - before.x, .036),
           "F07: saved mode 1 uses capped anchored-stick movement");

    auto ending = std::make_unique<World>();
    Scene endScene(*ending);
    Frontend endUi;
    endUi.attach(*ending, endScene);
    ending->tick({});
    endUi.command(*ending, endScene, mino::PAD_A);
    ending->tick({});
    endUi.activate(*ending, 0);
    ending->tick({});
    ending->level = 5;
    ending->lives = 1;
    ending->ticks = 400;
    ending->blueScore = 20;
    ending->score = 20;
    ending->blueCollected = 10001;
    ending->redCollected = 0;
    ending->player->state = Ship::State::Waiting;
    ending->player->stateTicks = 0;
    ending->tick({});
    expect(ending->level == 4 && ending->ticks == 0 && endUi.records.blueBest == 20 &&
               !(ending->sessionAchievements & 0x102u),
           "F15: death callback banks records during player update, without moving later "
           "presentation awards");
    ending->tick({});
    expect(ending->ticks == 1 && ending->level == 4 && endUi.screen == Frontend::Screen::GameOver &&
               (ending->sessionAchievements & 0x102u) == 0x102u,
           "F15: next owner presents awards without repeating banking or clock reset");
    auto renew = std::make_unique<World>();
    Ship damaged(*renew);
    Entity hazard(*renew);
    renew->lifeDropBudget = 0;
    damaged.didHit(hazard);
    expect(renew->lifeDropBudget == 0, "500d0: grace suppresses the entire hit path");
    damaged.grace = 0;
    damaged.didHit(hazard);
    expect(renew->lifeDropBudget == 1 && damaged.state == Ship::State::Hit,
           "500d0: ship hit renews the life-pickup allowance");
    int afterGameOver = 0;
    ending->checkpoint = [&](int) { ++afterGameOver; };
    ending->paused = true;
    ending->ticks = 1001;
    endScene.bossPhase = true;
    ending->bossPresent = false;
    ending->tick({});
    expect(afterGameOver == 0 && ending->demo && endUi.phase == Frontend::Phase::Attract &&
               endUi.screen == Frontend::Screen::Title,
           "421e0: game-over dismissal publishes demo before campaign transition and resets "
           "attract in the same owner update");

    auto decay = std::make_unique<World>();
    Scene decayScene(*decay);
    Ship dying(*decay);
    decay->player = &dying;
    dying.state = Ship::State::Hit;
    decayScene.speed = 0;
    decay->polarity = .3f;
    for (int n = 0; n < 600; ++n)
        decayScene.tick();
    expect(decay->polarity == .2660730481147766f,
           "421e0: polarity decay retains the original double constant before float storage");
}
