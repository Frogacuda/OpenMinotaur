// Pure geometry, shared by drawing, hit testing and directional navigation.
#pragma once
#include <array>
#include <cmath>
#include <limits>

namespace mino {

struct MenuRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    float cx() const {
        return x + w * 0.5f;
    }
    float cy() const {
        return y + h * 0.5f;
    }
};

struct LauncherLayout {
    static constexpr float statusBarHeight = 22;
    bool portrait;
    float width, height;
    MenuRect dock;
    std::array<MenuRect, 12> items; // Nine games, Settings, Achievements, About.

    explicit LauncherLayout(bool vertical)
        : portrait(vertical), width(vertical ? 480.0f : 640.0f),
          height(vertical ? 640.0f : 480.0f) {
        constexpr float top = statusBarHeight, footer = 28, dockSize = 124;
        float gridW = portrait ? width : width - dockSize;
        float gridH = height - top - footer - (portrait ? dockSize : 0);
        for (int i = 0; i < 9; ++i) {
            float cx = (float(i % 3) + 0.5f) * gridW / 3;
            float cy = top + (float(i / 3) + 0.5f) * gridH / 3;
            items[size_t(i)] = {cx - 65, cy - 62, 130, 124};
        }
        if (portrait) {
            dock = {0, height - footer - dockSize, width, dockSize};
            for (int i = 0; i < 3; ++i)
                items[size_t(9 + i)] = {width * (i + .5f) / 3 - 65, dock.y, 130, dockSize};
        } else {
            dock = {width - dockSize, top, dockSize, height - top - footer};
            for (int i = 0; i < 3; ++i)
                items[size_t(9 + i)] = {dock.x, dock.y + dock.h * (i + .5f) / 3 - 62, dockSize,
                                        124};
        }
    }

    int hit(float x, float y) const {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].contains(x, y))
                return int(i);
        return -1;
    }

    int move(int from, int dx, int dy) const {
        if (from < 0 || from >= int(items.size()))
            return 0;
        const auto &a = items[size_t(from)];
        int best = from;
        float score = std::numeric_limits<float>::max();
        for (size_t i = 0; i < items.size(); ++i) {
            float vx = items[i].cx() - a.cx(), vy = items[i].cy() - a.cy();
            float along = vx * dx + vy * dy;
            if (along <= 1)
                continue;
            float candidate = along + 3 * std::abs(vx * dy - vy * dx);
            if (candidate < score) {
                score = candidate;
                best = int(i);
            }
        }
        return best;
    }
};

} // namespace mino
