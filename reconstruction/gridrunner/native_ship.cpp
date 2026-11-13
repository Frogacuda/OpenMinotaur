#include "native_ship.h"
#include <algorithm>
#include <cmath>
namespace gr::reconstruction::ios {
Ship::Ship() = default;
Ship::Ship(ShipWorld &w) : world_(&w) {
    w.bindShip(*this);
}
Ship::~Ship() {
    if (world_)
        world_->unbindShip(*this);
}
void ShipWorld::bindShip(Ship &s) {
    if (!bullets_)
        bullets_ =
            std::make_unique<BulletWorld>(BulletWorld{lists, collision, &s, ac.powers, {}, {}, {}});
    else
        bullets_->ship = &s;
}
void ShipWorld::unbindShip(Ship &s) {
    if (bullets_ && bullets_->ship == &s)
        bullets_->ship = nullptr;
}
BulletWorld &ShipWorld::bulletBindings() {
    if (!bullets_)
        throw std::logic_error("Original bullet game binding missing");
    return *bullets_;
}
ShipWorld &Ship::sourceWorld() {
    if (!world_)
        throw std::logic_error("Original Ship update requires its game binding");
    return *world_;
}
void Ship::fire(float rotation) {
    auto &world = sourceWorld();
    auto bullet = std::make_unique<MyBull>(world.bulletBindings());
    bullet->initialize(*this);
    bullet->angle = rotation;
    world.lists.prepend(List::L3ac, std::move(bullet));
}
void Ship::firePlaying() {
    auto &world = sourceWorld();
    auto &ac = world.ac;
    fireTimer = add32(fireTimer, -1);
    if (fireTimer >= 1)
        return;
    const float unrounded = float(firePeriod) - fireBoost;
    fireTimer = int32_t(unrounded);
    if (ac.powers[1])
        fireTimer /= 2;
    if (ac.powers[0] > 0) {
        const int32_t difference = add32(600, -ac.powers[0]);
        const int32_t candidate = add32(difference / 30, 3);
        // Source compares against the original FLOAT period, not the current
        // (possibly halved) timer. Do not replace this with std::min.
        if (float(candidate) < unrounded)
            fireTimer = candidate;
        if ((difference <= -30) && ac.powers[4])
            world.achievement(8);
    }
    if (ac.powers[1] && !ac.powers[4]) {
        fire(-.3926991f);
        fire(.3926991f);
    }
    float step = ac.powers[0] ? 3.1415927f : 0;
    int32_t count = ac.powers[0] ? 2 : 1;
    float rotation = 0;
    if (ac.powers[2]) {
        count = 4;
        step = 1.5707964f;
    }
    if (ac.powers[4]) {
        count = 32;
        step = .19634955f;
        rotation = (float(world.clock & 255) / 255.f) * 3.1415927f;
    }
    for (int32_t n = 0; n < count; ++n) {
        fire(rotation);
        rotation += step;
    }
    world.sound(34);
}
void Ship::moveAndTouch(Vec2 displacement) {
    auto &world = sourceWorld();
    double top = -.2280000001192093;
    if (world.demo) {
        Vec2 target{0, .3};
        if (auto *pickup = world.lists.first(List::L3c0)) {
            auto *entity = dynamic_cast<Entity *>(pickup);
            if (!entity)
                throw std::logic_error("Original pickup list requires Entity records");
            target = entity->position;
        } else {
            for (auto *node = world.lists.first(List::L3c4); node; node = node->next) {
                auto *entity = dynamic_cast<Entity *>(node);
                if (!entity)
                    throw std::logic_error("Original enemy list requires Entity records");
                if (entity->type != 2)
                    continue;
                const double dx = entity->position.x - position.x,
                             dy = entity->position.y - position.y;
                if (float(std::sqrt(dx * dx + dy * dy)) < 1000) {
                    target = entity->position;
                    if (target.y < .3)
                        target.y += .2;
                } // Original never reduces the 1000 threshold: last qualifying head wins.
            }
        }
        displacement = {target.x - position.x, target.y - position.y};
        const double length =
            std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y);
        if (length > 0) {
            displacement.x /= length;
            displacement.y /= length;
        } else
            displacement = {};
        displacement.x *= .003;
        displacement.y *= .003;
        top = -.05000000074505806;
    }
    position.x += double(float(displacement.x));
    position.x = std::clamp(position.x, -.4, .43);
    position.y += double(float(displacement.y));
    if (position.y < top)
        position.y = top;
    if (position.y > .428 && velocity.y >= 0)
        position.y = .428;
    world.collision.touch(*this, world.lists.first(List::L3c4));
    world.collision.touch(*this, world.lists.first(List::L3c0));
    world.collision.touch(*this, world.lists.first(List::L3c8));
}
void Ship::run() {
    auto &world = sourceWorld();
    auto &ac = world.ac;
    const ShipInput input = world.readInput();
    fireBoost = float(double(fireBoost) * .995);
    Vec2 displacement = input.value;
    if (!input.directDisplacement) {
        const double factor = world.alternateLayout ? .12 : .09;
        displacement.x *= factor;
        displacement.y *= factor;
    }
    switch (state) {
    case State::Arriving:
        Tao::run();
        if (arrival == 1) {
            state = State::Playing;
            ac.powers[5] = 150;
            if (ac.gridsWithoutDeath > 4)
                world.achievement(3);
        }
        fireTimer = add32(fireTimer, -1);
        if (fireTimer < 1) {
            fireTimer = int32_t(float(firePeriod) - fireBoost);
            fire(0);
            world.sound(34);
        }
        break;
    case State::Playing:
        age = add32(age, 1);
        if (ac.powers[5] == 0) {
            if (double(radius) > .02)
                radius = float(double(radius) * .99);
            if (double(radius) < .02)
                radius = .02f;
        } else if (double(radius) < .08)
            radius = float(double(radius) + .001);
        if (!ac.completed)
            ac.shipActive = true;
        Tao::run();
        if (ac.completed) {
            state = State::Departing;
            arrival = 1;
            world.feedback = .9f;
            // The source loop fills 511 slots, unlike init/rerez's 512.
            std::fill_n(trail.begin(), 511, position);
        }
        firePlaying();
        break;
    case State::Dying:
        age = add32(age, 1);
        tint = world.dyingTint;
        if (age > 30) {
            state = State::Departing;
            arrival = 1;
            world.effects.explode({position, 2, .02f, .06f, 5, 500, .01f});
            world.feedback = .9f;
        }
        return;
    case State::Departing:
        if (ac.completed) {
            velocity.y -= .0002;
            position.y += velocity.y;
            arrival += -.003f;
        } else
            arrival += -.01f;
        trail[trailFrame & 511] = position;
        ++trailFrame;
        if (arrival < 0) {
            if (ac.completed) {
                dead = true;
                world.stopSound(54);
            } else {
                ac.lives = add32(ac.lives, -1);
                world.livesChanged(ac.lives);
                if (ac.lives > 0 && !world.demo) {
                    ac.powers.fill(0);
                    rerez(world.alternateLayout);
                    ac.deathRepel = 0;
                    world.sound(50);
                    ac.dropChance = 500;
                    return;
                }
                if (ac.lives < 1 && !world.demo) {
                    ac.gameOver = true;
                    world.finishGame();
                }
                ac.phaseTimer = 3;
                ac.phase = 20;
                state = State::Hidden;
                if (ac.powers[6]) {
                    world.stopSound(54);
                    ac.powers[6] = 0;
                }
                ac.powers.fill(0);
            }
        }
        return;
    default:
        break; // Original state >=4 still reaches common movement/contact.
    }
    moveAndTouch(displacement);
}
void Ship::didHit(Entity &) {
    foom();
}
void Ship::foom() {
    auto &world = sourceWorld();
    auto &ac = world.ac;
    if (state == State::Playing && ac.powers[5] == 0 && double(radius) <= .03) {
        if (ac.powers[6]) {
            world.stopSound(54);
            ac.powers[6] = 0;
        }
        ac.gridsWithoutDeath = 0;
        ac.deathRepel = 500;
        world.sound(39);
        world.effects.explode({position, 2, .01f, .03f, 1, 50, .01f});
        state = State::Dying;
        world.feedback = .5f;
        age = 0;
        Vec2 v{};
        v.rotateBy(world.random.range(-3.141592654, 3.141592654));
        world.effects.outline({position, v, 0, .06f, 1.1f});
    } else {
        ac.deflectionCount = add32(ac.deflectionCount, 1);
        if (ac.deflectionCount > 500)
            world.achievement(2);
    }
}
} // namespace gr::reconstruction::ios
