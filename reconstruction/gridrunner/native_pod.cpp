#include "native_pod.h"
#include <cmath>
namespace gr::reconstruction::ios {
void PodWorld::boostShip() {
    if (!ship)
        throw std::logic_error("Original pod/bomb bullet hit requires Ship record");
    if (ship->fireBoost < 10)
        ship->fireBoost += 2.f;
}
void PodWorld::award100(const Entity &source) {
    auto award = std::make_unique<Fhbon>();
    award->sprite = 68;
    score(100);
    award->initialize(source, random);
    lists.prepend(List::L3bc, std::move(award));
}
void Pod::initialize(Vec2 p, int32_t index, int32_t gridRow, float halfCellHeight) {
    cell = index;
    row = gridRow;
    position = p;
    type = 1;
    sprite = 16;
    radius = float(double(halfCellHeight) * .8);
    scale = {1.6, 1.6};
    baseScale = float(scale.x);
    tint = {1, 1, 0, 1};
}
void Pod::run() {
    if (double(baseScale) < scale.x)
        scale.x *= .95;
    scale.y = scale.x;
    int32_t &state = world.cells.at(size_t(cell));
    sprite = state;
    tint = state < 14 ? world.dimTint : Rgba{1, 1, 0, .7};
    if (world.decayPulse) {
        state = add32(state, -1);
        if (state < 13) {
            state = 0;
            dead = true;
            auto bomb = std::make_unique<Bomb>(world);
            bomb->initialize(*this);
            world.lists.prepend(List::L3c4, std::move(bomb));
        }
    }
    if ((world.clearRow != 0 || ac.completed) && (world.clearRow == row || ac.completed)) {
        state = 0;
        world.effects.explode({position, 2, .01f, .013f, 1, 20, .01f});
        world.score(1);
        dead = true;
        world.sound(32);
        Vec2 v{};
        v.rotateBy(world.random.range(-3.141592654, 3.141592654));
        world.effects.outline({position, v, 0, .03f, 1.04f, 33, 9, .05f});
        world.feedback = .8f;
    }
}
void Pod::hitBy(Entity &other) {
    if (other.type == 1)
        return;
    other.didHit(*this);
    if (other.type == 5) {
        world.boostShip();
        int32_t &state = world.cells.at(size_t(cell));
        state = add32(state, 2);
        if (state < 19) {
            world.sound(32);
            scale.x = 2.2;
        } else {
            state = 0;
            world.effects.explode({position, 2, .01f, .013f, 1, 20, .01f});
            world.score(1);
            dead = true;
            world.sound(33);
        }
    } else if (other.type == 8) {
        world.effects.explode({position, 2, .01f, .014f, 1, 90, .01f});
        dead = true;
    }
}
void Bomb::initialize(const Entity &source) {
    position = source.position;
    type = 4;
    radius = .03f;
    sprite = 19;
    scale.x = .6;
}
void Bomb::run() {
    velocity.y += .0005;
    position.y += velocity.y * double(ac.timeScale);
    if (position.y > .45)
        dead = true;
    if (world.clearRow != 0) {
        world.effects.explode({position, 2, .02f, .024f, 6, 30, .01f});
        world.award100(*this);
        dead = true;
    }
}
void Bomb::hitBy(Entity &other) {
    other.didHit(*this);
    if (other.type == 5) {
        if (double(std::abs(other.angle)) >= .8)
            return;
        world.boostShip();
        if (ac.powers[1] == 0 && double(radius) <= .04) {
            if (velocity.y > 0) {
                velocity.y *= -.5;
                world.sound(49);
                radius = float(double(radius) * 1.06);
            }
            return;
        }
        world.effects.explode({position, 2, .02f, .024f, 6, 30, .01f});
        // Original sound is requested between score and fhbon initialization.
        auto award = std::make_unique<Fhbon>();
        award->sprite = 68;
        world.score(100);
        world.sound(58);
        award->initialize(*this, world.random);
        world.lists.prepend(List::L3bc, std::move(award));
        dead = true;
    }
    if (other.type == 8) {
        world.effects.explode({position, 2, .01f, .014f, 0xffffffff, 90, .01f});
        dead = true;
    }
}
void Pod2::initialize(Vec2 p, int32_t index, int32_t gridRow, float halfCellHeight) {
    position = p;
    cell = index;
    row = gridRow;
    type = 1;
    radius = float(double(halfCellHeight) * .8);
    baseRadius = radius;
    scale = {1.6, 1.6};
    sprite = 16;
    tint = {1, 1, 0, 1};
}
void Pod2::run() {
    age = add32(age, 1);
    if (baseRadius < radius)
        radius = float(double(radius) * .95);
    constexpr int32_t art[] = {73, 74, 75, 76, 76, 75, 74, 73};
    sprite = art[(uint32_t(age) & 15) >> 1];
    if (world.decayPulse) {
        world.cells.at(size_t(cell)) = 0;
        dead = true;
        world.fireRing(*this, ac.level < 9 ? 4 : ac.level - 4);
    } else if ((world.clearRow != 0 || ac.completed) && (world.clearRow == row || ac.completed)) {
        world.cells.at(size_t(cell)) = 0;
        world.effects.explode({position, 2, .01f, .013f, 1, 20, .01f});
        world.score(1);
        dead = true;
        world.sound(32);
        Vec2 v{};
        v.rotateBy(world.random.range(-3.141592654, 3.141592654));
        world.effects.outline({position, v, 0, .03f, 1.04f, 33, 9, .05f});
        world.feedback = .8f;
        world.fireRing(*this, 4);
    }
}
void Pod2::hitBy(Entity &other) {
    if (other.type == 1)
        return;
    other.didHit(*this);
    if (other.type == 5) {
        auto &state = world.cells.at(size_t(cell));
        state = add32(state, 1);
        if (state < 19) {
            world.sound(32);
            radius = .06f;
            world.fireRing(*this, 4);
        } else {
            state = 0;
            world.effects.explode({position, 2, .01f, .013f, 3, 20, .01f});
            world.score(10);
            dead = true;
            world.sound(33);
            world.fireRing(*this, ac.level < 9 ? 4 : ac.level - 4);
        }
    } else if (other.type == 8) {
        world.effects.explode({position, 2, .01f, .014f, 3, 90, .01f});
        dead = true;
    }
}
void Spod::initialize(Vec2 p, int32_t gridRow, int32_t children) {
    position = p;
    row = gridRow;
    offspring = children;
    sprite = 46;
    age = int32_t(world.random.next() & 7);
    type = 1;
    scale = {1.2, 1.2};
    radius = .02f;
    acceleration = .0001f;
    damping = {.95, .95};
}
void Spod::open() {
    world.sound(56);
    world.spawnSwarmers(*this, offspring);
}
void Spod::run() {
    age = add32(age, 1);
    sprite = int32_t((uint32_t(age) & 31) >> 2) + 46;
    const Vec2 target = world.ship ? world.ship->position : Vec2{};
    Vec2 direction{target.x - position.x, target.y - position.y};
    const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0) {
        direction.x /= length;
        direction.y /= length;
    } else
        direction = {};
    velocity.x += direction.x * double(acceleration);
    velocity.y += direction.y * double(acceleration);
    position.x += velocity.x * double(ac.timeScale);
    position.y += velocity.y * double(ac.timeScale);
    velocity.x *= damping.x;
    velocity.y *= damping.y;
    if (ac.completed) {
        world.effects.explode({position, 2, .01f, .013f, 8, 20, .01f});
        world.score(100);
        dead = true;
        open();
        Vec2 v{};
        v.rotateBy(world.random.range(-3.141592654, 3.141592654));
        world.effects.outline({position, v, 0, .03f, 1.04f, 33, 7, .05f});
        world.feedback = .8f;
    }
}
void Spod::hitBy(Entity &other) {
    other.didHit(*this);
    if (other.type == 5) {
        world.effects.explode({position, 2, .01f, .013f, 1, 20, .01f});
        world.score(100);
        dead = true;
        open();
    } else if (other.type == 8) {
        world.effects.explode({position, 2, .01f, .014f, 1, 90, .01f});
        open();
        dead = true;
    }
}
} // namespace gr::reconstruction::ios
