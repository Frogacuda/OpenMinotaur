#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_ship.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
struct Random : RandomStream {
    int calls = 0;
    uint32_t next() override {
        ++calls;
        return 0;
    }
};
struct World : ShipWorld {
    ShipInput input;
    int reads = 0, finishes = 0, livesDisplay = -99;
    std::vector<int32_t> sounds, stops, goals;
    using ShipWorld::ShipWorld;
    ShipInput readInput() override {
        ++reads;
        return input;
    }
    void sound(int32_t s) override {
        sounds.push_back(s);
    }
    void stopSound(int32_t s) override {
        stops.push_back(s);
    }
    void achievement(int32_t a) override {
        goals.push_back(a);
    }
    void livesChanged(int32_t n) override {
        livesDisplay = n;
    }
    void finishGame() override {
        ++finishes;
    }
};
struct Scene {
    HeadAcState ac;
    EntityLists lists;
    TouchCollider collision;
    Random random;
    std::array<Rgba, 33> palette{};
    Effects effects{lists, random, palette, [](int32_t) { return Vec2{1, 1}; }};
    World world{ac, lists, collision, effects, random};
    Ship ship{world};
    Scene() {
        ship.initialize(false);
    }
    ~Scene() {
        lists.clear();
    }
};
std::vector<MyBull *> createdBullets(EntityLists &lists) {
    std::vector<MyBull *> result;
    for (auto *n = lists.first(List::L3ac); n; n = n->next) {
        auto *b = dynamic_cast<MyBull *>(n);
        assert(b);
        result.insert(result.begin(), b);
    }
    return result;
}
struct Contact : Entity {
    std::vector<int> &order;
    int tag;
    bool notify;
    Contact(std::vector<int> &o, int n, bool hit = false) : order(o), tag(n), notify(hit) {}
    void hitBy(Entity &e) override {
        order.push_back(tag);
        if (notify)
            e.didHit(*this);
    }
};
int main() {
    {
        Scene s;
        s.world.input = {{.125, -.0625}, false};
        s.ship.fireBoost = 2;
        s.ship.age = 41;
        const Vec2 start = s.ship.position;
        s.ship.run();
        assert(s.ship.trail[0].x == start.x && s.ship.trail[0].y == start.y &&
               s.ship.trailFrame == 1 && s.ship.age == 41);
        assert(s.ship.position.x == double(float(.125 * .09)) &&
               s.ship.position.y == start.y + double(float(-.0625 * .09)));
        assert(s.ship.fireBoost == float(2. * .995) && s.ship.fireTimer == 18 &&
               s.world.reads == 1);
        const auto shots = createdBullets(s.lists);
        assert(shots.size() == 1 && shots[0]->position.x == 0 &&
               shots[0]->position.y == start.y - double(.02f));
        assert(s.world.sounds == std::vector<int32_t>{34} && s.random.calls == 0);
        s.ship.arrival = .999f;
        s.ship.arrivalStep = .005f;
        s.ship.fireTimer = 10;
        s.ac.gridsWithoutDeath = 5;
        s.ship.run();
        assert(s.ship.state == Ship::State::Playing && s.ac.powers[5] == 150 &&
               s.world.goals == std::vector<int32_t>{3});
    }
    {
        Scene s;
        s.world.alternateLayout = true;
        s.world.input = {{.125, -.25}, false};
        s.ship.fireTimer = 99;
        s.ship.run();
        assert(s.ship.position.x == double(float(.125 * .12)));
        s.world.input = {{.00123456789, -.00123456789}, true};
        const Vec2 before = s.ship.position;
        s.ship.run();
        assert(s.ship.position.x == before.x + double(float(.00123456789)) &&
               s.ship.position.y == before.y + double(float(-.00123456789)));
        s.world.input = {{-99, -99}, true};
        s.ship.run();
        assert(s.ship.position.x == -.4 && s.ship.position.y == -.2280000001192093);
        s.world.input = {{99, 99}, true};
        s.ship.run();
        assert(s.ship.position.x == .43 && s.ship.position.y == .428);
        s.ship.velocity.y = -.001;
        s.ship.run();
        assert(s.ship.position.y > .428); // source bottom-clamp velocity gate
    }
    {
        for (unsigned mask = 0; mask < 16; ++mask) {
            Scene s;
            s.ship.state = Ship::State::Playing;
            s.ship.position = {.2, .1};
            s.world.clock = 37;
            s.ac.powers[0] = (mask & 1) ? 300 : 0;
            s.ac.powers[1] = (mask & 2) ? 1 : 0;
            s.ac.powers[2] = (mask & 4) ? 1 : 0;
            s.ac.powers[4] = (mask & 8) ? 1 : 0;
            s.world.input = {{.01, 0}, true};
            s.ship.run();
            std::vector<float> expected;
            if ((mask & 2) && !(mask & 8)) {
                expected.push_back(-.3926991f);
                expected.push_back(.3926991f);
            }
            int count = (mask & 1) ? 2 : 1;
            float step = (mask & 1) ? 3.1415927f : 0, angle = 0;
            if (mask & 4) {
                count = 4;
                step = 1.5707964f;
            }
            if (mask & 8) {
                count = 32;
                step = .19634955f;
                angle = (37.f / 255.f) * 3.1415927f;
            }
            for (int i = 0; i < count; ++i) {
                expected.push_back(angle);
                angle += step;
            }
            const auto shots = createdBullets(s.lists);
            assert(shots.size() == expected.size());
            for (size_t i = 0; i < shots.size(); ++i) {
                assert(shots[i]->angle == expected[i] && shots[i]->position.x == .2);
            }
            assert(s.ship.position.x == .2 + double(float(.01)) && s.ac.shipActive);
            const int expectedTimer = (mask & 1) ? 13 : ((mask & 2) ? 10 : 20);
            assert(s.ship.fireTimer == expectedTimer); // power0 can overwrite halved 10 with 13
        }
        Scene s;
        s.ship.state = Ship::State::Playing;
        s.ac.powers[0] = 630;
        s.ac.powers[4] = 1;
        s.ship.run();
        assert(s.ship.fireTimer == 2 && s.world.goals == std::vector<int32_t>{8});
    }
    {
        Scene s;
        s.ship.state = Ship::State::Playing;
        s.ship.fireTimer = 99;
        s.ship.radius = .0301f;
        s.ship.run();
        assert(s.ship.radius == float(double(.0301f) * .99));
        s.ship.radius = .01f;
        s.ship.run();
        assert(s.ship.radius == .02f);
        s.ac.powers[5] = 1;
        s.ship.radius = .0799f;
        s.ship.run();
        assert(s.ship.radius == float(double(.0799f) + .001) && s.ship.radius > .08f);
        s.ac.completed = true;
        s.ship.trailFrame = 0;
        s.ship.trail[511] = {9, 9};
        s.ship.fireTimer = 0;
        s.ship.run();
        assert(s.ship.state == Ship::State::Departing && s.ship.arrival == 1 &&
               s.world.feedback == .9f);
        assert(s.ship.trail[510].y == s.ship.position.y && s.ship.trail[511].y == 9 &&
               createdBullets(s.lists).size() == 1);
        s.ship.arrival = .001f;
        const double y = s.ship.position.y;
        s.ship.run();
        assert(s.ship.dead && s.ship.velocity.y == -.0002 && s.ship.position.y == y - .0002 &&
               s.world.stops == std::vector<int32_t>{54});
    }
    {
        Scene s;
        s.ship.state = Ship::State::Playing;
        s.ac.powers[6] = 100;
        s.ac.gridsWithoutDeath = 4;
        s.ship.age = 100;
        s.ship.foom();
        assert(s.ship.state == Ship::State::Dying && s.ship.age == 0 && s.ac.deathRepel == 500 &&
               s.ac.gridsWithoutDeath == 0 && s.ac.powers[6] == 0);
        assert(s.world.feedback == .5f && s.random.calls == 101 &&
               s.lists.ids(List::L3bc).size() == 51);
        assert(s.world.stops == std::vector<int32_t>{54} &&
               s.world.sounds == std::vector<int32_t>{39});
        const Vec2 position = s.ship.position;
        s.world.input = {{99, 99}, true};
        s.world.dyingTint = {.1, .2, .3, .4};
        for (int i = 0; i < 30; ++i)
            s.ship.run();
        assert(s.ship.state == Ship::State::Dying && s.ship.position.x == position.x &&
               s.ship.position.y == position.y);
        s.ship.run();
        assert(s.ship.state == Ship::State::Departing && s.world.feedback == .9f &&
               s.random.calls == 1101);
        assert(s.ship.tint.a == .4 && s.world.reads == 31 && s.lists.ids(List::L3bc).size() == 551);
        s.ship.arrival = .001f;
        s.ship.fireTimer = 7;
        s.ship.fireBoost = 2;
        s.ac.powers[0] = 600;
        s.ship.run();
        assert(s.ac.lives == 2 && s.world.livesDisplay == 2 &&
               s.ship.state == Ship::State::Arriving && s.ac.deathRepel == 0 &&
               s.ac.dropChance == 500);
        assert(s.ship.position.x == 0 && s.ship.position.y == .1 && s.ship.fireTimer == 7 &&
               s.ship.fireBoost == float(2 * .995));
        for (auto power : s.ac.powers)
            assert(power == 0);
        assert(s.world.sounds.back() == 50 && s.ship.trail[511].y == .1);
    }
    {
        Scene s;
        s.ship.state = Ship::State::Departing;
        s.ship.arrival = 0;
        s.ac.lives = 1;
        s.ac.powers[6] = 1;
        s.ship.run();
        assert(s.ac.gameOver && s.world.finishes == 1 && s.ac.lives == 0 && s.ac.phase == 20 &&
               s.ac.phaseTimer == 3);
        assert(s.ship.state == Ship::State::Hidden && s.world.stops == std::vector<int32_t>{54});
        s.world.input = {{.01, 0}, true};
        s.ship.run();
        assert(s.ship.position.x == double(float(.01))); // source hidden common path
        Scene demo;
        demo.world.demo = true;
        demo.ship.state = Ship::State::Departing;
        demo.ship.arrival = 0;
        demo.ship.run();
        assert(demo.ac.lives == 2 && demo.ship.state == Ship::State::Hidden && !demo.ac.gameOver &&
               demo.world.finishes == 0);
        Scene shield;
        shield.ship.state = Ship::State::Playing;
        shield.ac.powers[5] = 1;
        shield.ac.deflectionCount = 500;
        shield.ship.foom();
        assert(shield.ship.state == Ship::State::Playing && shield.random.calls == 0 &&
               shield.world.goals == std::vector<int32_t>{2});
    }
    {
        Scene s;
        s.ship.fireTimer = 99;
        std::vector<int> order;
        for (auto binding :
             {std::pair<List, int>{List::L3c4, 1}, {List::L3c0, 2}, {List::L3c8, 3}}) {
            auto target = std::make_unique<Contact>(order, binding.second);
            target->position = s.ship.position;
            s.lists.prepend(binding.first, std::move(target));
        }
        s.ship.run();
        assert((order == std::vector<int>{1, 2, 3}));
        // A concrete Ship on the first list fires bullets that update on the
        // later bullet list in this very pass, before the ship's next update.
        Scene live;
        auto ship = std::make_unique<Ship>(live.world);
        ship->initialize(false);
        auto *p = ship.get();
        live.lists.prepend(List::L3b0, std::move(ship));
        live.lists.runUnpaused(1);
        const auto shots = createdBullets(live.lists);
        assert(shots.size() == 1 && shots[0]->position.y == .1 - double(.02f) - .015 &&
               p->trailFrame == 1);
    }
    {
        Scene s;
        auto player = std::make_unique<Ship>(s.world);
        auto *p = player.get();
        p->initialize(false);
        p->run(); // create a real shot before completion retires the Ship
        p->state = Ship::State::Departing;
        p->arrival = 0;
        s.ac.completed = true;
        s.lists.prepend(List::L3b0, std::move(player));
        s.lists.runUnpaused(1);
        assert(s.lists.ids(List::L3b0).empty());
        const auto shots = createdBullets(s.lists);
        assert(shots.size() == 1 && shots[0]->world.ship == nullptr &&
               shots[0]->position.y == .1 - double(.02f) - .015);
        s.lists.runUnpaused(1); // no dangling binding after Ship destruction
        assert(shots[0]->position.y == (.1 - double(.02f) - .015) - .015);
    }
    {
        Scene s;
        s.world.demo = true;
        s.ship.state = Ship::State::Playing;
        s.ship.fireTimer = 99;
        s.ship.position = {0, .1};
        auto near = std::make_unique<Entity>();
        near->type = 2;
        near->position = {-.01, .1};
        auto far = std::make_unique<Entity>();
        far->type = 2;
        far->position = {.3, .1};
        s.lists.append(List::L3c4, std::move(near));
        s.lists.append(List::L3c4, std::move(far));
        s.ship.run();
        assert(s.ship.position.x > 0); // last, not nearest, qualifying leader
        auto pickup = std::make_unique<Entity>();
        pickup->position = {-.2, .2};
        s.lists.prepend(List::L3c0, std::move(pickup));
        const double x = s.ship.position.x;
        s.ship.run();
        assert(s.ship.position.x < x);
    }
    std::cout << "Original Ship states, all firing power combinations, precise movement, "
                 "contact/list order, death/respawn/demo and effects passed\n";
}
