#include "pickups.h"
#include "effects.h"
#include "projectiles.h"
#include "original_tables.h"
#include <algorithm>
namespace superoxwars {
ColourOrb::ColourOrb(World &w, const Entity &source) : GameEntity(w) {
    type = 0x5001;
    position = source.position;
    radius = source.radius;
    const auto choice = w.random.next() & 1;
    frame = int(choice) + 43;
    velocity.y = -.0005;
    colour = unpack(choice ? 0xff3f20ff : 0x206fffff);
    if ((choice == 0 && w.polarity <= 0) || (choice == 1 && w.polarity > 0))
        velocity.y *= .5;
    age = 0;
    mass = 10;
}
void ColourOrb::run() {
    clock += clockRate;
    const double size = double(clock) * .3 + 1.;
    clockRate = float(double(clockRate) - .0003);
    scale = {size, size};
    velocity.y += .00008;
    position.y += velocity.y;
    position.x += world.scrollDelta.x * size;
    const uint32_t tick = uint32_t(++age) & 63;
    const int distance = std::abs(int(tick) - 31) - 15;
    scale.y = size * double(float(distance) * .0625f);
    if (clock < -1 || position.y > .6)
        dead = true;
}
void ColourOrb::hitBy(Entity &) {
    dead = true;
    world.noise(10);
    const bool blue = frame == 43;
    if (blue)
        ++world.blueCollected;
    else
        ++world.redCollected;
    const float old = world.polarity;
    int count = 8;
    float modulation = .2f;
    const float speed = blue ? .008f : .00040000002f;
    if (blue && old < 0) {
        count = std::min(64, int(old * -100.f + 40.f));
        modulation = float(double(old) * -3.6 + .2);
    }
    if (!blue && old > 0) {
        count = std::min(64, int(old * 100.f + 40.f));
        modulation = float(double(old) * 1.6 + .2);
    }
    if (blue)
        world.polarity = std::max(-1.f, float(double(old) - .04));
    else
        world.polarity = float(double(old) + .04);
    world.polarityPulse = blue ? -.04f : .04f;
    double phase = blue ? 0 : 1.570796327, second = 0;
    const double step = 6.2831853072 / double(float(count));
    const double secondStep = (blue ? 31.415926536 : 9.424777962) / double(float(count));
    for (int n = 0; n < count; ++n) {
        auto *shot = new PlayerBullet(world);
        shot->position = shot->retained = position;
        shot->palette = blue ? 8 : 1;
        shot->type = 0x2000;
        shot->radius = .015f;
        shot->frame = blue ? 28 : 29;
        shot->scale.x = 1;
        const float angleOffset = float(phase);
        if (blue) {
            shot->velocity = {double(float(std::cos(angleOffset) * speed)),
                              double(float(std::sin(angleOffset) * -speed))};
            shot->velocity =
                shot->velocity *
                double(float(double(std::sin(float(second))) * .2 * double(modulation) + 1.));
        } else {
            const float y = float(float(std::cos(angleOffset) * 13.f) +
                                  float(std::cos(float(phase + phase)) * -5.f)) +
                            float(std::cos(float(phase * 3.)) * -2.f);
            shot->velocity = {double(speed) * std::pow(double(std::sin(angleOffset)), 3.) * 16.,
                              double(-speed) * (double(y) - std::cos(phase * 4.))};
        }
        shot->angle = heading(shot->velocity);
        world.add(Domain::PlayerShots, shot);
        phase += step;
        second += secondStep;
    }
}
BonusTile::BonusTile(World &w, int tile, int kind) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    armour = 0;
    gunState = -1;
    spin = turnRate = 0;
    layerFrame = 0;
    angle = 0;
    reward = 10;
    bias = kind == 3 ? -1 : kind == 4 ? 1 : 0;
    frame = kind == 3 ? 20 : kind == 4 ? 21 : 42;
    radius = kind == 8 ? .02f : .03f;
    baseColour = kind == 3 ? unpack(0x5080ffff) : kind == 4 ? unpack(0xff8050ff) : Colour{};
    colour = baseColour;
}
void BonusTile::hitBy(Entity &source) {
    GroundEnemy::hitBy(source);
    if (!dead)
        return;
    if (frame == 20)
        --world.blueRemaining;
    else if (frame == 21)
        --world.redRemaining;
    const float strength = std::abs(world.polarity);
    float chance = strength;
    int points = reward;
    if (frame == 20) {
        world.polarity = std::max(-1.f, world.polarity - .04f);
        world.polarityPulse = -.04f;
        if (source.frame == 28)
            points *= world.chain + 1;
        if (world.scoreNotice)
            world.scoreNotice(points, position, -1, {0, .003f}, .02f);
    } else if (frame == 21) {
        world.polarity = std::min(1.f, world.polarity + .04f);
        world.polarityPulse = .04f;
        if (source.frame == 29)
            points *= world.chain + 1;
        if (world.scoreNotice)
            world.scoreNotice(points, position, 1, {0, .003f}, .02f);
    } else if (frame == 42) {
        world.add(Domain::Ground, new ColourOrb(world, *this));
        chance = -1;
    }
    if (world.level == 9 && frame != 42)
        chance = 100;
    // The extra pickup decision always consumes its source draw, even when the
    // neutral L tile has made the threshold negative.
    if (world.random.range(0, 2) < double(chance))
        world.add(Domain::Ground, new WeaponPickup(world, cell, frame + 8));
}
WeaponPickup::WeaponPickup(World &w, int tile, int sprite) : GroundEnemy(w, tile, 0) {
    frame = sprite;
    radius = .02f;
    gunState = -1;
    armour = 0;
    layerFrame = 0;
    spin = turnRate = 0;
    angle = 0;
    colour = baseColour = {};
    archetype = -1;
}
void WeaponPickup::hitBy(Entity &) {
    if (frame == 28)
        ++world.blueCollected;
    else if (frame == 29)
        ++world.redCollected;
    dead = true;
    world.noise(10);
    const bool accepted = world.polarity != 0 && ((frame == 28 && world.polarity < 0) ||
                                                  (frame == 29 && world.polarity >= 0));
    if (accepted && world.player) {
        world.add(Domain::Decor, new Follower(world, frame));
    } else {
        world.polarity = float(double(world.polarity) * -.1);
        world.add(Domain::Decor, new Explosion(world, position, .08f, .04f, 1.04f));
    }
}
Follower::Follower(World &w, int sprite) : GameEntity(w) {
    frame = sprite;
    type = 0x5000;
    radius = .02f;
    if (w.player) {
        slot = w.player->power++;
        if (w.player->power == 5)
            w.achieveGoal(4);
    }
}
void Follower::run() {
    ++age;
    auto *ship = world.player;
    if (!ship || ship->state != Ship::State::Flying) {
        dead = true;
        return;
    }
    position = ship->history[size_t((ship->trailCursor + 496 - slot * 16) & 511)];
    lifetime -= decay;
    if (lifetime < .1f)
        colour.a = (uint32_t(age) & 2) ? 0 : 1;
    if (lifetime < 0) {
        dead = true;
        --ship->power;
        for (auto *e = world.lists[size_t(Domain::Decor)]; e; e = e->next)
            if (auto *nextAngle = dynamic_cast<Follower *>(e);
                nextAngle && !nextAngle->dead && nextAngle->slot > slot)
                --nextAngle->slot;
    }
    world.holdPolarity = true;
    if ((world.polarity < 0 && frame == 29) || (world.polarity > 0 && frame == 28)) {
        lifetime = -1;
        world.add(Domain::Decor, new Explosion(world, position, .08f, .04f, 1.02f));
    }
    if (!ship->shotThisTick || slot >= 5)
        return;
    const int tint = world.polarity < 0 ? 8 : world.polarity > 0 ? 1 : 0;
    const auto bullet = [&](Vec2 origin) {
        auto *b = new PlayerBullet(world);
        b->position = b->retained = origin;
        b->type = 0x2000;
        b->frame = tint == 8 ? 28 : tint == 1 ? 29 : 15;
        b->palette = tint;
        b->radius = .015f;
        b->scale.x = tint ? 1 : .7;
        if (frame == 29)
            b->health = tint == 1 ? ship->power + 1 : tint == 8 ? 1 : 0;
        b->velocity = {0, -.008};
        return b;
    };
    if (frame == 28) {
        for (int sign : {1, -1}) {
            auto *b = bullet(position);
            b->angle = 3.1415927f;
            b->velocity.rotateBy(3.1415927410125732);
            b->arcStep = float(double(slot) * .07 + .1) * float(sign);
            b->arcRemaining = float(3.141592654 - double(slot + 1) * .1);
            world.add(Domain::PlayerShots, b);
        }
    } else if (frame == 29) {
        for (int sign : {1, -1}) {
            auto *b = bullet(ship->position);
            b->arcStep = float(double(slot) * .04 + .004) * float(sign);
            b->arcRemaining = 3.6651914f;
            world.add(Domain::PlayerShots, b);
        }
    }
}
LifePickup::LifePickup(World &w, int tile, int sprite) : GroundEnemy(w, tile, 0) {
    frame = sprite;
    radius = .04f;
    armour = 0;
    gunState = -1;
    layerFrame = 0;
    spin = turnRate = 0;
    angle = 0;
    archetype = -1;
    colour = baseColour = {};
}
void LifePickup::run() {
    GroundEnemy::run();
    if (collected) {
        fade = float(double(fade) + .02);
        scale = scale * 1.06;
        colour.a = double(1.f - fade);
        if (double(fade) > .98)
            dead = true;
    } else if (departed)
        dead = true;
}
void LifePickup::hitBy(Entity &) {
    if (collected)
        return;
    collected = true;
    world.noise(10);
    world.noise(6);
    world.weaponPower = std::min(1.f, world.weaponPower + .9f);
    world.add(Domain::Decor, new LifeToken(world, frame));
}
LifeToken::LifeToken(World &w, int sprite) : GameEntity(w) {
    type = 0x6000;
    frame = sprite;
    side = frame == 46 ? 1 : 0;
    radius = .03f;
    scale.y = .9;
    position.y = -.45;
    ++w.lifeTokens[side];
    if (w.lifeTokens[1 - side] > 0)
        --w.lifeTokens[1 - side];
    slot = w.lifeTokens[side];
    position.x = side ? double(float(slot)) * -.05 + .4 : double(float(slot)) * .05 + .1;
    if (slot > 0) {
        w.lifeTokens[side] = 0;
        ++w.lives;
        if (w.lives > 6)
            w.achieveGoal(5);
        state = 1;
        w.message(8, side ? 1u : 8u, 100, "EXTRA LIFE!");
    }
    retained = position;
}
void LifeToken::run() {
    if (state == 2) {
        progress += .01f;
        colour.a = double(1.f - progress);
        position.x = double(float(float(retained.x) + (targetX - float(retained.x)) * progress));
        position.y = retained.y + double(progress) * .3;
        if (progress >= 1)
            dead = true;
    } else if (state == 1) {
        if (slot > world.lifeTokens[side]) {
            dead = true;
            world.add(Domain::Decor, new Explosion(world, position, .06f, .1f, 1.06f));
        }
    } else {
        progress -= .04f;
        if (progress < 0) {
            progress = 0;
            state = 1;
            if (slot >= 5)
                for (auto *e = world.lists[size_t(Domain::Decor)]; e; e = e->next)
                    if (auto *token = dynamic_cast<LifeToken *>(e);
                        token && token->frame == frame) {
                        token->state = 2;
                        token->progress = 0;
                        token->targetX = float(token->slot - 1) * .06f - .12f;
                    }
        }
        const double size = double(float(progress + progress)) + 1.;
        colour.a = double(1.f - progress);
        scale = {size, size * .9};
    }
}
OxBlock::OxBlock(World &w, int tile) : GroundEnemy(w, tile, 0) {
    frame = 40;
    radius = .03f;
    armour = 3;
    gunState = -1;
    spin = turnRate = 0;
    angle = 0;
    reward = 0;
    layerFrame = 0;
    layerScale = .2f;
    layerAlpha = .2f;
    layerOffset = {.02, .01};
    archetype = -1;
    colour = baseColour = unpack(original::AreaColours[size_t(w.area % 10) * 16]);
}
void OxBlock::prepare(std::vector<ActorDraw> &out) {
    position = world.tiles.positions[size_t(cell)];
    active = true;
    const Vec2 base = position, oldScale = scale;
    const Colour oldColour = colour;
    Vec2 spacing = position * layerOffset;
    spacing.x += world.layerTilt * .012;
    if (armour > 0) {
        scale = oldScale * double(layerScale);
        colour = oldColour;
        colour.a = double(layerAlpha);
        frame = 0;
        for (int n = 0; n < armour; ++n) {
            Entity::prepare(out);
            position += spacing;
        }
    }
    colour = world.palette[2];
    colour.a = 1;
    frame = 40;
    muzzle = position;
    scale = oldScale * double(pulse);
    Entity::prepare(out);
    position += spacing * 2.;
    scale = scale * .7;
    frame = 41;
    colour = world.palette[world.polarity < 0 ? 8 : world.polarity > 0 ? 1 : 0];
    Entity::prepare(out);
    position = base;
    scale = oldScale;
    frame = 40;
    colour = oldColour;
}
void OxBlock::hitBy(Entity &other) {
    GroundEnemy::hitBy(other);
    if (!dead)
        return;
    world.noise(int((world.random.next() & 255) % 5));
    ++world.oxStreak;
    world.oxTimer = 60;
    const float old = world.polarity;
    if (old == 0)
        return;
    const int points = world.oxStreak * ((int(std::abs(double(old)) * 8.) + 1)
                                         << int(other.frame == (old > 0 ? 29 : 28)));
    world.polarity = std::clamp(float(double(old) + (old > 0 ? .01 : -.01)), -1.f, 1.f);
    world.polarityPulse = old > 0 ? .01f : -.01f;
    world.award(points, old > 0 ? 1 : -1, position);
    if (world.scoreNotice)
        world.scoreNotice(points, position, old > 0 ? 1 : -1,
                          {double(float(position.x * .03)), double(float(position.y * -.02))},
                          .02f);
}
} // namespace superoxwars
