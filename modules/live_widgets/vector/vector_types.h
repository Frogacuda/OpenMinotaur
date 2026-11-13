#pragma once
#include <string>
#include <vector>
namespace neon::vectors {
struct Point {
    float x = 0, y = 0;
};
struct Colour {
    float r = 1, g = 1, b = 1;
};
struct Transform {
    Point position{};
    Point scale{1, 1};
    float rotation = 0;
    Point apply(Point) const;
};
struct Stroke {
    Point a, b;
    Colour colour;
    float energy = 1, width = 1;
};
struct Object {
    std::vector<Stroke> strokes;
    Transform transform;
    float opacity = 1;
    std::string name;
};
} // namespace neon::vectors
