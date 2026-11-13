#include "world.h"
#include "effects.h"
#include "ground_features.h"
#include <algorithm>
namespace superoxwars {
Ship::Ship(World &w) : GameEntity(w) {
    init();
}
void Ship::init(bool respawning) {
    mass = 10;
    frame = 10;
    position.y = .3;
    radius = .025f;
    scale = {2.4, 2};
    shotSpeed = .009f;
    fireRate = .05f;
    firePhase = 1;
    retained = position;
    trailDistance = 0;
    trailCursor = power = shotCycle = 0;
    grace = 500;
    if (!respawning)
        world.camera = {};
    world.weaponThreat = 0;
    world.weaponFollowers = 0;
    state = State::Flying;
    world.polarity = float(double(world.polarity) * .1);
    for (auto &point : history)
        point = position;
}
void Ship::didHit(Entity &) {
    if (state != State::Flying || grace > 0)
        return;
    state = State::Hit;
    world.healthFade = 0;
    world.lifeDropBudget = 1;
    stateTicks = 10;
    world.noise(28, 0x3fff);
    world.add(Domain::Decor, new Explosion(world, position, .08f, .1f, 1.13f));
}
void Ship::shoot() {
    firePhase = float(double(firePhase) -
                      (double(fireRate) + double(float(fireRate * world.weaponPower)) * .9));
    if (firePhase > 0)
        return;
    firePhase += 1;
    shotThisTick = true;
    shotCycle = (shotCycle + 1) & 3;
    world.noise(11);
    const int particleColour = world.polarity < 0 ? 8 : world.polarity > 0 ? 1 : 0;
    const int depth = particleColour == 1 ? power + 1 : particleColour == 8 ? 1 : 0;
    world.add(Domain::PlayerShots, new PlayerBullet(world, *this, -.04, particleColour, depth));
    world.add(Domain::PlayerShots, new PlayerBullet(world, *this, .04, particleColour, depth));
}
void Ship::run() {
    shotThisTick = false;
    world.healthFade = std::min(1.f, world.healthFade + .0016666667f);
    if (state != State::Flying) {
        world.scrollDelta.x = 0;
        if (state == State::Hit) {
            colour = world.palette[1];
            if (--stateTicks < 1) {
                stateTicks = 150;
                state = State::Waiting;
                colour.a = 0;
                breakup(world, position, 150, world.polarity < 0 ? 3 : world.polarity > 0 ? 4 : 0);
                world.noise(29);
                world.chain = 0;
            }
        }
        if (state == State::Waiting && --stateTicks < 0) {
            if (!world.demo)
                --world.lives;
            if (world.lives < 1) {
                if (world.runEnded)
                    world.runEnded();
                state = State::GameOver;
            } else
                init(true);
        }
        return;
    }
    if (grace > 0)
        --grace;
    world.weaponThreat = float(power) + std::abs(world.polarity) * 3.f;
    world.weaponFollowers = power;
    const Vec2 before = position;
    if (world.demo) {
        const auto *enemy = world.lists[size_t(Domain::Air)];
        const double target = enemy ? enemy->position.x : 0.;
        if (target < position.x)
            retained.x -= .004;
        if (position.x < target)
            retained.x += .004;
    } else if (world.relativeControls) {
        world.updateTouch();
        const Vec2 movement = world.touchRelative + world.input.movement;
        const float length = float(movement.length());
        const float low = world.tabletTouch ? .001f : .002f,
                    high = world.tabletTouch ? .01f : .015f;
        float gain = 1;
        if (length > high)
            gain = float(float(length - high) * (world.tabletTouch ? 10.f : 2.f) + 1.f);
        if (length < low)
            gain = 0;
        const Vec2 change = movement * double(gain);
        retained.x += change.x * 1.8;
        retained.y += change.y;
    } else {
        world.updateTouch();
        const Vec2 movement = world.touchStick + world.input.movement;
        retained.x += movement.x * .4;
        retained.y += movement.y * .2;
    }
    // Direct analog displacement is independent of the original touch gain.
    // Retained X projects to half-width; convert displayed intent explicitly.
    if (!world.demo) {
        retained.x += world.input.analogMovement.x * 2;
        retained.y += world.input.analogMovement.y;
    }
    retained.x = std::clamp(retained.x, -.9, .9);
    retained.y = std::clamp(retained.y, -.2, .44);
    position = {retained.x * .5, retained.y};
    const float priorCamera = float(world.camera.x);
    world.camera.x = retained.x * -2.3;
    world.scrollDelta.x = double(float((world.camera.x - double(priorCamera)) * .0625));
    shoot();
    trailDistance = float(double(trailDistance) + (position - before).length());
    if (double(trailDistance) > .0001) {
        history[size_t(trailCursor)] = position;
        trailCursor = (trailCursor + 1) & 511;
        do {
            trailDistance = float(double(trailDistance) - .0001);
        } while (double(trailDistance) > .0001);
    }
    radius = .035f;
    world.contacts(*this, Domain::Ground);
    radius = .025f;
    world.contacts(*this, Domain::Enemies);
    radius = .01f;
    world.contacts(*this, Domain::Pickups);
    radius = .025f;
    if (world.shipTiles)
        world.shipTiles(*this);
    else {
        const auto cell = world.tiles.cellAt(position);
        if (cell[0] >= 1 && cell[0] <= 19)
            for (int y = 0; y < 2; ++y)
                for (int x = -1; x < 2; ++x) {
                    if ((world.tiles.peek(cell[0] + x, cell[1] + y) & 255) == 50) {
                        world.tiles.poke(cell[0] + x, cell[1] + y, 0);
                        world.add(Domain::Decor, new Explosion(world, position, .08f, .1f, 1.02f));
                    }
                }
    }
    if (world.polarity == 0) {
        mass = 0;
        colour = {1, 1, 1, 1};
    } else {
        if (world.polarity > 0)
            mass =
                float((double(float(power) + world.polarity) + .1 + double(world.polarity) + .1) *
                      -4096.);
        else {
            const float field = float(double(std::abs(world.polarity)) + .1);
            mass = float(double(float(float(power) + field) + field) * -8192.);
        }
        const float phase = float(world.ticks & 63) * .015625f;
        const float pulse = float(double(std::sin(float(phase * 6.2831855f))) * .5 + .5);
        const auto target = unpack(world.polarity > 0 ? 0xff7010ffu : 0x1070ffffu);
        const float t = std::abs(world.polarity) * pulse;
        colour = mixColour({1, 1, 1, 1}, target, t);
    }
}
void Ship::prepare(std::vector<ActorDraw> &out) {
    const auto saved = colour;
    if (grace > 0)
        colour = world.palette[world.polarity < 0 ? 8 : world.polarity > 0 ? 1 : 11];
    Entity::prepare(out);
    colour = saved;
}
PlayerBullet::PlayerBullet(World &w, const Ship &ship, double offset, int tint, int depth)
    : GameEntity(w), palette(tint) {
    health = depth;
    position = ship.position + Vec2{offset, 0};
    retained = position;
    type = 0x2000;
    velocity = {0, double(-ship.shotSpeed)};
    frame = 15;
    radius = .015f;
    scale.x *= .7;
    if (palette == 1) {
        frame = 29;
        scale.x = 1;
    } else if (palette == 8) {
        frame = 28;
        scale.x = 1;
    }
}
void PlayerBullet::run() {
    colour = world.palette[size_t(palette)];
    position += velocity;
    const auto cell = world.tiles.cellAt(position);
    if (world.bulletTiles)
        world.bulletTiles(*this);
    else {
        const int tile = world.tiles.peek(cell[0], cell[1]);
        if ((tile & 255) == 50) {
            world.tiles.poke(cell[0], cell[1], 0);
            const auto at = world.tiles.positions[size_t(cell[1] * TileScreen::Stride + cell[0])];
            world.add(Domain::Decor, new Explosion(world, at, .08f, .1f, 1.02f));
            world.weaponPower = std::min(1.f, world.weaponPower + .35f);
            dead = true;
        }
        if (tile == 0x72e) {
            world.tiles.poke(cell[0], cell[1], 0x72f + (cell[0] & 1));
            --world.terminalTilesRemaining;
            if (world.terminalTilesRemaining <= 0 && !world.foxFound && position.x > -.3 &&
                position.x < .3 && position.y < -.1) {
                world.foxFound = true;
                world.achieveGoal(0);
                world.add(Domain::Decor, new Fox(world, cell[1] * TileScreen::Stride + cell[0]));
            }
        }
    }
    if (position.y > -.5) {
        if (world.polarity > 0)
            world.contacts(*this, Domain::Pickups);
        world.contacts(*this, Domain::Enemies);
        world.contacts(*this, Domain::Air);
        world.contacts(*this, Domain::Foreground);
    }
    if (position.x < -.6 || position.x > .6 || position.y < -.6 || position.y > .6)
        dead = true;
    angle += arcStep;
    velocity.rotateBy(double(arcStep));
    arcRemaining -= std::abs(arcStep);
    if (arcRemaining < 0)
        arcStep = 0;
    repeatClock -= repeatStep;
    if (repeatClock < 0) {
        angle += repeatAngle;
        velocity.rotateBy(double(repeatAngle));
        if (--repeatCount < 1) {
            repeatStep = 0;
            repeatClock = 0;
        } else
            repeatClock += repeatInterval;
    }
}
} // namespace superoxwars
