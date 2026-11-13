#include "projectiles.h"
#include "ground.h"
#include "effects.h"
#include <algorithm>
namespace superoxwars {
float heading(Vec2 v) {
    float result = float(std::atan2(double(-float(v.y)), double(float(v.x))) - 1.570796327);
    if (double(result) < -3.141592654)
        result = float(double(result) + 6.2831853072);
    return result;
}
void flowerSparks(World &w, const Entity &source) {
    for (int n = 0; n < 10; ++n) {
        auto *p = new ShotSpark(w);
        p->position = source.position;
        p->colour = source.colour;
        p->radius = .01f;
        p->frame = 0;
        p->velocity = {0, w.random.range(.001, .003)};
        p->velocity.rotateBy(w.random.range(-3.141592654, 3.141592654));
        p->fade = float(w.random.range(.005, .02));
        p->turn = float(w.random.range(-.1, .1));
        w.add(Domain::Decor, p);
    }
}
Butterfly::Butterfly(World &w, const Entity &source) : HomingMissile(w) {
    frame = 59;
    position = source.position;
    radius = .025f;
    turnRate = float(w.random.range(.05, .2));
    tracking = int(w.random.range(30, 70));
    velocity = {0, double(-.005f)};
    health = 0;
    angle = 0;
    radius = .035f;
    grace = 10;
    colour = source.colour;
    tracking = int(w.random.range(100, 200));
}
void Butterfly::run() {
    HomingMissile::run();
    wingPhase += speed;
    scale.x = double(std::sin(wingPhase * 20.f));
}
void Butterfly::hitBy(Entity &other) {
    if (grace != 0)
        return;
    HomingMissile::hitBy(other);
    flowerSparks(world, *this);
}
HomingMissile::HomingMissile(World &w, Vec2 at) : GameEntity(w) {
    grace = 2;
    frame = 23;
    position = at;
    radius = .025f;
    turnRate = float(w.random.range(.05, .2));
    tracking = int(w.random.range(30, 70));
    velocity = {0, double(-.005f)};
    health = 0;
}
void HomingMissile::run() {
    if (grace != 0)
        --grace;
    if (tracking > 0) {
        --tracking;
        const float target = heading((world.player ? world.player->position : Vec2{}) - position),
                    delta = angle - target;
        float change = target > angle ? turnRate : -turnRate;
        if (std::abs(double(delta)) > 3.141592654)
            change = -change;
        if (std::abs(delta) < std::abs(turnRate))
            change = 0;
        angle += change;
        if (double(angle) > 3.141592654)
            angle = float(double(angle) - 6.2831853072);
        if (double(angle) < -3.141592654)
            angle = float(double(angle) + 6.2831853072);
        velocity = {0, double(-speed)};
        velocity.rotateBy(double(angle));
    }
    position += velocity;
    if (position.x < -.6 || position.x > .6 || position.y < -.6 || position.y > .6)
        dead = true;
}
void HomingMissile::hitBy(Entity &other) {
    other.didHit(*this);
    world.weaponPower = std::min(1.f, world.weaponPower + .35f);
    world.add(Domain::Decor, new Explosion(world, position, .04f, .2f, 1.02f));
    if (--health < 1)
        dead = true;
}
bool emitEnemyShots(World &w, Entity &source, int mode, float speed, int count, float spread,
                    bool deathShot) {
    if (!w.player || w.player->state != Ship::State::Flying)
        return false;
    if (w.polarity < 0)
        speed = float(double(speed) * w.random.range(double(1.f - .2f), double(1.f + .2f)));
    const int chance =
        int((double(w.enemyShotChance) + double(w.chain) * 8.) * double(w.healthFade));
    auto *ground = dynamic_cast<GroundEnemy *>(&source);
    const Vec2 origin = ground ? ground->muzzle : source.position;
    Vec2 target = w.player->position - origin;
    if (!deathShot && target.length() < .2)
        return false;
    if (target.y < 0)
        return false;
    if (ground)
        ground->pulse = 1.4f;
    Vec2 offset;
    if (ground) {
        offset = ground->emissionOffset;
        ground->emissionOffset = ground->emissionOffset * ground->emissionMultiplier;
        offset.rotateBy(double(source.angle));
    }
    Vec2 v;
    switch (mode) {
    case 0:
        v = target.normalize() * double(speed);
        break;
    case 1: {
        target.rotateBy(-double(source.angle));
        const float a = heading(target);
        int quadrant = int(std::abs(double(a)) / 3.141592654 * 2. + .5);
        if (a < 0)
            quadrant = -quadrant;
        v = {0, -double(speed)};
        v.rotateBy(double(quadrant) * 1.570796327 + double(source.angle));
        break;
    }
    case 2:
        v = {0, -double(speed)};
        v.rotateBy(double(source.angle));
        break;
    case 3:
        v = {0, -double(speed)};
        v.rotateBy(w.random.range(-3.141592654, 3.141592654));
        break;
    case 4:
        v = {0, double(speed)};
        break;
    default:
        w.unsupported("Projectile emission mode");
        return false;
    }
    count &= 255;
    if (w.polarity > 0) {
        count &= 254;
        if (count == 0)
            count = 2;
    }
    count = std::min(count, 32);
    // Even the single-shot path draws its probability token (4e630).
    const int shots = std::max(1, count);
    float a = count < 2 ? 0 : spread * -.5f, step = count < 2 ? 0 : spread / float(count - 1);
    for (int i = 0; i < shots; ++i, a += step) {
        if (int(w.random.next() & 255) >= chance)
            continue;
        Vec2 shot = v;
        if (count >= 2)
            shot.rotateBy(double(a));
        const float localPolarity = ground && ground->bias < 0 ? std::abs(w.polarity) : w.polarity;
        if (w.polarity < 0) {
            const float jitter = .1f * (1.f - localPolarity);
            shot.rotateBy(w.random.range(double(-jitter), double(jitter)));
        }
        w.add(Domain::Pickups,
              new EnemyBullet(w, origin + offset + shot.normalize() * (double(source.radius) * .5),
                              shot));
    }
    return true;
}
EnemyBullet::EnemyBullet(World &w, Vec2 p, Vec2 v) : GameEntity(w) {
    position = retained = p;
    velocity = v;
    mass = 10;
    type = 0x4002;
    frame = w.enemyBulletOverride ? w.enemyBulletFrame : 2;
    radius = .02f;
    paletteSlot = w.enemyBulletOverride ? w.enemyBulletPalette : 10;
}
void EnemyBullet::run() {
    if (--lifetime < 0)
        dead = true;
    if (paletteSlot > 0)
        colour = world.palette[size_t(paletteSlot)];
    Vec2 attraction;
    if (world.player && grace == 0) {
        const Vec2 delta = (world.player->position - position) * 512.;
        const double distance = delta.length();
        const float f = float(distance);
        const float numerator = float(float(mass * 1e-6f) * world.player->mass);
        const float force = numerator / float(f * f);
        attraction = delta.normalize() * double(force);
    }
    velocity += attraction;
    if (world.polarity < 0 && double(float(attraction.length())) > 2e-5) {
        const double limit = world.random.range(.00002, .0002);
        if (limit < double(float(attraction.length())) && world.player &&
            world.particleBudget > 0) {
            world.add(Domain::Decor, new ShotSpark(world, *world.player, true));
            --world.particleBudget;
        }
    }
    position += velocity;
    position += world.scrollDelta;
    velocity.x += world.layerTilt * .0001;
    angle = heading(velocity);
    if (position.x < -.6 || position.x > .6 || position.y < -.6 || position.y > .6)
        dead = true;
}
void EnemyBullet::hitBy(Entity &other) {
    if (other.type == 0x2000) {
        velocity += other.velocity * (.02 * double(world.polarity));
        if (world.particleBudget > 0) {
            world.add(Domain::Decor, new ShotSpark(world, *this, false));
            --world.particleBudget;
        }
    } else {
        other.didHit(*this);
        dead = true;
    }
}
ShotSpark::ShotSpark(World &w, const Entity &source, bool blue) : GameEntity(w) {
    radius = .01f;
    frame = blue ? 28 : 29;
    paletteSlot = blue ? 8 : 1;
    colour = unpack(blue ? 0x0000ffff : 0xff0000ff);
    velocity = {0, w.random.range(.002, blue ? .016 : .006)};
    if (blue) {
        position = {0, .08};
        const double a = double(float(w.random.range(-3.141592654, 3.141592654)));
        velocity.rotateBy(a);
        position.rotateBy(a);
        position += source.position;
    } else {
        position = source.position;
        velocity.rotateBy(w.random.range(-3.141592654, 3.141592654));
    }
    fade = float(w.random.range(.05, .2));
    turn = float(w.random.range(-.1, .1));
}
void ShotSpark::run() {
    position += velocity;
    clock += fade * 4.f;
    if (clock >= 1) {
        clock = 1;
        dead = true;
    }
    const float left = 1.f - clock;
    angle += left * turn;
    if (paletteSlot > 0)
        colour = world.palette[size_t(paletteSlot)];
    colour.a = double(left);
    radius = float(double(left) * .009 + .0001);
}
} // namespace superoxwars
