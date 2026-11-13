#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_pickup.h"
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
struct World : PickupWorld {
    using PickupWorld::PickupWorld;
    int points = 0;
    std::vector<int> sounds;
    void sound(int32_t n) override {
        sounds.push_back(n);
    }
    void score(int32_t n) override {
        points += n;
    }
};
struct ShipProbe : Ship {
    void run() override {
        throw std::logic_error("Probe only");
    }
};
struct ShipBindings : ShipWorld {
    using ShipWorld::ShipWorld;
    ShipInput readInput() override {
        return {};
    }
    void sound(int32_t) override {}
    void achievement(int32_t) override {}
};
struct Scene {
    HeadAcState ac;
    EntityLists lists;
    Random random;
    std::array<Rgba, 33> palette{};
    std::array<Rgba, 8> pickupTints{};
    Effects effects{lists, random, palette, {}};
    World world{ac, lists, effects, random, pickupTints};
    ShipProbe ship;
    Scene() {
        world.ship = &ship;
        ship.initialize(false);
    }
    ~Scene() {
        lists.clear();
    }
};
int main() {
    {
        Scene s;
        Powerup p(s.world);
        p.initialize({.2, -.3}, 3);
        assert(p.kind == 3 && p.type == 0 && p.sprite == 26 && p.radius == .04f &&
               p.velocity.y == (-.3 + .7) * -.015);
        assert(p.tint.r == 96. / 255 && p.tint.g == 224. / 255 && p.tint.b == 1 &&
               s.random.calls == 0);
        s.pickupTints[3] = {.1, .2, .3, .4};
        s.ship.position.x = -.8;
        const double vy = p.velocity.y, y = p.position.y;
        p.run();
        assert(p.position.x == .2 - .001 && p.position.y == y + (vy + .0002) && p.sprite == 26 &&
               p.age == 1 && s.ac.liveThreats == 1 && p.tint.r == .1);
        p.position.x = 0;
        s.ship.position.x = .3;
        p.run();
        assert(p.position.x == 0 && p.sprite == 27); // asymmetric attraction
        s.ship.position.x = 0;
        p.run();
        assert(p.position.x == .001); // exact zero moves right
        p.kind = 6;
        s.ac.timeScale = .25f;
        const double last = p.position.y;
        const double speed = p.velocity.y + .0002;
        p.run();
        assert(p.kind == 4 && p.position.y == last + speed * .25 && s.ac.liveThreats == 4);
        p.age = 32767;
        p.run();
        assert(p.sprite == 26);
        p.position.y = .601;
        p.velocity.y = 0;
        p.run();
        assert(p.dead);
    }
    {
        constexpr int32_t amounts[] = {600, 600, 600, 600, 250, 400, 1800, 20};
        for (int kind = 0; kind < 8; ++kind) {
            Scene s;
            Powerup p(s.world);
            p.initialize({.1, -.2}, kind);
            s.ac.powers.fill(7);
            Entity any;
            any.type = 999;
            p.hitBy(any);
            const bool replace = kind == 3 || kind == 6 || kind == 7;
            assert(s.ac.powers[size_t(kind)] ==
                   (replace ? amounts[kind] : 7 + amounts[kind]) + (kind == 5 ? 20 : 0));
            assert(s.ac.powers[5] == (kind == 5 ? 427 : 27) && p.dead && s.world.points == 100 &&
                   s.random.calls == 61);
            assert(s.ac.warpUsed == (kind == 6) &&
                   s.world.sounds == std::vector<int>{kind == 6 ? 54 : 48});
            assert(s.lists.ids(List::L3bc).size() == 31);
            auto *award = dynamic_cast<Fhbon *>(s.lists.first(List::L3bc));
            assert(award && award->baseSprite == 68 && award->sprite == 68);
            Vec2 v{0, -.003};
            v.rotateBy(-.5);
            assert(award->velocity.x == v.x && award->position.x == .1 && award->scale.y == .5 &&
                   award->radius == .03f);
            auto *spark = dynamic_cast<Xpart *>(award->next);
            assert(spark && spark->colour == kind + 6 && spark->sprite == 34 &&
                   spark->radius == .015f);
            assert(spark->fade == 0); // constructors do not advance effects
        }
    }
    {
        Scene s;
        Fhbon b;
        b.sprite = 68;
        Entity source;
        source.position = {.1, .2};
        b.initialize(source, s.random);
        const Vec2 p = b.position, v = b.velocity;
        b.run();
        assert(b.age == 1 && b.sprite == 68 && b.position.x == p.x + v.x &&
               b.position.y == p.y + v.y && b.tint.a == .995);
        b.age = 3;
        b.run();
        assert(b.sprite == 69);
        b.age = 7;
        b.run();
        assert(b.sprite == 70);
        b.age = 11;
        b.run();
        assert(b.sprite == 68);
        b.tint.a = .01001;
        b.run();
        assert(b.dead);
    }
    {
        // Real ship contact collects a pickup; because its list follows the
        // effects list but precedes enemies, no invented global newborn delay.
        Scene s;
        TouchCollider collision;
        ShipBindings bindings{s.ac, s.lists, collision, s.effects, s.random};
        auto ship = std::make_unique<Ship>(bindings);
        auto *player = ship.get();
        player->initialize(false);
        player->fireTimer = 99;
        s.world.ship = player;
        auto pickup = std::make_unique<Powerup>(s.world);
        pickup->initialize(player->position, 0);
        s.lists.prepend(List::L3b0, std::move(ship));
        s.lists.prepend(List::L3c0, std::move(pickup));
        s.lists.runUnpaused(1);
        assert(s.world.points == 100 && s.ac.powers[0] == 600 && s.ac.powers[5] == 20 &&
               s.lists.ids(List::L3c0).empty());
        auto *award = dynamic_cast<Fhbon *>(s.lists.first(List::L3bc));
        assert(award && award->age == 1); // born before effects visit
        assert(s.ac.liveThreats == 1);    // already-dead pickup still runs before unlink
    }
    std::cout << "Original powerup movement, all eight power rules, artwork, shared RNG and real "
                 "Ship/list collection passed\n";
}
