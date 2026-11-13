#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_campaign.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
struct Game : HeadWorld {
    EntityLists lists;
    DarwinRandom random{31};
    std::array<Rgba, 33> palette{};
    Ship player;
    Effects effects{lists, random, palette, [](int) { return Vec2{32, 16}; }};
    struct Enemies : EnemyWorld {
        int sample = -1;
        explicit Enemies(Game &g) : EnemyWorld(g, g.lists, g.effects, g.random) {}
        void sound(int32_t s, int32_t) override {
            sample = s;
        }
    } enemies{*this};
    int points = 0, drop = -1, pointsAtDrop = -1;
    Game() {
        ship = &player;
        shipPosition = &player.position;
    }
    ~Game() {
        lists.clear();
    }
    void touchEnemyList(Head &) override {}
    void score(int32_t n) override {
        points += n;
    }
    void pickup(Vec2, int32_t kind) override {
        drop = kind;
        pointsAtDrop = points;
    }
};
struct Hit : Entity {
    int notifications = 0;
    void didHit(Entity &) override {
        ++notifications;
    }
};
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-13 * std::max(1., std::abs(b));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream fixture(argv[1]);
    assert(fixture.good());
    std::string line;
    int rows = 0;
    while (std::getline(fixture, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        Game g;
        AnalSurprise e(g.enemies);
        double scale, ex, ey, evx, evy;
        int ticks, sprite;
        std::istringstream in(line);
        assert(in >> e.position.x >> e.position.y >> e.velocity.x >> e.velocity.y >>
               g.player.position.x >> g.player.position.y >> scale >> ticks >> ex >> ey >> evx >>
               evy >> sprite);
        g.ac.timeScale = float(scale);
        g.ac.liveThreats = 13;
        e.fireTimer = -1;
        for (int i = 0; i < ticks; ++i)
            e.run();
        if (!close(e.position.x, ex) || !close(e.position.y, ey) || !close(e.velocity.x, evx) ||
            !close(e.velocity.y, evy) || e.sprite != sprite) {
            std::cerr << "Flying worksheet row " << rows << " differs\n";
            return 1;
        }
        assert(g.ac.liveThreats == 13 && e.age == 0 && !e.dead);
        ++rows;
    }
    assert(rows == 89);
    for (int level = 6; level <= 31; ++level) {
        Game g;
        g.ac.level = level;
        AnalSurprise e(g.enemies);
        e.initialize(2, level / 23);
        DarwinRandom random(31);
        assert(e.position.x == random.range(-.2, .2) && g.random.state() == random.state());
        assert(e.type == 0 && e.sprite == 56 && e.radius == .035f && e.tint.a == 0 &&
               e.position.y == -.422 && e.velocity.y == .0005);
        assert(e.armour == 2 + level / 23 &&
               e.fireTimer == (level < 23 ? -1 : int(((uint32_t(level) & 7) << 1) ^ 14) + 5));
        if (level >= 23) {
            const int period = e.fireTimer;
            e.fireTimer = 1;
            e.run();
            assert(e.fireTimer == period);
            auto *right = dynamic_cast<EnemyBullet *>(g.lists.first(List::L3c8));
            auto *left = right ? dynamic_cast<EnemyBullet *>(right->next) : nullptr;
            assert(right && left && !left->next && right->angle == 1.5707964f &&
                   left->angle == -1.5707964f);
            assert(right->position.x == e.position.x && right->position.y == e.position.y &&
                   right->velocity.x > 0 && left->velocity.x < 0);
        }
    }
    {
        Game g;
        AnalSurprise e(g.enemies);
        e.position = {.2, .1};
        e.velocity = {.02, -.015};
        g.ship = nullptr;
        e.run();
        assert(e.velocity.y == -.015 &&
               e.velocity.x == .02 * .98); // rejected candidates retain existing/dragged velocities
        Hit hit;
        hit.type = 1;
        e.armour = 2;
        e.hitBy(hit);
        assert(hit.notifications == 1 && e.armour == 2 && g.points == 0);
        hit.type = 5;
        const auto vy = e.velocity.y;
        e.hitBy(hit);
        assert(e.armour == 1 && e.velocity.y == vy - .005 && e.radius == .08f);
        e.hitBy(hit);
        assert(e.armour == 0 && !e.dead && g.points == 0);
        e.hitBy(hit);
        assert(e.dead && g.points == 50 && g.pointsAtDrop == 0 && hit.notifications == 4);
        e.hitBy(hit);
        assert(g.points == 50 && hit.notifications == 5);
    }
    for (int type : {5, 8}) {
        Game g;
        g.survival = true;
        g.ac.level = 6;
        AnalSurprise e(g.enemies);
        Hit hit;
        hit.type = type;
        e.hitBy(hit);
        assert(e.dead && g.points == 50 && g.drop == -1 && hit.notifications == 1);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 31, 0, 3, false, 6);
        g.tick({});
        auto *e = dynamic_cast<AnalSurprise *>(g.lists.first(List::L3c4));
        assert(e && e->armour == 0 && e->fireTimer == -1 && e->tint.a == .05);
        assert(e->velocity.y == double(float(.0005 + .00005)) &&
               e->position.y == -.422 + e->velocity.y);
    }
    std::cout << rows
              << " source-derived movement cases, initialization/RNG, all firing periods, damage "
                 "callbacks and actual Grid 7 dispatch passed\n";
}
