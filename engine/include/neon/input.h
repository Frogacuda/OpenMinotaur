// Device-neutral input contract. SDL belongs to the platform adapter only.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace neon::input {
inline constexpr int MaxControllers = 4;
enum Button : uint32_t {
    South = 1u << 0,
    East = 1u << 1,
    West = 1u << 2,
    North = 1u << 3,
    LeftShoulder = 1u << 4,
    RightShoulder = 1u << 5,
    Start = 1u << 6,
    Back = 1u << 7,
    Up = 1u << 8,
    Down = 1u << 9,
    Left = 1u << 10,
    Right = 1u << 11,
    LeftStick = 1u << 12,
    RightStick = 1u << 13
};
struct ControllerState {
    bool connected = false;
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    uint32_t buttons = 0, pressed = 0, released = 0;
};
enum class TouchPhase { Down, Move, Up, Cancel };
// Transport coordinates are native surface pixels, origin top-left, unclamped.
// The platform maps the device; each module converts to its own world/gesture
// coordinates. Identity stays independent of controller/player ownership.
struct TouchEvent {
    TouchPhase phase;
    uint32_t id;
    float x, y;
};
struct Axis {
    float x = 0, y = 0;
};
// Platform has already applied the user's radial dead zone. Modules select
// their response/quantization; do not apply another shared response curve.
inline Axis movement(const ControllerState &p) {
    if (!p.connected)
        return {};
    Axis a{p.lx, p.ly};
    if (p.buttons & Left)
        a.x = -1;
    if (p.buttons & Right)
        a.x = 1;
    if (p.buttons & Up)
        a.y = -1;
    if (p.buttons & Down)
        a.y = 1;
    return a;
}
struct MenuBindings {
    uint32_t confirm = South, cancel = East | Back, pause = Start;
    uint32_t advance = South | East | West | North | Start;
};
struct MenuButtons {
    bool confirm = false, cancel = false, pause = false, advance = false;
};
inline MenuButtons menuButtons(uint32_t pressed, const MenuBindings &b = {}) {
    return {bool(pressed & b.confirm), bool(pressed & b.cancel), bool(pressed & b.pause),
            bool(pressed & b.advance)};
}
// Activation IDs are defined by the module's menus. Touch hit-testing and
// controller focus select the same IDs; activation never creates a touch.
struct MenuActions {
    int activateLabel = 0;
    bool advance = false, back = false, controller = false;
    float scroll = 0;
    // Optional UI feedback location, independent of touch slots/gestures.
    bool feedback = false;
    double feedbackX = 0, feedbackY = 0;
};
struct DigitalDirections {
    bool left = false, right = false, up = false, down = false;
    bool any() const {
        return left || right || up || down;
    }
};
struct PlayerMotion {
    bool active = false;
    double x = 0, y = 0;
};
// Independent analog world command; opposing triggers cancel proportionally.
inline float worldInfluenceAxis(const ControllerState &p) {
    if (!p.connected)
        return 0;
    const auto finite = [](float x) { return std::isfinite(x) ? x : 0.f; };
    const float triggers = std::clamp(finite(p.rt), 0.f, 1.f) - std::clamp(finite(p.lt), 0.f, 1.f);
    return std::clamp(finite(p.rx) + triggers, -1.f, 1.f);
}
// Shared text-window operation. The module owns page content/cursor and speed.
template <class Grid, class Line>
void scrollText(Grid *g, int &nextLine, int end, float axis, double speed, Line line) {
    const int visible = g->rowsN + 1;
    if (std::fabs(axis) < .2f)
        axis = 0;
    const double offset = std::clamp(double(nextLine - visible) + g->scrollY + axis * speed, 0.,
                                     double(std::max(0, end - visible)));
    const int top = int(std::floor(offset));
    g->rowOffset = 0;
    g->scrollY = offset - top;
    for (int row = 0; row < visible; ++row) {
        g->cll(row, 0x20);
        g->printAt(0, row, line(top + row));
    }
    nextLine = top + visible;
    g->scrollCount[2] = g->scrollCount[3] = 0;
}
} // namespace neon::input
