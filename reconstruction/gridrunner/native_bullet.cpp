#include "native_bullet.h"
namespace gr::reconstruction::ios {
void MyBull::initialize(const Entity &source) {
    // Init exported with names on Android (0x8b498); the same assignments
    // also occur inline at the iOS Ship firing sites (0x4a530).
    radius = .02f;
    sprite = 6;
    type = 5;
    scale.x = .4;
    tint = {1, 1, 0, 1};
    position = source.position;
    velocity = {0, -.015};
    position.y -= double(radius);
}
void MyBull::run() {
    world.collision.touch(*this, world.lists.first(List::L3c4));
    world.collision.touch(*this, world.lists.first(List::L3c8));
    if (world.powers[0])
        tint = world.power0Tint;
    if (world.powers[2])
        tint = world.power2Tint;
    if (world.powers[1])
        tint = world.power1Tint;
    Vec2 step = velocity;
    step.rotateBy(double(angle));
    position.x += step.x;
    position.y += step.y;
    if (position.y < -.378 || position.y > .448 || position.x < -.4 || position.x > .43)
        dead = true;
}
void MyBull::didHit(Entity &other) {
    // iOS 0x4bc00 recovered directly; matches Android 0x85d60.
    if (world.powers[1] == 0 && other.type != 9 && !dead) {
        dead = true;
        if (angle == 0) {
            if (!world.ship)
                throw std::logic_error(
                    "Original primary bullet contact requires a live game Ship binding");
            if (world.ship->fireTimer > 3)
                world.ship->fireTimer = 3;
        }
    }
}
} // namespace gr::reconstruction::ios
