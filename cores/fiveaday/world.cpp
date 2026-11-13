#include "neon/particles.h"
#include "world.h"
#include "original_tables.h"
#include <algorithm>
#include <stdexcept>
#include <cstdio>

namespace fiveaday {
Colour unpack(uint32_t v) {
    return {double(v >> 24) / 255, double((v >> 16) & 255) / 255, double((v >> 8) & 255) / 255,
            double(v & 255) / 255};
}
uint32_t interpolatePacked(uint32_t low, uint32_t high, float progress) {
    // iclerp 0x100dc interpolates normalized binary64 channels, then truncates
    // back to bytes. Interpolating the byte integers in float changes rounding.
    uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const double a = double((low >> shift) & 255) / 255.,
                     b = double((high >> shift) & 255) / 255.;
        result |= uint32_t(int((a + (b - a) * double(progress)) * 255.) & 255) << shift;
    }
    return result;
}
void Entity::prepare(std::vector<ActorDraw> &out) {
    if (culled) {
        culled = false;
        return;
    }
    if (frame >= 0)
        out.push_back({position, scale, radius, angle, frame, colour});
}
void Wrapped::run() {
    if (targetRadius < radius)
        radius = std::max(targetRadius, float(double(radius) * .95));
    angle = float(float(world.cameraAngle + spinOffset) + heading);
    retained += velocity;
    retained += world.camera * double(cameraCoupling);
    wrapped = 0;
    if (double(wrapX) < retained.x) {
        wrapped |= 1;
        retained.x += double(float(wrapX * -2.f));
    }
    if (retained.x < double(-wrapX)) {
        wrapped |= 2;
        retained.x += double(float(wrapX + wrapX));
    }
    if (double(wrapY) < retained.y) {
        wrapped |= 4;
        retained.y += double(float(wrapY * -2.f));
    }
    if (retained.y < double(-wrapY)) {
        wrapped |= 8;
        retained.y += double(float(wrapY + wrapY));
    }
    position = retained;
}
void Wrapped::prepare(std::vector<ActorDraw> &out) {
    const double limit = double(cullMargin) + .5;
    culled = position.x < -limit || position.x > limit || position.y < -limit ||
             position.y > limit || colour.a < .015;
    if (!culled)
        Entity::prepare(out);
    if (floating)
        culled = false;
}
World::World(uint32_t seed) : random(seed), ship(*this), scoreDisplay(*this) {}
World::~World() {
    for (auto **list : {&bullets, &effects, &fruits, &enemies, &shots, &clouds, &scenery, &hud})
        neon::entities::destroyList(*list, [](Entity *e) { delete e; });
}
void World::start(int day, int startingLives, int startingScore, bool demonstration) {
    demo = demonstration;
    openingTutorial = !demo;
    repairHintPending = bossHintPending = true;
    // Real play goes through theGame::reset (scene +0x32 cleared). Returning
    // to attract mode uses tPi::newGame and lets retained letters finish.
    if (!demo)
        letterFeedbackPending = false;
    for (auto **list : {&bullets, &effects, &fruits, &enemies, &shots, &clouds, &scenery, &hud})
        neon::entities::destroyList(*list, [](Entity *e) { delete e; });
    // theGame's colour/reflection clock survives newGame/reset. goGameOver
    // explicitly resets it; title/day selection must not reset its phase.
    level = day * 5 - 2;
    score = startingScore;
    lives = startingLives;
    fruitCount = fruitSets = population = 0;
    formationKills = formations = 0;
    sequencePaused = gameOver = paused = bossLaunched = bossDefeated = false;
    cleanStage = true;
    goal = -1;
    awardMessages.clear();
    levelEndedTicks = 0;
    checkpoint.clear();
    camera = {};
    cameraAngle = 0;
    progress = 0;
    week = month = year = 0;
    scoreMultiplier = 1;
    sky = original::skycols[0];
    cloudColour = original::cloudcols[0];
    ship.init();
    for (int n = 0; n < 48; ++n)
        add(clouds, new Cloud(*this));
    week = float(float((level + 1) % 5) / 5.f);
    month = float((level + 1) / 5 % 6);
    year = float((level + 1) / 30);
    gameOverTicks = 0;
    gameOverFade = 0;
    ordinaryFruitTotal = powerFruitTotal = 0;
    perfectDay = true;
    messages = {};
    input = {};
    scoreDisplay.text = "      00";
    scoreDisplay.lastScore = 0;
    scoreDisplay.colour.a = 1;
    newLevel();
}
void World::newLevel() {
    ++level;
    if (demo)
        level = 0;
    if (level >= 0)
        openingTutorial = false;
    checkpointDay();
    if ((level + 1) % 5 == 0)
        perfectDay = true;
    population = 0;
    spawnTicks = 10;
    spawnInterval = 120;
    populationLimit = 3;
    bossLaunched = bossDefeated = false;
    bonusRemaining = 0;
    formationKills = 0;
    cleanStage = true;
    ship.init();
    script = demo ? original::DemoLevel : original::Levels[size_t(std::max(level + 1, 0) % 30)];
    scriptCursor = 0;
    while (script.at(scriptCursor) != ',') {
        obey(script.at(scriptCursor));
        ++scriptCursor;
    }
    ++scriptCursor;
    int count = 0;
    for (auto at = scriptCursor; script.at(at) != ':'; ++at) {
        const auto c = script.at(at);
        if (unsigned(c - '0') > 9 && c < 'a')
            ++count;
    }
    progressStep = 1.f / float(count);
    progress = 0;
    week = float(double(week) + .2);
    if (week >= 1)
        week -= 1.f;
    month = float(((level + 1) / 5) % 6);
    year = float((level + 1) / 30);
    sky = original::skycols[size_t(std::max(level, 0) % 30)];
    cloudColour = original::cloudcols[size_t(std::max(level, 0) % 30)];
    if (demo)
        return;
    messages = {};
    char label[64];
    const int date = level + 1;
    std::snprintf(label, sizeof label, "%s, %d %s", original::weekdays[size_t((date / 5) % 6)],
                  (date / 5) % 30 + 1, original::months[size_t((date / 150) % 6)]);
    message(9, label, 300);
    message(11, original::times[size_t(date % 5)], 300);
}

Launch World::launch() {
    Launch p;
    p.heading = float(random.range(-Pi, Pi));
    const double high = double(week) * .05 + 1. + double(month) * .1;
    p.speedScale = float(random.range(.8, high));
    p.turnScale = float(random.range(.8, high));
    p.orbitRadius =
        float(random.range(-.1, std::max(double(float(month * -.3f + .5f)), double(.2f))));
    p.orbitGain = float(random.range(2., 6.));
    p.position = {random.range(-.8, .8), random.range(-.8, .8)};
    const auto side = random.next();
    p.formation = int(random.next() & 3);
    if ((side & 1) == 0) {
        if ((side & 7) == 0) {
            p.position.x = -.8;
            p.sideSign = 1;
        } else {
            p.position.x = .8;
            p.sideSign = -1;
        }
    } else {
        p.horizontal = true;
        if ((side & 3) == 0) {
            p.position.y = -.8;
            p.sideSign = 1;
        } else {
            p.position.y = .8;
            p.sideSign = -1;
        }
    }
    return p;
}
void World::obey(char command) {
    const auto p = currentLaunch =
        launch(); // Also consumed by headers and no-op characters in 0x62c60.
    if (command == 'd' || command == 'm') {
        int value = 0;
        while (scriptCursor + 1 < script.size() && script[scriptCursor + 1] >= '0' &&
               script[scriptCursor + 1] <= '9')
            value = value * 10 + script[++scriptCursor] - '0';
        if (command == 'd')
            spawnInterval = value;
        else
            populationLimit = value;
        spawnTicks = 0;
        return;
    }
    if (command == '.') {
        auto *e = new Attacker(*this);
        e->position = p.position;
        e->init(p);
        e->heading = p.heading;
        add(enemies, e);
        return;
    }
    if (command == 'S' || command == 'L' || command == 'T') {
        if (command == 'L')
            noise(23, 13);
        if (command != 'T')
            noise(6);
        formationKills = 0;
        for (int n = 0; n < 5; ++n) {
            auto *e = new Attacker(*this);
            e->formationMember = e->carrier = true;
            const auto base = size_t(p.formation * 10 + n * 2);
            const double a = double(original::Formation[base]) * .12,
                         b = double(original::Formation[base + 1]) * .12;
            e->position =
                p.position + (p.horizontal ? Vec2{b, a * p.sideSign} : Vec2{a * p.sideSign, b});
            e->init(p);
            e->reward = 0;
            e->formationLocked = true;
            e->turnStep = float(double(e->turnStep) * (double(week) * .6 + .5));
            e->tailDecay = float(double(week) * .2 + .012 + double(std::min(year, 4.f)) * .1);
            e->speed = float(double(e->speed) * (double(week) * .5 + .5));
            e->formationColour = 0;
            e->aimThreshold = .5f;
            e->shotChance = int(year) * 20 + int(month) * 10;
            auto *fruit = new Fruit(*this, *e);
            add(fruits, fruit);
            e->heading = p.heading;
            add(enemies, e);
        }
        return;
    }
    if (command == 'B') {
        add(enemies, new Bomber(*this, p, {0, 0, 0, 0}));
        return;
    }
    if (command == 'J') {
        add(enemies, new Jellyfish(*this, p));
        return;
    }
    if (command == 'P') {
        add(enemies, new PinkPlane(*this, p));
        return;
    }
    if (command == 'D') {
        add(enemies, new DecoyCloud(*this, p));
        return;
    }
    if (command == 'V')
        return; // Original opcode is a no-op, still consuming launch draws.
    if (command == 'N') {
        add(enemies, new Miner(*this, p));
        return;
    }
    if (command == 'M') {
        noise(48);
        auto *e = new MinotaurKiller(*this, p);
        auto *f = new Fruit(*this, *e);
        f->spin = float(random.range(.02, .02));
        f->frame = 32;
        f->minotaur = true;
        f->scale = {1, 1.6};
        f->radius = .03f;
        add(fruits, f);
        add(enemies, e);
        return;
    }
    if (command == ':') {
        auto *boss = new Boss(*this, p, {false});
        add(enemies, boss);
        bossLaunched = true;
        scriptCursor = script.size();
        if (openingTutorial && bossHintPending) {
            bossHintPending = false;
            message(22, "DESTROY BOSS WITH", 100);
            message(24, "SHOTS OR FRUIT TO", 100);
            message(26, "COMPLETE LEVEL", 100);
        }
        return;
    }
    if (command == ' ' || command == ',')
        return;
    checkpoint = std::string("Untranslated campaign command: ") + command;
}
void World::sequence() {
    if (bonusRemaining != 0 && --bonusTicks < 1) {
        bonusTicks = bonusInterval;
        bonusInterval = std::max(5, bonusInterval - 1);
        add(effects, new BonusFruit(*this));
        --bonusRemaining;
    }
    if (bossDefeated && !bossLaunched && bonusRemaining < 1)
        newLevel();
    if (sequencePaused || ship.state == Ship::State::Entering)
        return;
    // 0x64e24: the population cap holds the countdown; equality still waits
    // for its remaining interval.
    if (population > populationLimit)
        return;
    if (--spawnTicks <= 0) {
        spawnTicks = spawnInterval;
        if (scriptCursor < script.size()) {
            progress = std::min(1.f, float(progress + progressStep));
            // iclerp truncates each channel before repacking it.
            if (level >= 0) {
                const auto mix = [&](const auto &colours) {
                    return interpolatePacked(colours[size_t(level % 30)],
                                             colours[size_t(level % 30 + 1)], progress);
                };
                sky = mix(original::skycols);
                cloudColour = mix(original::cloudcols);
            }
            const char command = script[scriptCursor];
            obey(command);
            ++scriptCursor;
        }
    }
}
void World::tick(const Input &controls) {
    if (letterFeedbackTicks > 0)
        --letterFeedbackTicks;
    if (!checkpoint.empty())
        return;
    if (paused) {
        // doxmsg and the 0x304 UI list run even while the gameplay lists pause.
        advanceMessages();
        neon::entities::runAndCull(hud, [](Entity *e) { delete e; });
        return;
    }
    if (!demo)
        input = controls;
    else if (enemies)
        input.direction = enemies->position;
    ++ticks;
    if (levelEndedTicks > 0)
        --levelEndedTicks;
    bool holdSequence = false;
    if (letterFeedbackPending) {
        holdSequence = letterFeedbackTicks > 0;
        if (!holdSequence) {
            letterFeedbackPending = false;
            const int gain = 8 * int(random.range(512., 2047.));
            // Unlike ordinary demo sound, this owner feedback is unconditional:
            // retained game-over letters also make it on their title departure.
            if (sound)
                sound(59, gain);
            letterFeedbackTicks = int(random.range(5., 10.));
        }
    }
    if (gameOver)
        ++gameOverTicks;
    if (gameOver && !holdSequence) {
        sequencePaused = true;
        const float old = gameOverFade;
        gameOverFade = std::min(1.f, float(double(gameOverFade) + .01));
        if (old < 1 && gameOverFade == 1)
            noise(60, 14);
        auto mix = [&](uint32_t a, uint32_t b) { return interpolatePacked(a, b, gameOverFade); };
        sky = mix(finalSky, original::skycols[0]);
        cloudColour = mix(finalCloud, original::cloudcols[0]);
    } else if (!gameOver && !holdSequence)
        sequence();
    if (!checkpoint.empty())
        return;
    advanceMessages();
    ship.run();
    // runUnpausedEntities 0x54f60: ship; effects; player shots; fruit;
    // attackers; auxiliary list; hostile shots; clouds. Newborns retain list order.
    for (auto **list : {&effects, &scenery, &bullets, &fruits, &enemies})
        neon::entities::runAndCull(*list, [](Entity *e) { delete e; });
    neon::entities::runAndCull(shots, [](Entity *e) { delete e; });
    // Score display is the head of the original 0x2e4 list, before its clouds.
    scoreDisplay.run();
    for (auto **list : {&clouds, &hud})
        neon::entities::runAndCull(*list, [](Entity *e) { delete e; });
}
void World::touch(Entity &source, Entity *targets) {
    if (source.dead || source.culled)
        return;
    const Vec2 position = source.position;
    for (auto *e = targets; e; e = e->next) {
        if (e == &source || e->dead)
            continue;
        const float distance = float((position - e->position).length());
        if (distance < float(source.radius + e->radius) && !e->culled && e->collidable)
            e->hitBy(source);
    }
}
Vec2 World::attractShip(Vec2 position, float mass, float strength) const {
    // controlBlock::attract 0xff6c, for the single-player target list used here.
    const Vec2 scaled = (ship.position - position) * 512.;
    const double distance = scaled.length();
    const float radius = float(distance);
    float force = float(mass * .000001f);
    force *= strength;
    force /= float(radius * radius);
    const Vec2 normal = distance > 0 ? Vec2{scaled.x / distance, scaled.y / distance} : Vec2{};
    return normal * double(force);
}
Explosion &World::explode(Vec2 position, float radius, float step, float growth, int frame) {
    if (frame < 0)
        frame = int(month) % 3 == 1 ? 29 : 6;
    auto *e = new Explosion(*this);
    e->position = position;
    e->radius = radius;
    e->progressStep = step;
    e->growth = growth;
    e->frame = frame;
    e->angle = float(random.range(0, Pi));
    if (frame == 29)
        e->colour = {1, 1, 0, 1};
    add(effects, e);
    return *e;
}
void Explosion::run() {
    position += velocity;
    position += world.camera;
    progress += progressStep;
    neon::particles::retireProgress(progress, dead, neon::particles::ProgressEnd::AboveAndClamp);
    if (frame == 51)
        colour = world.palette(4);
    if (frame == 29)
        colour = unpack(interpolatePacked(0xffff00ff, 0x8000ffff, progress));
    colour.a = double(float(1.f - progress));
    radius += growth;
}
void Glow::run() {
    position += world.camera;
    colour.a = colour.a * double(fade) - .001;
    if (colour.a < .04)
        dead = true;
}
void World::flash(Vec2 position, int colour, float amount) {
    // tPi::flash affects existing clouds in place; no substitute particle births.
    for (auto *e = clouds; e; e = e->next) {
        auto &cloud = static_cast<Cloud &>(*e);
        const auto distance = float((cloud.position - position).length());
        if (distance <= amount) {
            cloud.flash = 1;
            const auto c = uint32_t(colour) < 16 ? palette(colour) : unpack(uint32_t(colour));
            const double t = double(float(1.f - distance / amount));
            cloud.flashColour = {cloud.colour.r + (c.r - cloud.colour.r) * t,
                                 cloud.colour.g + (c.g - cloud.colour.g) * t,
                                 cloud.colour.b + (c.b - cloud.colour.b) * t,
                                 cloud.colour.a + (c.a - cloud.colour.a) * t};
        }
    }
}
void World::prepareForeground(std::vector<ActorDraw> &out, bool drawHud) {
    out.clear();
    for (auto *e = bullets; e; e = e->next)
        e->prepare(out);
    for (auto *e = shots; e; e = e->next)
        e->prepare(out);
    for (auto *e = enemies; e; e = e->next)
        e->prepare(out);
    for (auto *e = fruits; e; e = e->next)
        e->prepare(out);
    for (auto *e = scenery; e; e = e->next)
        e->prepare(out);
    for (auto *e = effects; e; e = e->next)
        e->prepare(out);
    ship.prepare(out);
    if (!demo && drawHud)
        scoreDisplay.prepare(out);
    for (auto *e = clouds; e; e = e->next)
        e->prepare(out);
    if (drawHud)
        for (auto *e = hud; e; e = e->next)
            e->prepare(out);
}
} // namespace fiveaday
