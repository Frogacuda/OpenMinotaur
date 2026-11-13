#include "native_cannons.h"
#include "native_pickup.h"
#include <cmath>
namespace gr::reconstruction::ios {
void OGE::initialize(Vec2 source) {
    physical = source;
    cellX = cellY = mode = -1;
    pog();
}
void OGE::pog() {
    const int32_t x = int32_t((physical.x + .5) * 28), y = int32_t((physical.y + .5) * 36);
    if (mode < 0) {
        position = {double(float(x) / 28.f) - .5, double(float(y) / 36.f) - .5};
        mode = std::abs(velocity.y) <= std::abs(velocity.x) ? 1 : 0;
        cellX = x;
        cellY = y;
    } else if (mode == 0 && y != cellY && x != cellX) {
        mode = 2;
        cellX = x;
        cellY = y;
        bridgeVelocity = float(std::abs(velocity.y));
        // Carry the horizontal travel sign into the bridge velocity (ARM IT NE).
        if (float(velocity.x) < 0)
            bridgeVelocity = -bridgeVelocity;
    } else if (mode == 1 && x != cellX && y != cellY) {
        mode = 3;
        cellX = x;
        cellY = y;
        bridgeVelocity = float(std::abs(velocity.x));
        if (float(velocity.y) < 0)
            bridgeVelocity = -bridgeVelocity;
    }
    if (mode == 0 || mode == 1) {
        physical.x += velocity.x * double(world.ac.timeScale);
        physical.y += velocity.y * double(world.ac.timeScale);
        if (mode == 0)
            position.y = physical.y;
        else
            position.x = physical.x;
    }
    if (mode == 3) {
        position.y += double(bridgeVelocity * world.ac.timeScale);
        if (int32_t((position.y + .5) * 36) == cellY)
            mode = 1;
    } else if (mode == 2) {
        position.x += double(bridgeVelocity * world.ac.timeScale);
        if (int32_t((position.x + .5) * 28) == cellX)
            mode = 0;
    }
}
void OGE::run() {
    if (physical.x < -.6 || physical.x > .6 || physical.y < -.6 || physical.y > .6)
        dead = true;
    pog();
}
void Zakato::initialize(Vec2 source, Vec2 speed) {
    if (world.ac.level < 8)
        throw std::logic_error("Original Zakato launch requires grid index >= 8");
    velocity = speed;
    OGE::initialize(source);
    radius = .025f;
    age = 0;
    sprite = 71;
    trail.fill(position);
    trailFrame = 0;
    fading = false;
    fade = 1;
    world.random.next(); // overwritten variant is still a real random draw
    variant = (world.ac.level - 8) >> 2;
    if (variant > 3)
        variant = int32_t(world.random.next() & 3);
    // The original sets armour for >=2, then clears it unless variant == 3.
    // Preserve this surprising branch, rather than inventing variant-2 armour.
    armour = variant == 3 ? 3 : 0;
    if (variant == 0)
        world.game.achievement(4);
    if (variant == 3)
        world.game.achievement(5);
}
void Zakato::run() {
    if (fading) {
        fade = float(double(fade) * .95);
        if (double(fade) < .01)
            dead = true;
        return;
    }
    constexpr int32_t frames[] = {72, 82, 83, 71}; // iOS table 0x66730
    sprite = (uint32_t(age) & 4) ? 71 : frames[variant];
    age = add32(age, 1);
    if (position.x < -.39 && velocity.x < 0)
        velocity.x = -velocity.x;
    if (position.x > .41 && velocity.x > 0)
        velocity.x = -velocity.x;
    if ((variant | 2) == 3) {
        if (position.y < -.322 && velocity.y < 0)
            velocity.y = -velocity.y;
        if (position.y > .418 && velocity.y > 0)
            velocity.y = -velocity.y;
    }
    OGE::run();
    trail[trailFrame & 127] = position;
    ++trailFrame;
    if (double(radius) > .025)
        radius = float(double(radius) * .99);
    if (world.clearRow != 0)
        foom();
}
void Zakato::hitBy(Entity &other) {
    if (fading)
        return;
    other.didHit(*this);
    world.sound(58);
    if (armour < 1)
        foom();
    else {
        armour = add32(armour, -1);
        radius = float(double(radius) * 1.2);
    }
}
void Zakato::foom() {
    world.effects.explode({position, 2, .005f, .015f, 4, 20, .01f});
    fading = true;
    world.game.score(100);
    auto award = std::make_unique<Fhbon>();
    award->sprite = 67;
    award->initialize(*this, world.random);
    world.lists.prepend(List::L3bc, std::move(award));
    Vec2 v{};
    v.rotateBy(world.random.range(-3.141592654, 3.141592654));
    world.effects.outline({position, v, 0, .03f, 1.1f, 33, 4, .05f});
    dead = true;
}
void Barrel::initialize(Vec2 source) {
    position = source;
    sprite = 84;
    velocity = {.003, world.random.range(-.004, -.001)};
    radius = .05f;
    armour = (world.barrelChance >> 2) + 2;
    spin = float(world.random.range(.1, .3));
    baseRadius = radius;
    age = 0;
}
void Barrel::run() {
    angle += spin * float(float(velocity.x) < 0 ? -1 : 1);
    if (baseRadius < radius)
        radius = float(double(radius) * .95);
    sprite = int32_t((uint32_t(age) >> 4) & 1) | 84;
    age = add32(age, 1);
    velocity.y += .0001;
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    if (velocity.y > 0 && position.y > .418)
        velocity.y = -velocity.y;
    if ((velocity.x > 0 && position.x > .41) || (velocity.x < 0 && position.x < -.39))
        velocity.x = -velocity.x;
}
void Barrel::hitBy(Entity &other) {
    other.didHit(*this);
    world.sound(58);
    if (armour > 0) {
        armour = add32(armour, -1);
        radius = .08f;
        if (velocity.y > -.004)
            velocity.y -= .004;
        spin = float(double(spin) * 1.2);
        velocity.x *= 1.1;
        return;
    }
    world.effects.explode({position, 2, .005f, .025f, 7, 50, .01f});
    dead = true;
    world.game.score(150);
    const int32_t count = world.barrelChance * 2;
    if (count > 0) {
        const float twice = float(world.barrelChance) + float(world.barrelChance),
                    four = twice + twice;
        const float step = float(6.2831853072 / double(twice));
        float rotation = float(-3.141592654 / double(four));
        for (int32_t i = 0; i < count; ++i) {
            world.enemyBullet(*this, rotation, double(.006f));
            rotation += step;
        }
    }
    world.specialPickup(position);
}
} // namespace gr::reconstruction::ios
