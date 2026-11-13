#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_effects.h"
#include <cassert>
#include <deque>
#include <iostream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
struct ScriptedRandom : RandomStream {
    std::deque<uint32_t> values;
    int calls = 0;
    uint32_t next() override {
        assert(!values.empty());
        ++calls;
        const auto v = values.front();
        values.pop_front();
        return v;
    }
};
struct ShipProbe : Ship {
    void run() override {
        throw std::logic_error("Ship update pending");
    }
};
struct World : HeadWorld {
    Effects &effects;
    ScriptedRandom &random;
    int total = 0;
    explicit World(Effects &e, ScriptedRandom &r) : effects(e), random(r) {}
    void touchEnemyList(Head &) override {}
    void checkSweep(Head &) override {}
    void score(int32_t n) override {
        total += n;
    }
    void sound(int32_t) override {}
    void explode(const HeadExplosion &s) override {
        effects.explode(s);
    }
    void bonus(const HeadBonus &s) override {
        effects.bonus(s, *ship);
    }
    uint32_t globalRandom() override {
        return random.next();
    }
    double headRandomRange(Head &, double low, double high) override {
        return random.range(low, high);
    }
    void outline(const HeadOutline &s) override {
        effects.outline(s);
    }
};
int main() {
    {
        DarwinRandom r;
        for (const auto value :
             {16807u, 282475249u, 1622650073u, 984943658u, 1144108930u, 470211272u})
            assert(r.next() == value);
        r.seed(0);
        for (const auto value :
             {520932930u, 28925691u, 822784415u, 890459872u, 145532761u, 2132723841u})
            assert(r.next() == value);
        r.seed(2147483647);
        assert(r.next() == 0 && r.next() == 520932930);
        r.seed(0xffffffffu);
        assert(r.next() == 16807); // defined original 32-bit seed domain
    }
    std::array<Rgba, 33> palette{};
    palette[2] = {.2, .3, .4, .5};
    palette[4] = {.4, .5, .6, .7};
    {
        ScriptedRandom random;
        random.values = {0, 2147483647};
        assert(random.range(-3.141592654, 3.141592654) == -3.141592654);
        assert(random.range(.002, .007) == (.007 - .002) + .002 && random.calls == 2);
    }
    {
        EntityLists lists;
        ScriptedRandom random;
        random.values = {0, 0, 2147483647, 2147483647};
        Effects effects{lists, random, palette, {}};
        effects.explode({{.1, .2}, 2, .002f, .01f, 2, 2, .01f});
        auto *second = dynamic_cast<Xpart *>(lists.first(List::L3bc));
        assert(second);
        auto *first = dynamic_cast<Xpart *>(second->next);
        assert(first && !first->next);
        Vec2 v1{0, double(.002f)}, v2{0, double(.01f)};
        v1.rotateBy(-3.141592654);
        v2.rotateBy(3.141592654);
        assert(first->velocity.x == v1.x && first->velocity.y == v1.y &&
               second->velocity.x == v2.x && second->velocity.y == v2.y);
        assert(random.calls == 4 && first->fadeStep == .02f && first->growth == 1 &&
               first->colour == 2);
        const double x = first->position.x;
        lists.runUnpaused(1);
        assert(first->position.x == x + v1.x && first->fade == .02f && first->age == 0 &&
               random.calls == 4);
        assert(first->tint.r == .2 && first->tint.a == double(1.f - .02f));
        effects.explode({{}, 2, 0, 0, 0, 0, .01f});
        assert(random.calls == 4);
    }
    {
        EntityLists lists;
        ScriptedRandom random;
        random.values = {0};
        ShipProbe ship;
        ship.position = {3, 4};
        int lookup = 0;
        Effects effects{lists, random, palette, [&](int32_t sprite) {
                            lookup = sprite;
                            return Vec2{40, 20};
                        }};
        effects.bonus({{}, 25, .02f, 4}, ship);
        auto *b = dynamic_cast<Xpart *>(lists.first(List::L3bc));
        assert(b);
        assert(lookup == 27 && b->scale.x == 1 && b->scale.y == .5 && b->fadeStep == .01f &&
               b->sprite == 25);
        assert(b->velocity.x == (3. / 5) * .002 && b->velocity.y == (4. / 5) * .002 &&
               random.calls == 1);
        effects.outline({{}, {}, .25f, .06f, 1.1f});
        auto *ring = dynamic_cast<Xpart *>(lists.first(List::L3bc));
        assert(ring && ring->sprite == 33 && ring->colour == 4 && random.calls == 1);
        ring->run();
        assert(ring->scale.x == double(1.1f) && ring->fade == .05f && ring->angle == .25f);
        ring->fade = .99f;
        ring->run();
        assert(ring->dead && ring->tint.a < 0); // no alpha clamp
        const auto c = unpackRgba(0x12345678);
        assert(c.r == 18. / 255 && c.g == 52. / 255 && c.b == 86. / 255 && c.a == 120. / 255);
        Xpart staticTint(palette);
        staticTint.colour = 0;
        staticTint.tint = {.9, .8, .7, .6};
        staticTint.run();
        assert(staticTint.tint.r == .9);
    }
    {
        // Concrete original effect factories share the same stream as the
        // Head drop draw. They are not recording substitutes in this check.
        EntityLists lists;
        ScriptedRandom random;
        random.values = std::deque<uint32_t>(42, 0);
        Effects effects{lists, random, palette, [](int32_t) { return Vec2{40, 20}; }};
        ShipProbe ship;
        ship.position = {0, .1};
        World world(effects, random);
        world.ship = &ship;
        world.shipPosition = &ship.position;
        auto head = std::make_unique<Head>(world);
        auto *h = head.get();
        h->type = 2;
        h->age = 30;
        h->opacity = 1;
        h->position = {.2, -.1};
        world.ac.dropChance = 0;
        world.ac.dropIncrement = 17;
        lists.prepend(List::L3c4, std::move(head));
        Entity attacker;
        attacker.type = 8;
        lists.prepend(List::L3ac, [&](ListNode &b) {
            h->hitBy(attacker);
            b.dead = true;
        });
        lists.runUnpaused(1);
        assert(world.total == 30 && random.calls == 42 && random.values.empty() &&
               world.ac.dropChance == 17);
        assert(lists.ids(List::L3bc).size() == 21 && h->type == 6 && h->deathCountdown == 4);
        auto *bonus = dynamic_cast<Xpart *>(lists.first(List::L3bc));
        assert(bonus && bonus->sprite == 25 && bonus->fade == 0);
        // Effects list was visited before the bullet list: these births wait.
        lists.runUnpaused(1);
        assert(bonus->fade == .01f && h->deathCountdown == 3 && random.calls == 42);
    }
    std::cout << "Original xpart, effect factories, shared RNG consumption and real "
                 "Head/effect/list integration passed\n";
}
