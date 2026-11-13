#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_bullet.h"
#include "native_head.h"
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
struct Target : Entity {
    int hits = 0, shots = 0;
    std::function<void(Entity &)> onHit, onShot;
    void hitBy(Entity &e) override {
        ++hits;
        if (onHit)
            onHit(e);
    }
    void wasShot(Entity &e) override {
        ++shots;
        if (onShot)
            onShot(e);
    }
};
struct ShipProbe : Ship {
    void run() override {
        throw std::logic_error("Ship run pending");
    }
};
struct HeadBindings : HeadWorld {
    void touchEnemyList(Head &) override {}
    void checkSweep(Head &) override {}
};
int main() {
    {
        TouchCollider c;
        Entity m;
        Target a, b;
        m.radius = a.radius = b.radius = .1f;
        a.next = &b;
        a.position = {.05, 0};
        b.position = {.15, 0};
        std::vector<int> order;
        m.collisionFlags = 1;
        a.onShot = [&](Entity &) { order.push_back(1); };
        a.onHit = [&](Entity &e) {
            order.push_back(2);
            e.dead = true;
            e.position = {3, 4};
            e.radius = 0;
        };
        b.onHit = [&](Entity &) { order.push_back(3); };
        assert(c.touch(m, &a));
        assert((order == std::vector<int>{1, 2, 3}));
        assert(m.dead && m.contact09 && a.shots == 1 && b.shots == 1 && b.hits == 1);
        assert(c.lastPosition.x == .15 && c.normal.x < 0 && c.normal.y < 0);
        // Dead movers are gated only on ENTRY to each touch call.
        assert(!c.touch(m, &a) && a.hits == 1);
    }
    {
        TouchCollider c;
        Entity m;
        Target a, b, d;
        a.next = &b;
        b.next = &d;
        m.radius = a.radius = b.radius = d.radius = .1f;
        a.acceptsContact = false;
        b.position = {9, 9};
        d.position = {};
        m.contact08 = true;
        assert(!c.touch(m, &a));
        assert(!m.contact08 && d.hits == 0); // blocked survives distant target
        a.acceptsContact = true;
        m.contact08 = false;
        m.collisionGroup = a.collisionGroup = 2;
        assert(!c.touch(m, &a) && d.hits == 0); // group block also persists
        m.collisionGroup = 0;
        a.drawDelay = 1;
        assert(!c.touch(m, &a));
        a.drawDelay = 0;
        m.drawDelay = 1;
        assert(!c.touch(m, &a));
        m.drawDelay = 0;
        assert(c.touch(m, &a) && a.hits == 1 && d.hits == 1);
    }
    {
        TouchCollider c;
        Entity m;
        Target a, b, d;
        a.next = &b;
        m.radius = a.radius = b.radius = d.radius = .1f;
        a.onHit = [&](Entity &) { a.next = &d; };
        assert(c.touch(m, &a) && a.hits == 1 && b.hits == 0 && d.hits == 1);
        m.contact09 = false;
        a.dead = d.dead = true;
        assert(!c.touch(m, &a) && !m.contact09);
        a.dead = false;
        a.next = nullptr;
        m.position = a.position = {};
        assert(c.touch(m, &a) && c.normal.x == 0 && c.normal.y == 0);
        a.position = {double(m.radius + a.radius), 0};
        assert(!c.touch(m, &a)); // strict overlap
        c.lastPosition = {5, 6};
        assert(!c.touch(m, nullptr) && c.lastPosition.x == 5);
    }
    {
        EntityLists lists;
        TouchCollider c;
        ShipProbe ship;
        ship.initialize(false);
        ship.fireTimer = 12;
        std::array<int32_t, 8> powers{};
        BulletWorld w{lists, c, &ship, powers};
        MyBull b(w);
        b.initialize(ship);
        assert(b.type == 5 && b.sprite == 6 && b.radius == .02f &&
               b.position.y == .1 - double(.02f));
        assert(b.scale.x == .4 && b.tint.r == 1 && b.tint.g == 1 && b.tint.b == 0);
        const double before = b.position.y;
        b.run();
        assert(b.position.y == before - .015 && b.age == 0);
        b.angle = .5f;
        const Vec2 initial = b.position;
        b.run();
        Vec2 step{0, -.015};
        step.rotateBy(double(.5f));
        assert(b.position.x == initial.x + step.x && b.position.y == initial.y + step.y &&
               b.velocity.x == 0);
        Entity hit;
        hit.type = 9;
        b.didHit(hit);
        assert(!b.dead && ship.fireTimer == 12);
        hit.type = 2;
        w.powers[1] = 1;
        b.didHit(hit);
        assert(!b.dead);
        w.powers[1] = 0;
        b.didHit(hit);
        assert(b.dead && ship.fireTimer == 12); // angled shot cannot accelerate
        b.dead = false;
        b.angle = 0;
        b.didHit(hit);
        assert(b.dead && ship.fireTimer == 3);
        ship.fireTimer = 1;
        b.dead = false;
        b.didHit(hit);
        assert(ship.fireTimer == 1);
    }
    {
        EntityLists lists;
        TouchCollider c;
        ShipProbe ship;
        ship.initialize(false);
        std::array<int32_t, 8> powers{};
        BulletWorld w{lists, c, &ship, powers};
        auto target = std::make_unique<Target>();
        auto *a = target.get();
        auto hostile = std::make_unique<Target>();
        auto *h = hostile.get();
        MyBull bullet(w);
        bullet.initialize(ship);
        a->position = h->position = bullet.position;
        a->onHit = [&](Entity &e) { e.didHit(*a); };
        lists.prepend(List::L3c4, std::move(target));
        lists.prepend(List::L3c8, std::move(hostile));
        const double y = bullet.position.y;
        bullet.run();
        assert(a->hits == 1 && h->hits == 0 && bullet.dead && bullet.position.y == y - .015);
        // The second list is gated after the first call; movement still runs.
        bullet.dead = false;
        w.powers[1] = 1;
        bullet.position = a->position;
        bullet.run();
        assert(h->hits == 1 && !bullet.dead);
        w.power0Tint = {.1, .2, .3, .4};
        w.power2Tint = {.2, .3, .4, .5};
        w.power1Tint = {.3, .4, .5, .6};
        w.powers[0] = w.powers[2] = 1;
        bullet.position = {};
        bullet.run();
        assert(bullet.tint.a == .6);
        w.powers[1] = 0;
        bullet.run();
        assert(bullet.tint.a == .5);
        for (const Vec2 p : {Vec2{-.401, 0}, Vec2{.431, 0}, Vec2{0, -.379}, Vec2{0, .449}}) {
            bullet.dead = false;
            bullet.position = p;
            bullet.velocity = {};
            bullet.run();
            assert(bullet.dead);
        }
    }
    {
        // A real Head declines contact before age/opacity eligibility, so its
        // inherited attacker notification must not consume a bullet early.
        EntityLists lists;
        TouchCollider c;
        ShipProbe ship;
        ship.initialize(false);
        HeadBindings bindings;
        BulletWorld w{lists, c, &ship, bindings.ac.powers};
        auto head = std::make_unique<Head>(bindings);
        auto *h = head.get();
        h->position = ship.position;
        h->age = 29;
        h->opacity = 1;
        lists.prepend(List::L3c4, std::move(head));
        MyBull bullet(w);
        bullet.position = ship.position;
        bullet.type = 5;
        bullet.velocity = {};
        bullet.run();
        assert(!bullet.dead);
        h->age = 30;
        h->opacity = .99f;
        bullet.run();
        assert(!bullet.dead);
    }
    std::cout << "Original touch traversal, callback order, snapshot precision, myBull "
                 "movement/contact/power and early Head gates passed\n";
}
