#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_campaign.h"
#include <algorithm>
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
    DarwinRandom random{31};
    std::array<Rgba, 33> palette{};
    Ship player;
    std::vector<int32_t> cells = std::vector<int32_t>(1200);
    Effects effects{lists, random, palette, [](int) { return Vec2{32, 16}; }};
    struct Enemies : EnemyWorld {
        int sample = -1;
        explicit Enemies(Game &g) : EnemyWorld(g, g.lists, g.effects, g.random) {}
        void sound(int32_t s, int32_t) override {
            sample = s;
        }
    } enemies{*this};
    struct Pods : PodWorld {
        Game &game;
        explicit Pods(Game &g) : PodWorld(g.ac, g.lists, g.effects, g.random, g.cells), game(g) {
            ship = &g.player;
        }
        void sound(int32_t n) override {
            game.sound(n);
        }
        void score(int32_t n) override {
            game.score(n);
        }
        void spawnSwarmers(const Entity &from, int32_t n) override {
            game.enemies.spawnSwarmers(from, n);
        }
    } pods{*this};
    int points = 0, bonusSprite = -1, drop = -1, pointsAtDrop = -1, sample = -1;
    Game() {
        ship = &player;
        shipPosition = &player.position;
        player.position = {0, .3};
    }
    ~Game() {
        lists.clear();
    }
    void touchEnemyList(Head &) override {}
    void score(int32_t n) override {
        points += n;
    }
    void sound(int32_t n) override {
        sample = n;
    }
    void bonus(const HeadBonus &s) override {
        bonusSprite = s.sprite;
    }
    void pickup(Vec2, int32_t kind) override {
        drop = kind;
        pointsAtDrop = points;
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
        Game g;
        Cube cube(g.enemies);
        cube.initialize(3, 2, 4);
        assert(cube.armour == 5 && cube.fragmentCount == 8 && cube.tint.a == 1 && !cube.entered);
        DarwinRandom expected(31);
        expected.next();
        expected.next();
        Vec2 away{0, -.003};
        away.rotateBy(expected.range(-.7853981634, .7853981634));
        const double distance = double((expected.next() & 31) + 250);
        expected.next();
        assert(g.random.state() == expected.state() && cube.position.x == away.x * distance &&
               cube.position.y == away.y * distance);
        assert(cube.velocity.x == -away.x && cube.velocity.y == -away.y);
        cube.position = {0, -.501};
        cube.velocity = {0, .002};
        g.ac.liveThreats = 0;
        cube.run();
        assert(cube.entered && g.ac.liveThreats == 1 && cube.velocity.y == .002);
        g.ac.liveThreats = 0;
        cube.run();
        assert(g.ac.liveThreats == 2 && cube.acceleration == .0001f);
        assert(cube.velocity.y == (.002 + double(.0001f)) * .99);
        cube.position = {.6, 0};
        cube.velocity = {};
        cube.run();
        assert(!cube.dead);
        cube.position = {.6, .6};
        cube.run();
        assert(cube.dead);
        Cube absent(g.enemies);
        g.ship = nullptr;
        absent.run();
        assert(absent.dead && absent.age == 1);
    }
    {
        Game g;
        Cube cube(g.enemies);
        cube.armour = 2;
        cube.fragmentCount = 3;
        cube.velocity.y = .004;
        Attacker shot;
        shot.type = 5;
        cube.hitBy(shot);
        assert(shot.hits == 1 && cube.armour == 1 && cube.radius == .08f &&
               cube.velocity.y == .004 * -1.5 && g.points == 0);
        assert(count<Shrapnel>(g.lists, List::L3c4) == 6);
        // The FIRST pair starts at +/- half a spacing. Newest-first list
        // insertion puts the first fragment at the tail, independent of the
        // small rounding differences accumulated by later rotations.
        auto *firstFragment = g.lists.first(List::L3c4);
        while (firstFragment->next)
            firstFragment = firstFragment->next;
        assert(std::abs(dynamic_cast<Shrapnel *>(firstFragment)->velocity.x -
                        std::sin(double(3.1415927f / 3.f) * .5) * .006) < 1e-15);
        cube.fragmentCount = 0;
        cube.hitBy(shot);
        assert(cube.armour == 0 && !cube.dead && count<Shrapnel>(g.lists, List::L3c4) == 6);
        assert(std::isfinite(cube.velocity.x) && std::isfinite(cube.velocity.y));
        cube.hitBy(shot);
        assert(cube.dead && g.points == 50 && g.bonusSprite == 45 && g.drop >= 1 &&
               g.pointsAtDrop == 50);
        cube.hitBy(shot);
        assert(g.points == 50 && shot.hits == 4);
    }
    {
        Game g;
        g.survival = true;
        g.ac.level = 6;
        const auto before = g.random.state();
        g.enemies.specialPickup({});
        assert(g.drop == -1 && g.random.state() == before); // sixth index, not eighth
    }
    {
        Game g;
        AnalSurprise enemy(g.enemies);
        enemy.initialize(2, 1);
        assert(enemy.armour == 3 && enemy.fireTimer == -1 && enemy.position.y == -.422 &&
               enemy.velocity.y == .0005 && enemy.tint.a == 0);
        enemy.position = {0, -.2};
        enemy.velocity = {};
        g.player.position = {.1, .3};
        enemy.run();
        assert(enemy.velocity.x < 0 && enemy.velocity.y > 0); // player below: horizontal repulsion
        enemy.position = {0, 0};
        enemy.velocity = {};
        g.player.position = {0, -.3};
        enemy.run();
        assert(enemy.velocity.x == double(.0004f) && enemy.velocity.y == double(float(-.0001)));
        // Float candidates are compared against a DOUBLE .01 boundary in iOS.
        enemy.position = {0, -.2};
        enemy.velocity = {0, double(.01f) - .00005};
        g.player.position = {0, .3};
        enemy.run();
        assert(enemy.velocity.y == double(.01f));
        enemy.armour = 1;
        Attacker shot;
        shot.type = 5;
        const double vy = enemy.velocity.y;
        enemy.hitBy(shot);
        assert(enemy.armour == 0 && enemy.radius == .08f && enemy.velocity.y == vy - .005 &&
               !enemy.dead);
        enemy.hitBy(shot);
        assert(enemy.dead && g.points == 50 && g.pointsAtDrop == 0); // pickup precedes score
        AnalSurprise armed(g.enemies);
        g.ac.level = 23;
        armed.initialize(0, 0);
        assert(armed.fireTimer == 5);
        armed.fireTimer = 1;
        armed.run();
        assert(count<EnemyBullet>(g.lists, List::L3c8) == 2 && armed.fireTimer == 5);
    }
    {
        Game g;
        Entity parent;
        parent.position = {0, -.2};
        Swarmer enemy(g.enemies);
        enemy.initialize(parent);
        DarwinRandom expected(31);
        const double speed = expected.range(.005, .003);
        Vec2 v{0, -speed};
        v.rotateBy(expected.range(-.8, .8));
        v.x *= 3.;
        const float acceleration = float(expected.range(.0001, .00005));
        const double damping = expected.range(.98, .995);
        assert(enemy.type == 0 && enemy.velocity.x == v.x && enemy.velocity.y == v.y &&
               enemy.acceleration == acceleration && enemy.damping.x == damping &&
               g.random.state() == expected.state());
        Attacker shot;
        shot.type = 5;
        enemy.hitBy(shot);
        assert(shot.hits == 0 && g.points == 0);
        enemy.position = {0, .2};
        enemy.velocity = {};
        enemy.acceleration = .0001f;
        enemy.damping = {.9, .9};
        enemy.tint.a = .95;
        g.player.position = {0, .4};
        g.ac.timeScale = .5;
        g.ac.liveThreats = 11;
        enemy.run();
        assert(enemy.position.y == .2 + double(.0001f) * .5 &&
               enemy.velocity.y == double(.0001f) * .9 && enemy.tint.a == 1 &&
               g.ac.liveThreats == 11);
        g.ac.swarmerKills = 49;
        enemy.hitBy(shot);
        assert(enemy.dead && shot.hits == 1 && g.points == 500 && g.ac.swarmerKills == 50 &&
               g.enemies.sample == 57);
        assert(count<Fhbon>(g.lists, List::L3bc) == 1);
    }
    {
        Game g;
        Spod pod(g.pods);
        pod.initialize({0, 0}, 4, 3);
        DarwinRandom expected(31);
        const int age = int(expected.next() & 7);
        assert(pod.age == age && pod.type == 1 && pod.radius == .02f && pod.offspring == 3 &&
               pod.damping.x == .95 && g.random.state() == expected.state());
        Attacker ship;
        ship.type = 8;
        pod.hitBy(ship);
        assert(pod.dead && ship.hits == 1 && g.points == 0 &&
               count<Swarmer>(g.lists, List::L3c4) == 3);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 1, 0, 3, false, 6);
        g.makePod({-.3, -.2}, true);
        g.tick({});
        g.makePod({0, -.2}, true);
        g.tick({});
        DarwinRandom expected(g.random.state());
        expected.next();
        g.makePod({.3, -.2}, true);
        auto *pod = dynamic_cast<Spod *>(g.lists.first(List::L3c4));
        assert(pod && pod->offspring == 5 && g.random.state() == expected.state());
        const int x = int((.3 + .5) * 28), y = int((-.2 + .5) * 36);
        assert(g.cells[size_t(y * 29 + x)] == 0);
        g.ac.completed = true;
        pod->run();
        assert(pod->dead && g.points() == 100 && count<Swarmer>(g.lists, List::L3c4) == 5);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 1, 0, 3, false, 7);
        g.tick({});
        int n = 0, leaders = 0;
        for (auto *p = g.lists.first(List::L3c4); p; p = p->next)
            if (auto *h = dynamic_cast<Head *>(p)) {
                if (!h->parent) {
                    ++leaders;
                    assert(h->mode == 1 && h->returnMode == 4 &&
                           std::abs(h->movementAngle) == 2.3561945f);
                } else
                    assert(h->type == 7 && h->mode == -1); // delayed history has not matured
                ++n;
            }
        assert(n == 8 && leaders == 2); // left/right pair starts boundary turns immediately
    }
    std::cout << "Cube entry/armour/fans, original sgn and float boundaries, Spod quota/offspring, "
                 "Swarmer RNG/callbacks and Grid 8 launches passed\n";
}
