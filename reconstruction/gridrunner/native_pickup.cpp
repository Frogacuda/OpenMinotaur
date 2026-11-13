#include "native_pickup.h"
namespace gr::reconstruction::ios {
void Fhbon::initialize(const Entity &source, RandomStream &random) {
    baseSprite = sprite;
    age = 0;
    radius = .03f;
    scale.y = .5;
    velocity = {0, -.003};
    velocity.rotateBy(random.range(-.5, .5));
    position = source.position;
}
void Fhbon::run() {
    age = add32(age, 1);
    sprite = (age >> 2) % 3 + baseSprite;
    position.x += velocity.x;
    position.y += velocity.y;
    tint.a *= .995;
    if (tint.a < .01)
        dead = true;
}
void Powerup::initialize(Vec2 p, int32_t selectedKind) {
    if (selectedKind < 0 || selectedKind >= 8)
        throw std::invalid_argument("Original pickup kind outside table");
    constexpr std::array<uint32_t, 8> colours = {0xff6060ff, 0xff60ffff, 0xffc060ff, 0x60e0ffff,
                                                 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    kind = selectedKind;
    age = 0;
    sprite = 26;
    position = p;
    velocity.y = (p.y + .7) * -.015;
    tint = unpackRgba(colours[size_t(kind)]);
    radius = .04f;
}
void Powerup::run() {
    if (world.ship) {
        const double difference = world.ship->position.x - position.x;
        // Source is asymmetric: no fabs before the <.2 test.
        if (difference < .2)
            position.x += difference < 0 ? -.001 : .001;
    }
    age = add32(age, 1);
    if (ac.timeScale < 1 && kind == 6)
        kind = 4;
    ac.liveThreats = add32(ac.liveThreats, 1);
    tint = world.tints[size_t(kind)];
    sprite = int32_t((uint32_t(age) & 0x7fff) >> 1) % 7 + 26;
    velocity.y += .0002;
    position.y += velocity.y * double(ac.timeScale);
    if (position.y > .6)
        dead = true;
}
void Powerup::hitBy(Entity &) {
    constexpr std::array<int32_t, 8> amounts = {600, 600, 600,  600,
                                                250, 400, 1800, 20}; // iOS 0x66270
    if (kind == 6) {
        world.sound(54);
        ac.warpUsed = true;
    } else
        world.sound(48);
    dead = true;
    const bool replace = kind == 3 || kind == 6 || kind == 7;
    ac.powers[size_t(kind)] =
        replace ? amounts[size_t(kind)] : add32(ac.powers[size_t(kind)], amounts[size_t(kind)]);
    ac.powers[5] = add32(ac.powers[5], 20);
    world.effects.explode({position, 34, .01f, .015f, uint32_t(kind + 6), 30, .015f});
    auto award = std::make_unique<Fhbon>();
    award->sprite = 68;
    world.score(100);
    award->initialize(*this, world.random);
    world.lists.prepend(List::L3bc, std::move(award));
}
} // namespace gr::reconstruction::ios
