#include "world.h"
#include "original_tables.h"
#include "neon/foundation_rules.h"
#include <algorithm>

namespace fiveaday {
Colour dynamicColour(int kind, uint64_t ticks, float alpha) {
    const auto c = neon::paletteColour(kind, alpha, uint32_t(ticks),
                                       {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
namespace {
void wrapHeading(float &angle) {
    if (double(angle) < -Pi)
        angle = float(double(angle) + Tau);
    if (double(angle) > Pi)
        angle = float(double(angle) - Tau);
}
struct EnemyShot : Wrapped {
    int ticks = 200;
    explicit EnemyShot(World &w) : Wrapped(w) {
        type = 2;
        frame = 2;
        radius = targetRadius = .02f;
    }
    void run() override {
        Wrapped::run();
        --ticks;
        if ((wrapped & 15) || ticks < 0)
            dead = true;
        colour = world.palette(frame == 46 ? 5 : 1);
        if (frame == 46)
            angle = -.7853982f;
        if (ticks < 50)
            colour.a = (ticks & 4) ? 0 : 1;
    }
    void hitBy(Entity &e) override {
        e.didHit(*this);
        world.explode(position, .02f, .01f, .002f);
        dead = true;
    }
};
} // namespace
void World::radialShoot(const Entity &source, int count, float angle, float spread, float speed) {
    if (count < 1)
        return;
    const float step = spread / float(count);
    float direction = float(float(spread * -.5f) + angle);
    for (int n = 0; n < count; ++n) {
        auto *e = new EnemyShot(*this);
        e->position = e->retained = source.position;
        e->velocity = {0, double(speed)};
        e->velocity.rotate(double(direction));
        e->culled = true;
        e->cullMargin = 2;
        add(shots, e);
        direction += step;
    }
}
Attacker::Attacker(World &w) : Wrapped(w) {}
void Attacker::init(const Launch &p) {
    constexpr int frames[] = {7, 34, 39, 45, 52, 59};
    constexpr double heights[] = {1, 1, 1, double(.6f), double(1.2f), 1};
    constexpr uint32_t colours[] = {0xffffffff, 0x80ff807f, 0xffffffcf,
                                    0xffffffff, 0xffffffff, 0xffffffff};
    radius = targetRadius = .05f;
    cullMargin = .05f;
    wrapX = wrapY = .8f;
    retained = position;
    spinOffset = angle;
    heading = 0;
    culled = true;
    missileChance = int(double(world.month) * 16) + int(double(world.year) * 32) + 128;
    if (world.month < 1)
        missileChance = 0;
    world.population += weight;
    type = 3;
    const int era = int(world.month) % 6;
    frame = frames[era];
    scale = {1, heights[era]};
    colour = unpack(colours[era]);
    turnStep = float(float(float(world.progress * .005f) + .04f) + float(world.week * .02f));
    turnStep *= p.turnScale;
    speed = float(double(world.progress) * .0005 + .006 + double(world.week) * .001 +
                  double(world.year) * .001);
    speed *= p.speedScale;
    orbitRadius = p.orbitRadius;
    orbitGain = p.orbitGain;
    shotInterval = shotTicks = int(float(world.year * -5.f + 20.f));
    shotChance = (int(world.year) + int(world.week)) * 32 + 64;
    health = 1;
    shotSpeed = float(double(world.week) * .3 + 1.2 + double(world.year) * .2);
    if (frame == 7)
        spinOffset = .7853982f;
    else if (frame == 34)
        spinOffset = 1.5707964f;
    else if (frame == 39)
        radius = targetRadius = .04f;
    else if (frame == 45) {
        shotMode = 2;
        shotSpeed = float(double(world.week) * .003 + .003);
        aimThreshold = float(float(world.week + world.week) + .5f);
    }
}
void Attacker::run() {
    if (deathTicks > 0) {
        if (--deathTicks < 1) {
            dead = true;
            world.population -= weight;
            world.addScore(int(float(float(reward) * world.scoreMultiplier)));
        }
        return;
    }
    if (world.bossLaunched)
        orbitRadius = float(double(orbitRadius) * .99);
    velocity = {0, double(-speed)};
    velocity.rotate(double(heading));
    course += velocity;
    Wrapped::run();
    history[historyCursor & 255] = course;
    ++historyCursor;
    float desiredOrbit = carrier ? .2f : orbitRadius;
    if (formationLocked && !world.gameOver) {
        tailLength -= tailDecay;
        if (tailLength < 1) {
            carrier = false;
            formationLocked = false;
            orbitRadius = .15f;
            shotInterval = 5;
            missileChance <<= 1;
            missileInterval /= 2;
            enraged = true;
            health = 1;
            speed = float(double(speed) * 1.4);
            turnStep = float(double(turnStep) * .4);
            const auto value = world.random.next();
            world.noise((value & 255) < 8 ? 11 : 22, (value & 255) < 8 ? 12 : 13);
        }
    }
    if (enraged)
        colour = world.palette(2);
    if (world.sequencePaused)
        desiredOrbit = 1;
    const Vec2 delta = world.ship.position - retained;
    float desired = float(std::atan2(-delta.y, delta.x) - 1.570796327);
    if (double(desired) < -Pi)
        desired = float(double(desired) + Tau);
    distance = float(delta.length());
    const float factor = std::clamp(float(float(distance - desiredOrbit) * orbitGain), 0.f, 1.f);
    const float difference = float(desired - heading), absolute = std::abs(difference);
    float step = difference <= 0 ? -turnStep : turnStep;
    if (absolute > 3.1415927f)
        step = -step;
    heading += neon::interpolateFloat(-step, step, factor);
    wrapHeading(heading);
    if (frame == 45)
        shotChance = int((1. - position.length()) * 299.);
    if (missileTicks < 1) {
        missileTicks = missileInterval;
        const auto r = world.random.next();
        if (int(r & 511) < missileChance && !world.sequencePaused)
            world.add(world.enemies, new HomingMissile(world, *this));
    } else
        --missileTicks;
    if (absolute < aimThreshold && !world.gameOver && --shotTicks < 1) {
        shotTicks = shotInterval;
        const auto r = world.random.next();
        if (int(r & 511) < shotChance && !world.sequencePaused) {
            if (shotMode == 0) {
                if (distance > .4f) {
                    auto *e = new EnemyShot(world);
                    e->velocity = velocity * double(shotSpeed);
                    e->position = e->retained = position;
                    world.add(world.shots, e);
                }
            } else if (shotMode == 1) {
                world.radialShoot(*this, 1, heading, 0, shotSpeed);
                world.noise(19);
            } else if (shotMode == 2) {
                auto *e = new EnemyShot(world);
                e->position = e->retained = position;
                e->velocity = {0, double(shotSpeed)};
                e->velocity.rotate(-.7853981852531433);
                e->angle = -.7853982f;
                e->spinOffset = angle + 3.1415927f;
                e->frame = 46;
                e->radius = .03f;
                e->scale.x = .5;
                e->position.x += world.random.range(-.04, .04);
                e->retained = e->position;
                world.add(world.enemies, e);
                world.noise(19);
            } else if (shotMode == 3)
                world.add(world.enemies, new HomingMissile(world, *this, true));
            else if (shotMode == 4) {
                world.radialShoot(*this, shotCount, gunAngle, shotSpread, shotSpeed);
                world.noise(19);
            }
        }
    }
    if (frame == 45)
        angle = 0;
    if (world.bossDefeated && deathTicks != 1) {
        world.explode(position, .02f, .02f, .006f);
        world.flash(position, 1, .3f);
        deathTicks = 2;
        colour.a = 0;
    }
}
void Attacker::hitBy(Entity &e) {
    if (deathTicks > 0)
        return;
    e.didHit(*this);
    const float at = float(position.length());
    if (e.type != 4 || radius <= targetRadius) {
        if (at < damageRange)
            --health;
        wasHit = true;
        if (health < 1) {
            killedNow = true;
            if (!suppressDeathBlast)
                world.explode(position, .02f, .02f, .006f);
            world.flash(position, 1, .3f);
            deathTicks = 2;
            colour.a = 0;
            world.noise(59);
            if (enraged) {
                const auto r = world.random.next();
                world.noise((r & 255) < 32 ? 28 : 70);
            }
            if (formationMember) {
                // bro::hitBy 0x5ebc8 rewards consecutive carriers within one wave.
                const int bonus = world.formationBonusMultiplier * (100 << world.formationKills);
                const int points = int(float(float(bonus) * world.scoreMultiplier));
                world.popupScore(position, points);
                world.addScore(points);
                world.noise(45, 10);
                if (++world.formationKills > 4) {
                    world.noise((world.formations & 1) ? 21 : 26);
                    world.formationKills = 0;
                    ++world.formations;
                }
            }
        } else {
            radius = targetRadius * 1.5f;
            world.explode(e.position, .01f, .04f, .002f, 51);
            if (at >= damageRange)
                wasHit = false;
        }
    }
}
void Attacker::prepare(std::vector<ActorDraw> &out) {
    if (carrier) {
        Colour col = world.palette(formationColour);
        col.a = .6;
        for (int n = int(tailLength); n > 0; n -= 2) {
            if (culled) {
                culled = false;
                continue;
            }
            out.push_back({history[(historyCursor - unsigned(n)) & 255] - course + position,
                           {1, 1},
                           .02f,
                           angle,
                           2,
                           col});
        }
    }
    // attacker::draw calls entity directly, unlike cloud/fruit wentity culling.
    Entity::prepare(out);
}
Fruit::Fruit(World &w, Attacker &owner) : Wrapped(w), parent(&owner) {
    position = owner.position;
    radius = targetRadius = .03f;
    spin = float(world.random.range(.2, .2));
    if (world.random.next() & 1)
        spin = -spin;
    frame = int(original::thefruit[size_t((world.level + 1) % 30)]);
    const auto size = world.dimensions(frame);
    scale.y = size.y / size.x;
    retained = position;
    floating = true;
    wrapX = wrapY = float(.8f * 4.f);
    culled = true;
    cullMargin = float(std::max(scale.x * double(radius), scale.y * double(radius)));
}
void Fruit::run() {
    culled = false;
    if (state == State::Carried) {
        if (!parent)
            return;
        if (!parent->carrier) {
            energy = .09f;
            state = State::Fading;
        } else {
            retained =
                parent->history[(parent->historyCursor - unsigned(int(parent->tailLength))) & 255] -
                parent->course + parent->position;
            position = retained;
            velocity = parent->velocity * 1.2;
            drift = velocity;
            if (parent->deathTicks > 0) {
                state = State::Loose;
                parent = nullptr;
            }
        }
        return;
    }
    spinOffset += float(spin * energy);
    angle = float(world.cameraAngle + spinOffset);
    auto &ship = world.ship;
    if (state == State::Collected) {
        const auto n = (ship.trailCursor - unsigned(index * 12)) & 511;
        position = retained = ship.trail[n] - ship.cameraOrigin;
        drift = velocity;
        if (int(ship.state) < 4)
            colour.a = position.y < -.5 ? 0 : 1;
        else {
            colour.a *= .995;
            if (int(ship.state) > 4)
                dead = true;
        }
        if (ship.state == Ship::State::Waiting) {
            state = State::Loose;
            drift = velocity = {};
        }
        return;
    }
    velocity = drift * double(energy);
    drift = drift * .95;
    retained += velocity;
    retained += world.camera * double(cameraCoupling);
    position = retained;
    angle = float(float(world.cameraAngle + spinOffset) + heading);
    energy = float(double(energy) - .002);
    if (double(energy) < .1 && state != State::Fading) {
        state = State::Fading;
        world.noise(int((world.random.next() & 15) % 3) + 16);
        if (minotaur) {
            world.noise(int((world.random.next() & 15) % 5));
            world.message(11, "A MINOTAUR HAS FALLEN", 200, 0xff00ff00u);
        }
    }
    if (state != State::Fading) {
        if (ship.state == Ship::State::Entering || ship.state == Ship::State::Flying) {
            const Vec2 attraction = world.attractShip(position, 100.f, 50000.f);
            if (attraction.length() < double(.01f))
                drift += attraction;
        }
    } else {
        colour.a = double(float(energy * 10.f));
        scale = scale * .99;
        if (colour.a < .01)
            dead = true;
    }
}
void Fruit::hitBy(Entity &e) {
    if (state == State::Fading)
        return;
    if (e.type == 7 || e.type == 3) {
        e.didHit(*this);
        return;
    }
    if (e.type != 4 || state != State::Loose)
        return;
    auto &ship = static_cast<Ship &>(e);
    if (frame == 32) {
        ship.burstCount = std::clamp(ship.collected * 3 + 10, 10, 100);
        ship.burstTimeout = 5.497787f;
        world.noise(71);
    }
    if (ship.weaponEnergy < 1)
        ship.weaponEnergy = float(double(ship.weaponEnergy) + .2);
    index = ship.collected++;
    state = State::Collected;
    if (ship.integrity < 1) {
        ship.hitFlash = 1;
        ship.integrity = float(double(ship.integrity) + .1);
    }
    if (ship.integrity >= 1) {
        if (ship.shieldTicks < 1) {
            world.noise(31, 13);
            ship.shieldTicks = 180;
        }
        world.scoreMultiplier = 1;
    }
    if (ship.integrity > 1)
        ship.integrity = 1;
    world.noise(int(world.random.next() & 3) + 12);
}
void Bullet::run() {
    if (frame == 2)
        colour = world.palette(4);
    else if (frame == 21)
        world.flash(position, int(0xffcf207fu), .05f);
    velocity.rotate(double(-world.ship.cameraTurn));
    position.rotate(double(-world.ship.cameraTurn));
    position += velocity;
    world.touch(*this, world.enemies);
    if (burst && world.bossLaunched && !world.bossDefeated) {
        const auto delta = world.bossPosition - position;
        const auto heading = [](Vec2 v) {
            float a = float(std::atan2(-v.y, v.x) - 1.570796327);
            if (double(a) < -Pi)
                a = float(double(a) + Tau);
            return a;
        };
        float difference = float(heading(delta) - heading(initialVelocity));
        if (double(difference) > Pi)
            difference = float(Tau - double(difference));
        if (double(difference) < -Pi)
            difference = float(double(difference) + Tau);
        const float aligned = std::max(0.f, float(1.f - std::abs(difference)));
        velocity = initialVelocity * double(float(double(aligned) * -.1 + 1.));
        const auto a = world.palette(4), b = world.palette(1);
        const double t = double(aligned);
        colour = {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                  a.a + (b.a - a.a) * t};
    }
    if (position.x < -.6 || position.x > .6 || position.y < -.6 || position.y > .6)
        dead = true;
}
void Bullet::didHit(Entity &) {
    dead = true;
    if (world.ship.weaponEnergy < 1)
        world.ship.weaponEnergy = float(double(world.ship.weaponEnergy) + .2);
    world.flash(position, 4, .1f);
    world.explode(position, .01f, .04f, .002f);
}

Cloud::Cloud(World &w) : Wrapped(w) {
    type = 1;
    frame = 140;
    phase = float(w.random.range(-Pi, Pi));
    phaseStep = float(w.random.range(.0006, .006));
    const float sine = std::sin(phase);
    const double coupling = w.random.range(0, 1);
    radius = float(double(sine) * .05 + .1);
    details(int(original::cloudShapes[size_t(int(w.month) % 6)]));
    cullMargin = .4f;
    position = {w.random.range(-.9000000059604645, .9000000059604645),
                w.random.range(-(double(cullMargin) + .5), double(cullMargin) + .5)};
    const Vec2 base{1e-05, 0.0002};
    velocity = base * w.random.range(1, 1.2);
    velocity.rotate(w.random.range(-.02, .02));
    retained = position;
    heading = 0;
    spinOffset = angle;
    cameraCoupling = float(float(coupling) + 1.f);
    wrapX = wrapY = .9f;
    targetRadius = radius;
    culled = true;
}
void Cloud::details(int requested) {
    if (frame == requested)
        return;
    const float detail = float(world.random.range(0, 1));
    const float sine = std::sin(phase);
    frame = requested;
    spin = 0;
    angle = 0;
    targetAlpha = 0;
    alphaScale = 1;
    colour.a = double(sine) * .1 + .1;
    if (frame == 30) {
        scale = {1, .7};
        alphaScale = .5f;
    } else if (frame == 31) {
        scale = {.42, .6};
        alphaScale = .3f;
        angle = float(world.random.range(0, Tau));
        spin = float(world.random.range(-.01, .01));
    } else if (frame == 58) {
        const double size = world.random.range(.7, 1.2);
        alphaScale = 1;
        targetAlpha = .9f;
        scale = {double(float(float(size) + float(size))), double(float(size)) * .8};
    } else {
        const double max = frame == 2 ? 3. : 1.5;
        if (frame == 2)
            alphaScale = .7f;
        scale = {double(detail) * world.random.range(0, max) + 1.,
                 1. + double(detail) * world.random.range(0, max)};
        if (frame != 2)
            angle = float(world.random.range(0, Tau));
        else {
            spinOffset = angle;
            return;
        }
    }
    spinOffset = angle;
}
void Cloud::run() {
    Wrapped::run();
    if (minAlpha < targetAlpha) {
        minAlpha = float(double(minAlpha) + .001);
        if (minAlpha > targetAlpha)
            minAlpha = targetAlpha;
    }
    if (targetAlpha < minAlpha) {
        minAlpha = float(double(minAlpha) - .01);
        if (minAlpha < targetAlpha)
            minAlpha = targetAlpha;
    }
    phase += phaseStep;
    spinOffset += spin;
    const double sine = double(std::sin(phase));
    auto current = unpack(world.cloudColour);
    current.a = std::max(double(minAlpha), current.a * (sine * .12 + .12) * double(alphaScale));
    const int requested = int(original::cloudShapes[size_t(int(world.month) % 6)]);
    if (requested == 5 || world.gameOver)
        targetAlpha = 0;
    if (current.a < .01)
        details(requested);
    if (frame != 58)
        radius = float(sine * .05 + .1);
    if (double(flash) > .01) {
        const double t = double(flash);
        colour = {current.r + (flashColour.r - current.r) * t,
                  current.g + (flashColour.g - current.g) * t,
                  current.b + (flashColour.b - current.b) * t,
                  current.a + (flashColour.a - current.a) * t};
        flash = float(double(flash) * .95);
    } else
        colour = current;
}
} // namespace fiveaday
