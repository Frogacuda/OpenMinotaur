#include "head_entry.h"
#include <cmath>
#include <stdexcept>
namespace gr::reconstruction {
void stepHeadEntry(HeadEntry &h) {
    if (h.type != 2 || h.mode != 0 || h.age < 0 || h.age >= 28 || h.opacity < 0 ||
        h.opacity > 1.01f || h.timeScale != 1 || !std::isfinite(h.x) || !std::isfinite(h.y) ||
        h.y >= h.shipY || std::abs(h.x) > .6 || std::abs(h.y) > .6)
        throw std::invalid_argument("Outside captured immature-head entry slice");
    ++h.age;
    h.sprite = (int32_t((h.x + .5) * 40) & 3) + h.baseSprite;
    // Velocity fields are double, but angle and speed are stored as floats.
    // In particular float(-pi/2) has a small nonzero cosine.
    const double a = h.angle, s = h.speed;
    h.vx = std::sin(a) * s;
    h.vy = std::cos(a) * s;
    h.x += h.vx * double(h.timeScale);
    h.y += h.vy * double(h.timeScale);
    if (h.opacity < 1)
        h.opacity += .05f; // preserve the original float overshoot
    if (h.opacity >= 1 && ((h.vx > 0 && h.x > .41) || (h.vx < 0 && h.x < -.37)))
        throw std::invalid_argument("Turn requires a separate captured slice");
    ++h.frame;
}
} // namespace gr::reconstruction
