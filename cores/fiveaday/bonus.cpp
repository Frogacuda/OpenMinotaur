#include "world.h"
#include "original_tables.h"
#include <algorithm>
#include <cstdio>

namespace fiveaday {
void World::message(int row, const std::string &text, int duration, uint32_t packed) {
    Message *slot = nullptr;
    for (auto &m : messages)
        if (!m.text.empty() && m.row == row) {
            slot = &m;
            break;
        }
    if (!slot)
        for (auto &m : messages)
            if (m.text.empty()) {
                slot = &m;
                break;
            }
    if (!slot)
        return; // Original ten-slot xmsgAt drops a message when full.
    *slot = {text, row, duration, unpack(packed)};
}
void World::advanceMessages() {
    for (auto &m : messages)
        if (!m.text.empty()) {
            if (m.ticks > 0) {
                --m.ticks;
                if (m.colour.a < .6)
                    m.colour.a += .02;
            }
            if (m.ticks < 1) {
                m.colour.a *= .99;
                if (m.colour.a < .01)
                    m.text.clear();
            }
        }
}
ProgressFruit::ProgressFruit(World &w) : Entity(w) {
    type = 5;
    frame = int(original::thefruit[size_t((w.level + 1) % 30)]);
    radius = .025f;
    const auto size = w.dimensions(frame);
    scale.y = size.y / size.x;
    position = {.4, -.6};
    velocity = {};
    for (auto *e = w.hud; e; e = e->next)
        if (auto *p = dynamic_cast<ProgressFruit *>(e)) {
            p->state = 1;
            p->slideTicks = 30;
        }
}
void ProgressFruit::run() {
    switch (state) {
    case 0:
        velocity.y += .0001;
        position.y += velocity.y;
        if (position.y > .42) {
            position.y = .42;
            state = 2;
            ++world.fruitCount;
            if (world.fruitCount > 4) {
                if (world.tilt)
                    world.achieveGoal(5);
                for (auto *e = world.hud; e; e = e->next)
                    if (auto *p = dynamic_cast<ProgressFruit *>(e)) {
                        p->state = 1;
                        p->slideTicks = 25;
                    }
            }
        }
        break;
    case 1:
        if (--slideTicks < 1)
            state = 2;
        position.x += double(slideStep);
        break;
    case 2:
        if (colour.a > .4)
            colour.a *= .997;
        if (world.fruitCount > 4) {
            if ((world.ticks & 32) == 0)
                colour.a = .2;
            else {
                if ((world.ticks & 31) == 0)
                    world.noise(67, 10);
                colour.a = 1;
            }
        }
        break;
    case 3:
        colour.a = 1;
        position.y *= .85;
        if (std::abs(position.y) < .01)
            state = 4;
        break;
    case 4:
        scale = scale * 1.1;
        colour.a -= .03;
        if (colour.a < .01) {
            colour.a = 0;
            dead = true;
        }
        break;
    case 5:
        position += velocity;
        velocity.y += .0001;
        angle += spin;
        spin = float(double(spin) * .95);
        if (position.x < -.6 || position.x > .6 || position.y > .6)
            dead = true;
        break;
    }
}
BonusFruit::BonusFruit(World &w) : Entity(w) {
    position.y = .6;
    velocity.x = w.random.range(-.003, .003);
    velocity.y = w.random.range(-.01, -.006);
    spin = float(w.random.range(-.2, .2));
    frame = w.bonusFrames.at(w.bonusCursor++);
    radius = .025f;
    const auto size = w.dimensions(frame);
    scale.y = size.y / size.x;
    world.noise(int(world.random.next() & 3) + 12);
}
void BonusFruit::run() {
    position += velocity;
    travel = float(double(travel) + velocity.length());
    velocity.y += .00004;
    if (position.x < -.6 || position.x > .6 || position.y > .6) {
        dead = true;
        if (frame == 32)
            world.noise(47);
        const int points = frame == 32 ? world.level * 100 + 1200 : world.level * 10 + 120;
        world.addScore(int(float(float(points) * world.scoreMultiplier)));
    }
    angle += spin;
    if (double(angle) > Pi)
        angle = float(double(angle) - Tau);
    if (double(angle) < -Pi)
        angle = float(double(angle) + Tau);
    if (frame == 32) {
        spin = float(double(spin) * .995);
        angle = float(double(angle) * .999);
    }
    if (double(travel) > .01) {
        travel = float(double(travel) - .01);
        if (double(std::abs(float(velocity.y))) <= .009) {
            auto *e = new Glow(world);
            e->position = position;
            e->frame = 18;
            e->radius = .02f;
            e->fade = .99f;
            e->angle = float(std::atan2(-velocity.y, velocity.x) - 1.570796327);
            if (double(e->angle) < -Pi)
                e->angle = float(double(e->angle) + Tau);
            e->colour.a = double(float(double(std::abs(float(velocity.y))) / -.009 + 1.)) * .3;
            world.add(world.effects, e);
        }
    }
}
namespace {
struct Dash : Entity {
    float travel = 0;
    using Entity::Entity;
    void run() override {
        position.x += velocity.x;
        travel = float(double(travel) + velocity.x);
        if (travel > .007f) {
            travel -= .007f;
            auto *e = new Glow(world);
            e->position = position;
            e->frame = 18;
            e->angle = 1.5707964f;
            e->radius = .02f;
            e->colour.a = .4;
            world.add(world.effects, e);
        }
        if (position.x > .6)
            dead = true;
    }
};
} // namespace
StageSun::StageSun(World &w) : Entity(w) {
    constexpr uint32_t colours[] = {0xffffffff, 0xffe080ff, 0xff8040ff,
                                    0xffc070ff, 0xc0c080ff, 0xffffffff};
    constexpr uint32_t flashes[] = {0xffff8aa0, 0xffc080a0, 0xff8020c0,
                                    0xf08040c0, 0xa0a0c080, 0xffff8070};
    constexpr float radii[] = {.1f, .1f, .15f, .3f, .2f, .1f};
    const int day = std::max(w.level, 0) % 30 % 5;
    position = {-.6, -.2};
    velocity = {0, .0008};
    velocity.rotate(2.0943951026666667);
    radius = .08f;
    scale.x = 1.2;
    colour = unpack(colours[day]);
    flashColour = flashes[day];
    flashRadius = radii[day];
    if (day == 3 || day == 5) {
        frame = 28;
        radius = .12f;
        position.x = -.65;
        if (w.level > 0 && (w.level + 1) % 5 == 4 && w.perfectDay)
            w.achieveGoal(4);
    } else
        frame = w.month == 3 ? 44 : 21;
}
void StageSun::run() {
    ++age;
    position += velocity;
    velocity.rotate(-.0007853981635);
    if (age & 1)
        world.flash(position, int(flashColour), flashRadius);
    if (position.x > -.3 && !sounded) {
        const auto r = world.random.next();
        if ((r & 255) < 2) {
            world.noise(34, 8);
            auto *e = new Dash(world);
            e->position = {-.6, world.random.range(-.3, .3)};
            e->velocity = {.004, 0};
            e->radius = .02f;
            e->scale.x = 1.3;
            e->frame = 108;
            world.add(world.effects, e);
        } else
            world.noise(34, 7);
        sounded = true;
    }
    if (position.x > .2 && world.bossLaunched && !released) {
        world.noise(30);
        world.bossLaunched = false;
        released = true;
    }
    if (position.x > .7)
        dead = true;
}
void World::beginBonus() {
    if (demo) {
        neon::entities::destroyList(fruits, [](Entity *e) { delete e; });
        newLevel();
        return;
    }
    ship.state = Ship::State::Bonus;
    bonusRemaining = ship.collected - 1;
    const auto phrase = random.next();
    const char *const *sentences;
    const char *const *names = original::thefruitNames.data();
    bonusFrames.clear();
    bonusCursor = 0;
    if (bonusRemaining < 1) {
        message(12, "FRUITLESS", 500);
        fruitSets = 0;
        if (fruitCount > 0) {
            noise(72);
            for (auto *e = hud; e; e = e->next)
                if (auto *p = dynamic_cast<ProgressFruit *>(e)) {
                    p->state = 5;
                    p->velocity.x = -p->position.x * random.range(.001, .002);
                    p->velocity.y = random.range(-.01, -.003);
                    p->spin = float(random.range(-.2, .2));
                }
        }
        fruitCount = 0;
        sentences = original::nothingSentences.data();
        names = original::thefruitPlurals.data();
    } else {
        message(12, "THE FRUITS OF YOUR", 500);
        message(14, "LABOURS", 500);
        int normal = 0, power = 0;
        for (auto *e = fruits; e; e = e->next)
            if (auto *f = dynamic_cast<Fruit *>(e); f && f->state == Fruit::State::Collected) {
                bonusFrames.push_back(f->frame);
                if (f->frame == 32)
                    ++power;
                else
                    ++normal;
            }
        ordinaryFruitTotal += normal;
        powerFruitTotal += power;
        if (powerFruitTotal > 20)
            achieveGoal(2);
        if (ordinaryFruitTotal > 200)
            achieveGoal(3);
        if (normal == 1)
            sentences = original::onefruitSentences.data();
        else if (normal < 1)
            sentences = original::nofruitSentences.data();
        else {
            sentences = original::thefruitSentences.data() + ((phrase & 4095) % 60) * 4;
            names = original::thefruitPlurals.data();
        }
        if (normal > 0) {
            add(hud, new ProgressFruit(*this));
            noiseGain(55, 8 * 0xbc0);
        }
    }
    for (int n = 0; n < 4; ++n)
        if (sentences[n]) {
            char text[128];
            std::snprintf(text, sizeof text, sentences[n], names[size_t((level + 1) % 30)]);
            message(18 + n * 2, text, 900);
        }
    bonusTicks = bonusInterval = 30;
    add(scenery, new StageSun(*this));
    noise(29);
}
void World::deliverFruitSet() {
    for (auto *e = hud; e; e = e->next)
        if (auto *p = dynamic_cast<ProgressFruit *>(e))
            p->state = 3;
    fruitCount = 0;
    noise(56);
    achieveGoal(1);
    if (++fruitSets > 4)
        achieveGoal(9);
    message(16, "FIVE A DAY!", 500);
    message(18, "FIVE A DAY!", 210);
    message(20, "FIVE A DAY!", 220);
    message(22, "OH I SAY!", 230);
    message(24, "YOU GOT FIVE A DAY!", 240);
    if (lives < 9) {
        ++lives;
        checkpointDay();
        message(27, "AN EXTRA LIFE IS YOU!", 500);
        message(29, "(SO HEALTHY!)", 250);
    } else {
        addScore(int(float(scoreMultiplier * 10000.f)));
        message(27, "HERE'S A BIG BONUS!", 500);
        message(29, "(SO DELICIOUS!)", 250);
    }
}
} // namespace fiveaday
