#include "native_campaign.h"
#include "native_levels.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace gr::reconstruction::ios {
namespace {
constexpr float speeds[] = {.004f, .0045f, .005f, .0055f, .006f, .0065f, .007f, .0075f,
                            .008f, .0085f, .009f, .0095f, .01f,  .0105f, .011f, .0115f};
constexpr int separations[] = {10, 9, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4};
constexpr int turns[] = {16, 15, 15, 14, 14, 13, 13, 13, 12, 11, 10, 10, 9, 9, 8, 8};
uint32_t interpolate(uint32_t a, uint32_t b, float amount) {
    uint32_t out = 0;
    for (int shift : {24, 16, 8, 0}) {
        const double x = double((a >> shift) & 255) / 255., y = double((b >> shift) & 255) / 255.;
        out |= uint32_t(int32_t((x + double(amount) * (y - x)) * 255.) & 255) << shift;
    }
    return out; // iOS 0x9a78
}
} // namespace
LevelProgram::LevelProgram(int32_t level, bool survival) {
    if (level < 0)
        throw std::invalid_argument("Negative grid index");
    constexpr int survivalWaves[] = {0, 2, 4, 3, 8, 6};
    script_ = levelData::waves[survival ? survivalWaves[level % 6] : level % 23];
    variables['S' - 'A'] = 9999;
    variables['P' - 'A'] = 50;
    variables['I' - 'A'] = 20;
    variables['W' - 'A'] = 5;
    variables['V' - 'A'] = 3;
    variables['Y' - 'A'] = 5;
    variables['Z' - 'A'] = 230;
}
int32_t LevelProgram::number() {
    if (script_[pc_] >= 'A' && script_[pc_] <= 'Z')
        return variables[size_t(script_[pc_++] - 'A')];
    if (script_[pc_] < '0' || script_[pc_] > '9')
        throw std::logic_error("Invalid original level operand");
    int32_t n = 0;
    while (script_[pc_] >= '0' && script_[pc_] <= '9')
        n = add32(int32_t(uint32_t(n) * 10), script_[pc_++] - '0');
    return n;
}
void LevelProgram::step(int32_t priorLive, const std::function<void(int32_t, int32_t)> &launch) {
    if (wait < 0)
        return;
    if (wait > 0) {
        wait = add32(wait, -1);
        if (priorLive < 4)
            wait = 0;
        return;
    }
    for (int guard = 0; guard < 10000; ++guard) {
        const char c = script_[pc_];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
            value = number();
            continue;
        }
        ++pc_;
        switch (c) {
        case '=': {
            const auto v = script_[pc_++];
            if (v < 'A' || v > 'Z')
                throw std::logic_error("Invalid script assignment");
            variables[size_t(v - 'A')] = value;
            break;
        }
        case '+':
            value = add32(value, number());
            break;
        case '-':
            value = add32(value, -number());
            break;
        case '(':
            loops_.push_back({pc_, value});
            break;
        case ')':
            if (loops_.empty())
                throw std::logic_error("Original script loop underflow");
            if (--loops_.back().remaining > 0)
                pc_ = loops_.back().pc;
            else
                loops_.pop_back();
            break;
        case 't':
            launchType = value;
            break;
        case 'l':
            launch(value, launchType);
            break;
        case 'w':
            wait = value;
            return;
        case ':':
            return;
        case 'q':
            wait = -1;
            return;
        default:
            throw std::logic_error("Original level script ended unexpectedly");
        }
    }
    throw std::logic_error("Original level script failed to yield");
}
Campaign::Ships::Ships(Campaign &g)
    : ShipWorld(g.ac, g.lists, g.collision, g.effects, g.random, &g.feedback_), game(g) {}
ShipInput Campaign::Ships::readInput() {
    return {game.input_, true};
}
void Campaign::Ships::sound(int32_t n) {
    game.sound(n);
}
void Campaign::Ships::stopSound(int32_t n) {
    game.sounds.push_back(-1 - n);
}
void Campaign::Ships::finishGame() {
    game.state_ = State::GameOver;
}
void Campaign::Ships::unbindShip(Ship &s) {
    ShipWorld::unbindShip(s);
    if (game.ship == &s) {
        game.ship = nullptr;
        game.shipPosition = nullptr;
    }
    if (game.pickups_.ship == &s)
        game.pickups_.ship = nullptr;
    if (game.pods_.ship == &s)
        game.pods_.ship = nullptr;
}
Campaign::Pickups::Pickups(Campaign &g)
    : PickupWorld(g.ac, g.lists, g.effects, g.random, g.pickupTints), game(g) {}
void Campaign::Pickups::sound(int32_t n) {
    game.sound(n);
}
void Campaign::Pickups::score(int32_t n) {
    game.score(n);
}
Campaign::Pods::Pods(Campaign &g)
    : PodWorld(g.ac, g.lists, g.effects, g.random, g.cells, &g.feedback_), game(g) {}
void Campaign::Pods::sound(int32_t n) {
    game.sound(n);
}
void Campaign::Pods::score(int32_t n) {
    game.score(n);
}
void Campaign::Pods::fireRing(const Entity &from, int32_t count) {
    game.enemies_.fireRing(from, count);
}
void Campaign::Pods::spawnSwarmers(const Entity &from, int32_t count) {
    game.enemies_.spawnSwarmers(from, count);
}
Campaign::Lasers::Lasers(Campaign &g)
    : LaserWorld(g.ac, g.lists, g.effects, g.random, &g.feedback_), game(g) {}
void Campaign::Lasers::sound(int32_t n) {
    game.sound(n);
}
void Campaign::Lasers::makePod(Vec2 p, bool laser) {
    game.makePod(p, laser);
}
void Campaign::Lasers::barrel(Vec2 p) {
    auto object = std::make_unique<Barrel>(game.enemies_);
    object->initialize(p);
    game.lists.prepend(List::L3c8, std::move(object));
}
void Campaign::Lasers::zakato(Vec2 p, Vec2 v) {
    auto object = std::make_unique<Zakato>(game.enemies_);
    object->initialize(p, v);
    game.lists.prepend(List::L3c8, std::move(object));
}
Campaign::Enemies::Enemies(Campaign &g) : EnemyWorld(g, g.lists, g.effects, g.random), game(g) {}
void Campaign::Enemies::sound(int32_t sample, int32_t volume) {
    game.soundAt(sample, volume);
}
Campaign::Campaign(std::function<Vec2(int32_t)> textureSize, uint32_t seed, uint32_t startClock,
                   int lives, bool isDemo, int32_t level, bool survivalMode, int32_t score,
                   DarwinRandom *sharedRandom)
    : ownedRandom(seed), random(sharedRandom ? *sharedRandom : ownedRandom),
      effects{lists, random, palette, std::move(textureSize)}, ships_(*this), pickups_(*this),
      pods_(*this), lasers_(*this), enemies_(*this) {
    if (level < 0)
        throw std::invalid_argument("Negative grid index");
    clock = startClock;
    ac.lives = lives;
    ac.level = level;
    survival = survivalMode;
    demo_ = isDemo;
    points_ = score;
    ships_.demo = isDemo;
    initLevel();
}
void Campaign::initLevel() {
    lists.clear();
    ship = nullptr;
    shipPosition = nullptr;
    pickups_.ship = pods_.ship = nullptr;
    program = LevelProgram(ac.level, survival);
    state_ = State::Playing;
    launches_ = completionTicks_ = 0;
    feedback_ = feedbackColour_ = 0;
    soundGains_.fill(7 << 12);
    sounds.clear();
    cells.assign(1200, 0);
    ac.hitFlash = 0;
    ac.deathRepel = 0;
    ac.liveThreats = 0;
    ac.timeScale = 1;
    ac.completed = ac.shipActive = ac.gameOver = ac.laserFrozen = ac.bottomTrigger =
        ac.leftTrigger = false;
    ac.powers.fill(0);
    ac.laserCharge = std::max(20, 50 - ac.level / 2);
    pods_.clearRow = 0;
    pods_.decayPulse = false;
    lasers_.zakatoCount = ac.level < 8 || (ac.level & 1) ? 0 : 4 - (ac.level & 3);
    lasers_.barrelChance = ac.level <= 8 ? 0 : (ac.level - 8) & (((ac.level - 8) >> 3) + 7);
    enemies_.barrelChance = lasers_.barrelChance;
    // iOS divides in single precision, then multiplies/adds in double.
    lasers_.zakatoSpeed = float(double(float((uint32_t(ac.level) + 8) & 15) / 15.f) * .004 + .003);
    program.step(0, [this](int32_t n, int32_t t) {
        launch(n, t);
    }); // original init colon before entities exist
    laserPeriod_ = laserTimer_ = program.variables['Z' - 'A'];
    podPeriod_ = podTimer_ = laserPeriod_ / 2;
    podQuota_ = program.variables['S' - 'A'];
    ac.dropChance = ac.dropReset = program.variables['P' - 'A'];
    ac.dropIncrement = program.variables['I' - 'A'];
    auto player = std::make_unique<Ship>(ships_);
    player->initialize(false);
    ship = player.get();
    shipPosition = &ship->position;
    lists.prepend(List::L3b0, std::move(player));
    pickups_.ship = pods_.ship = ship;
    lists.prepend(List::L3bc, std::make_unique<LeftLaser>(lasers_));
    lists.prepend(List::L3bc, std::make_unique<BottomLaser>(lasers_));
    ac.headTint.unpack(0xcfff05ff);
    ac.bodyTint.unpack(0x16878fff);
}
bool Campaign::advanceLevel() {
    if (state_ != State::Complete || ac.level == std::numeric_limits<int32_t>::max())
        return false;
    ac.level = add32(ac.level, 1);
    if (!survival)
        ac.gridsWithoutDeath = add32(ac.gridsWithoutDeath, 1);
    if ((ac.level & 1) == 0)
        ac.warpUsed = false;
    initLevel();
    return true;
}
uint32_t Campaign::gridColour() const {
    return levelData::gridColours[(uint32_t(ac.level) >> 2) & 7];
}
Campaign::~Campaign() {
    lists.clear();
}
Ship *Campaign::currentShip() const {
    return dynamic_cast<Ship *>(lists.first(List::L3b0));
}
void Campaign::score(int32_t n) {
    // iOS theGame::score 3f324: no credits in attract mode or after life exhaustion.
    if (ac.lives > 0 && !demo_)
        points_ = add32(points_, n);
}
void Campaign::achievement(int32_t n) {
    // iOS theGame::achieveGoal 0x3f1b0: per-run gate. The second source
    // guard is player count <2; this reconstruction is single-player.
    if (demo_ || n < 0 || n >= 11 || (goalsDone_ & (1u << n)))
        return;
    goalsDone_ |= 1u << n;
    achievements.push_back(n);
}
void Campaign::sound(int32_t n) {
    if (!ships_.demo)
        sounds.push_back(n);
}
void Campaign::soundAt(int32_t n, int32_t volume) {
    sound(n);
    if (n >= 0 && n < 65)
        soundGains_[size_t(n)] = volume;
}
void Campaign::touchEnemyList(Head &h) {
    collision.touch(h, lists.first(List::L3c4));
}
void Campaign::bonus(const HeadBonus &s) {
    if (auto *player = currentShip())
        effects.bonus(s, *player);
    else
        throw std::logic_error("Original bonus requires Ship");
}
void Campaign::pickup(Vec2 p, int32_t kind) {
    auto object = std::make_unique<Powerup>(pickups_);
    object->initialize(p, kind);
    lists.prepend(List::L3c0, std::move(object));
}
void Campaign::makePod(Vec2 p, bool laser) {
    if (madePod_)
        return;
    const int32_t x = std::clamp(int32_t((p.x + .5) * 28), 2, 25), y = int32_t((p.y + .5) * 36),
                  index = y * 29 + x;
    if (index < 0 || index >= int32_t(cells.size()))
        throw std::logic_error("Original pod position outside allocated cell field");
    auto &cell = cells[size_t(index)];
    if (uint32_t(cell - 13) < 6) {
        cell = 16;
        return;
    }
    madePod_ = true;
    const float dx = 1.f / 28.f, dy = 1.f / 36.f;
    const Vec2 centre{double(dx * .5f + dx * float(x)) - .5, double(dy * .5f + dy * float(y)) - .5};
    if (laser) {
        podQuota_ = add32(podQuota_, -1);
        if (podQuota_ <= 0 && p.y < 0) {
            podQuota_ = program.variables['S' - 'A'];
            auto object = std::make_unique<Spod>(pods_);
            object->initialize(
                centre, y,
                add32(program.variables['W' - 'A'], (ac.level / (survival ? 6 : 23)) * 4));
            lists.prepend(List::L3c4, std::move(object));
            return;
        }
    }
    cell = 16;
    const bool alternate = int32_t(random.next() & 511) < ac.level;
    if (alternate) {
        auto object = std::make_unique<Pod2>(pods_);
        object->initialize(centre, index, y, dy * .5f);
        lists.prepend(List::L3c4, std::move(object));
    } else {
        auto object = std::make_unique<Pod>(pods_);
        object->initialize(centre, index, y, dy * .5f);
        lists.prepend(List::L3c4, std::move(object));
    }
}
void Campaign::launch(int32_t count, int32_t type) {
    if (type == 3) {
        auto object = std::make_unique<Cube>(enemies_);
        object->initialize(count, ac.level / (survival ? 6 : 23), program.variables['F' - 'A']);
        lists.prepend(List::L3c4, std::move(object));
        ++launches_;
        return;
    }
    if (type == 4) {
        auto object = std::make_unique<ShrapnelBomb>(enemies_);
        object->initialize(count, ac.level / (survival ? 6 : 23));
        lists.prepend(List::L3c4, std::move(object));
        ++launches_;
        return;
    }
    if (type == 5) {
        auto object = std::make_unique<AnalSurprise>(enemies_);
        object->initialize(count, ac.level / (survival ? 6 : 23));
        lists.prepend(List::L3c4, std::move(object));
        ++launches_;
        return;
    }
    if (type != 1 && type != 2 && type != 6 && type != 7 && type != 8 && type != 9)
        throw std::logic_error("Invalid original launch family");
    const int speed = program.variables['V' - 'A'] & 15;
    HeadSpawn spawn;
    spawn.x = currentShip() && currentShip()->position.x > 0 ? -.5f : .5f;
    spawn.y = float(program.variables['Y' - 'A']) / 36.f - .48f;
    spawn.baseSpeed = speeds[speed];
    spawn.separation = separations[speed];
    spawn.turnTicks = turns[speed];
    if (type == 6) {
        spawn.x = float(random.range(-.35, .35));
        spawn.y = -.412f;
    }
    if (type == 1) {
        spawn.x = (random.next() & 1) ? -.5f : .5f;
        spawn.y = -.352f;
    }
    if (type == 2 || type == 8) {
        spawn.x = float(program.variables['X' - 'A']) / 28.f - .5f;
    }
    spawn.cycle = ac.level / (survival ? 6 : 23);
    count = add32(count, spawn.cycle * 4);
    spawn.headArmour = program.variables['H' - 'A'];
    spawn.bodyArmour = program.variables['B' - 'A'];
    Head *parent = nullptr;
    for (int i = 0; i < count; ++i) {
        auto h = std::make_unique<Head>(*this);
        if (type == 6)
            spawn.randomBit = random.next();
        h->initialize(spawn, parent, type == 6 ? 4 : (type == 8 || type == 9) ? 0x104 : 0);
        parent = h.get();
        lists.prepend(List::L3c4, std::move(h));
    }
    ++launches_;
}
void Campaign::updatePalette(const std::array<Rgba, 33> &p) {
    palette = p;
    for (size_t i = 0; i < 8; ++i)
        pickupTints[i] = palette[i + 6];
    ships_.dyingTint = pods_.dimTint = palette[1];
    lasers_.tint = palette[4];
    lasers_.activeBeamTint = palette[6];
    if (currentShip()) {
        auto &b = ships_.bulletBindings();
        b.power0Tint = pickupTints[0];
        b.power1Tint = pickupTints[1];
        b.power2Tint = pickupTints[2];
    }
}
void Campaign::runController() {
    // iOS ac::runLaser 0x4a238: sequence sees the PREVIOUS pass's count.
    if (ac.deathRepel)
        ac.deathRepel = add32(ac.deathRepel, -1);
    ac.hitFlash = float(double(ac.hitFlash) * .95);
    const uint32_t colour = (uint32_t(ac.level) & 31) >> 2;
    ac.headTint.unpack(interpolate(levelData::headColours[colour],
                                   levelData::headColours[colour + 8], ac.hitFlash));
    ac.bodyTint.unpack(interpolate(levelData::bodyColours[colour],
                                   levelData::bodyColours[colour + 8], ac.hitFlash));
    if (program.wait < 0) {
        if (ac.liveThreats < 1 && ac.shipActive && ac.lives > 0 && !ac.completed) {
            sound(52);
            if (ac.powers[6]) {
                sounds.push_back(-55);
                ac.powers[6] = 0;
            }
            ac.completed = true;
            ac.shipActive = false;
            state_ = State::Completing;
        }
    } else
        program.step(ac.liveThreats, [this](int32_t n, int32_t t) { launch(n, t); });
    ac.liveThreats = 0;
    for (auto &power : ac.powers)
        if (power > 0)
            power = add32(power, -2);
    if (ac.powers[3] > 0) {
        pods_.clearRow = (600 - ac.powers[3]) / 2;
        if (600 - ac.powers[3] > 81)
            pods_.clearRow = 0;
    }
    if (ac.powers[6] == 0)
        ac.timeScale = 1;
    else {
        const float elapsed = float(1800 - ac.powers[6]);
        float f = 0;
        if (elapsed <= 100)
            f = (100.f - elapsed) / 100.f;
        if (elapsed > 1500)
            f = (elapsed - 1500.f) / 300.f;
        ac.timeScale = float(double(f) * .6 + .4);
        feedback_ = float(double(1.f - f) * .5 + .1);
    }
    ac.bottomTrigger = false;
    madePod_ = false;
    if (ac.timeScale >= 0) {
        if (--laserTimer_ < 1) {
            laserTimer_ = laserPeriod_;
            ac.bottomTrigger = true;
            ac.laserFrozen = true;
        }
        pods_.decayPulse = false;
        if (--podTimer_ < 1) {
            podTimer_ = podPeriod_;
            pods_.decayPulse = true;
        }
    }
}
void Campaign::tick(Vec2 directDisplacement) {
    if (state_ == State::Complete || state_ == State::GameOver)
        return;
    input_ = directDisplacement;
    sounds.clear();
    achievements.clear();
    soundGains_.fill(7 << 12);
    ++clock;
    ships_.clock = clock;
    ship = currentShip();
    shipPosition = ship ? &ship->position : nullptr;
    pickups_.ship = pods_.ship = ship;
    // theGame::run precedes ac/entities; all producers write the same +c6c.
    // Later writes replace earlier ones even when their value is smaller.
    if (feedback_ > 0) {
        feedback_ = float(double(feedback_) * .95);
        if (feedback_ < .04f)
            feedback_ = 0;
        feedbackColour_ = feedback_;
    }
    runController();
    enemies_.clearRow = pods_.clearRow;
    enemies_.feedback = 0;
    lists.runUnpaused(1);
    for (auto *n = lists.first(List::L3bc); n; n = n->next)
        if (auto *bolt = dynamic_cast<LaserBolt *>(n))
            bolt->prepareDraw();
    if (state_ == State::Completing) {
        ++completionTicks_;
        if (completionTicks_ == 60) {
            if (!survival)
                ++ac.lives;
            if (ac.lives > 9)
                achievement(7);
        }
        if (completionTicks_ >= 357)
            state_ = State::Complete;
    }
}
} // namespace gr::reconstruction::ios
