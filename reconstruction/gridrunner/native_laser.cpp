#include "native_laser.h"
#include <cmath>
namespace gr::reconstruction::ios {
void LaserZap::initialize(const Tao &source) {
    type = 3;
    position = source.position;
    sprite = 9;
    age = 0;
    radius = .03f;
    scale.x = .3;
    angle = 1.5707964f;
    velocity.x = .02;
}
void LaserZap::run() {
    auto &ac = world.ac;
    if (ac.completed || ac.timeScale < 1) {
        dead = true;
        return;
    }
    age = add32(age, 1);
    position.x += velocity.x;
    if (uint32_t(age) & 2)
        sprite = int32_t(world.random.next() & 2) | 9;
    tint = world.tint;
    if (double(ac.laserX) < position.x) {
        ac.laserFrozen = false;
        dead = true;
        world.makePod(position, true);
        world.sound(35);
    }
}
void LaserBolt::initialize(const Tao &source) {
    position = source.position;
    age = 0;
    seed = world.random.next();
    radius = .05f;
}
void LaserBolt::run() {
    auto &ac = world.ac;
    if (!ac.completed && ac.timeScale >= 1) {
        age = add32(age, 1);
        ac.laserX = float(position.x);
        if ((uint32_t(age) & 7) == 0)
            seed = world.random.next();
        if (age >= ac.laserCharge) {
            if (age == ac.laserCharge)
                ac.leftTrigger = true;
            auto *ship = dynamic_cast<Ship *>(world.lists.first(List::L3b0));
            if (ship && std::abs(ship->position.x - position.x) < .04)
                ship->foom();
        }
        if (ac.laserFrozen)
            return;
    }
    dead = true;
}
void LaserBolt::prepareDraw() {
    // Original saves _rand(), seeds the visual pattern, then restores using
    // that saved value. The net global stream change is exactly one draw.
    world.random.next();
    DarwinRandom local(seed);
    for (auto &frame : frames)
        frame = age < world.ac.laserCharge ? 2 : int32_t((local.next() & 2) | 9) + 2;
}
Rgba LaserBolt::drawTint() const {
    // iOS 0x4c680 copies game+0x107c (gcol[6]) before its loop. Only the
    // charging branch replaces it with game+0xfdc (gcol[4]) and ramp alpha.
    if (age >= world.ac.laserCharge)
        return world.activeBeamTint;
    auto colour = world.tint;
    colour.a = double(float(age) / float(world.ac.laserCharge)) * .7;
    return colour;
}
LeftLaser::LeftLaser(LaserWorld &w) : world(w) {
    position = {-.45, -.45};
    velocity.y = .004;
    scale = {1.4, .9};
    sprite = 7;
    radius = .03f;
    initTrail();
}
BottomLaser::BottomLaser(LaserWorld &w) : world(w) {
    position = {-.39, -.4};
    velocity.x = .004;
    sprite = 7;
    radius = .03f;
    angle = 4.712389f;
    initTrail();
}
void LeftLaser::run() {
    auto &ac = world.ac;
    if (ac.completed) {
        dead = true;
        return;
    }
    Tao::run();
    tint = world.tint;
    if (ac.timeScale < 1)
        return;
    if (!ac.laserFrozen) {
        position.y += velocity.y;
        if (position.y > .448)
            position.y = -.362;
        if (position.y < -.362)
            position.y = .448;
    }
    if (!ac.leftTrigger || arrivalStep != 0)
        return;
    world.sound(30);
    ac.leftTrigger = false;
    auto zap = std::make_unique<LaserZap>(world);
    zap->initialize(*this);
    world.lists.prepend(List::L3bc, std::move(zap));
    Vec2 v{};
    v.rotateBy(world.random.range(-3.141592654, 3.141592654));
    world.effects.outline({{position.x + .05, position.y}, v, 0, .03f, 1.1f});
    world.feedback = .8f;
    const auto chance = world.random.next() & 63;
    if (int32_t(chance) < world.barrelChance && position.y < .2)
        world.barrel(position);
}
void BottomLaser::run() {
    auto &ac = world.ac;
    if (ac.completed) {
        dead = true;
        return;
    }
    Tao::run();
    tint = world.tint;
    if (ac.timeScale < 1)
        return;
    if (!ac.laserFrozen) {
        position.x += velocity.x;
        if (position.x > .42)
            position.x = -.39;
        if (position.x < -.39)
            position.x = .42;
    }
    if (!ac.bottomTrigger || arrivalStep != 0)
        return;
    auto bolt = std::make_unique<LaserBolt>(world);
    bolt->initialize(*this);
    world.lists.prepend(List::L3bc, std::move(bolt));
    world.sound(51);
    for (int32_t i = 0; i < world.zakatoCount; ++i) {
        Vec2 v{0, world.random.range(double(world.zakatoSpeed), double(world.zakatoSpeed) * 1.2)};
        v.rotateBy(world.random.range(-1.208304866923077, 1.208304866923077));
        world.zakato(position, v);
    }
}
} // namespace gr::reconstruction::ios
