#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "neon/integer.h"
namespace neon::entities {
using neon::integers::add32;
// Source integration order and binary32 half extents. Wrapping performs one
// positive and one negative test per axis, never modulo/repeated wrapping.
// noWrap preserves the previous wrapped flag. Defaults belong to the entity.
template <class Point>
void integrate(Point &position, Point &velocity, const Point &damping, int32_t &age, bool noWrap,
               bool &wrapped, const Point &extent) {
    position.x += velocity.x;
    position.y += velocity.y;
    velocity.x *= damping.x;
    velocity.y *= damping.y;
    age = add32(age, 1);
    if (noWrap)
        return;
    wrapped = false;
    const float halfX = float(extent.x * .5), halfY = float(extent.y * .5);
    if (position.x > double(halfX)) {
        position.x -= extent.x;
        wrapped = true;
    }
    if (position.y > double(halfY)) {
        position.y -= extent.y;
        wrapped = true;
    }
    if (position.x < double(-halfX)) {
        position.x += extent.x;
        wrapped = true;
    }
    if (position.y < double(-halfY)) {
        position.y += extent.y;
        wrapped = true;
    }
}
// The caller supplies component order, retaining the original expression and
// the double sqrt followed by one binary32 rounding at the circle boundary.
inline float roundedDistance(double first, double second) {
    return float(std::sqrt(first * first + second * second));
}
} // namespace neon::entities
