#include "world.h"
#include <algorithm>

namespace fiveaday {
Ship::Ship(World &w) : Entity(w) {}
void Ship::init() {
    type = 4;
    frame = 3;
    radius = .04f;
    age = 0;
    state = State::Entering;
    position = {0, .6};
    velocity = {0, .013};
    turnRate = targetHeading = angle = cameraTurn = flame = displayHealth = weaponEnergy = travel =
        damageTravel = hitFlash = 0;
    integrity = speedMultiplier = 1;
    health = 1;
    chasing = false;
    impulse = cameraOrigin = {};
    fireTicks = fireInterval = 10;
    collected = 1;
    shieldTicks = 0;
    trailCursor = 0;
    damageSounds_ = 0;
    exitAcceleration = 0;
    burstCount = 1;
    burstTimeout = 0;
    // ship::init resets scene +0x38 even when the owning scene is reused.
    for (auto &point : trail)
        point = {0, -1};
    colour = {1, 1, 1, 1};
    world.formationBonusMultiplier = 1;
    world.scoreMultiplier = 1;
    world.noise(7);
}
void Ship::steer(const Input &input) {
    const double magnitude = double(float(input.direction.length()));
    if (magnitude > .001) {
        targetHeading = float(std::atan2(-input.direction.y, input.direction.x) - 1.570796327);
        if (double(targetHeading) < -Pi)
            targetHeading = float(double(targetHeading) + Tau);
        turnRate = angle < targetHeading ? .1f : -.1f;
        if (double(std::abs(float(angle - targetHeading))) > Pi)
            turnRate = -turnRate;
    } else {
        turnRate = float(double(turnRate) * .9);
    }
    if (std::abs(float(angle - targetHeading)) <= std::abs(turnRate)) {
        angle = targetHeading;
        turnRate = 0;
    }
    cameraTurn = 0;
    angle += turnRate;
    if (double(angle) > Pi)
        angle = float(double(angle) - Tau);
    if (double(angle) < -Pi)
        angle = float(double(angle) + Tau);
}
void Ship::fire() {
    if (--fireTicks > 0)
        return;
    fireTicks = fireInterval - int(double(weaponEnergy) * .8 * double(float(fireInterval)));
    auto *e = new Bullet(world);
    e->type = 600;
    e->radius = .015f;
    e->position = position;
    e->velocity = {0, -.01};
    e->velocity.rotate(double(angle));
    e->frame = 2;
    const int day = std::max(world.level, 0) % 30 % 5;
    if ((day == 3 && world.progress > .5f) || (day == 4 && world.progress < .5f))
        e->frame = 21;
    world.add(world.bullets, e);
    if (burstCount > 1) {
        const int count = burstCount;
        const float factor = float(1.f - float(count) / 100.f);
        const double low = double(float(double(factor) * .6 + .15)), high = low * 1.05;
        float direction = -3.1415927f;
        for (int n = 0; n < count; ++n) {
            auto *b = new Bullet(world);
            b->type = 600;
            b->radius = .015f;
            b->position = position;
            b->frame = e->frame;
            b->burst = true;
            b->velocity = {0, -.01};
            b->velocity.rotate(double(angle));
            b->velocity = b->velocity * world.random.range(low, high);
            b->velocity.rotate(double(direction));
            b->initialVelocity = b->velocity;
            world.add(world.bullets, b);
            direction += 6.2831855f / float(count - 1);
        }
        burstCount = 1;
    }
}
void Ship::run() {
    ++age;
    double nearest = 100;
    if (!chasing) {
        for (auto *e = world.enemies; e; e = e->next) {
            const auto *attacker = dynamic_cast<Attacker *>(e);
            if (!attacker || e->type != 3 || !attacker->armed)
                continue;
            const float distance = float((position - e->position).length());
            if (double(distance) >= .3)
                continue;
            nearest = std::min(nearest, double(distance));
            float diff = float(angle - attacker->heading);
            if (double(diff) > Pi)
                diff = float(Tau - double(diff));
            if (double(diff) < -Pi)
                diff = float(double(diff) + Tau);
            if (double(std::abs(diff)) < .3) {
                chasing = true;
                break;
            }
        }
        if (!chasing) {
            if (speedMultiplier > 1)
                speedMultiplier = float(double(speedMultiplier) - .05);
            if (speedMultiplier < 1)
                speedMultiplier = 1;
        }
    }
    if (chasing) {
        if (double(speedMultiplier) < 1.2)
            speedMultiplier = float(double(speedMultiplier) + .01);
        if (double(speedMultiplier) >= 1.2 || nearest > .4)
            chasing = false;
    }
    // The earned bonus persists after shield expiry until damage or ship init.
    world.formationBonusMultiplier = integrity < 1     ? 1
                                     : shieldTicks > 0 ? 2
                                                       : world.formationBonusMultiplier;
    if (shieldTicks > 0)
        --shieldTicks;
    if (burstTimeout > 0) {
        burstTimeout = float(double(burstTimeout) - .02);
        if (burstTimeout < 0) {
            burstTimeout = 0;
            burstCount = 1;
        }
    }
    impulse = impulse * .95;
    if (displayHealth < integrity)
        displayHealth = std::min(integrity, float(double(displayHealth) + .01));
    if (displayHealth > integrity)
        displayHealth = std::max(integrity, float(double(displayHealth) - .01));
    if (world.input.held)
        flame = std::min(1.f, float(double(flame) + .05));
    else
        flame = float(double(flame) * .97);
    if (weaponEnergy > 0)
        weaponEnergy = float(double(weaponEnergy) * .993);
    switch (state) {
    case State::Flying:
        world.sequencePaused = false;
        steer(world.input);
        if (double(world.cameraAngle) > Tau)
            world.cameraAngle = float(double(world.cameraAngle) - Tau);
        if (world.cameraAngle < 0)
            world.cameraAngle = float(double(world.cameraAngle) + Tau);
        velocity.y = double(float(speedMultiplier * .007f));
        fire();
        if (!world.demo && integrity <= 0) {
            state = State::Dying;
            world.explode(position, .02f, .02f, .018f);
            world.noise(10, 13);
            world.noise(9, 10);
        }
        world.touch(*this, world.shots);
        world.touch(*this, world.enemies);
        world.touch(*this, world.fruits);
        velocity.y *= .95;
        break;
    case State::Dying:
        colour = world.palette(1);
        velocity.y += .00008;
        if (velocity.y > .015)
            velocity.y = .015;
        if (velocity.y > .014) {
            state = State::Waiting;
            world.perfectDay = false;
            waitTicks = 200;
            colour.a = 0;
            velocity.y = 0;
            world.explode(position, .02f, .005f, .004f);
            world.noise(10, 13);
        }
        world.touch(*this, world.shots);
        world.touch(*this, world.shots);
        world.touch(*this, world.enemies);
        break;
    case State::Waiting:
        world.sequencePaused = true;
        if (--waitTicks < 1) {
            --world.lives;
            if (world.lives < 1) {
                world.goGameOver();
                state = State::GameOver;
            } else {
                integrity = 1;
                world.camera = {};
                travel = 0;
                trailCursor = 0;
                state = State::Entering;
                world.noise(7);
                position.y = .6;
                velocity = {0, .013};
                angle = 0;
                collected = 1;
            }
        }
        break;
    case State::Entering:
        world.sequencePaused = true;
        steer(world.input);
        colour = world.palette(4);
        velocity.y = (position.y / .6) * .01 + double(.007f);
        position.y *= .98;
        if (position.y < .5 && world.fruitCount > 4)
            world.deliverFruitSet();
        if (position.y < .004) {
            position.y = 0;
            state = State::Flying;
            colour = {1, 1, 1, 1};
        }
        world.touch(*this, world.shots);
        world.touch(*this, world.enemies);
        world.touch(*this, world.fruits);
        fire();
        break;
    case State::Leaving:
        angle = float(double(angle) * .99);
        exitAcceleration = float(double(exitAcceleration) + .000005);
        if (world.input.held)
            exitAcceleration = float(double(exitAcceleration) + .00005);
        position.y -= double(exitAcceleration);
        {
            const double distance = double(float(std::abs(position.y)));
            velocity.y =
                distance <= .6 ? (distance / .6) * .01 + double(.007f) : .017000000613431137;
        }
        colour = world.palette(4);
        if (position.y < -1.5)
            world.beginBonus();
        break;
    case State::Bonus:
        velocity.y *= .99;
        break;
    default:
        return;
    }
    world.camera = velocity;
    world.camera.rotate(double(-float(world.cameraAngle - angle)));
    world.camera = world.camera - impulse;
    cameraOrigin = cameraOrigin - world.camera;
    if (state == State::Bonus)
        return;
    const float distance = float(world.camera.length());
    travel += distance;
    if (double(travel) > .03) {
        const float speed = std::min(1.f, float(double(distance) / .009));
        const float base = float(double(float(speed * integrity)) * .4 + .2);
        const float alpha = std::max(base, float(double(flame) * .7));
        travel = float(double(travel) - .03);
        auto *e = new Glow(world);
        e->frame = 18;
        e->position = position;
        e->angle = angle;
        e->radius = .05f;
        e->colour.a = double(alpha);
        world.add(world.effects, e);
    }
    damageTravel += distance;
    if (double(damageTravel) > .06) {
        damageTravel = float(double(damageTravel) - .06);
        if (integrity < 1) {
            if (integrity <= 0)
                integrity = 0;
            if (double(integrity) < world.random.range(0, 1)) {
                const auto r = world.random.next();
                if ((r & 255) < 32) {
                    world.noiseGain(35 + (damageSounds_ & 1), 8 * 0x300);
                    ++damageSounds_;
                }
                const double growth =
                    world.random.range(.001, double(float(1.f - integrity)) * .001 + .001);
                world.explode(position, .01f, .015f, float(growth));
            }
        }
    }
    Vec2 point = cameraOrigin;
    if (state == State::Leaving)
        point.y += position.y;
    trail[trailCursor & 511] = point;
    ++trailCursor;
}
void Ship::didHit(Entity &other) {
    if (state == State::Entering)
        return;
    if (other.type != 2 && other.type != 3)
        return;
    if (state == State::Flying) {
        impulse += other.velocity * .3;
        if (!world.demo)
            integrity = float(double(integrity) - .2);
    }
    world.flash(position, 4, other.type == 2 ? .3f : .5f);
    world.noise(20);
    world.noise(int(world.random.next() & 1) + 57);
    world.cleanStage = false;
    world.scoreMultiplier = float(double(world.scoreMultiplier) * .98);
    angle = float(std::atan2(-(position.y - other.position.y), position.x - other.position.x) -
                  1.570796327);
    if (double(angle) < -Pi)
        angle = float(double(angle) + Tau);
    hitFlash = 1;
    if (world.openingTutorial && world.repairHintPending && integrity <= .5f) {
        world.repairHintPending = false;
        world.message(12, "COLLECT FRUIT", 60);
        world.message(14, "TO REPAIR SHIP", 60);
    }
}
void Ship::prepare(std::vector<ActorDraw> &out) {
    hitFlash = float(double(hitFlash) * .92);
    // Original ship::draw: centre-aligned health strip and remaining-life ships.
    if (!world.demo) {
        out.push_back({{0, -.46},
                       {double(displayHealth) * .7, .02},
                       .5f,
                       0,
                       0,
                       {1, 1, 1, double(hitFlash) * .4 + .1}});
        if (world.lives > 1) {
            const float width =
                world.lives == 2 ? 0.f
                                 : std::min(.7f, float(double(float(world.lives - 1)) * .1 + .2));
            const float step = world.lives == 2 ? 0 : width / float(world.lives - 2);
            float x = world.lives == 2 ? 0 : width * -.5f;
            for (int n = 0; n < world.lives - 1; ++n) {
                out.push_back({{double(x), -.46},
                               {.03, .04},
                               .5f,
                               0,
                               3,
                               {1, 1, 1, double(hitFlash) * .4 + .5}});
                x += step;
            }
        }
    }
    Entity::prepare(out);
}
} // namespace fiveaday
