#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_pod.h"
#include "native_bullet.h"
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
struct ShipProbe : Ship {
    void run() override {
        throw std::logic_error("Probe only");
    }
};
struct Attacker : Entity {
    int notified = 0;
    void didHit(Entity &) override {
        ++notified;
    }
};
struct World : PodWorld {
    using PodWorld::PodWorld;
    int points = 0;
    std::vector<int> sounds;
    void sound(int32_t n) override {
        sounds.push_back(n);
    }
    void score(int32_t n) override {
        points += n;
    }
};
struct Scene {
    HeadAcState ac;
    EntityLists lists;
    Random random;
    TouchCollider collision;
    std::array<Rgba, 33> palette{};
    Effects effects{lists, random, palette, {}};
    std::vector<int32_t> cells = std::vector<int32_t>(256, 0);
    World world{ac, lists, effects, random, cells};
    ShipProbe ship;
    Scene() {
        ship.initialize(false);
        world.ship = &ship;
        world.dimTint = {.1, .2, .3, .4};
    }
};
int main() {
    {
        Scene s;
        Pod p(s.world);
        p.initialize({.1, -.2}, 17, 2, .025f);
        s.cells[17] = 16;
        assert(p.type == 1 && p.sprite == 16 && p.scale.x == 1.6 && p.baseScale == 1.6f &&
               p.radius == float(double(.025f) * .8));
        Attacker a;
        a.type = 1;
        p.hitBy(a);
        assert(a.notified == 0 && s.cells[17] == 16);
        a.type = 2;
        p.hitBy(a);
        assert(a.notified == 1 && s.cells[17] == 16);
        a.type = 5;
        s.ship.fireBoost = 9;
        p.hitBy(a);
        assert(a.notified == 2 && s.ship.fireBoost == 11 && s.cells[17] == 18 && p.scale.x == 2.2 &&
               !p.dead);
        p.run();
        assert(p.scale.x == 2.2 * .95 && p.scale.y == p.scale.x && p.sprite == 18 &&
               p.tint.a == .7 && p.age == 0);
        p.hitBy(a);
        assert(p.dead && s.cells[17] == 0 && s.world.points == 1 && s.random.calls == 40);
        assert((s.world.sounds == std::vector<int>{32, 33}) && s.ship.fireBoost == 11);
        auto *spark = dynamic_cast<Xpart *>(s.lists.first(List::L3bc));
        assert(spark && spark->colour == 1 && spark->radius == .01f);
    }
    {
        Scene s;
        Pod p(s.world);
        p.initialize({}, 3, 4, .02f);
        s.cells[3] = 16;
        Attacker ship;
        ship.type = 8;
        p.hitBy(ship);
        assert(p.dead && s.cells[3] == 16 && ship.notified == 1 && s.random.calls == 180 &&
               s.world.points == 0);
        Scene decay;
        auto pod = std::make_unique<Pod>(decay.world);
        pod->initialize({.1, .2}, 3, 4, .02f);
        decay.cells[3] = 13;
        decay.world.decayPulse = true;
        decay.lists.prepend(List::L3c4, std::move(pod));
        decay.lists.runUnpaused(1);
        auto *bomb = dynamic_cast<Bomb *>(decay.lists.first(List::L3c4));
        assert(bomb && decay.cells[3] == 0 && bomb->velocity.y == 0 && bomb->position.y == .2);
        assert(bomb->type == 4 && bomb->radius == .03f && bomb->sprite == 19 &&
               bomb->scale.x == .6 && bomb->scale.y == 1);
        decay.world.decayPulse = false;
        decay.lists.runUnpaused(1);
        assert(bomb->velocity.y == .0005 && bomb->position.y == .2 + .0005);
    }
    {
        Scene s;
        Pod p(s.world);
        p.initialize({.1, .2}, 17, 4, .02f);
        s.cells[17] = 16;
        s.world.clearRow = 3;
        p.run();
        assert(!p.dead && s.cells[17] == 16 && s.random.calls == 0);
        s.world.clearRow = 4;
        p.run();
        assert(p.dead && s.cells[17] == 0 && s.world.points == 1 && s.random.calls == 41 &&
               s.world.feedback == .8f);
        auto *ring = dynamic_cast<Xpart *>(s.lists.first(List::L3bc));
        assert(ring && ring->colour == 9 && ring->growth == 1.04f && ring->radius == .03f);
        Scene complete;
        Pod q(complete.world);
        q.initialize({}, 1, 99, .02f);
        complete.cells[1] = 13;
        complete.ac.completed = true;
        complete.world.decayPulse = true;
        q.run();
        assert(q.dead && complete.cells[1] == 0 && complete.world.points == 1 &&
               dynamic_cast<Bomb *>(complete.lists.first(List::L3c4)));
        // Decay and completion are separate source branches, even after dead=true.
    }
    {
        Scene s;
        Entity source;
        source.position = {.1, .2};
        Bomb b(s.world);
        b.initialize(source);
        s.ac.timeScale = .25f;
        b.run();
        assert(b.position.y == .2 + .0005 * .25 && b.velocity.y == .0005 && b.age == 0);
        Attacker a;
        a.type = 5;
        a.angle = 1;
        b.hitBy(a);
        assert(a.notified == 1 && s.ship.fireBoost == 0 && !b.dead);
        a.angle = 0;
        b.velocity.y = .01;
        b.hitBy(a);
        assert(b.velocity.y == -.005 && b.radius == float(double(.03f) * 1.06) &&
               s.ship.fireBoost == 2 && s.world.sounds == std::vector<int>{49});
        b.hitBy(a);
        assert(b.velocity.y == -.005 && s.ship.fireBoost == 4 &&
               s.world.sounds.size() == 1); // rising bomb does not bounce twice
        b.radius = .0401f;
        b.hitBy(a);
        assert(b.dead && s.world.points == 100 && s.random.calls == 61 &&
               s.world.sounds.back() == 58);
        auto *award = dynamic_cast<Fhbon *>(s.lists.first(List::L3bc));
        assert(award && award->sprite == 68);
        Scene piercing;
        Bomb p(piercing.world);
        p.initialize(source);
        piercing.ac.powers[1] = 1;
        p.hitBy(a);
        assert(p.dead && piercing.world.points == 100 && piercing.random.calls == 61);
    }
    {
        Scene s;
        Bomb b(s.world);
        Entity origin;
        b.initialize(origin);
        Attacker a;
        a.type = 8;
        b.hitBy(a);
        assert(a.notified == 1 && b.dead && s.random.calls == 180 && s.world.points == 0);
        auto *spark = dynamic_cast<Xpart *>(s.lists.first(List::L3bc));
        assert(spark && spark->colour == -1 && spark->tint.r == 1);
        Scene clear;
        Bomb c(clear.world);
        c.initialize(origin);
        c.position.y = .451;
        clear.world.clearRow = 9;
        c.run();
        assert(c.dead && clear.world.points == 100 &&
               clear.random.calls == 61); // sweep still executes after out-of-bounds flag
    }
    {
        Scene s;
        s.cells[3] = 16;
        s.ship.fireTimer = 12;
        BulletWorld bullets{s.lists, s.collision, &s.ship, s.ac.powers};
        auto pod = std::make_unique<Pod>(s.world);
        pod->initialize({}, 3, 1, .02f);
        auto *p = pod.get();
        auto bullet = std::make_unique<MyBull>(bullets);
        bullet->position = {};
        bullet->type = 5;
        bullet->velocity = {};
        s.lists.prepend(List::L3c4, std::move(pod));
        s.lists.prepend(List::L3ac, std::move(bullet));
        s.lists.runUnpaused(1);
        assert(s.cells[3] == 18 && p->sprite == 18 && p->scale.x == 2.2 * .95 &&
               s.ship.fireBoost == 2 && s.ship.fireTimer == 3);
        assert(s.lists.ids(List::L3ac).empty());
        auto next = std::make_unique<MyBull>(bullets);
        next->position = {};
        next->type = 5;
        next->velocity = {};
        s.lists.prepend(List::L3ac, std::move(next));
        s.lists.runUnpaused(1);
        assert(s.lists.ids(List::L3c4).empty() && s.cells[3] == 0 && s.world.points == 1);
    }
    std::cout << "Original ordinary Pod cell state, hits/decay/sweep, Bomb bounce/destruction and "
                 "actual bullet/list interactions passed\n";
}
