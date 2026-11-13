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
struct Game : HeadWorld {
    EntityLists lists;
    DarwinRandom random{31};
    std::array<Rgba, 33> palette{};
    Effects effects{lists, random, palette, [](int) { return Vec2{32, 16}; }};
    struct Audio : EnemyWorld {
        int sample = -1;
        explicit Audio(Game &g) : EnemyWorld(g, g.lists, g.effects, g.random) {}
        void sound(int32_t s, int32_t) override {
            sample = s;
        }
    } audio{*this};
    int points = 0, drop = -1;
    std::vector<int> goals;
    void achievement(int32_t n) override {
        goals.push_back(n);
    }
    void touchEnemyList(Head &) override {}
    void score(int32_t n) override {
        points += n;
    }
    void sound(int32_t) override {}
    void pickup(Vec2, int32_t n) override {
        drop = n;
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
template <class T> int count(const EntityLists &l, List list) {
    int n = 0;
    for (auto *e = l.first(list); e; e = e->next)
        n += dynamic_cast<T *>(e) != nullptr;
    return n;
}
int main() {
    // Test the two independent positions, all four stepping states, both
    // bridge signs, ties, and slow motion. Corner catch-up must not integrate
    // the physical trajectory a second time.
    for (int axis = 0; axis < 2; ++axis)
        for (int sign : {-1, 1}) {
            Game g;
            OGE mover(g.audio);
            g.ac.timeScale = .5f;
            mover.velocity = axis == 0 ? Vec2{sign * .002, .004} : Vec2{.004, sign * .002};
            mover.initialize({.013, .021});
            assert(mover.mode == axis);
            assert(mover.physical.x == .013 + mover.velocity.x * .5 &&
                   mover.physical.y == .021 + mover.velocity.y * .5);
            if (axis == 0)
                assert(mover.position.x == double(float(14) / 28.f) - .5);
            else
                assert(mover.position.y == double(float(18) / 36.f) - .5);
            mover.mode = axis;
            mover.physical = {.04, .04};
            mover.cellX = 14;
            mover.cellY = 18;
            mover.position = sign > 0 ? Vec2{0, 0} : Vec2{.1, .1};
            const Vec2 prior = mover.position;
            mover.pog();
            assert(mover.mode == axis + 2 && mover.bridgeVelocity == sign * .004f);
            assert(mover.physical.x == .04 && mover.physical.y == .04);
            if (axis == 0)
                assert(mover.position.x == prior.x + double(sign * .004f * .5f));
            else
                assert(mover.position.y == prior.y + double(sign * .004f * .5f));
            mover.position = {.04, .04};
            mover.pog();
            assert(mover.mode == axis);
        }
    {
        Game g;
        OGE tie(g.audio);
        tie.velocity = {.002, .002};
        tie.initialize({0, 0});
        assert(tie.mode == 1);
        tie.physical.x = .61;
        const auto prior = tie.physical.x;
        tie.run();
        assert(tie.dead && tie.physical.x == prior + .002);
    }
    for (int variant = 0; variant < 4; ++variant) {
        Game g;
        g.ac.level = 8 + variant * 4;
        Zakato z(g.audio);
        z.initialize({.01, .4}, {.001, -.004});
        assert(z.variant == variant && z.armour == (variant == 3 ? 3 : 0) && z.trailFrame == 0 &&
               z.sprite == 71);
        assert(g.goals == (variant == 0   ? std::vector<int>{4}
                           : variant == 3 ? std::vector<int>{5}
                                          : std::vector<int>{}));
        DarwinRandom expected(31);
        expected.next();
        assert(g.random.state() == expected.state());
        for (auto p : z.trail)
            assert(p.x == z.position.x && p.y == z.position.y);
        z.position = {-.4, -.33};
        z.velocity = {-.001, -.004};
        z.run();
        assert(z.velocity.x == .001 && z.velocity.y == ((variant & 1) ? .004 : -.004));
        assert(z.trailFrame == 1 && z.trail[0].x == z.position.x);
        constexpr int frames[] = {72, 82, 83, 71};
        assert(z.sprite == frames[variant]);
        z.age = 4;
        z.run();
        assert(z.sprite == 71);
        z.armour = 1;
        Attacker shot;
        z.hitBy(shot);
        assert(shot.hits == 1 && !z.fading && z.armour == 0 && g.audio.sample == 58);
        z.hitBy(shot);
        assert(shot.hits == 2 && z.dead && z.fading && g.points == 100 &&
               count<Fhbon>(g.lists, List::L3bc) == 1);
        z.hitBy(shot);
        assert(shot.hits == 2 && g.points == 100);
        const auto frame = z.trailFrame;
        z.run();
        assert(z.trailFrame == frame && z.fade == float(.95));
    }
    {
        Game g;
        g.ac.level = 24;
        Zakato z(g.audio);
        z.initialize({0, .4}, {0, -.004});
        DarwinRandom expected(31);
        expected.next();
        assert(z.variant == int(expected.next() & 3) && g.random.state() == expected.state());
        g.audio.clearRow = 1;
        z.run();
        assert(z.fading && z.dead && g.points == 100);
    }
    {
        Game g;
        g.audio.barrelChance = 8;
        Barrel b(g.audio);
        b.initialize({-.45, .1});
        DarwinRandom expected(31);
        const auto vy = expected.range(-.004, -.001);
        const auto spin = float(expected.range(.1, .3));
        assert(b.velocity.x == .003 && b.velocity.y == vy && b.spin == spin && b.armour == 4 &&
               g.random.state() == expected.state());
        b.position = {.42, .419};
        b.velocity = {.003, .002};
        b.age = 16;
        b.run();
        assert(b.sprite == 85 && b.age == 17 && b.angle == spin && b.velocity.x == -.003 &&
               b.velocity.y == -(.002 + .0001));
        Attacker hit;
        b.hitBy(hit);
        assert(hit.hits == 1 && b.armour == 3 && b.radius == .08f && b.velocity.x == -.003 * 1.1);
        assert(b.spin == float(double(spin) * 1.2) && b.velocity.y == -(.002 + .0001) - .004);
        b.armour = 0;
        b.hitBy(hit);
        assert(b.dead && g.points == 150 && g.drop > 0 &&
               count<EnemyBullet>(g.lists, List::L3c8) == 16);
        auto *first = g.lists.first(List::L3c8);
        while (first->next)
            first = first->next;
        auto *bullet = dynamic_cast<EnemyBullet *>(first);
        assert(bullet && bullet->angle == float(-3.141592654 / 32.) &&
               std::abs(std::hypot(bullet->velocity.x, bullet->velocity.y) - double(.006f)) <
                   1e-15);
    }
    {
        Game g;
        g.survival = true;
        g.ac.level = 6;
        Barrel b(g.audio);
        Attacker hit;
        b.hitBy(hit);
        assert(b.dead && g.drop == -1 && g.points == 150);
    }
    // Configuration boundaries and real cannon factories, including high
    // indices beyond the first script cycle. Freeze the emitters' arrival
    // animation so each explicit trigger reaches the source launch branch.
    for (int level : {0, 7, 8, 9, 10, 12, 16, 20, 24, 50, 511}) {
        Campaign g([](int) { return Vec2{32, 16}; }, 31, 0, 3, false, level);
        auto *bottom = dynamic_cast<BottomLaser *>(g.lists.first(List::L3bc));
        assert(bottom);
        auto &world = bottom->world;
        assert(world.zakatoCount == (level < 8 || (level & 1) ? 0 : 4 - (level & 3)));
        assert(world.barrelChance == (level <= 8 ? 0 : (level - 8) & (((level - 8) >> 3) + 7)));
        assert(world.zakatoSpeed == float(double(float((level + 8) & 15) / 15.f) * .004 + .003));
        bottom->arrivalStep = 0;
        g.ac.bottomTrigger = true;
        g.ac.laserFrozen = true;
        bottom->run();
        assert(count<Zakato>(g.lists, List::L3c8) == world.zakatoCount);
        for (auto *n = g.lists.first(List::L3c8); n; n = n->next)
            if (auto *z = dynamic_cast<Zakato *>(n)) {
                const double speed = std::hypot(z->velocity.x, z->velocity.y);
                assert(speed >= double(world.zakatoSpeed) - 1e-15 &&
                       speed <= double(world.zakatoSpeed) * 1.2 + 1e-15);
            }
        world.barrel({-.45, 0});
        assert(count<Barrel>(g.lists, List::L3c8) == 1);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 31, 0, 3, false, 8);
        auto *bottom = dynamic_cast<BottomLaser *>(g.lists.first(List::L3bc));
        auto &world = bottom->world;
        g.lists.clear();
        world.zakato({0, .4}, {0, -.004});
        auto *z = dynamic_cast<Zakato *>(g.lists.first(List::L3c8));
        assert(z);
        z->drawDelay = 2;
        z->flashTicks = 12;
        z->trailFrame = 7;
        z->position = {.13, .17};
        g.palette[6] = {.1, .2, .3, 1};
        for (int i = 0; i < 128; ++i)
            z->trail[size_t(i)] = {double(i) / 128., 0};
        DrawFrame frame;
        const auto random = g.random.state();
        frame.prepare(g);
        assert(frame.sprites().size() == 9 && z->drawDelay == 0 && z->flashTicks == 1 &&
               g.random.state() == random);
        assert(frame.sprites()[0].position.x == double((7 + 117) & 127) / 128. &&
               frame.sprites()[0].frame == 4);
        assert(frame.sprites().back().position.x == .13 && z->radius == .025f && z->tint.r == 1 &&
               z->tint.a == 1);
        z->flashTicks = 0;
        frame.prepare(g);
        assert(frame.sprites().size() == 11 && frame.sprites()[0].tint.r == .1 &&
               frame.sprites()[0].tint.a == double(.9f));
        assert(frame.sprites().back().tint.r == 1 && frame.sprites().back().tint.a == 1);
        z->fading = true;
        z->fade = .5;
        frame.prepare(g);
        assert(frame.sprites().size() == 10 && frame.sprites()[0].tint.a == double(.9f * .5f));
    }
    {
        // Ship retirement occurs in the FIRST list. Later lists must see an
        // absent ship in this same pass, not a pointer refreshed next tick.
        Campaign g([](int) { return Vec2{32, 16}; }, 31, 0, 3, false, 8);
        auto *bottom = dynamic_cast<BottomLaser *>(g.lists.first(List::L3bc));
        bottom->world.zakato({0, .4}, {0, -.004});
        auto *z = dynamic_cast<Zakato *>(g.lists.first(List::L3c8));
        assert(z);
        auto s = std::make_unique<Swarmer>(z->world);
        s->initialize(*g.currentShip());
        g.lists.prepend(List::L3c4, std::move(s));
        g.makePod({0, -.2}, false);
        g.pickup({.2, -.2}, 1);
        g.currentShip()->dead = true;
        g.tick({});
        assert(!g.ship && !g.shipPosition && !g.currentShip());
        auto *pickup = dynamic_cast<Powerup *>(g.lists.first(List::L3c0));
        assert(pickup && !pickup->world.ship);
        for (auto *n = g.lists.first(List::L3c4); n; n = n->next)
            if (auto *p = dynamic_cast<Pod *>(n))
                assert(!p->world.ship);
    }
    std::cout << "Original OGE cell bridging, Zakato variants/trails, Barrel damage/ring and "
                 "cannon configuration passed\n";
}
