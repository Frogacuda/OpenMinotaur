#pragma once
namespace neon::notices {
// Early floating scores publish the OLD opacity, then advance their life.
// The owner supplies its palette copy, pause gate and birth/list timing.
// Retirement is strict negative, without clamping. GU2's pre-move fading and
// Five a Day's delayed multiplicative popup fade are separate source behaviors.
template <class Point, class Publish>
void advanceFloating(Point &position, const Point &velocity, float &opacity, float delta,
                     bool &dead, Publish publish) {
    position = {position.x + velocity.x, position.y + velocity.y};
    publish(double(opacity));
    opacity = opacity + delta;
    if (opacity < 0.f)
        dead = true;
}
// Minos/GoatUp centring uses float(count), then this precise double operation
// order. Glyph dimensions, spacing factors and sprite bases stay with callers.
template <class Point> Point centredOrigin(const Point &position, const Point &advance, int count) {
    return {position.x + advance.x * double(float(count)) * -.5 + advance.x * .5,
            position.y + advance.y * -.5};
}
} // namespace neon::notices
