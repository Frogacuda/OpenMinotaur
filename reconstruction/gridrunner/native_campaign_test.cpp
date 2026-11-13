#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_campaign.h"
#include "native_draw.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
template <class T> int count(const EntityLists &lists, List list) {
    int n = 0;
    for (auto *e = lists.first(list); e; e = e->next)
        n += dynamic_cast<T *>(e) != nullptr;
    return n;
}
struct Game : HeadWorld {
    EntityLists lists;
    DarwinRandom random{17};
    std::array<Rgba, 33> palette{};
    Effects effects{lists, random, palette, [](int) { return Vec2{32, 16}; }};
    struct Audio : EnemyWorld {
        int sample = -1, volume = 0;
        Audio(Game &g) : EnemyWorld(g, g.lists, g.effects, g.random) {}
        void sound(int32_t s, int32_t v) override {
            sample = s;
            volume = v;
        }
    } audio{*this};
    int points = 0, bonusSprite = -1, drop = -1;
    void touchEnemyList(Head &) override {}
    void score(int32_t n) override {
        points += n;
    }
    void sound(int32_t) override {}
    void bonus(const HeadBonus &s) override {
        bonusSprite = s.sprite;
    }
    void pickup(Vec2, int32_t kind) override {
        drop = kind;
    }
    ~Game() {
        lists.clear();
    }
};
struct Attacker : Entity {
    int hits = 0;
    void didHit(Entity &) override {
        ++hits;
    }
};
int main() {
    {
        Campaign g([](int32_t) { return Vec2{16, 16}; });
        g.achievement(4);
        g.achievement(4);
        g.achievement(5);
        assert((g.achievements == std::vector<int32_t>{4, 5}));
        g.tick({});
        assert(g.achievements.empty());
        g.achievement(4);
        assert(g.achievements.empty());
        Campaign demo([](int32_t) { return Vec2{16, 16}; }, 1, 0, 3, true);
        demo.achievement(4);
        assert(demo.achievements.empty());
    }
    for (int mode = 0; mode < 2; ++mode)
        for (int grid = 0; grid < (mode ? 6 : 23); ++grid) {
            LevelProgram p(grid, mode != 0);
            int launches = 0;
            for (int i = 0; i < 50000 && p.wait >= 0; ++i)
                p.step(0, [&](int, int type) {
                    assert(type >= 1 && type <= 9);
                    ++launches;
                });
            assert(p.wait == -1 && launches > 0);
        }
    {
        constexpr int launches[] = {7, 13, 10, 19, 18, 32, 16, 27};
        for (int grid = 0; grid < 8; ++grid) {
            LevelProgram script(grid);
            int n = 0, headSegments = 0, bombs = 0;
            for (int ticks = 0; ticks < 30000 && script.wait >= 0; ++ticks)
                script.step(0, [&](int amount, int type) {
                    ++n;
                    if (type == 4)
                        ++bombs;
                    else if (type == 6 || type == 7 || type == 8)
                        headSegments += amount;
                    else
                        assert(type == 3 || type == 5);
                });
            assert(script.wait == -1 && n == launches[grid] && headSegments > 0);
            if (grid > 0 && grid < 6)
                assert(bombs > 0);
        }
    }
    {
        Game g;
        ShrapnelBomb bomb(g.audio);
        bomb.initialize(3, 0);
        assert(bomb.fanCount == 3 && bomb.smart == 0 && bomb.position.y == -.428 &&
               bomb.velocity.y == .002);
        DarwinRandom expected(17);
        for (int i = 0; i < 4; ++i)
            expected.next();
        assert(g.random.state() == expected.state());
        bomb.foom();
        assert(bomb.dead && count<Shrapnel>(g.lists, List::L3c4) == 6);
        for (int i = 0; i < 7; ++i)
            expected.next();
        assert(g.random.state() == expected.state());
        for (auto *e = g.lists.first(List::L3c4); e; e = e->next) {
            auto *p = dynamic_cast<Shrapnel *>(e);
            assert(p && std::abs(std::hypot(p->velocity.x, p->velocity.y) - .006) < 1e-15 &&
                   p->radius == .014f);
        }
        Attacker shot;
        shot.type = 5;
        ShrapnelBomb armoured(g.audio);
        armoured.armour = 1;
        armoured.fanCount = 0;
        armoured.hitBy(shot);
        assert(shot.hits == 1 && !armoured.dead && armoured.armour == 0 && g.points == 50 &&
               armoured.radius == .05f && g.audio.sample == 53 && g.audio.volume == 0x4000);
        armoured.hitBy(shot);
        assert(shot.hits == 2 && armoured.dead && g.points == 100 && g.bonusSprite == 45 &&
               g.drop >= 1 && g.drop <= 7);
    }
    {
        Game g;
        Entity source;
        source.position = {.01, .02};
        g.audio.fireRing(source, 4);
        assert(count<EnemyBullet>(g.lists, List::L3c8) == 4);
        for (auto *e = g.lists.first(List::L3c8); e; e = e->next) {
            auto *b = dynamic_cast<EnemyBullet *>(e);
            assert(b && b->position.x == .01 && b->position.y == .02 && b->tint.a == 0 &&
                   b->scale.y == -1.8);
            assert(std::abs(std::hypot(b->velocity.x, b->velocity.y) - double(.01f)) < 1e-15);
        }
        EnemyBullet b(g.audio);
        b.initialize(source, 0, .01);
        g.ac.timeScale = .5;
        b.run();
        assert(b.position.y == .025 && b.tint.a == .1);
        b.position.x = .42;
        b.run();
        assert(b.dead);
        Attacker hit;
        b.hitBy(hit);
        assert(hit.hits == 1 && g.points == 5 && g.audio.sample == 58);
        Shrapnel piece(g.audio);
        piece.type = 9;
        piece.position = {.6, 0};
        piece.run();
        assert(!piece.dead);
        piece.position.y = .6;
        piece.run();
        assert(piece.dead);
        Shrapnel clear(g.audio);
        g.ac.powers[7] = 10;
        clear.run();
        assert(!clear.dead);
        g.audio.clearRow = 1;
        clear.run();
        assert(clear.dead && g.points == 10);
    }
    {
        Game g;
        g.ac.level = 8;
        // Exercise the original smart variant independently of the currently
        // enabled campaign range; its fourth init draw selects the variant.
        uint32_t seed = 1;
        for (;; ++seed) {
            DarwinRandom r(seed);
            r.next();
            r.next();
            r.next();
            if ((r.next() & 127) == 0)
                break;
            assert(seed < 10000);
        }
        g.random.seed(seed);
        ShrapnelBomb smart(g.audio);
        smart.initialize(2, 0);
        assert(smart.smart == 1 && smart.armour == 3 && smart.firePeriod == 15 &&
               smart.sprite == 86);
        smart.detonationY = .5f;
        smart.fireTimer = 1;
        smart.run();
        assert(!smart.dead && smart.velocity.x == -.0002 && smart.fireTimer == 15 &&
               count<EnemyBullet>(g.lists, List::L3c8) == 1);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 512, 0, 3, false, 1);
        g.makePod({0, -.2}, false);
        auto *pod = dynamic_cast<Pod2 *>(g.lists.first(List::L3c4));
        assert(pod && g.cells[size_t(pod->cell)] == 16);
        Attacker shot;
        shot.type = 5;
        const auto point = g.points();
        pod->hitBy(shot);
        assert(!pod->dead && pod->radius == .06f && g.cells[size_t(pod->cell)] == 17 &&
               count<EnemyBullet>(g.lists, List::L3c8) == 4);
        pod->hitBy(shot);
        pod->hitBy(shot);
        assert(pod->dead && g.cells[size_t(pod->cell)] == 0 && g.points() == point + 10 &&
               count<EnemyBullet>(g.lists, List::L3c8) == 12);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 512, 0, 3, false, 1);
        g.makePod({0, -.2}, false);
        auto *pod = dynamic_cast<Pod2 *>(g.lists.first(List::L3c4));
        assert(pod);
        pod->world.decayPulse = true;
        pod->run();
        assert(pod->dead && g.cells[size_t(pod->cell)] == 0 &&
               count<EnemyBullet>(g.lists, List::L3c8) == 4);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        g.score(100);
        g.ac.lives = 0;
        g.score(500);
        assert(g.points() == 100);
        Campaign demo([](int) { return Vec2{32, 16}; }, 1, 0, 3, true);
        demo.score(100);
        assert(demo.points() == 0);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        g.makePod({0, -.2}, false);
        g.ac.completed = true;
        g.currentShip()->state = Ship::State::Playing;
        g.tick({});
        assert(g.feedback() == .8f &&
               g.presentedFeedback() == 0); // later Pod .8 replaces departing Ship .9
        g.tick({});
        assert(g.presentedFeedback() == float(double(.8f) * .95)); // owner publishes next frame
        g.clearPausedFeedback();
        assert(g.feedback() == 0);
    }
    {
        DarwinRandom session(1);
        Starfield stars(session);
        Campaign g([](int) { return Vec2{32, 16}; }, 1, 0, 3, false, 0, false, 0, &session);
        DrawFrame frame;
        int previousPoints = 0, previousLives = 3;
        for (int grid = 0; grid < 51; ++grid) {
            assert(g.level() == grid && g.ac.lives == previousLives &&
                   g.points() == previousPoints);
            uint32_t awards = 0;
            int tick = 0;
            for (; tick < 30000 && g.state() != Campaign::State::Complete; ++tick) {
                g.ac.powers[5] = 100000;
                g.ac.powers[7] = 100000;
                g.tick({});
                frame.prepare(g);
                for (int goal : g.achievements)
                    awards |= 1u << goal;
            }
            if (grid < 5)
                assert((awards & (1u << 3)) == 0);
            if (grid == 5)
                assert(awards & (1u << 3));
            assert(g.state() == Campaign::State::Complete && tick < 30000 &&
                   g.ac.lives == previousLives + 1);
            std::cout << "Grid " << grid + 1 << ": launches=" << g.launches()
                      << " score=" << g.points() << " clock=" << g.clock << '\n';
            previousLives = g.ac.lives;
            previousPoints = g.points();
            g.ac.warpUsed = true;
            assert(g.advanceLevel());
            assert(g.ac.gridsWithoutDeath == grid + 1);
            assert(g.ac.warpUsed == ((g.level() & 1) != 0));
        }
    }
    for (int level = 0; level < 51; ++level) {
        Campaign g([](int) { return Vec2{32, 16}; }, 19, 0, 3, false, level);
        DrawFrame frame;
        for (int i = 0; i < 30000 && g.state() != Campaign::State::Complete &&
                        g.state() != Campaign::State::GameOver;
             ++i) {
            g.tick({});
            frame.prepare(g);
        }
        assert(g.state() == Campaign::State::Complete || g.state() == Campaign::State::GameOver);
    }
    for (int level = 0; level < 18; ++level) {
        Campaign g([](int) { return Vec2{32, 16}; }, 23, 0, 1, false, level, true);
        DrawFrame frame;
        for (int i = 0; i < 30000 && g.state() != Campaign::State::Complete; ++i) {
            g.ac.powers[5] = 100000;
            g.ac.powers[7] = 100000;
            g.tick({});
            frame.prepare(g);
        }
        assert(g.state() == Campaign::State::Complete && g.ac.lives == 1);
    }
    {
        DarwinRandom session(101);
        Starfield stars(session);
        Campaign g([](int) { return Vec2{32, 16}; }, 101, 0, 1, false, 0, true, 0, &session);
        DrawFrame frame;
        for (int level = 0; level < 18; ++level) {
            for (int i = 0; i < 30000 && g.state() != Campaign::State::Complete; ++i) {
                g.ac.powers[5] = 100000;
                g.ac.powers[7] = 100000;
                g.tick({});
                frame.prepare(g);
            }
            assert(g.state() == Campaign::State::Complete && g.ac.lives == 1 &&
                   g.level() == level && g.ac.gridsWithoutDeath == 0);
            assert(g.advanceLevel());
        }
    }
    std::cout << "Original grid programs, concrete enemy/pod projectiles, collision callbacks and "
                 "multi-grid progression passed\n";
}
