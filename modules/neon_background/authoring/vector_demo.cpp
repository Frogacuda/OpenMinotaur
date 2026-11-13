#include "vector/vector_scene.h"
#include <cmath>
namespace neon::vectors {
namespace {
constexpr float pi = 3.14159265358979323846f;
}
void demo(Canvas &c, int scene, double seconds) {
    c.clear();
    float t = float(seconds);
    Colour cyan{.15f, .8f, 1}, green{.25f, 1, .4f}, amber{1, .55f, .12f};
    Object ship;
    ship.strokes = {{{0, -12}, {10, 10}, cyan, 1, 1},
                    {{10, 10}, {0, 5}, cyan, 1, 1},
                    {{0, 5}, {-10, 10}, cyan, 1, 1},
                    {{-10, 10}, {0, -12}, cyan, 1, 1}};
    if (scene == 0) {
        c.text({24, 22}, "MINOTAUR PROJECT", 14, cyan);
        c.text({24, 49}, "SCORE 00123450", 10, green, .9f);
        c.text({492, 24}, "LIVES", 10, amber, .9f);
        for (int i = 0; i < 3; i++) {
            ship.transform.position = {510.f + i * 38, 58};
            ship.transform.scale = {.65f, .65f};
            c.object(ship);
        }
        for (Point p : std::vector<Point>{{18, 16}, {622, 16}, {18, 464}, {622, 464}}) {
            float x = p.x < 320 ? 1.f : -1.f, y = p.y < 240 ? 1.f : -1.f;
            c.line(p, {p.x + x * 22, p.y}, cyan, .5f);
            c.line(p, {p.x, p.y + y * 22}, cyan, .5f);
        }
        c.arc({320, 240}, 18, 0, 2 * pi, cyan, .6f);
        for (int k = 0; k < 4; k++) {
            float a = k * pi * .5f;
            c.line({320 + std::cos(a) * 24, 240 + std::sin(a) * 24},
                   {320 + std::cos(a) * 32, 240 + std::sin(a) * 32}, cyan, .9f);
        }
        c.text({24, 410}, "ENERGY", 10, green, .7f);
        for (int i = 0; i < 12; i++) {
            float x = 24.f + i * 14;
            c.path({{x, 434}, {x + 10, 434}, {x + 10, 446}, {x, 446}}, true, green,
                   i < 9 ? 1.f : .18f);
        }
        c.arc({550, 390}, 49, 0, 2 * pi, green, .5f);
        c.arc({550, 390}, 25, 0, 2 * pi, green, .25f);
        float angle = t * .9f;
        c.line({550, 390}, {550 + std::cos(angle) * 46, 390 + std::sin(angle) * 46}, green, 1.3f);
        c.dot({568, 367}, green, 2, 1.8f);
        c.text({510, 450}, "TARGET", 9, green, .7f);
    } else if (scene == 1) {
        c.text({40, 30}, "VECTOR OBJECTS", 18, cyan);
        for (int i = 0; i < 8; i++) {
            float angle = t * .3f + i * pi * .25f;
            ship.transform.position = {320 + 150 * std::cos(angle), 250 + 105 * std::sin(angle)};
            ship.transform.rotation = angle + pi * .5f;
            ship.transform.scale = {1.5f, 1.5f};
            c.object(ship);
        }
        c.arc({320, 250}, 45, 0, 2 * pi, amber, 1);
        c.arc({320, 250}, 58, t, -pi * 1.4f, amber, .6f);
        c.text({110, 420}, "PATHS  DOTS  TEXT  TRANSFORMS", 11, green);
    } else {
        c.text({40, 24}, "BEAM RESPONSE", 18, cyan);
        for (int row = 0; row < 9; row++) {
            float y = 78.f + row * 39;
            float energy = .12f + row * .22f;
            c.line({145, y}, {540, y}, cyan, energy);
            c.dot({580, y}, cyan, energy, 1.2f);
            c.text({35, y - 5}, std::to_string(row + 1), 10, green, .8f);
        }
        c.text({40, 452}, "THIN CORE  HALO  ROUND CAPS", 11, amber, .8f);
    }
}
} // namespace neon::vectors
