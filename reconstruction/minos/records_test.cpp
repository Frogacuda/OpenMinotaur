// Host adaptation integration: independent lifetime records and source goal gates.
#include "../../cores/minos/local_records.h"
#include "enemies.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <array>
#include <cmath>
#include <memory>

static void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << what << '\n';
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    check(argc == 2, "test directory required");
    const std::string dir = std::string(argv[1]) + "/";
    std::filesystem::create_directories(dir);
    // Only these three scratch files belong to this test.
    for (const char *f : {"highscores.txt", "achievements.txt", "achievements.txt.tmp"})
        std::filesystem::remove(dir + f);
    minos::LocalRecords local;
    check(!local.load(dir), "fresh profile needs one-time personal-best migration");
    check(local.scores.boards() == 4 && local.achievements == 0,
          "four fresh boards and no lifetime goals");
    check(local.scores.entry(0, 0).score == 0 && local.scores.entry(0, 0).level == 0,
          "empty slots do not invent records");
    std::array<int32_t, 800> oldPrefs{};
    oldPrefs[100] = 500;
    local.importPersonalBests(oldPrefs.data());
    check(local.scores.entry(1, 0).score == 500 && local.scores.entry(1, 0).level == 0,
          "legacy personal best retained without inventing name/cavern metadata");
    check(local.observe(4 | 0x80) && !local.observe(4),
          "simultaneous goals retained; repeats do not dirty record");
    local.observe(0xfffffc00u);
    check(local.achievements == 0x84, "only ten original goal bits accepted");
    local.scores.insert(2, "TAXI", 12345, 7);
    local.scores.lastName = "TAXI";
    check(local.save(dir), "records persist");
    minos::LocalRecords loaded;
    check(loaded.load(dir), "existing table suppresses another migration");
    check(loaded.achievements == 0x84 && !loaded.achievementsDirty,
          "goals survive restart without replaying notifications");
    check(loaded.scores.entry(2, 0).score == 12345 && loaded.scores.entry(2, 0).level == 7 &&
              loaded.scores.entry(0, 0).score == 0 && loaded.scores.lastName == "TAXI",
          "board partition, name and cavern survive restart");
    loaded.scores.insert(0, "RETRY", 100, 2);
    check(!loaded.save(dir + "missing/") && loaded.scores.dirty,
          "failed write retains pending score for retry");
    check(loaded.save(dir), "pending score saves after failed path");
    {
        std::ofstream out(dir + "achievements.txt");
        out << "99 1023\n";
    }
    loaded.load(dir);
    check(loaded.achievements == 0, "unknown achievement version rejected");

    // Supplied whole-play replay does not reach CavernGoal::hitBy's achievement.
    // Original 0x758d4: cavern=4, cleanRun, not muted, +488<2, bit4 unset.
    for (int mode = 0; mode < 6; ++mode) {
        minos::PlayState g;
        neon::DarwinRandom rng(123);
        g.rand = &rng;
        g.cavernIndex = mode == 1 ? 3 : 4;
        g.cavern = &minos::cavern::kCavernDefinitions[g.cavernIndex];
        g.cleanRun = mode != 2;
        g.muted = mode == 3;
        g.awardEligibilityGate = mode == 4 ? 2 : 1;
        g.goalBits = mode == 5 ? 4 : 0;
        minos::Ship ship;
        ship.mapPos = {9.5, 100};
        minos::CavernGoal goal;
        goal.g = &g;
        goal.hitBy(&ship);
        check(goal.dead && g.flag203808 && g.scoreTotal == 1000 && g.respawnPos.y == 100,
              "goal's other contact behavior preserved");
        check((g.goalState == 6) == (mode == 0), "all TONE 40 achievement gates");
        if (mode == 0)
            check(g.goalBits == 4 && std::string(g.slots[8].text) == "'TONE 40'" &&
                      g.slots[7].count == 120,
                  "original achievement bits and message slots");
    }
    {
        // Original floatingScore::draw uses its own sprite dimensions, not entity::draw's diameter.
        struct Capture : minos::SpriteRenderer {
            std::vector<minos::SpriteFields> sprites;
            int drawEntity(minos::Entity &, minos::Vec2) override {
                check(false, "score glyph must use sprite fields");
                return 0;
            }
            int drawSprite(const minos::SpriteFields &f) override {
                sprites.push_back(f);
                return 6;
            }
        } capture;
        minos::FloatingScore score;
        score.pos = {.25, -.2};
        score.size = .5f;
        score.scale = {.1, .2};
        score.first = 13;
        score.digits[14] = 1;
        score.digits[15] = 2;
        score.rot = .25f;
        const auto index = score.spriteIndex;
        check(score.draw(capture, {320, 480}) == 12 && capture.sprites.size() == 2,
              "two score glyph packets");
        const auto &a = capture.sprites[0];
        const auto &b = capture.sprites[1];
        auto near = [](double x, double y) { return std::fabs(x - y) < 1e-10; };
        check(near(a.size.x, 16) && near(a.size.y, 48) && near(a.pos.x, 70.4) &&
                  near(a.pos.y, -124.8) && near(b.pos.x, 89.6),
              "original score dimensions, centring, advance and half-height origin");
        check(a.frame == 21 && b.frame == 22 && a.angle == .25 && a.orFlags == 3,
              "score atlas frames, angle and retained flags");
        check(score.pos.x == .25 && score.pos.y == -.2 && score.spriteIndex == index,
              "drawing does not mutate simulation sprite state");
    }
    {
        minos::PlayState g;
        neon::DarwinRandom rng(7);
        auto map = std::make_unique<minos::cavern::StripMap>();
        g.map = map.get();
        g.rand = &rng;
        g.clearAll = 1;
        struct Services : minos::GameServices {
            minos::PlayState &g;
            explicit Services(minos::PlayState &state) : g(state) {}
            minos::Vec2 scrollDelta() const override {
                return g.scrollDelta;
            }
            int32_t clearAll() const override {
                return g.clearAll;
            }
            minos::cavern::StripMap &map() override {
                return *g.map;
            }
            neon::DarwinRandom &rand() override {
                return *g.rand;
            }
            void addDebris(minos::Entity *e) override {
                g.addEntity(g.debris, e);
            }
            void addMapExplosion(const minos::ExplosionBirth &b) override {
                g.addMapExplosion(b);
            }
        } services(g);
        minos::MapEntity e;
        e.game = &services;
        e.row = -1;
        e.burstOnClear = 1;
        e.run();
        auto *burst = dynamic_cast<minos::Explo *>(g.debris);
        check(e.dead && burst && burst->g == &g && rng.draws() == 1,
              "generic map clear creates the actual explosion with one RNG draw");
        burst->run();
        check(burst->t == .06f && std::fabs(burst->scale.x - .59) < 1e-6,
              "born explosion executes its original scale/fade update");
        g.resetLists(0);
    }
    {
        // shentity::doControls 0x65134 clears the same +0x48c counter used by
        // rescues/scoring.
        minos::PlayState g;
        minos::Ship ship;
        ship.g = &g;
        g.shipList = &ship;
        ship.dying = true;
        ship.deathTimer = 1;
        g.lives = 2;
        g.minosSaved = 5;
        g.minosSavedTotal = 105;
        g.rescueBonus = 10000;
        g.respawnPos = {9.5, 100};
        ship.doControls();
        check(g.lives == 1 && !ship.dying && g.minosSaved == 0 && g.rescueBonus == 0,
              "respawn clears actual cavern rescue count and bonus");
        check(g.minosSavedTotal == 105 && g.respawnedThisCavern,
              "run rescue total retained; fuel bonus marked ineligible");
        g.shipList = nullptr;
    }
    std::cout << "local records, goal gates, presentation, explosion binding and respawn rescue "
                 "accounting pass\n";
}
