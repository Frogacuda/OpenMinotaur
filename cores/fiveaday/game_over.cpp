#include "world.h"
#include "original_tables.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace fiveaday {
void World::checkpointDay() {
    if (!demo && level > 0 && (level + 1) % 5 == 0 && dayCheckpoint)
        dayCheckpoint((level + 1) / 5, lives, score);
}
GameOverFruit::GameOverFruit(World &w, Vec2 at, int image, int wait)
    : Entity(w), target(at), delay(wait) {
    frame = image;
    radius = .02f;
    const auto size = w.dimensions(frame);
    scale.y = size.y / size.x;
    spin = float(w.random.range(-.2, .2));
    position = at;
    position.y = .6;
    velocity.y = w.random.range(-.015, -.013);
    velocity.x = w.random.range(-.003, .003);
    position.x += velocity.x * -100.;
}
void GameOverFruit::run() {
    angle += spin;
    if (world.demo && state != 1) {
        velocity.y = world.random.range(-.02, -.01);
        velocity.x = world.random.range(-.003, .003);
        state = 1;
    }
    if (state == 0) {
        if (delay > 0) {
            --delay;
            return;
        }
        position += velocity;
        velocity.y += .0001;
        if (std::abs(position.x - target.x) <= std::abs(velocity.x)) {
            position.x = target.x;
            velocity.x = 0;
        }
        if (velocity.y > 0 && position.y >= target.y) {
            position.y = target.y;
            velocity.y = 0;
        }
        if (velocity.x == 0 && velocity.y == 0) {
            state = 999;
            world.letterFeedbackPending = true;
        }
    } else if (state == 1) {
        position += velocity;
        velocity.y += .0004;
        if (position.x < -.6 || position.x > .6 || position.y > .6) {
            dead = true;
            world.letterFeedbackPending = true;
            world.checkLifetimeGoals();
        }
    }
}
void World::checkLifetimeGoals() {
    // Original goFruit retirement thresholds. The core also calls this after
    // banking a real run: the original demo-only retirement path cannot award.
    if (lifetimeFruit > 10000)
        achieveGoal(7);
    if (lifetimeMinotaurs > 1000)
        achieveGoal(8);
}
void World::goGameOver() {
    if (gameOver || demo)
        return;
    const int first = int((random.next() & 511) % 30), step = int(random.next() & 7) + 1;
    float y = -.2f;
    for (int row = 0; row < 7; ++row) {
        float x = -.45f;
        const char *map = original::gomap[size_t(row)];
        for (size_t col = 0; col < std::strlen(map) && col < 50; ++col) {
            const unsigned index = unsigned(map[col] - '1');
            if (index < 8)
                add(hud, new GameOverFruit(
                             *this, {double(x), double(y)},
                             int(original::thefruit[size_t((first + int(index) * step) % 30)]),
                             row * 50 + int(col) * 4));
            x += .018f;
        }
        y += .02857142873108387f;
    }
    gameOver = true;
    sequencePaused = true;
    gameOverFade = 0;
    gameOverTicks = 0;
    finalSky = sky;
    finalCloud = cloudColour;
    month = 0;
    ticks = 0;
    char text[64];
    std::snprintf(text, sizeof text, "FRUIT GOT: %d", ordinaryFruitTotal);
    message(18, text, 1900);
    std::snprintf(text, sizeof text, "MINOTAURS GOT: %d", powerFruitTotal);
    message(20, text, 1900);
}
} // namespace fiveaday
