#include "world.h"
#include "original_tables.h"
#include <algorithm>
namespace fiveaday {
Miner::Miner(World &w, const Launch &p) : Attacker(w) {
    constexpr int frames[] = {64, 64, 68, 47, 82, 52};
    constexpr uint32_t colours[] = {0xffffc07f, 0xffff207f, 0xffffffff,
                                    0x404040ff, 0xffffffff, 0xffffffff};
    world.noise(66);
    position = p.position;
    init(p);
    carrier = false;
    scale.x = 1.2;
    reward = 25;
    formationLocked = false;
    speed = float(double(speed) * (double(w.week) * .5 + .5 + double(w.year) * .3));
    shotChance = int(w.year) * 20 + int(w.month) * 10;
    frame = frames[int(w.month) % 6];
    colour = unpack(colours[int(w.month) % 6]);
    radius = .04f;
    mineSpacing = float(double(w.week) * -.06 + .1);
    spawnRange = float(double(w.week) * .2 + .4);
    baseTurn = turnStep;
    spinOffset = 0;
    heading = p.heading;
    if (frame == 47) {
        scale.x = 1.5;
        mineSpacing = .04f;
        mineStyle = int(world.random.next() & 1);
    } else if (frame == 52)
        mineSpacing = .085f;
    else if (frame == 64)
        radius = .05f;
}
void Miner::run() {
    Attacker::run();
    const float drawnAngle = angle;
    if (frame == 47)
        angle = 0;
    else if (frame == 64)
        spinOffset += .03f;
    if (killedNow) {
        world.noise(65, 13);
        killedNow = false;
        return;
    }
    const float at = float(position.length());
    if (at >= spawnRange)
        return;
    mineTravel = float(double(mineTravel) + velocity.length());
    if (mineSpacing < mineTravel) {
        mineTravel -= mineSpacing;
        auto *mine = new Mine(world, *this);
        if (frame == 47) {
            mine->spinOffset = drawnAngle + 1.5707964f;
            mine->frame += mineStyle;
            if (mineStyle)
                mine->scale.y = .7;
            mine->health = 2;
        }
        if (double(mineSpacing) >= .08) {
            mine->spinOffset = float(double(angle) + 1.570796327);
            mine->health = 1;
        }
        world.noise(int(world.random.next() & 3) + 61, 13);
        world.add(world.enemies, mine);
    }
    const float factor = std::max(0.f, float(.5f - at));
    turnStep = baseTurn + float(float(factor * baseTurn) + float(factor * baseTurn));
}
Mine::Mine(World &w, const Entity &parent) : Wrapped(w) {
    constexpr int frames[] = {65, 65, 69, 71, 83, 4}, life[] = {200, 300, 300, 400, 500, 600};
    constexpr uint32_t colours[] = {0xffc0ff7f, 0xffc0ff7f, 0xffffffff,
                                    0x202020ff, 0xffffffff, 0xffffffff};
    const int era = int(w.month) % 6;
    position = retained = parent.position;
    type = 2;
    frame = frames[era];
    colour = unpack(colours[era]);
    radius = targetRadius = .03f;
    culled = true;
    cullMargin = 2;
    remaining = life[era];
    spin = float(w.random.range(-.04, .04));
    if (frame == 4)
        frame = int(original::thefruit[size_t(int(double(w.week) * 5.) % 30)]);
}
void Mine::run() {
    Wrapped::run();
    if ((wrapped & 15) || --remaining < 0)
        dead = true;
    colour = world.palette(frame == 46 ? 5 : 1);
    if (frame == 46)
        angle = -.7853982f;
    if (remaining < 50)
        colour.a = (remaining & 4) ? 0 : 1;
    switch (frame) {
    case 65:
        colour = unpack(0xffc0ff7f);
        spinOffset += spin;
        break;
    case 67:
        colour = unpack(0xffe000ff);
        spinOffset += spin;
        break;
    case 69:
        colour = {1, 1, 1, 1};
        spinOffset += spin;
        break;
    case 71:
        colour = world.palette(5);
        break;
    case 72:
        colour = world.palette(1);
        break;
    default:
        colour = {1, 1, 1, 1};
        spinOffset += spin;
        break;
    }
}
void Mine::hitBy(Entity &other) {
    other.didHit(*this);
    if (--health < 1) {
        world.explode(position, .02f, .02f, .006f);
        dead = true;
        world.noise(59);
    } else
        radius = targetRadius * 1.5f;
}
} // namespace fiveaday
