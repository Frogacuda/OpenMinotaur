#include "ground_features.h"
#include "effects.h"
#include "projectiles.h"
#include "original_tables.h"
#include <algorithm>
namespace superoxwars {
void Nose::run() {
    GroundEnemy::run();
    world.bossPresent = true;
}
void Nose::hitBy(Entity &other) {
    GroundEnemy::hitBy(other);
    if (dead) {
        world.noise(34, 0x2000);
        scatter(world, position, 40, world.polarity < 0 ? 3 : world.polarity > 0 ? 4 : 0);
    }
}
Horn::Horn(World &w, int tile) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = 56;
    radius = .1f;
    armour = 20;
    layerFrame = 0;
    gunState = -1;
    turnRate = 0;
    angle = 0;
    shotCount = 1;
    spread = 1.5707964f;
    gunMode = 0;
    gunChance = 128;
    scale = {tile % TileScreen::Stride < 11 ? -1. : 1., .7};
    spin = 0;
    colour = baseColour = {};
}
Ear::Ear(World &w, int tile) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = 55;
    radius = .08f;
    armour = 20;
    layerFrame = 0;
    gunState = -1;
    turnRate = 0;
    angle = 0;
    shotCount = 5;
    spread = .7853982f;
    burstCount = 4;
    gunMode = 0;
    gunChance = 25555;
    spin = 0;
    scale.x = tile % TileScreen::Stride < 11 ? -1.2 : 1.2;
    hingeStep = scale.x < 0 ? .1f : -.1f;
    colour = baseColour = {};
}
void Ear::run() {
    GroundEnemy::run();
    world.bossPresent = true;
    if (hingeState == 2) {
        angle -= hingeStep;
        if (std::abs(angle) <= std::abs(hingeStep)) {
            angle = 0;
            hingeState = 0;
        }
    } else if (hingeState == 1) {
        angle += hingeStep + hingeStep;
        if (std::abs(angle) > 1.5f) {
            hingeState = 2;
            emitEnemyShots(world, *this, 0, float(double(world.weaponThreat) * .0001 + .003),
                           shotCount, spread);
        }
    } else if (hingeState == 0) {
        if ((world.random.next() & 0x3f0) < 0x10)
            queued = true;
        if (queued) {
            hingeState = 1;
            queued = false;
        }
    }
}
void Ear::hitBy(Entity &other) {
    queued = true;
    GroundEnemy::hitBy(other);
    if (dead) {
        world.noise(37);
        scatter(world, position, 40, world.polarity < 0 ? 3 : world.polarity > 0 ? 4 : 0);
    }
    pulse = 1.4f;
}
void Ear::prepare(std::vector<ActorDraw> &out) {
    position = world.tiles.positions[size_t(cell)];
    active = true;
    muzzle = position;
    const size_t before = out.size();
    Entity::prepare(out);
    if (out.size() > before)
        out.back().pivot = {double(radius) * scale.x, 0};
}
Tooth::Tooth(World &w, int tile, bool inverted) : GroundEnemy(w, tile, 9) {
    armour = 20;
    if (inverted)
        scale.y = -scale.y;
}
void Tooth::run() {
    world.enemyBulletOverride = true;
    world.enemyBulletFrame = 57;
    world.enemyBulletPalette = 0;
    GroundEnemy::run();
    world.enemyBulletOverride = false;
    world.bossPresent = true;
    colour = sequenceColour(1.f - float(armour) / 20.f, original::ToothColours.data());
}
Flower::Flower(World &w, int tile) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = 58;
    armour = 0;
    gunState = -1;
    turnRate = spin = 0;
    layerFrame = 0;
    angle = float(w.random.range(-3.141592654, 3.141592654));
    radius = float(w.random.range(.03, .04));
    colour = baseColour = w.flowerColour;
}
void Flower::hitBy(Entity &other) {
    other.didHit(*this);
    dead = true;
    flowerSparks(world, *this);
    const auto chance = world.random.next();
    if ((chance & 0xc0) < 0x40 && active && world.player &&
        (world.player->position - position).length() > .3)
        world.add(Domain::Enemies, new Butterfly(world, *this));
}
Fox::Fox(World &w, int tile) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = 60;
    radius = .1f;
    scale.x = .6;
    armour = 0;
    gunState = -1;
    spin = turnRate = 0;
    layerFrame = 0;
    angle = 0;
    colour = baseColour = {};
}
void Horn::run() {
    GroundEnemy::run();
    world.bossPresent = true;
    if ((world.ticks & 0x60) == 0x60) {
        world.enemyBulletOverride = true;
        world.enemyBulletFrame = 57;
        world.enemyBulletPalette = 0;
        emitEnemyShots(world, *this, 0, float(double(world.weaponThreat) * .0001 + .003), shotCount,
                       spread);
        world.enemyBulletOverride = false;
    }
}
void Horn::hitBy(Entity &other) {
    GroundEnemy::hitBy(other);
    if (dead) {
        const auto a = world.random.next(), b = world.random.next();
        world.noise(original::HornSounds[size_t((a & 3) + (b & 1))]);
        world.noise(34, 0x2000);
        scatter(world, position, 40, world.polarity < 0 ? 3 : world.polarity > 0 ? 4 : 0);
    }
    pulse = 1.4f;
}
Fence::Fence(World &w, int tile, int variant) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = layerFrame = 54;
    scale.x = .5;
    radius = .03f;
    armour = 8;
    layerOffset = {.05, .03};
    layerScale = 1;
    layerPalette = 4;
    gunState = -1;
    spin = turnRate = 0;
    angle = variant < 0 ? 1.5707964f : 0;
    colour = baseColour = {};
    if (w.area == 5) {
        frame = layerFrame = 61;
        layerPalette = 0xc00040;
        layerAlpha = 1;
        armour = 8;
        radius = .035f;
        material = 0x00ff00ff;
        colour = baseColour = unpack(material);
        layerOffset = {.02, .012};
    }
}
void Fence::run() {
    GroundEnemy::run();
    colour = material < 16 ? world.palette[3] : unpack(material);
}
void Fence::hitBy(Entity &other) {
    if (other.type == 0x2002)
        return;
    other.didHit(*this);
    world.add(Domain::Decor, new Explosion(world, other.position, .04f, .1f, 1.02f));
}
Mine::Mine(World &w, int tile, int row)
    : GroundEnemy(w, tile, 0), column(tile % TileScreen::Stride), scriptRow(row) {
    archetype = -1;
    type = 0x3002;
    frame = 51;
    radius = .04f;
    armour = 0;
    reward = 0;
    deathSound = 31;
    gunState = -1;
    spin = turnRate = 0;
    layerFrame = 0;
    angle = 0;
    colour = baseColour = {};
}
void Mine::run() {
    GroundEnemy::run();
    if (countdown > 0 && --countdown < 1) {
        detonate();
        world.add(Domain::LaunchPods, new Crater(world, cell, radius * .6f));
    }
}
void Mine::hitBy(Entity &other) {
    GroundEnemy::hitBy(other);
    if (dead)
        detonate();
}
void Mine::detonate() {
    // mine::blow: logical script row/column, not circular-cache proximity.
    world.add(Domain::Decor, new Explosion(world, position, .08f, .04f, 1.02f));
    type = -1;
    dead = true;
    for (auto *e = world.lists[size_t(Domain::Foreground)]; e; e = e->next)
        if (auto *m = dynamic_cast<Mine *>(e); m && m->type == 0x3002 && m->countdown == 0 &&
                                               std::abs(column - m->column) < 2 &&
                                               std::abs(scriptRow - m->scriptRow) < 2)
            m->countdown = 10;
    const int count = std::min(world.level + world.weaponFollowers * 4 + 4, 32);
    float phase = 0;
    const float step = float(6.2831855 / double(float(count)));
    for (int n = 0; n < count; ++n) {
        Vec2 v{0, double(-.004f)};
        v.rotateBy(double(phase));
        phase += step;
        world.add(Domain::Pickups, new EnemyBullet(world, position, v));
    }
}
Pyramid::Pyramid(World &w, int tile) : GroundEnemy(w, tile, 0) {
    archetype = -1;
    frame = 52;
    radius = .04f;
    armour = 0;
    gunState = -1;
    spin = turnRate = 0;
    layerFrame = 0;
    angle = 0;
    colour = baseColour = {};
    missiles = std::min(w.level * 4 + 10, 40);
}
void Pyramid::run() {
    GroundEnemy::run();
    if (activated && --countdown < 1) {
        countdown = period;
        if (missiles > 0) {
            --missiles;
            if (spawnPhase == 0)
                world.add(Domain::Enemies, new HomingMissile(world, position));
        }
    }
}
void Pyramid::hitBy(Entity &other) {
    if (!active || activated)
        return;
    other.didHit(*this);
    activated = true;
    world.add(Domain::Decor, new Explosion(world, position, .08f, .04f, 1.06f));
    world.noise(38);
}
void Pyramid::prepare(std::vector<ActorDraw> &out) {
    position = world.tiles.positions[size_t(cell)];
    active = true;
    Entity::prepare(out);
    if (activated) {
        const auto savedScale = scale;
        const auto savedColour = colour;
        const int savedFrame = frame;
        scale = scale * .5;
        colour = world.palette[2];
        frame = 53;
        Entity::prepare(out);
        scale = savedScale;
        colour = savedColour;
        frame = savedFrame;
    }
}
} // namespace superoxwars
