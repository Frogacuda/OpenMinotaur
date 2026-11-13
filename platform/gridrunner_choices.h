// Explicit launch choices. Neither selection nor availability depends on the
// window orientation. Classic must have its own registered native game path.
#pragma once
#include "launcher_layout.h"
namespace mino {
struct GridrunnerChoice {
    const char *id;
    const char *title;
    const char *description;
};
inline constexpr GridrunnerChoice kGridrunnerChoices[] = {
    {"gridrunner", "Gridrunner",
     "Minotaur Project's reimagined Gridrunner for iOS, inspired by classic arcade machines."},
    {"gridrunner-classic", "Gridrunner Classic",
     "Old-school gameplay, inspired by the Commodore VIC-20 original."}};
struct GridrunnerChoiceLayout {
    MenuRect panel, header, back;
    static constexpr int count = int(sizeof(kGridrunnerChoices) / sizeof(*kGridrunnerChoices));
    std::array<MenuRect, count> rows, previews;
    explicit GridrunnerChoiceLayout(bool portrait) {
        const float width = portrait ? 480.f : 640.f;
        const float top = portrait ? 86.f : 64.f, rowHeight = portrait ? 160.f : 110.f;
        panel = {20, top, width - 40, 48 + count * rowHeight + 12};
        header = {panel.x, panel.y, panel.w, 48};
        back = {panel.x + 8, panel.y + 7, 70, 34};
        for (int i = 0; i < count; ++i) {
            rows[size_t(i)] = {panel.x + 1, panel.y + 48 + i * rowHeight, panel.w - 2, rowHeight};
            const auto &r = rows[size_t(i)];
            previews[size_t(i)] = {r.x + r.w - 164, r.y + 12, 128, r.h - 24};
        }
    }
    int hit(float x, float y) const {
        for (int i = 0; i < count; ++i)
            if (rows[size_t(i)].contains(x, y))
                return i;
        return back.contains(x, y) ? count : -1;
    }
    int move(int from, int dx, int dy) const {
        if (from < 0 || from > count)
            return 0;
        const auto rect = [&](int i) { return i == count ? back : rows[size_t(i)]; };
        const auto current = rect(from);
        float best = std::numeric_limits<float>::max();
        int selected = from;
        for (int i = 0; i < count + 1; ++i) {
            const auto target = rect(i);
            const float x = target.cx() - current.cx(), y = target.cy() - current.cy();
            const float along = x * dx + y * dy;
            if (along <= 1)
                continue;
            const float cost = along + 3 * std::abs(x * dy - y * dx);
            if (cost < best) {
                best = cost;
                selected = i;
            }
        }
        return selected;
    }
};
} // namespace mino
