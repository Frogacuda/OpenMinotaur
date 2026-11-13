#include "world.h"
#include <cstdio>
namespace fiveaday {
ScoreText::ScoreText(World &w) : Entity(w) {
    position = {0, -.3};
    radius = .05f;
    scale = {1, 1.4};
    // Source stores offsets and advance derived from the binary32 sprite radius.
    const float extent = radius + radius;
    advance = float(double(extent) * 1.1);
    offset = {double(float(-float(columns) * advance * .5f)), double(extent) * scale.y * -.5};
}
void ScoreText::run() {
    if (popup) {
        position += world.camera;
        if (--remaining < 1)
            colour.a *= .95;
        if (colour.a < .01)
            dead = true;
        return;
    }
    if (lastScore != world.score) {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%8.2d", world.score);
        text = buffer;
        lastScore = world.score;
        colour.a = .76;
        return;
    }
    if (colour.a > .3) {
        colour.a *= .95;
        if (colour.a < .3)
            colour.a = .3;
    }
}
void ScoreText::prepare(std::vector<ActorDraw> &out) {
    auto at = position + offset;
    for (int n = 0; n < columns && n < int(text.size()); ++n) {
        const char digit = text[size_t(n)];
        if (digit >= '0' && digit <= '9')
            out.push_back({at, scale, radius, angle, digit - '0' + 8, colour});
        at.x += double(advance);
    }
}
void World::popupScore(Vec2 position, int points) {
    auto *e = new ScoreText(*this);
    e->popup = true;
    e->position = position;
    e->radius = .02f;
    e->columns = 4;
    e->remaining = 50;
    const float extent = e->radius + e->radius;
    e->advance = float(double(extent) * 1.1);
    e->offset = {double(float(-4.f * e->advance * .5f)), double(extent) * 1.4 * -.5};
    e->colour = {1, 0, 1, 1};
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%4.2d", points);
    e->text = buffer;
    add(effects, e);
}
} // namespace fiveaday
