// Numeric witnesses from execution of the original iOS gameplay routines.
#include "campaign_actors.h"
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace goatup2;
namespace {
uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
std::unique_ptr<StageRuntime> arena() {
    TerrainBounds bounds;
    auto layout = calculateTerrainLayout(100, 100, 21, 40, {1, 1}, &bounds);
    std::vector<uint32_t> cells(10000);
    auto w = std::make_unique<StageRuntime>(layout, bounds, cells, cells,
                                            StageAllocationInputs{0, 0, 0, false, 0},
                                            MediumSharedState{0, 0, layout.cellSize.y, 8, true}, 1);
    w->spriteScales.resize(188, {1, 1});
    w->spawnGoat({0, 0}, false, {1, 1});
    return w;
}
void compare(std::istringstream &in, std::initializer_list<uint32_t> fields,
             const std::string &line) {
    size_t field = 0;
    for (auto actual : fields) {
        uint32_t expected;
        if (!(in >> expected))
            throw std::runtime_error("Malformed original fixture");
        if (actual != expected) {
            std::cerr << "Field " << field << ": expected " << expected << ", got " << actual
                      << '\n';
            throw std::runtime_error("Original gameplay mismatch: " + line);
        }
        ++field;
    }
}
template <class F> void rows(const std::filesystem::path &p, int expected, F check) {
    std::ifstream in(p);
    if (!in)
        throw std::runtime_error("Missing " + p.string());
    int count = 0;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        check(row, line);
        ++count;
    }
    if (count != expected)
        throw std::runtime_error("Incomplete " + p.string());
    std::cout << count << " original " << p.filename().string() << " cases passed\n";
}
void contacts(const std::filesystem::path &root) {
    rows(root / "original-goat-contact.tsv", 360, [](auto &row, const auto &line) {
        int category, sprite, state, inv;
        row >> category >> sprite >> state >> inv;
        auto w = arena();
        EnemyActor other(*w, {0, 0}, 4, 0);
        other.category = category;
        other.sprite = sprite;
        w->goat->state = state;
        w->goat->invulnerability = inv;
        w->goat->deathSoundSuppressed = true;
        w->goat->onContact(&other);
        compare(row,
                {w->goat->hazard, uint32_t(w->hazardContactCount), w->goat->deathSoundSuppressed},
                line);
    });
}
void damage(const std::filesystem::path &root) {
    rows(root / "original-enemy-damage.tsv", 128, [](auto &row, const auto &line) {
        float health, power;
        int category, protectedState, multiplier;
        row >> health >> power >> category >> protectedState >> multiplier;
        auto w = arena();
        EnemyActor enemy(*w, {0, 0}, 4, 0);
        EnemyActor other(*w, {0, 0}, 4, 0);
        enemy.contactHealth = health;
        enemy.headbuttState = protectedState;
        other.category = category;
        other.attackPower = power;
        w->multiplier = multiplier;
        enemy.hitBy(&other);
        compare(row,
                {bits(enemy.contactHealth), enemy.dead, uint32_t(enemy.freezeTicks),
                 uint32_t(w->score), uint32_t(w->stageTotal), uint32_t(w->dropFruitSequence)},
                line);
        // Original insertions count only reward fruit and label: visual
        // explosion was isolated while capturing its caller's decisions.
        uint32_t inserted;
        row >> inserted;
        if (inserted != (enemy.dead ? 2U : 0U))
            throw std::runtime_error("Reward insertion mismatch");
    });
}
void pickups(const std::filesystem::path &root) {
    rows(root / "original-pickups.tsv", 180, [](auto &row, const auto &line) {
        int category, disabled, multiplier;
        float hunger;
        row >> category >> hunger >> disabled >> multiplier;
        auto w = arena();
        PickupActor pickup(*w, w->allocation, {0, 0}, 1);
        pickup.category = category;
        w->goat->invulnerability = 0;
        w->goat->hunger = hunger;
        w->headbuttDisabled = disabled != 0;
        w->multiplier = multiplier;
        w->requiredFruit = 5;
        pickup.hitBy(w->goat.get());
        compare(row,
                {bits(w->goat->hunger), uint32_t(w->score), uint32_t(w->stageTotal),
                 uint32_t(w->highScore), uint32_t(w->recordCounter), w->newRecord,
                 uint32_t(w->requiredFruit), uint32_t(w->sharedGrace), w->headbuttDisabled,
                 uint32_t(w->goat->followingCount), uint32_t(pickup.headbuttState), pickup.dead,
                 uint32_t(w->goat->invulnerability), uint32_t(w->bonusPickups)},
                line);
    });
}
void playerTraces(const std::filesystem::path &root) {
    std::unique_ptr<StageRuntime> w;
    int previous = -1;
    rows(root / "original-player-traces.tsv", 880, [&](auto &row, const auto &line) {
        int scenario, tick, jump;
        double movement, x, y, vx, vy;
        row >> scenario >> tick >> movement >> jump >> x >> y >> vx >> vy;
        if (scenario != previous) {
            w = arena();
            previous = scenario;
            if (scenario == 3)
                std::fill(w->working.begin(), w->working.end(), 43U);
            else {
                uint32_t surface = scenario == 6   ? 0x30059U
                                   : scenario == 7 ? 0x70001U
                                   : scenario == 8 ? 0x23000dU
                                   : scenario == 9 ? 0x23000cU
                                                   : 0x30001U;
                for (int column = 0; column < 100; ++column) {
                    w->working[(scenario == 2 ? 45 : 54) * 100 + column] = surface;
                    if (scenario == 4 || scenario == 5)
                        w->working[5300 + column] = scenario == 4 ? 4U : 92U;
                    if (scenario == 10)
                        w->working[4900 + column] = 0x30020U;
                }
            }
            w->goat->gravity = scenario == 2 ? -.0002f : .0002f;
            w->goat->invulnerability = 0;
            w->goat->zoom = 0;
            w->goat->renderOffset = 0;
            w->sharedGrace = scenario == 1 ? 100 : 0;
        }
        w->clock = uint32_t(tick);
        w->animationFrameFlags = uint8_t(tick);
        w->movementIntent = movement;
        w->jumpInput = jump;
        w->goat->run();
        auto &g = *w->goat;
        if (std::abs(g.stagePosition.x - x) > 1e-13 || std::abs(g.stagePosition.y - y) > 1e-13 ||
            std::abs(g.velocity.x - vx) > 1e-13 || std::abs(g.velocity.y - vy) > 1e-13) {
            std::cerr.precision(17);
            std::cerr << "Actual motion " << g.stagePosition.x << ' ' << g.stagePosition.y << ' '
                      << g.velocity.x << ' ' << g.velocity.y << '\n';
            throw std::runtime_error("Player trace motion mismatch: " + line);
        }
        compare(row,
                {bits(g.hunger), uint32_t(g.state), g.resting, uint32_t(g.heldJumpTicks),
                 g.heldJumpFinished, uint32_t(g.invulnerability), uint32_t(g.activeTicks), g.hazard,
                 g.mediumFlags, uint32_t(g.mediumEntryAge), bits(g.heading), bits(g.radius),
                 uint32_t(w->score), uint32_t(g.checkpoints.cursor), uint32_t(g.forcedJumpTicks),
                 bits(g.forcedJumpSpeed)},
                line);
    });
}
void machineTraces(const std::filesystem::path &root) {
    std::unique_ptr<StageRuntime> w;
    std::unique_ptr<EnemyActor> enemy;
    int previous = -1;
    rows(root / "original-machine-traces.tsv", 1280, [&](auto &row, const auto &line) {
        int behavior, tick;
        double x, y, vx, vy;
        row >> behavior >> tick >> x >> y >> vx >> vy;
        if (behavior != previous) {
            enemy.reset();
            w = arena();
            previous = behavior;
            w->goat->stagePosition = {.03, .1};
            enemy = std::make_unique<EnemyActor>(*w, neon::vec2d{0, -.2}, 8, 0);
            auto &e = *enemy;
            e.behavior = behavior;
            e.patroller = false;
            e.sprite = e.baseSprite = behavior == 15 ? 130 : 105;
            e.drawScale = e.savedDrawScale = {1, 1};
            e.animationMode = 0;
            e.animationWeights = {0, 0};
            e.gravity = .00035f;
            e.effectiveGravity = 0;
            e.velocity = {.001, .0015};
            e.heading = e.steeringAngle = e.steeringRate = 0;
            e.attackCooldown = e.launchCooldown = e.fireInterval = 0;
            e.hopCounter = e.hopPeriod = -1;
            e.motionFlags = 3;
            e.buoyancy = -.7f;
            e.submergedDamping = .99f;
            e.resting = true;
            e.contactHealth = 1;
            e.initializeMachine();
            e.radius = .03f;
            e.originalPosition = {0, -.2};
            e.originalVelocity = {.001, .0015};
            e.steeringRange = .3f;
            if (behavior == 7) {
                e.heading = 0;
                e.steeringRate = .003f;
                e.velocity = {.001, .0015};
                e.contactHealth = 5;
            }
            if (behavior == 6 || behavior == 16) {
                e.attackCooldown = 2;
                e.fireInterval = 7;
            }
        }
        auto &e = *enemy;
        w->clock = uint32_t(tick);
        w->animationFrameFlags = uint8_t(tick);
        e.run();
        if (std::abs(e.stagePosition.x - x) > 1e-12 || std::abs(e.stagePosition.y - y) > 1e-12 ||
            std::abs(e.velocity.x - vx) > 1e-12 || std::abs(e.velocity.y - vy) > 1e-12) {
            std::cerr.precision(17);
            std::cerr << "Actual motion " << e.stagePosition.x << ' ' << e.stagePosition.y << ' '
                      << e.velocity.x << ' ' << e.velocity.y << '\n';
            throw std::runtime_error("Machine trace motion mismatch: " + line);
        }
        uint32_t births = 0;
        for (auto *a = w->enemies; a; a = a->next)
            ++births;
        compare(row,
                {bits(e.heading), bits(e.steeringAngle), uint32_t(e.attackCooldown),
                 uint32_t(e.launchCooldown), uint32_t(e.cycleState), bits(e.contactHealth), e.dead,
                 w->goat->hazard, births},
                line);
    });
}
void patrollerTraces(const std::filesystem::path &root) {
    std::unique_ptr<StageRuntime> w;
    std::unique_ptr<EnemyActor> enemy;
    int previous = -1;
    rows(root / "original-patroller-traces.tsv", 2240, [&](auto &row, const auto &line) {
        int variant, inverted, wet, tick;
        double tx, ty, x, y, vx, vy;
        row >> variant >> inverted >> wet >> tick >> tx >> ty >> x >> y >> vx >> vy;
        const int scenario = (variant * 2 + inverted) * 2 + wet;
        if (scenario != previous) {
            enemy.reset();
            w = arena();
            previous = scenario;
            if (wet)
                std::fill(w->working.begin(), w->working.end(), 43U);
            enemy = std::make_unique<EnemyActor>(*w, neon::vec2d{0, -.2}, 4, 0);
            auto &e = *enemy;
            e.variant = variant;
            e.sprite = e.baseSprite = 105;
            e.animationMode = 0;
            e.animationWeights = {0, 0};
            e.drawScale = e.savedDrawScale = {1, 1};
            e.gravity = inverted ? -.00035f : .00035f;
            e.effectiveGravity = 0;
            e.velocity = {.001, .0015};
            e.maximumSpeed = {.004, .003};
            e.verticalRebound = .003f;
            e.hopCounter = e.hopPeriod = -1;
            e.buoyancy = -.7f;
            e.submergedDamping = .99f;
            e.radius = .03f;
            e.contactHealth = 1;
            e.motionFlags = 3;
        }
        auto &e = *enemy;
        w->goat->stagePosition = {tx, ty};
        w->clock = uint32_t(tick);
        w->animationFrameFlags = uint8_t(tick);
        e.run();
        if (std::abs(e.stagePosition.x - x) > 1e-12 || std::abs(e.stagePosition.y - y) > 1e-12 ||
            std::abs(e.velocity.x - vx) > 1e-12 || std::abs(e.velocity.y - vy) > 1e-12) {
            std::cerr.precision(17);
            std::cerr << "Actual motion " << e.stagePosition.x << ' ' << e.stagePosition.y << ' '
                      << e.velocity.x << ' ' << e.velocity.y << '\n';
            throw std::runtime_error("Patroller trace motion mismatch: " + line);
        }
        uint32_t births = 0;
        for (auto *a = w->enemies; a; a = a->next)
            ++births;
        compare(row,
                {bits(e.heading), bits(e.contactHealth), e.charging, e.resting, e.mediumFlags,
                 e.dead, births},
                line);
    });
}
void deathExit(const std::filesystem::path &root) {
    rows(root / "original-death-exit.tsv", 864, [](auto &row, const auto &line) {
        int state, inv, lives, followers, free;
        double alpha;
        float fade;
        row >> state >> inv >> lives >> followers >> free >> alpha >> fade;
        auto w = arena();
        auto &g = *w->goat;
        g.state = state;
        g.invulnerability = inv;
        g.followingCount = 9;
        g.stagePosition = {.1, .2};
        g.colour.a = alpha;
        g.checkpoints.record({.125, .25}, -.0002f);
        w->lives = lives;
        w->activeFollowers = followers;
        w->mode = free ? 4 : 0;
        w->modeDetail = free ? 1 : 0;
        w->sceneFade = w->displayFade = fade;
        g.run();
        compare(
            row,
            {uint32_t(g.state), uint32_t(g.invulnerability), uint32_t(w->lives), bits(g.radius)},
            line);
        double expectedAlpha;
        row >> expectedAlpha;
        if (std::abs(g.colour.a - expectedAlpha) > 1e-15)
            throw std::runtime_error("Death/exit alpha mismatch: " + line);
        compare(row,
                {bits(w->sceneFade), uint32_t(w->exitFadeTicks), w->playerVisible, w->terminal},
                line);
        double x, y;
        row >> x >> y;
        if (g.stagePosition.x != x || g.stagePosition.y != y)
            throw std::runtime_error("Checkpoint position mismatch: " + line);
        compare(row, {bits(g.gravity), uint32_t(g.followingCount)}, line);
    });
}
void headbutts(const std::filesystem::path &root) {
    class Target final : public StageActor {
      public:
        explicit Target(StageRuntime &w) : StageActor(w, w.allocation, .0002f, false) {}
        void run() override {}
    };
    rows(root / "original-headbutt-dispatch.tsv", 768, [](auto &row, const auto &line) {
        int sa, sb, sta, stb, dead, category, disabled, ledge, grace;
        row >> sa >> sb >> sta >> stb >> dead >> category >> disabled >> ledge >> grace;
        auto w = arena();
        auto *a = new Target(*w);
        auto *b = new Target(*w);
        w->contactObjects = a;
        a->next = b;
        b->prev = a;
        a->serial = sa;
        b->serial = sb;
        a->headbuttState = sta;
        b->headbuttState = stb;
        a->dead = dead != 0;
        a->category = b->category = category;
        a->stagePosition = {.1, .2};
        a->savedStagePosition = {.3, .4};
        b->stagePosition = {-.1, -.2};
        b->savedStagePosition = {-.3, -.4};
        w->multiplier = w->goat->followingCount = 3;
        w->headbuttDisabled = disabled != 0;
        w->goat->ledgeGrace = ledge;
        w->sharedGrace = grace;
        const bool success = w->attemptHeadbutt();
        compare(row,
                {success, uint32_t(w->multiplier), uint32_t(w->goat->followingCount), a->dead,
                 b->dead, uint32_t(a->serial), uint32_t(b->serial), uint32_t(a->headbuttState),
                 uint32_t(b->headbuttState), uint32_t(a->rewardValue), uint32_t(b->rewardValue)},
                line);
        double ax, ay, bx, by;
        row >> ax >> ay >> bx >> by;
        if (a->stagePosition.x != ax || a->stagePosition.y != ay || b->stagePosition.x != bx ||
            b->stagePosition.y != by)
            throw std::runtime_error("Headbutt target position mismatch: " + line);
    });
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Pass the fixture directory");
        contacts(argv[1]);
        damage(argv[1]);
        pickups(argv[1]);
        playerTraces(argv[1]);
        machineTraces(argv[1]);
        patrollerTraces(argv[1]);
        deathExit(argv[1]);
        headbutts(argv[1]);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
