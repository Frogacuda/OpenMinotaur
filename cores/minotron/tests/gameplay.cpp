// Captured original execution, independent of the reconstructed game routines.
#include "game_entities.h"
#include "game_tables.h"
#include "neon_modules.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace minotron::Neon_Internals;
namespace {
uint32_t bits(float value) {
    uint32_t result;
    std::memcpy(&result, &value, sizeof result);
    return result;
}
void compareState(std::istringstream &row, const std::vector<uint32_t> &actual,
                  const std::string &line, const char *mechanic) {
    for (size_t i = 0; i < actual.size(); ++i) {
        uint32_t expected;
        if (!(row >> expected))
            throw std::runtime_error("Malformed state fixture");
        if (actual[i] != expected) {
            std::cerr << mechanic << " field " << i << ": expected " << expected << ", got "
                      << actual[i] << '\n';
            throw std::runtime_error(std::string(mechanic) + " differs from original: " + line);
        }
    }
}
void patterns(const std::filesystem::path &root) {
    std::ifstream input(root / "player-shots-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original shot fixture");
    theGame game;
    playerMinotaur player;
    player.game = &game;
    player.pos = {.1, -.2};
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        double ax, ay;
        float speed;
        int spread, five, count;
        if (!(row >> ax >> ay >> speed >> spread >> five >> count))
            throw std::runtime_error("Malformed shot fixture");
        game.reset(0);
        game.shotSpeed = speed;
        game.spreadShots = spread != 0;
        game.fiveWayShots = five != 0;
        player.aimDir = {ax, ay};
        player.shoot();
        std::vector<basicBull *> shots;
        for (entity *e = game.projectiles; e; e = e->next)
            shots.push_back(static_cast<basicBull *>(e));
        std::reverse(shots.begin(), shots.end());
        if (shots.size() != size_t(count))
            throw std::runtime_error("Shot count mismatch: " + line);
        for (const auto *shot : shots) {
            double x, y, vx, vy, sx, sy;
            int sprite, life;
            if (!(row >> x >> y >> vx >> vy >> sx >> sy >> sprite >> life))
                throw std::runtime_error("Malformed projectile record");
            if (shot->pos.x != x || shot->pos.y != y || shot->vel.x != vx || shot->vel.y != vy ||
                shot->scaleX != sx || shot->scaleY != sy || shot->spriteIndex != sprite ||
                shot->life != life)
                throw std::runtime_error("Shot pattern differs from original: " + line);
        }
        ++cases;
    }
    if (cases != 60)
        throw std::runtime_error("Incomplete shot fixture");
    std::cout << cases << " original shot-pattern cases passed\n";
}
void bullets(const std::filesystem::path &root) {
    std::ifstream input(root / "bullet-motion-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original bullet fixture");
    theGame game;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        double x, y, vx, vy, nx, ny, nvx, nvy, sx;
        int sprite, life, reflective, ending, nextLife, dead;
        if (!(row >> x >> y >> vx >> vy >> sprite >> life >> reflective >> ending >> nx >> ny >>
              nvx >> nvy >> sx >> nextLife >> dead))
            throw std::runtime_error("Malformed bullet fixture");
        game.reflectiveShots = reflective != 0;
        game.levelEnding = ending != 0;
        basicBull shot;
        shot.game = &game;
        shot.pos = {x, y};
        shot.vel = {vx, vy};
        shot.spriteIndex = sprite;
        shot.life = life;
        shot.run();
        if (shot.pos.x != nx || shot.pos.y != ny || shot.vel.x != nvx || shot.vel.y != nvy ||
            shot.scaleX != sx || shot.life != nextLife || shot.dead != (dead != 0))
            throw std::runtime_error("Bullet reflection/lifetime differs from original: " + line);
        ++cases;
    }
    if (cases != 240)
        throw std::runtime_error("Incomplete bullet fixture");
    std::cout << cases << " original bullet-motion cases passed\n";
}
void rescues(const std::filesystem::path &root) {
    std::ifstream input(root / "rescues-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original rescue fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int chain, collectorType, rescued, love, practice, old, total;
        float speed;
        if (!(row >> chain >> collectorType >> rescued >> speed >> love >> practice >> old >>
              total))
            throw std::runtime_error("Malformed rescue fixture");
        theGame game;
        game.demo = true;
        game.levelEnding = true;
        game.practice = practice != 0;
        game.rescueChain = chain;
        game.score[0] = old;
        game.lives[0] = 3;
        game.shotSpeed = speed;
        game.beastLoveFrames = love;
        game.beastAttractionFrames = 3;
        game.extraLove = 2;
        game.f1198[1] = total;
        beast pickup;
        pickup.game = &game;
        pickup.pos = {.1, -.2};
        pickup.rescued = rescued != 0;
        pickup.followingSlot = 2;
        playerMinotaur collector;
        collector.type = collectorType;
        collector.rescued = 4;
        pickup.hitBy(&collector);
        compareState(row,
                     {uint32_t(game.score[0]), game.forcePowerup, uint32_t(game.rescueChain),
                      pickup.rescued, uint32_t(pickup.followingSlot), uint32_t(pickup.rescuerType),
                      uint32_t(collector.rescued), bits(game.shotSpeed),
                      uint32_t(game.hotShotFrames), uint32_t(game.beastLoveFrames),
                      uint32_t(game.beastAttractionFrames), uint32_t(game.f1198[1])},
                     line, "Rescue");
        ++cases;
    }
    if (cases != 1728)
        throw std::runtime_error("Incomplete rescue fixture");
    std::cout << cases << " original rescue cases passed\n";
}
void powerups(const std::filesystem::path &root) {
    std::ifstream input(root / "powerups-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original powerup fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int kind, bonus, old, spread, five, love;
        if (!(row >> kind >> bonus >> old >> spread >> five >> love))
            throw std::runtime_error("Malformed powerup fixture");
        theGame game;
        game.demo = true;
        game.score[0] = old;
        game.lives[0] = 3;
        game.pickupBonusIndex = bonus;
        game.spreadShots = spread != 0;
        game.fiveWayShots = five != 0;
        game.beastLoveFrames = love;
        game.beastAttractionFrames = 3;
        game.extraLove = 2;
        playerMinotaur collector;
        collector.type = 3;
        collector.pos = {.1, -.2};
        powerup pickup;
        pickup.game = &game;
        pickup.kind = kind;
        pickup.hitBy(&collector);
        compareState(row,
                     {uint32_t(game.score[0]), uint32_t(game.lives[0]),
                      uint32_t(game.pickupBonusIndex), uint32_t(game.bonusPickupChain),
                      uint32_t(game.beastLoveFrames), uint32_t(game.beastAttractionFrames),
                      uint32_t(game.hotShotFrames), game.hotShotsPermanent, game.spreadShots,
                      game.fiveWayShots, game.spreadPowerupDue, game.reflectiveShots,
                      game.levelFlags, uint32_t(game.bombsCollected), uint32_t(game.shakeFrames),
                      collector.invincible, bits(collector.invT), bits(collector.invDT),
                      pickup.dead},
                     line, "Powerup");
        ++cases;
    }
    if (cases != 432)
        throw std::runtime_error("Incomplete powerup fixture");
    std::cout << cases << " original powerup cases passed\n";
}
void damage(const std::filesystem::path &root) {
    struct ObservedTarget : entity {
        int hits = 0;
        void hitBy(entity *) override {
            ++hits;
        }
    };
    std::ifstream input(root / "damage-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original damage fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int hp, spread, five, shooterType, hot, level, old, explosive;
        if (!(row >> hp >> spread >> five >> shooterType >> hot >> level >> old >> explosive))
            throw std::runtime_error("Malformed damage fixture");
        theGame game;
        game.demo = true;
        game.spreadShots = spread != 0;
        game.fiveWayShots = five != 0;
        game.hotShotFrames = hot;
        game.level = level;
        game.score[0] = old;
        game.lives[0] = 3;
        game.powerupTimer = 100;
        game.levelFlags = explosive ? 8 : 0;
        entity shot;
        shot.type = shooterType;
        ObservedTarget target;
        target.hp = hp;
        target.pos = {.1, -.2};
        game.damage(shooterType ? &shot : nullptr, &target, 2, 100, 0);
        const auto count = [](entity *list) {
            uint32_t result = 0;
            for (; list; list = list->next)
                ++result;
            return result;
        };
        compareState(row,
                     {uint32_t(target.hp), uint32_t(target.flashFrames), shot.dead,
                      uint32_t(game.score[0]), game.forcePowerup, uint32_t(game.powerupTimer),
                      count(game.projectiles), count(game.particles), uint32_t(target.hits)},
                     line, "Damage");
        ++cases;
    }
    if (cases != 432)
        throw std::runtime_error("Incomplete damage fixture");
    std::cout << cases << " original damage cases passed\n";
}
void death(const std::filesystem::path &root) {
    std::ifstream input(root / "player-death-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original death fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int state, demo, delay, invincible, message;
        if (!(row >> state >> demo >> delay >> invincible >> message))
            throw std::runtime_error("Malformed death fixture");
        cellGrid grid;
        grid.setSize(40, 24);
        theGame game;
        game.grid = &grid;
        game.state = state;
        game.demo = demo != 0;
        game.message = message;
        game.deathsThisRun = 2;
        playerMinotaur player;
        player.game = &game;
        player.drawDelay = delay;
        player.invincible = invincible != 0;
        player.kill();
        compareState(row,
                     {uint32_t(game.deathPauseFrames), uint32_t(game.deathsThisRun),
                      uint32_t(game.message), uint32_t(game.msgTimer)},
                     line, "Death gating");
        ++cases;
    }
    if (cases != 48)
        throw std::runtime_error("Incomplete death fixture");
    std::cout << cases << " original death-gating cases passed\n";
}
void progression(const std::filesystem::path &root) {
    struct StationaryBeast : beast {
        void run() override {}
    };
    std::ifstream input(root / "progression-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original progression fixture");
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        int level, messageTimer, drained, enemyCount, herd, deaths, mode;
        if (!(row >> level >> messageTimer >> drained >> enemyCount >> herd >> deaths >> mode))
            throw std::runtime_error("Malformed progression fixture");
        cellGrid grid;
        grid.setSize(40, 24);
        std::vector<uint32_t> save(0x10000 / 4);
        theGame game;
        game.grid = &grid;
        game.saveData = reinterpret_cast<uint8_t *>(save.data());
        game.gridConfigured = true;
        game.savedVolumeLoaded = true;
        game.running = true;
        game.demo = false;
        game.state = 4;
        game.gameMode = mode;
        game.level = level;
        game.msgTimer = messageTimer;
        game.spawnQueueDrained = drained != 0;
        game.liveEnemyCount = enemyCount;
        game.levelFlags = herd ? 16 : 0;
        game.lives[0] = 3;
        game.score[0] = 500;
        game.deathsThisRun = deaths;
        if (herd) {
            auto *animal = new StationaryBeast();
            animal->game = &game;
            animal->type = 1;
            animal->rescued = herd == 2;
            game.beasts = animal;
        }
        game.run();
        compareState(row,
                     {uint32_t(game.state), uint32_t(game.level), game.levelEnding,
                      uint32_t(game.goalsDone), uint32_t(game.message), uint32_t(game.msgTimer),
                      game.newLevelBest, uint32_t(game.shellCommand)},
                     line, "Level progression");
        ++cases;
    }
    if (cases != 960)
        throw std::runtime_error("Incomplete progression fixture");
    std::cout << cases << " original progression cases passed\n";
}
void movement(const std::filesystem::path &root) {
    std::ifstream input(root / "movement-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original movement fixture");
    theGame game;
    int cases = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string method;
        double x, y, vx, vy, nx, ny, nvx, nvy;
        float heading;
        int target, edge, dead;
        uint32_t nextHeading, distance;
        if (!(row >> method >> x >> y >> vx >> vy >> heading >> target >> edge >> nx >> ny >> nvx >>
              nvy >> nextHeading >> distance >> dead))
            throw std::runtime_error("Malformed movement fixture");
        entity actor, other;
        actor.pos = {x, y};
        actor.vel = {vx, vy};
        actor.drag = {.98, .96};
        actor.speedLimitX = .01;
        actor.speedLimitY = .012;
        actor.seekAccelX = .0002;
        actor.seekAccelY = .0003;
        actor.seekSpeed = .004f;
        actor.seekTurnRate = .08f;
        actor.seekHeading = heading;
        actor.size = .02f;
        actor.elasticity = .75f;
        other.pos = {-.2, .3};
        if (method == "inertialSeek")
            game.inertialSeek(&actor, target ? &other : nullptr, edge);
        else if (method == "vectorSeek")
            game.vectorSeek(&actor, target ? &other : nullptr, edge);
        else if (method == "moveTowards")
            game.moveTowards(&actor, target ? &other : nullptr, edge);
        else
            throw std::runtime_error("Unknown movement method");
        if (std::abs(actor.pos.x - nx) > 1e-12 || std::abs(actor.pos.y - ny) > 1e-12 ||
            std::abs(actor.vel.x - nvx) > 1e-12 || std::abs(actor.vel.y - nvy) > 1e-12 ||
            bits(actor.seekHeading) != nextHeading || bits(actor.targetDistance) != distance ||
            actor.dead != (dead != 0)) {
            std::cerr.precision(17);
            std::cerr << "Actual: " << actor.pos.x << ' ' << actor.pos.y << ' ' << actor.vel.x
                      << ' ' << actor.vel.y << ' ' << bits(actor.seekHeading) << ' '
                      << bits(actor.targetDistance) << ' ' << actor.dead << '\n';
            throw std::runtime_error("Enemy movement differs from original: " + line);
        }
        ++cases;
    }
    if (cases != 324)
        throw std::runtime_error("Incomplete movement fixture");
    std::cout << cases << " original movement cases passed\n";
}
template <class T, size_t N> uint64_t tableHash(const T (&table)[N]) {
    static_assert(sizeof(T) == 4);
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (const auto &value : table) {
        uint32_t word;
        std::memcpy(&word, &value, sizeof word);
        for (int shift = 0; shift < 32; shift += 8) {
            hash ^= (word >> shift) & 255;
            hash *= 0x100000001b3ULL;
        }
    }
    return hash;
}
void tables(const std::filesystem::path &root) {
    struct Table {
        const char *name;
        int words;
        uint64_t hash;
    };
    const Table tables[] = {
        {"levelDescriptions", 2000, tableHash(levelDescriptions)},
        {"gameFlags", 100, tableHash(gameFlags)},
        {"minGruntDelay", 100, tableHash(minGruntDelay)},
        {"maxGruntDelay", 100, tableHash(maxGruntDelay)},
        {"gruntSpeeds", 100, tableHash(gruntSpeeds)},
        {"gruntSpeedMultipliers", 100, tableHash(gruntSpeedMultipliers)},
        {"stepSpeedAdders", 100, tableHash(stepSpeedAdders)},
        {"maxGruntSpeeds", 100, tableHash(maxGruntSpeeds)},
        {"extraLoveDurations", 100, tableHash(extraLoveDurations)},
        {"gruntBaseGraphic", 100, tableHash(gruntBaseGraphic)},
        {"gruntAnimationType", 100, tableHash(gruntAnimationType)},
    };
    std::ifstream input(root / "level-tables-ios.tsv");
    if (!input)
        throw std::runtime_error("Cannot read original level-table fixture");
    size_t count = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string name;
        int words;
        uint64_t hash;
        if (!(row >> name >> words >> hash) || count >= std::size(tables))
            throw std::runtime_error("Malformed table fixture");
        const auto &table = tables[count++];
        if (name != table.name || words != table.words || hash != table.hash)
            throw std::runtime_error("Level table differs from original: " + name);
    }
    if (count != std::size(tables))
        throw std::runtime_error("Incomplete table fixture");
    std::cout << count << " original level-table hashes passed\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Pass fixture directory");
        RenderSession renderer;
        ScopedRenderSession binding(renderer);
        patterns(argv[1]);
        bullets(argv[1]);
        rescues(argv[1]);
        powerups(argv[1]);
        damage(argv[1]);
        death(argv[1]);
        progression(argv[1]);
        movement(argv[1]);
        tables(argv[1]);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
