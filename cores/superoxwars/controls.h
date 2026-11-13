#pragma once
#include "world.h"
namespace superoxwars {
// Modern controller adaptation. Output is displayed-screen displacement per
// simulation tick; touch keeps its separate original relative-motion path.
struct AnalogProfile {
    float gain = 1;
    int curve = 0;
    Vec2 apply(Vec2 stick) const;
    const char *curveName() const;
};
struct AnalogControls {
    AnalogProfile normal{1.2f, 1}, focus{.65f, 2};
    static double worldInfluence(float rightX);
};
} // namespace superoxwars
