#include "native_enemies.h"
#include "native_pickup.h"
#include <algorithm>
#include <cmath>
namespace gr::reconstruction::ios {
namespace {
Vec2 unit(Vec2 v) {
    const double length = std::sqrt(v.x * v.x + v.y * v.y);
    if (length > 0)
        return {v.x / length, v.y / length};
    return {};
}
int32_t sign(float value) {
    return value < 0 ? -1 : 1;
} // source sgn(0), including -0, is +1
} // namespace
void EnemyWorld::specialPickup(Vec2 position) {
    if (game.survival && ac.level >= 6)
        return;
    const auto first = random.next() & 3;
    int32_t kind = int32_t(first + (random.next() & 3) + 1);
    if (kind == 6) {
        if (ac.warpUsed)
            kind = 7;
        else if (ac.timeScale < 1)
            kind = 4;
    }
    sound((kind & 7) | 40);
    game.pickup(position, kind);
}
void EnemyWorld::spawnSwarmers(const Entity &source, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        auto enemy = std::make_unique<Swarmer>(*this);
        enemy->initialize(source);
        lists.prepend(List::L3c4, std::move(enemy));
    }
}
void EnemyWorld::enemyBullet(const Entity &from, float angle, double speed) {
    auto b = std::make_unique<EnemyBullet>(*this);
    b->initialize(from, angle, speed);
    lists.prepend(List::L3c8, std::move(b));
}
void EnemyWorld::fireRing(const Entity &from, int32_t count) {
    if (count <= 0)
        return;
    const float step = 6.2831855f / float(count);
    float angle = step * -.5f;
    for (int32_t i = 0; i < count; ++i) {
        enemyBullet(from, angle, double(.01f));
        angle += step;
    }
}
void EnemyBullet::initialize(const Entity &source, float rotation, double speed) {
    position = source.position;
    sprite = 77;
    radius = .015f;
    scale.y = -1.8;
    tint.a = 0;
    angle = rotation;
    velocity = {0, speed};
    velocity.rotateBy(double(rotation));
}
void EnemyBullet::run() {
    if (tint.a < 1) {
        tint.a += .1;
        if (tint.a >= 1)
            tint.a = 1;
    }
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    if (position.x < -.39 || position.x > .41 || position.y > .418 || position.y < -.4)
        dead = true;
}
void EnemyBullet::hitBy(Entity &other) {
    other.didHit(*this);
    world.sound(58);
    world.game.score(5);
    world.effects.explode({position, 2, .005f, .025f, 1, 5, .01f});
    dead = true;
    const float rotation = float(world.random.range(-3.141592654, 3.141592654));
    world.effects.outline({position, {}, rotation, .02f, 1.1f, 33, 2, .05f});
}
void Shrapnel::run() {
    angle += spin;
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    if ((position.x < -.5 || position.x > .5) && (position.y < -.5 || position.y > .5))
        dead = true;
    if (world.clearRow != 0) {
        world.game.score(5);
        world.sound(53, 0x4000);
        world.effects.explode({position, 2, .03f, .031f, 6, 10, .01f});
        const float rotation = float(world.random.range(-3.141592654, 3.141592654));
        world.effects.outline({position, {}, rotation, .03f, 1.03f, 33, 1, .05f});
        dead = true;
    }
}
void ShrapnelBomb::initialize(int32_t count, int32_t cycle) {
    position = {world.random.range(-.3, .3), -.428};
    tint.a = 0;
    velocity = {0, .002};
    sprite = 40;
    armour = 0;
    world.random.next();
    radius = .03f;
    age = 0;
    detonationY = float(world.random.range(-.1, .3));
    fanCount = add32(count, cycle * 3);
    smart = 0;
    const int32_t level = world.ac.level;
    const int32_t chance = level < 5 ? 0 : int32_t((uint32_t(level) + 124) >> 2) & 0x13;
    if (int32_t(world.random.next() & 127) < chance) {
        sprite = 86;
        smart = 1;
        scale.x = 1.6;
        fireTimer = firePeriod = 19 - ((level + 12) & 15);
        armour = (((level + 12) & 15) >> 2) + 2;
    }
}
void ShrapnelBomb::foom() {
    Vec2 v{};
    v.rotateBy(world.random.range(-3.141592654, 3.141592654));
    world.effects.outline({position, v, 0, .06f, 1.1f, 33, 2, .05f});
    dead = true;
    if (fanCount <= 0)
        return;
    Vec2 fragmentVelocity{0, .006};
    const double step = double(1.5707964f / float(fanCount));
    for (int32_t i = 0; i < fanCount; ++i) {
        fragmentVelocity.rotateBy(step);
        for (int mirrored = 0; mirrored < 2; ++mirrored) {
            auto piece = std::make_unique<Shrapnel>(world);
            piece->type = 9;
            piece->position = position;
            piece->velocity = fragmentVelocity;
            piece->radius = .014f;
            piece->spin = float(world.random.range(-.8, .8));
            piece->sprite = 44;
            world.lists.prepend(List::L3c4, std::move(piece));
            fragmentVelocity.x = -fragmentVelocity.x;
        }
    }
}
void ShrapnelBomb::run() {
    if (tint.a < 1)
        tint.a += .05;
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    age = add32(age, 1);
    if (double(radius) > .03)
        radius = float(double(radius) * .95);
    sprite = int32_t((uint32_t(age) >> 1) & 3) | 40;
    world.ac.liveThreats = add32(world.ac.liveThreats, 1);
    if (double(detonationY) < position.y)
        foom();
    if (smart != 1)
        return;
    sprite = int32_t((uint32_t(age) >> 3) & 1) | 86;
    const float dx = world.game.ship ? float(world.game.ship->position.x - position.x) : 0;
    velocity.x += dx > 0 ? (velocity.x <= 0 ? .0002 : .0001) : (velocity.x >= 0 ? -.0002 : -.0001);
    fireTimer = add32(fireTimer, -1);
    if (fireTimer < 1) {
        fireTimer = firePeriod;
        world.enemyBullet(*this, float(world.random.range(-1, 1)), double(.009f));
    }
}
void ShrapnelBomb::hitBy(Entity &other) {
    other.didHit(*this);
    if (dead || &other == this || (other.type != 5 && other.type != 8))
        return;
    world.game.score(50);
    world.sound(53, 0x4000);
    if (armour > 0) {
        armour = add32(armour, -1);
        radius = .05f;
        return;
    }
    foom();
    if (sprite > 85)
        world.game.achievement(6); // iOS 0x51bb8, not timed detonation
    world.effects.explode({position, 2, .02f, .021f, 1, 30, .01f});
    world.game.bonus({position, 45, .02f, 4});
    world.specialPickup(position);
}
void Cube::initialize(int32_t count, int32_t cycle, int32_t fragments) {
    // Both initial random positions are overwritten by the radial launch.
    // Preserve their draws; omitting them shifts every later decision.
    world.random.range(-.3, .3);
    world.random.range(-.3, .3);
    velocity = {0, -.003};
    velocity.rotateBy(world.random.range(-.7853981634, .7853981634));
    const double distance = double(float((world.random.next() & 31) + 250));
    position = {velocity.x * distance, velocity.y * distance};
    velocity.x = -velocity.x;
    velocity.y = -velocity.y;
    sprite = 36;
    world.random.next();
    radius = .03f;
    entered = false;
    age = 0;
    armour = add32(count, cycle);
    fragmentCount = add32(fragments, cycle * 2);
}
void Cube::run() {
    Vec2 target{};
    if (world.game.ship)
        target = world.game.ship->position;
    else
        dead = true;
    auto &ac = world.ac;
    ac.liveThreats = add32(ac.liveThreats, 1);
    position.x += velocity.x * double(ac.timeScale);
    position.y += velocity.y * double(ac.timeScale);
    age = add32(age, 1);
    if (double(radius) > .03)
        radius = float(double(radius) * .95);
    sprite = int32_t((uint32_t(age) >> 2) & 3) | 36;
    if (!entered) {
        if (position.x < .5 && position.x > -.5 && position.y > -.5 && position.y < .5)
            entered = true;
        if (age < 1001)
            return;
    } else {
        ac.liveThreats = add32(ac.liveThreats, 1);
        acceleration = .0001f;
        const auto direction = unit({target.x - position.x, target.y - position.y});
        velocity.x += direction.x * double(acceleration);
        velocity.y += direction.y * double(acceleration);
        velocity.x *= .99;
        velocity.y *= .99;
        if (position.x >= -.5 && position.x <= .5)
            return;
        if (position.y >= -.5 && position.y <= .5)
            return;
    }
    dead = true;
}
void Cube::hitBy(Entity &other) {
    other.didHit(*this);
    if (other.type != 5 && other.type != 8)
        return;
    if (armour > 0) {
        armour = add32(armour, -1);
        radius = .08f;
        if (velocity.y > 0)
            velocity.y *= -1.5;
        // Half-step initial rotation, followed by the FULL step per pair.
        // ShrapnelBomb starts at zero instead; the two patterns are distinct.
        const float spacing = 3.1415927f / float(fragmentCount);
        Vec2 v{0, .006};
        v.rotateBy(double(spacing * -.5f));
        for (int32_t i = 0; i < fragmentCount; ++i) {
            v.rotateBy(double(spacing));
            for (int mirror = 0; mirror < 2; ++mirror) {
                auto piece = std::make_unique<Shrapnel>(world);
                piece->type = 9;
                piece->position = position;
                piece->velocity = v;
                piece->radius = .014f;
                piece->spin = float(world.random.range(-.8, .8));
                piece->sprite = 44;
                world.lists.prepend(List::L3c4, std::move(piece));
                v.x = -v.x;
            }
        }
        world.sound(32);
        return;
    }
    if (dead || &other == this)
        return;
    world.game.score(50);
    world.sound(53, 0x4000);
    world.effects.explode({position, 2, .02f, .021f, 3, 30, .01f});
    world.game.bonus({position, 45, .02f, 4});
    world.specialPickup(position);
    Vec2 v{};
    v.rotateBy(world.random.range(-3.141592654, 3.141592654));
    world.effects.outline({position, v, 0, .06f, 1.1f, 33, 2, .05f});
    dead = true;
}
void AnalSurprise::initialize(int32_t count, int32_t cycle) {
    position = {world.random.range(-.2, .2), -.422};
    tint.a = 0;
    velocity.y = .0005;
    sprite = 56;
    radius = .035f;
    armour = add32(count, cycle);
    if (world.ac.level < 23)
        fireTimer = -1;
    else
        fireTimer = firePeriod = int32_t((((uint32_t(world.ac.level) & 7) << 1) ^ 14) + 5);
}
void AnalSurprise::run() {
    if (tint.a < 1)
        tint.a += .05;
    if (double(radius) > .035)
        radius = float(double(radius) * .95);
    const Vec2 target = world.game.ship ? world.game.ship->position : Vec2{};
    const double dy = target.y - position.y;
    const float nextY = float(velocity.y + (dy <= 0 ? -.0001 : .00005));
    if (double(std::abs(nextY)) < .01)
        velocity.y = double(nextY);
    if (sign(float(dy)) != sign(float(velocity.y)))
        velocity.y *= .95;
    float nextX;
    if (target.y <= position.y) {
        const int32_t direction = sign(float(target.x - position.x));
        nextX = float(velocity.x + double(direction) * .0004);
        if (direction != sign(nextX))
            velocity.x *= .98;
    } else {
        velocity.x *= .92;
        nextX = float(velocity.x + double(float(target.x - position.x)) * -.0005);
    }
    if (double(std::abs(nextX)) < .01)
        velocity.x = double(nextX);
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    if (position.x < -.39 && velocity.x < 0)
        velocity.x = -velocity.x;
    if (position.x > .41 && velocity.x > 0)
        velocity.x = -velocity.x;
    const int32_t frame = int32_t(velocity.y * 900.);
    sprite = 59 - std::clamp(frame, -3, 3);
    if (fireTimer >= 0) {
        fireTimer = add32(fireTimer, -1);
        if (fireTimer < 1) {
            fireTimer = firePeriod;
            world.enemyBullet(*this, -1.5707964f, double(.009f));
            world.enemyBullet(*this, 1.5707964f, double(.009f));
        }
    }
}
void AnalSurprise::hitBy(Entity &other) {
    other.didHit(*this);
    if (other.type != 5 && other.type != 8)
        return;
    world.sound(58);
    if (armour > 0) {
        armour = add32(armour, -1);
        radius = .08f;
        velocity.y -= .005;
        return;
    }
    if (dead || &other == this)
        return;
    world.specialPickup(position);
    world.game.score(50);
    world.effects.explode({position, 2, .01f, .025f, 7, 20, .01f});
    dead = true;
}
void Swarmer::initialize(const Entity &source) {
    const Vec2 target = world.game.ship ? world.game.ship->position : Vec2{};
    position = source.position;
    const auto away = unit({position.x - target.x, position.y - target.y});
    const double speed = world.random.range(.005, .003);
    velocity = {away.x * speed, away.y * speed};
    velocity.rotateBy(world.random.range(-.8, .8));
    velocity.x *= 3.;
    tint.a = 0;
    sprite = 54;
    radius = .02f;
    acceleration = float(world.random.range(.0001, .00005));
    const double drag = world.random.range(.98, .995);
    damping = {drag, drag};
}
void Swarmer::run() {
    const Vec2 target = world.game.ship ? world.game.ship->position : Vec2{};
    const auto direction = unit({target.x - position.x, target.y - position.y});
    if (tint.a < 1)
        tint.a += .1;
    if (tint.a > 1)
        tint.a = 1;
    age = add32(age, 1);
    sprite = int32_t((uint32_t(age) >> 2) & 1) | 54;
    velocity.x += direction.x * double(acceleration);
    velocity.y += direction.y * double(acceleration);
    position.x += velocity.x * double(world.ac.timeScale);
    position.y += velocity.y * double(world.ac.timeScale);
    velocity.x *= damping.x;
    velocity.y *= damping.y;
    if (position.x < -.39 && velocity.x < 0)
        velocity.x = -velocity.x;
    if (position.x > .41 && velocity.x > 0)
        velocity.x = -velocity.x;
}
void Swarmer::hitBy(Entity &other) {
    if (!(tint.a >= 1))
        return;
    other.didHit(*this);
    if (dead || &other == this || (other.type != 5 && other.type != 8))
        return;
    if (other.type == 5) {
        world.ac.swarmerKills = add32(world.ac.swarmerKills, 1);
        if (world.ac.swarmerKills > 49)
            world.game.achievement(9);
    }
    world.game.score(500);
    world.sound(57);
    world.effects.explode({position, 2, .01f, .025f, 7, 20, .01f});
    dead = true;
    auto award = std::make_unique<Fhbon>();
    award->sprite = 63;
    award->initialize(*this, world.random);
    world.lists.prepend(List::L3bc, std::move(award));
}
} // namespace gr::reconstruction::ios
