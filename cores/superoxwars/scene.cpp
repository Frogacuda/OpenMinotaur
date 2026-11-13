#include "scene.h"
#include "air_enemy.h"
#include "pickups.h"
#include "projectiles.h"
#include "original_tables.h"
#include "effects.h"
#include "ground_features.h"
#include <stdexcept>
#include <algorithm>
namespace superoxwars {
namespace {
struct Backdrop : GameEntity {
    bool galaxy = false;
    float depth = 0, spin = 0;
    float &speed;
    Backdrop(World &w, float &scrollSpeed, Vec2 at, bool large)
        : GameEntity(w), galaxy(large), speed(scrollSpeed) {
        position = at;
        if (galaxy) {
            position.y = w.random.range(-.9, .9);
            appearance();
        } else {
            position = {w.random.range(-.7, .7), w.random.range(-.6, .6)};
            appearance();
        }
        retained = position;
    }
    void appearance() {
        if (galaxy)
            frame = int(world.random.next() & 3) + 11;
        else
            frame = 2;
        const float t = float(world.random.range(0, 1));
        colour = mixColour(unpack(original::AreaColours[size_t(world.area) * 16 + 2]),
                           unpack(original::AreaColours[size_t(world.area) * 16 + 3]), t);
        if (galaxy) {
            depth = float(double(t) * -.1 + .6);
            radius = float(double(t) * -.05 + .15);
            angle = float(world.random.range(-3.141592654, 3.141592654));
            spin = float(world.random.range(-.004, .004));
        } else {
            depth = float(double(t) * -.3 + .7);
            radius = float(double(t) * -.005 + .01);
        }
    }
    void run() override {
        if (galaxy)
            angle += spin;
        position.x = retained.x + world.tiles.scroll.x * (galaxy ? .02 : .06) * double(depth);
        retained.y += double(float(.06f * depth)) * (galaxy ? .04 : .06) * (double(speed) + .1);
        bool wrappedValue = false;
        const double edge = galaxy ? .9 : .6;
        if (retained.y > edge) {
            retained.y -= edge + edge;
            wrappedValue = true;
        }
        if (retained.y < -edge) {
            retained.y += edge + edge;
            wrappedValue = true;
        }
        if (wrappedValue)
            appearance();
        position.y = retained.y;
    }
};
struct Gauge : GameEntity {
    bool red;
    explicit Gauge(World &w, bool isRed) : GameEntity(w), red(isRed) {
        type = red ? 4 : 3;
        frame = 48;
        radius = .023f;
        position = {red ? .04 : -.04, -.4};
        scale.x = red ? -1.8 : 1.8;
        colour = unpack(red ? 0xff40106f : 0x1040ff6f);
    }
    void run() override {
        if (colour.a > .4)
            colour.a *= .99;
        position.x = double(world.polarity) * -.4 + (red ? .04 : -.04);
        if (red ? world.polarityPulse <= 0 : world.polarityPulse >= 0)
            return;
        world.polarityPulse = 0;
        colour.a = 1;
        for (int i = 0; i < 10; ++i) {
            auto *p = new ShotSpark(world);
            p->position = position;
            p->colour = colour;
            p->radius = .01f;
            p->frame = red ? 29 : 28;
            p->velocity = {0, world.random.range(.001, .003)};
            p->velocity.rotateBy(world.random.range(-3.141592654, 3.141592654));
            p->fade = float(world.random.range(.005, .02));
            p->turn = float(world.random.range(-.1, .1));
            world.add(Domain::Decor, p);
        }
    }
};
} // namespace
void Scene::start(int level) {
    world.clearGameplay();
    world.clearMessages();
    speed = 1;
    bossPhase = false;
    bonusState = bonusTimer = 0;
    world.bossPresent = false;
    world.chain = world.oxStreak = world.weaponFollowers = 0;
    world.lifeTokens[0] = world.lifeTokens[1] = 0;
    world.lifeDropBudget = 1;
    world.blueRemaining = world.redRemaining = world.blueTotal = world.redTotal = 0;
    world.terminalTilesRemaining = 4000;
    world.foxFound = false;
    world.healthFade = 1;
    world.scrollDelta = {0, double(.00375f)};
    world.polarityPulse = 0;
    world.gameMode = 1;
    world.backgroundMode = 0;
    if (world.demo)
        world.presentation.beginAttract();
    else
        world.presentation.beginPlay();
    neon::DarwinRandom original(0xf105513e);
    for (auto &seed : seeds)
        seed = original.next();
    world.level = level;
    world.area = level % 10;
    world.random.seed(seeds[size_t(level) & 1023]);
    terrain.init(world.area);
    waves.init(level, seeds);
    world.lives = 3;
    world.groupSize.fill(-1);
    world.groupKills.fill(0);
    world.weaponPower = world.polarity = 0;
    world.enemyShotChance = std::min(255.f, level < 11 ? 40.f : float(level * 10));
    shotChanceStep = (std::min(255.f, 100.f + float(level * 10)) - world.enemyShotChance) /
                     float(original::RowsLength[size_t(world.area)]);
    if (level / 10 > 0)
        world.achieveGoal(6);
    world.player = new Ship(world);
    world.add(Domain::Player, world.player);
    world.add(Domain::Decor, new Gauge(world, false));
    world.add(Domain::Decor, new Gauge(world, true));
    float x = -.45f;
    for (int n = 0; n < 3; ++n) {
        world.add(Domain::Scenery, new Backdrop(world, speed, {double(x), 0}, true));
        x = float(double(x) + .45);
    }
    for (int n = 0; n < 50; ++n)
        world.add(Domain::Scenery, new Backdrop(world, speed, {}, false));
    world.sceneTick = [this] { tick(); };
    world.terrainDraw = [this](auto &out) { terrain.prepare(out); };
    world.launchEnemy = [this](const LaunchPod &p) {
        world.add(Domain::Enemies, new AirEnemy(world, p));
    };
    world.scoreNotice = [this](int points, Vec2 at, int side, Vec2 motion, float radius) {
        world.add(Domain::Decor, new FloatingScore(world, points, at, side, motion, radius));
    };
    world.groupReward = [this](Vec2 at, int mode) {
        if (bossPhase)
            breakup(world, at, 200, mode);
    };
    world.runEnded = [this] { endRun(); };
    terrain.createActor = [this](char c, int tile, int variant) { create(c, tile, variant); };
    terrain.crossed = [this] {
        // 48f94/45c1c: this is a float ramp, not an integer probability cap.
        world.enemyShotChance += shotChanceStep;
        if (!world.bossPresent)
            waves.rowCrossed();
    };
}
void Scene::create(char c, int tile, int variant) {
    terrain.actorGlyph = c == '*' ? 46 : 17;
    if (c == 'A' || c == 'B' || c == 'D' || c == 'Q' || c == 'S') {
        int kind = c == 'B' ? 1 : c == 'D' ? 6 : c == 'Q' ? 0 : 2;
        auto *turret = new GroundEnemy(world, tile, kind);
        if (c == 'S') {
            turret->gunChance = 255;
            turret->burstCount *= 2;
            turret->warmup *= 2;
            turret->cooldown *= 2;
            turret->armour = 5;
        }
        world.add(Domain::Air, turret);
    } else if ((c == 'C' || c == 'G') && variant < 0) {
        const bool blue = (c == 'C') != terrain.mirrored;
        world.add(Domain::Foreground, new BonusTile(world, tile, blue ? 3 : 4));
        if (blue) {
            ++world.blueRemaining;
            ++world.blueTotal;
        } else {
            ++world.redRemaining;
            ++world.redTotal;
        }
    } else if ((c == 'C' || c == 'G') && variant >= 0 && variant < 3) {
        const bool blue = (c == 'C') != terrain.mirrored;
        auto *cannon = new GroundEnemy(world, tile, blue ? 3 : 4);
        if (variant > 0) {
            cannon->radius = .04f;
            cannon->emissionOffset = {};
            cannon->turnRate = 0;
            const int facing = terrain.mirrored ? 3 - variant : variant;
            cannon->angle = facing == 1 ? -1.5707964f : 1.5707964f;
        }
        world.add(Domain::Air, cannon);
    } else if (c == 'E') {
        world.add(Domain::Air, new Eyeball(world, tile));
        bossPhase = true;
        world.bossPresent = true;
    } else if (c == 'N') {
        world.add(Domain::Air, new Nose(world, tile));
        world.bossPresent = true;
    } else if (c == 'H') {
        world.add(Domain::Air, new Ear(world, tile));
        world.bossPresent = true;
        terrain.actorGlyph = tile % TileScreen::Stride < 11 ? 11 : 12;
        terrain.actorTint = (world.area & 1) | 4;
    } else if (c == 'R') {
        world.add(Domain::Air, new Horn(world, tile));
        world.bossPresent = true;
        terrain.actorGlyph = terrain.actorTint = 0;
    } else if (c == 'T' || c == 'U') {
        if (!world.demo) {
            world.add(Domain::Air, new Tooth(world, tile, c == 'U'));
            world.bossPresent = true;
        }
    } else if (c == '*')
        world.add(Domain::Air, new Flower(world, tile));
    else if (c == '?')
        terrain.actorGlyph = 0;
    else if (c == 'F') {
        world.add(Domain::Enemies, new Fence(world, tile, variant));
        if (variant == 1)
            terrain.actorGlyph = terrain.actorTint = 0;
    } else if (c == 'M') {
        terrain.actorGlyph = 31;
        world.add(Domain::Foreground, new Mine(world, tile, terrain.cursor));
    } else if (c == 'P')
        world.add(Domain::Foreground, new Pyramid(world, tile));
    else if (c == 'L')
        world.add(Domain::Foreground, new BonusTile(world, tile, 8));
    else if (c == 'O')
        world.add(Domain::Air, new OxBlock(world, tile));
    else
        world.unsupported("Opening terrain actor " + std::string(1, c));
    if (world.area == 5 && terrain.actorGlyph == 17)
        terrain.actorGlyph = 49;
}
void Scene::tick() {
    beginTick();
    finishTick();
}
void Scene::beginTick() {
    world.weaponPower = float(double(world.weaponPower) * .98);
    world.particleBudget = 5;
    flowerPhase += .002f;
    if (flowerPhase >= 1)
        flowerPhase -= 1;
    world.flowerColour = sequenceColour(flowerPhase, original::FlowerColours.data());
    if (!world.paused) {
        const float loopRate = std::min(1.6f, float(double(world.level / 10) * .2 + 1.));
        terrain.scroll(float(.06f * speed) * loopRate);
        world.scrollDelta.y = double(float(float(speed * .06f) * .0625f));
        waves.tick();
        if (!world.holdPolarity)
            world.polarity = float(double(world.polarity) * .9998);
        world.holdPolarity = false;
        if (world.oxStreak > 0 && --world.oxTimer < 0)
            world.oxStreak = 0;
    }
    const bool playerDead = world.player && world.player->state != Ship::State::Flying;
    if (!bossPhase && playerDead)
        speed = float(double(speed) * .995);
}
void Scene::finishTick() {
    // 421e0 resumes here after its game-over presentation/input block.
    const bool playerDead = world.player && world.player->state != Ship::State::Flying;
    if (!bossPhase) {
        if (!playerDead && speed < 1)
            speed = std::min(1.f, speed + .1f);
    } else {
        speed = float(double(speed) * .989);
        if (!world.bossPresent) {
            if (world.demo || (world.player && world.player->state == Ship::State::GameOver))
                nextLevel();
            else if (bonusTimer == 0)
                beginBonus();
            else
                tallyBonus();
        }
    }
    // Eye run() later in the original scheduler renews this for the next frame.
    if (!world.paused)
        world.bossPresent = false;
}
void Scene::endRun() {
    // 4eed4 halves the current terrain's base and edge colours, including alpha.
    const int bank = world.area & 1;
    for (int n : {bank + 1, bank | 4}) {
        auto &c = terrain.colours[size_t(n)];
        c = {c.r * .5, c.g * .5, c.b * .5, c.a * .5};
    }
    world.ticks = 0;
    world.freshPress = false;
    world.pressLatched = true;
    if (world.level > 0)
        --world.level;
    if (world.bankRun)
        world.bankRun();
}
void Scene::beginBonus() {
    // moo::run 4316e-434ac. Instructions 43270/43274 make the blue
    // champion doubling conditional (the supplied decompilation loses it).
    auto make = [&](int missed, int total, int oppositeMissed, int oppositeTotal, float factor,
                    int current, const char *name) {
        Bonus b;
        const bool champion = missed == 0 && oppositeMissed == oppositeTotal;
        b.detail = missed == 0 ? "NO MISS-SHOOTING" : std::to_string(missed) + " MISS-SHOOTING";
        b.heading = missed == 0 ? (champion ? "CHAMPION BONUS" : "PERFECT BONUS")
                                : std::string(name) + " BONUS";
        if (missed > 5) {
            factor = 0;
            b.detail = "NO BONUS";
            b.heading = "SORRY";
        }
        if (missed == total) {
            factor = 0;
            b.detail = "ALL MISS-SHOOTING";
            b.heading = "SORRY NO BONUS";
        }
        if (champion)
            factor += factor;
        if (missed > 0)
            factor /= float(missed + 1);
        b.remaining = b.initial = int(factor * 5000.f);
        b.steps = b.remaining / 400;
        b.step = b.steps ? b.remaining / b.steps : 0;
        b.finalScore = current + b.remaining;
        return b;
    };
    blueBonus = make(world.blueRemaining, world.blueTotal, world.redRemaining, world.redTotal,
                     float(1.f - world.polarity) + 1.f, world.blueScore, "CAPRICHOSO");
    redBonus = make(world.redRemaining, world.redTotal, world.blueRemaining, world.blueTotal,
                    world.polarity + 2.f, world.redScore, "GARANTIDO");
    if (world.redRemaining == 0 && world.blueRemaining == world.blueTotal)
        world.achieveGoal(2);
    bonusState = 2;
    bonusTimer = 120;
}
void Scene::tallyBonus() {
    if (--bonusTimer > 0)
        return;
    if (bonusState == 1) {
        nextLevel();
        return;
    }
    bonusTimer = 5;
    if (bonusState == 2) {
        bonusState = blueBonus.steps < 1 && redBonus.steps < 1 ? 1 : 3;
        if (bonusState == 1)
            bonusTimer = 1;
        return;
    }
    const bool eligible =
        !world.demo && world.player && world.player->state != Ship::State::GameOver;
    auto add = [eligible](Bonus &b, int &score) {
        if (b.steps > 0) {
            if (eligible)
                score += b.step;
            b.remaining -= b.step;
            if (--b.steps < 1 && eligible)
                score = b.finalScore;
        }
    };
    add(blueBonus, world.blueScore);
    add(redBonus, world.redScore);
    world.score = world.redScore + world.blueScore;
    world.noise(7);
    if (blueBonus.steps < 1 && redBonus.steps < 1)
        bonusState = 1;
}
void Scene::nextLevel() {
    if (!world.demo) {
        ++world.level;
        if (world.polarity == 0 && world.score == 0)
            world.achieveGoal(7);
    }
    if (!world.demo && world.checkpoint)
        world.checkpoint(world.level);
    world.area = world.level % 10;
    world.groupSize.fill(-1);
    world.groupKills.fill(0);
    world.chain = 0;
    if (world.level / 10 > 0)
        world.achieveGoal(6);
    world.blueRemaining = world.redRemaining = world.blueTotal = world.redTotal = 0;
    world.lifeDropBudget = 1;
    bonusTimer = bonusState = 0;
    bossPhase = false;
    world.bossPresent = false;
    waves.init(world.level, seeds);
    // 48b1c changes script/palette bank and refreshes row seeds; it keeps the
    // circular terrain and actors alive while the new area scrolls into view.
    // a098 seeds the global stream, before refreshing the terrain row seeds.
    world.random.seed(seeds[size_t(world.level) & 1023]);
    terrain.init(world.area, true);
    world.enemyShotChance = std::min(255.f, world.level < 11 ? 40.f : float(world.level * 10));
    shotChanceStep = (std::min(255.f, 100.f + float(world.level * 10)) - world.enemyShotChance) /
                     float(original::RowsLength[size_t(world.area)]);
    world.terminalTilesRemaining = 4000;
    world.foxFound = false;
}
} // namespace superoxwars
