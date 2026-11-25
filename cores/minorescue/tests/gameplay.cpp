// Original-execution witnesses for Rescue's weapon and gameplay rules.
#include "game_entities.h"
#include "../rescue_presentation.h"
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace minorescue::Neon_Internals;
namespace {
struct ObservedShipGun : spaceCraftGun {
    int shots = 0;
    void shoot() override {
        ++shots;
    }
};
struct ObservedTankGun : tankGun {
    int shots = 0;
    void shoot() override {
        ++shots;
    }
};
uint32_t bits(float value) {
    uint32_t result;
    std::memcpy(&result, &value, sizeof result);
    return result;
}
void firing(const std::filesystem::path &root) {
    std::ifstream input(root / "ship-fire-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original ship-fire fixture");
    spaceWars game;
    spaceCraft ship;
    ship.game = &game;
    ObservedShipGun gun;
    gun.owner = &ship;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int sun, black, rescues, timer, expectedTimer, expectedShots;
        uint32_t expectedSpeed;
        double x, y, vx, vy;
        if (!(row >> sun >> black >> rescues >> x >> y >> vx >> vy >> timer >> expectedTimer >>
              expectedSpeed >> expectedShots))
            throw std::runtime_error("Malformed firing fixture");
        game.sunOn = sun != 0;
        game.blackHole = black != 0;
        ship.weapon = rescues;
        ship.pos = {x, y};
        ship.vel = {vx, vy};
        gun.timer = timer;
        gun.speed = .25f;
        gun.shots = 0;
        gun.run();
        if (gun.timer != expectedTimer || bits(gun.speed) != expectedSpeed ||
            gun.shots != expectedShots) {
            std::cerr << "Ship-fire mismatch: " << line << "\nActual: " << gun.timer << ' '
                      << bits(gun.speed) << ' ' << gun.shots << '\n';
            throw std::runtime_error("Ship firing differs from original execution");
        }
        ++cases;
    }
    if (cases != 896)
        throw std::runtime_error("Incomplete original firing fixture");
    std::cout << cases << " original ship-fire cases passed\n";
}
void tankFiring(const std::filesystem::path &root) {
    std::ifstream input(root / "tank-fire-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original tank-fire fixture");
    spaceWars game;
    tank vehicle;
    vehicle.game = &game;
    ObservedTankGun gun;
    gun.owner = &vehicle;
    gun.reload = 80;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int demo, cpu, power, timer, burst, expectedTimer, expectedBurst, expectedShots, sample;
        float spin;
        if (!(row >> demo >> cpu >> power >> spin >> timer >> burst >> expectedTimer >>
              expectedBurst >> expectedShots >> sample))
            throw std::runtime_error("Malformed tank fixture");
        game.demo = demo != 0;
        vehicle.type = cpu ? 7 : 1;
        vehicle.powerLevel = power;
        vehicle.spin = spin;
        gun.timer = timer;
        gun.burst = burst;
        gun.shots = 0;
        gun.run();
        if (gun.timer != expectedTimer || gun.burst != expectedBurst || gun.shots != expectedShots)
            throw std::runtime_error("Tank firing differs from original: " + line);
        ++cases;
    }
    if (cases != 480)
        throw std::runtime_error("Incomplete tank fixture");
    std::cout << cases << " original tank-fire cases passed\n";
}
void pickups(const std::filesystem::path &root) {
    std::ifstream input(root / "rescue-pickup-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original pickup fixture");
    spaceWars game;
    game.demo = true;     // Keep audio/achievement presentation out of the state check.
    game.survival = true; // Score thresholds have a separate original-execution fixture.
    spaceCraft ship;
    ship.game = &game;
    ship.group = 1;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int sun, level, rescues, levelRescues, points, player, weapon, nextRescues,
            nextLevelRescues, noRescueLevels, dead;
        double x, y;
        if (!(row >> sun >> level >> x >> y >> rescues >> levelRescues >> points >> player >>
              weapon >> nextRescues >> nextLevelRescues >> noRescueLevels >> dead))
            throw std::runtime_error("Malformed pickup fixture");
        game.reset(0);
        game.sunOn = sun != 0;
        game.mode = 0;
        game.level = level;
        game.rescued = rescues;
        game.rescuedThisLevel = levelRescues;
        game.levelsNoRescue = 7;
        game.scores[0] = 0;
        ship.pos = {x, y};
        ship.weapon = 5;
        collectible pickup;
        pickup.game = &game;
        pickup.size = .03f;
        pickup.hitBy(&ship);
        if (player != 0 || game.scores[0] != points || ship.weapon != weapon ||
            game.rescued != nextRescues || game.rescuedThisLevel != nextLevelRescues ||
            game.levelsNoRescue != noRescueLevels || pickup.dead != (dead != 0))
            throw std::runtime_error("Rescue pickup differs from original: " + line);
        ++cases;
    }
    if (cases != 128)
        throw std::runtime_error("Incomplete pickup fixture");
    std::cout << cases << " original rescue pickup cases passed\n";
}
void scores(const std::filesystem::path &root) {
    std::ifstream input(root / "score-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original score fixture");
    spaceWars game;
    game.demo = true;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int old, points, lives, survival, nextScore, nextLives;
        if (!(row >> old >> points >> lives >> survival >> nextScore >> nextLives))
            throw std::runtime_error("Malformed score fixture");
        game.scores[0] = old;
        game.lives[0] = lives;
        game.survival = survival != 0;
        game.goalsDone = 0;
        game.score(points, 0);
        if (game.scores[0] != nextScore || game.lives[0] != nextLives)
            throw std::runtime_error("Score/life thresholds differ from original: " + line);
        ++cases;
    }
    if (cases != 288)
        throw std::runtime_error("Incomplete score fixture");
    std::cout << cases << " original score/life cases passed\n";
}
void damage(const std::filesystem::path &root) {
    std::ifstream input(root / "ship-damage-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original damage fixture");
    spaceWars game;
    spaceCraft ship;
    ship.game = &game;
    ship.joy = 0;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int demo, shields3, hp, lives, invulnerable, nextHp, nextShields, nextLives, nextWeapon,
            nextInvulnerable, dead;
        float spin;
        uint32_t nextSpin;
        double x, y;
        if (!(row >> demo >> shields3 >> hp >> lives >> spin >> invulnerable >> nextHp >>
              nextShields >> nextLives >> nextWeapon >> nextInvulnerable >> nextSpin >> dead >> x >>
              y))
            throw std::runtime_error("Malformed damage fixture");
        game.reset(0);
        game.demo = demo != 0;
        game.shields3 = shields3 != 0;
        game.shields[0] = hp;
        game.lives[0] = lives;
        ship.hp = hp;
        ship.weapon = 5;
        ship.spin = spin;
        ship.fc = invulnerable;
        ship.dead = false;
        ship.pos = {.2, .3};
        ship.vel = {.003, .004};
        ship.damage();
        if (ship.hp != nextHp || game.shields[0] != nextShields || game.lives[0] != nextLives ||
            ship.weapon != nextWeapon || ship.fc != nextInvulnerable ||
            bits(ship.spin) != nextSpin || ship.dead != (dead != 0) || ship.pos.x != x ||
            ship.pos.y != y)
            throw std::runtime_error("Ship damage differs from original: " + line);
        ++cases;
    }
    if (cases != 144)
        throw std::runtime_error("Incomplete damage fixture");
    std::cout << cases << " original ship-damage cases passed\n";
}
void motion(const std::filesystem::path &root) {
    std::ifstream input(root / "entity-motion-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original motion fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        double x, y, vx, vy, damping, extent, nx, ny, nvx, nvy;
        int noWrap, wrapped, nextWrapped, nextAge;
        if (!(row >> x >> y >> vx >> vy >> damping >> extent >> noWrap >> wrapped >> nx >> ny >>
              nvx >> nvy >> nextWrapped >> nextAge))
            throw std::runtime_error("Malformed motion fixture");
        entity actor;
        actor.pos = {x, y};
        actor.vel = {vx, vy};
        actor.drag = {damping, damping};
        actor.wrapWidth = actor.wrapHeight = extent;
        actor.noWrap = noWrap != 0;
        actor.wrapped = wrapped != 0;
        actor.age = 7;
        actor.run();
        if (actor.pos.x != nx || actor.pos.y != ny || actor.vel.x != nvx || actor.vel.y != nvy ||
            actor.wrapped != (nextWrapped != 0) || actor.age != nextAge)
            throw std::runtime_error("Entity motion differs from original: " + line);
        ++cases;
    }
    if (cases != 128)
        throw std::runtime_error("Incomplete motion fixture");
    std::cout << cases << " original motion cases passed\n";
}
void sunReset(const std::filesystem::path &root) {
    std::ifstream input(root / "sun-reset-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original sun fixture");
    spaceWars game;
    game.demo = true;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int done, black, collapsed, nextCollapsed, nextBlack, hp;
        float size;
        uint32_t nextSize, nextMass;
        double x, y;
        if (!(row >> done >> black >> collapsed >> size >> nextSize >> nextMass >> nextCollapsed >>
              nextBlack >> hp >> x >> y))
            throw std::runtime_error("Malformed sun fixture");
        game.levelDone = done != 0;
        game.blackHole = black != 0;
        theSun sun;
        sun.game = &game;
        sun.collapsed = collapsed != 0;
        sun.size = size;
        sun.mass = 1000000.f;
        sun.pos = {.2, .3};
        sun.hp = 1;
        sun.run();
        if (bits(sun.size) != nextSize || bits(sun.mass) != nextMass ||
            sun.collapsed != (nextCollapsed != 0) || game.blackHole != (nextBlack != 0) ||
            sun.hp != hp || sun.pos.x != x || sun.pos.y != y)
            throw std::runtime_error("Sun reset differs from original: " + line);
        ++cases;
    }
    if (cases != 40)
        throw std::runtime_error("Incomplete sun fixture");
    std::cout << cases << " original sun-reset cases passed\n";
}
void gravity(const std::filesystem::path &root) {
    std::ifstream input(root / "gravity-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original gravity fixture");
    controlBlock game;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        double x, y, tx, ty, rx, ry;
        float mass, otherMass, strength;
        int invulnerable;
        uint32_t nearest;
        if (!(row >> x >> y >> tx >> ty >> mass >> otherMass >> strength >> invulnerable >> rx >>
              ry >> nearest))
            throw std::runtime_error("Malformed gravity fixture");
        entity actor, other;
        actor.pos = {x, y};
        other.pos = {tx, ty};
        actor.mass = mass;
        other.mass = otherMass;
        actor.fc = invulnerable;
        const auto force = game.attract(&actor, &other, strength);
        // Host sqrt implementations can differ in their final binary64 bit.
        if (std::abs(force.x - rx) > 1e-14 || std::abs(force.y - ry) > 1e-14 ||
            bits(actor.nearest) != nearest)
            throw std::runtime_error("Gravity differs from original: " + line);
        ++cases;
    }
    if (cases != 96)
        throw std::runtime_error("Incomplete gravity fixture");
    std::cout << cases << " original gravity cases passed\n";
}
void sunGrowth(const std::filesystem::path &root) {
    std::ifstream input(root / "sun-growth-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original sun-growth fixture");
    spaceWars game;
    game.demo = false;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int level, collapsed, nextCollapsed, black, swallowed;
        float size, mass;
        uint32_t nextSize, nextMass;
        if (!(row >> level >> collapsed >> size >> mass >> nextSize >> nextMass >> nextCollapsed >>
              black >> swallowed))
            throw std::runtime_error("Malformed sun-growth fixture");
        game.reset(0);
        game.level = level;
        game.blackHole = collapsed != 0;
        theSun sun;
        sun.game = &game;
        sun.collapsed = collapsed != 0;
        sun.size = size;
        sun.mass = mass;
        thePlanet planet;
        planet.game = &game;
        planet.type = 2;
        planet.generation = 0; // Keep whole-planet respawn out of this witness.
        sun.hitBy(&planet);
        if (bits(sun.size) != nextSize || bits(sun.mass) != nextMass ||
            sun.collapsed != (nextCollapsed != 0) || game.blackHole != (black != 0) ||
            planet.dead != (swallowed != 0))
            throw std::runtime_error("Sun growth/collapse differs from original: " + line);
        ++cases;
    }
    if (cases != 64)
        throw std::runtime_error("Incomplete sun-growth fixture");
    std::cout << cases << " original sun-growth cases passed\n";
}
} // namespace
void presentation() {
    spaceWars game;
    minorescue::module::PresentationObserver observer;
    spaceCraft ship;
    ship.game = &game;
    ship.type = 1;
    ship.joy = 0;
    ship.drawAngle = -1.57079637f;
    ship.weapon = 13;
    game.players = &ship;
    game.running = true;
    game.demo = false;
    game.state = 4;
    game.subMode = 0;
    observer.beforeTick(game);
    observer.afterTick(game);
    auto state = observer.sample(&game);
    auto number = [&](const char *key) {
        return std::visit(
            [](const auto &x) -> double {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, std::string>)
                    return -1;
                else
                    return double(x);
            },
            observer.sample(&game).values.at(key));
    };
    auto require = [](bool condition) {
        if (!condition)
            throw std::runtime_error("Rescue presentation observation failed");
    };
    require(state.gameplay && state.session == 1 && number("player.fireBoost") == 1);
    require(std::abs(number("player.orientation") - 4.71238898) < 1e-6);
    entity center;
    game.sun = &center;
    game.blackHole = true;
    game.presentationContact(game.presentationContext, ship, center);
    require(number("center.contacts") == 1);
    game.blackHole = false;
    game.presentationContact(game.presentationContext, ship, center);
    require(number("center.contacts") == 1);
    game.sun = nullptr;
    tank first, second;
    first.game = second.game = &game;
    first.type = second.type = 1;
    first.joy = 0;
    second.joy = 1;
    first.next = &second;
    first.powerLevel = 8;
    game.players = &first;
    game.subMode = 1;
    observer.beforeTick(game);
    first.spin = second.spin = 1;
    observer.afterTick(game);
    require(number("player1.hits") == 1 && number("player2.hits") == 1);
    require(number("player.fireBoost") == 1);
    observer.beforeTick(game);
    observer.afterTick(game);
    require(number("player1.hits") == 1);
    entity pickup;
    pickup.type = 4;
    pickup.dead = false;
    game.presentationContact(game.presentationContext, first, pickup);
    require(number("minotaurs.rescued") == 0);
    pickup.dead = true;
    game.presentationContact(game.presentationContext, first, pickup);
    require(number("minotaurs.rescued") == 1 && game.rescued == 0);
    jet plane;
    plane.game = &game;
    plane.type = 1;
    plane.joy = 1;
    plane.hp = 1;
    game.subMode = 2;
    game.players = &plane;
    observer.beforeTick(game);
    plane.hp = 9;
    ++plane.life;
    observer.afterTick(game);
    require(number("player2.hits") == 2 && number("player1.hits") == 1);
    game.paused = true;
    require(observer.sample(&game).paused && observer.sample(&game).session == 1);
    game.demo = true;
    observer.afterTick(game);
    game.demo = false;
    game.paused = false;
    observer.beforeTick(game);
    observer.afterTick(game);
    require(observer.sample(&game).session == 2 && number("player2.hits") == 0);
    game.players = nullptr;
    std::cout
        << "Rescue presentation: heading, boost, contacts, tank/jet hits and sessions passed\n";
}
void widgetPresentation() {
    spaceWars game;
    minorescue::module::PresentationObserver observer;
    game.running = true;
    game.demo = false;
    game.state = 4;
    game.sunOn = true;
    game.lives[0] = 3;
    game.minesHit = 1;
    spaceCraft ship;
    ship.game = &game;
    ship.type = 1;
    ship.pos = {0.25, 0};
    game.players = &ship;
    theSun sun;
    sun.size = 0.1f;
    game.sun = &sun;
    collectible minotaur;
    minotaur.game = &game;
    minotaur.type = 4;

    auto require = [](bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    };
    auto read = [&](const char *key) { return observer.widgets(&game).values.at(key); };
    observer.beforeTick(game);
    observer.afterTick(game);
    require(std::get<double>(read("efficiencyLevel")) == -1., "No rescues have an efficiency yet");
    require(std::abs(std::get<double>(read("sunStability")) - 0.5) < 1e-6,
            "Sun radius should map to the remaining stability");
    require(std::get<int64_t>(read("nextExtra")) == 10000, "First extra-life milestone");
    require(std::get<double>(read("firepower")) == 0.3, "Firepower should use actual shot cadence");
    require(!std::get<bool>(read("minesVisible")) && std::get<int64_t>(read("minesShot")) == 0,
            "Original mine award sequence starts at 1");

    observer.beforeTick(game);
    minotaur.dead = true;
    game.presentationContact(game.presentationContext, ship, minotaur);
    game.presentationContact(game.presentationContext, minotaur, sun);
    ++game.lives[0]; // An extra life and a death can occur in the same frame.
    game.loseShip(0);
    game.minesHit = 2;
    observer.afterTick(game);
    const double efficiency =
        double(collectible::pointsAt(game, 0.25f)) / collectible::pointsAt(game, 0.f);
    require(std::get<double>(read("efficiencyLevel")) == efficiency &&
                std::get<double>(read("efficiencyGame")) == efficiency,
            "A Solar rescue must contribute actual and reference points");
    require(std::get<int64_t>(read("livesLost")) == 1 &&
                std::get<int64_t>(read("minotaursLost")) == 1 && game.lives[0] == 3,
            "Actual loss events must survive net-zero life changes");
    require(std::get<bool>(read("minesVisible")) && std::get<int64_t>(read("minesShot")) == 1,
            "Mine counter must reflect actual shots");

    game.state = 6;
    observer.beforeTick(game);
    game.state = 4;
    game.minesHit = 1;
    observer.afterTick(game);
    require(std::get<double>(read("efficiencyLevel")) == -1. &&
                std::get<double>(read("efficiencyGame")) == efficiency &&
                !std::get<bool>(read("minesVisible")),
            "New level must clear only level-scoped widget state");
    game.sunOn = false;
    game.mode = 1;
    game.survival = true;
    require(!std::get<bool>(read("solar")) && !std::get<bool>(read("nextExtraVisible")) &&
                std::get<int64_t>(read("highScore")) == game.hs.entry(3, 0).score,
            "Deep Space Survival must use its own leaderboard and hide extra lives");
    game.subMode = 1;
    require(!observer.widgets(&game).gameplay, "Tanks should not show ship widgets");
    game.players = nullptr;
    game.sun = nullptr;
}

int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Pass fixture directory");
        firing(argv[1]);
        tankFiring(argv[1]);
        pickups(argv[1]);
        scores(argv[1]);
        damage(argv[1]);
        motion(argv[1]);
        sunReset(argv[1]);
        gravity(argv[1]);
        sunGrowth(argv[1]);
        presentation();
        widgetPresentation();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
