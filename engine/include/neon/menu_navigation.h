// Shared spatial menu focus and tick-based repeat. Modules supply their menu
// geometry and activation IDs; this layer never invokes gameplay or touches.
#pragma once
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>
namespace neon::input {
struct MenuItem {
    int label = 0;
    double cx = 0, cy = 0;                            // centre (screen columns / rows)
    int tx = 0, ty = 0;                               // its cell nearest the centre
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1; // bounding box
};

inline const MenuItem *findButton(const std::vector<MenuItem> &v, int label) {
    for (const MenuItem &b : v)
        if (b.label == label)
            return &b;
    return nullptr;
}

inline int moveSelection(const std::vector<MenuItem> &v, int from, int dx, int dy) {
    const MenuItem *cur = findButton(v, from);
    if (!cur)
        return from;
    auto gap = [](int a0, int a1, int b0, int b1) {
        return std::max(0, std::max(b0 - a1, a0 - b1));
    };
    double best = 1e9;
    int pick = from;
    for (const MenuItem &b : v) {
        if (b.label == from)
            continue;
        double along = (b.cx - cur->cx) * dx + (b.cy - cur->cy) * dy;
        if (along < 0.5)
            continue;
        double alongGap =
            dx ? gap(cur->x0, cur->x1, b.x0, b.x1) : gap(cur->y0, cur->y1, b.y0, b.y1);
        double perpGap = dx ? gap(cur->y0, cur->y1, b.y0, b.y1) : gap(cur->x0, cur->x1, b.x0, b.x1);
        double score = alongGap + 3.0 * perpGap + 0.01 * along;
        if (score < best) {
            best = score;
            pick = b.label;
        }
    }
    return pick;
}

struct PadMenu {
    int sel = 0;
    int screen = -1; // game state the selection belongs to
    int moveHeld = 0;

    // Updates the selection; returns the label to press (0 = none).
    // `preferred`: default buttons for a new screen, in order;
    // `back`: what B presses, first one present.
    int update(const std::vector<MenuItem> &buttons, int state, float sx, float sy, bool a, bool b,
               int startLabel, std::initializer_list<int> preferred,
               std::initializer_list<int> back) {
        if (buttons.empty())
            return 0;
        if (state != screen) {
            screen = state;
            sel = 0;
        }
        if (!findButton(buttons, sel)) {
            sel = buttons.front().label;
            for (int p : preferred)
                if (findButton(buttons, p)) {
                    sel = p;
                    break;
                }
        }
        int dx = 0, dy = 0;
        if (std::fabs(sx) > 0.5f || std::fabs(sy) > 0.5f) {
            if (std::fabs(sx) > std::fabs(sy))
                dx = sx > 0 ? 1 : -1;
            else
                dy = sy > 0 ? 1 : -1;
        }
        if (dx || dy) {
            if (moveHeld == 0 || (moveHeld >= 20 && (moveHeld - 20) % 8 == 0))
                sel = moveSelection(buttons, sel, dx, dy);
            moveHeld++;
        } else {
            moveHeld = 0;
        }
        int press = 0;
        if (a)
            press = sel;
        if (b)
            for (int l : back)
                if (findButton(buttons, l)) {
                    press = l;
                    break;
                }
        if (startLabel && findButton(buttons, startLabel))
            press = startLabel;
        return findButton(buttons, press) ? press : 0;
    }
};
} // namespace neon::input
