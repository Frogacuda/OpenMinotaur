#include "launcher.h"
#include <cmath>
namespace mino {
float Launcher::focusPulse() const {
    return .30f + .24f * (.5f + .5f * std::sin(float(selectionTime_) * 6.2831853f / 1.8f));
}
void Launcher::recessedButton(const MenuRect &back, const std::string &label, bool selected,
                              bool arrow, bool brown) {
    selected = selected && menuFocusVisible();
    const Colour white{1, 1, 1};
    const auto pointed = [&](const MenuRect &r, Colour top, Colour lower, bool gloss) {
        const std::array<std::array<float, 2>, 9> outline{{{{12, 0}},
                                                           {{r.w - 4, 0}},
                                                           {{r.w - 1.2f, 1.2f}},
                                                           {{r.w, 4}},
                                                           {{r.w, r.h - 4}},
                                                           {{r.w - 1.2f, r.h - 1.2f}},
                                                           {{r.w - 4, r.h}},
                                                           {{12, r.h}},
                                                           {{0, r.h * .5f}}}};
        const auto vertex = [&](float x, float y) {
            const float t = y / r.h;
            const auto p = drawTransform_.apply({r.x + x, r.y + y, 0, 0});
            return Vertex{p.x,
                          p.y,
                          x / r.w,
                          t,
                          top.r + (lower.r - top.r) * t,
                          top.g + (lower.g - top.g) * t,
                          top.b + (lower.b - top.b) * t,
                          top.a + (lower.a - top.a) * t};
        };
        for (size_t i = 0; i < outline.size(); ++i) {
            const auto &a = outline[i];
            const auto &b = outline[(i + 1) % outline.size()];
            vertices_.push_back(vertex(r.w * .5f, r.h * .5f));
            vertices_.push_back(vertex(a[0], a[1]));
            vertices_.push_back(vertex(b[0], b[1]));
        }
        draw(white_, false, gloss);
    };
    if (!arrow) {
        rect({back.x, back.y + 1, back.w, back.h}, {.83f, .89f, .95f}, {.83f, .89f, .95f}, true);
        rect(back, {.20f, .29f, .42f}, {.30f, .40f, .54f}, true);
        rect({back.x + 1, back.y + 1, back.w - 2, back.h - 2}, {.36f, .49f, .66f},
             {.28f, .42f, .61f}, true, true);
        if (selected)
            rect(back, {.05f, .48f, 1.f, focusPulse()}, {.03f, .28f, .85f, focusPulse()}, true,
                 true);
        text(label, back.cx(), back.cy() - 8.6f, 16, {.12f, .20f, .31f, .65f}, true, true);
        text(label, back.cx(), back.cy() - 8, 16, white, true, true);
        return;
    }
    const auto theme = [&](Colour blue, Colour earth) { return brown ? earth : blue; };
    pointed({back.x, back.y + 1, back.w, back.h}, theme({.83f, .89f, .95f}, {.65f, .48f, .34f}),
            theme({.83f, .89f, .95f}, {.65f, .48f, .34f}), false);
    pointed(back, theme({.20f, .29f, .42f}, {.13f, .075f, .045f}),
            theme({.30f, .40f, .54f}, {.29f, .17f, .105f}), false);
    pointed({back.x + 1, back.y + 1, back.w - 2, back.h - 2},
            theme({.36f, .49f, .66f}, {.34f, .22f, .145f}),
            theme({.28f, .42f, .61f}, {.23f, .13f, .075f}), true);
    if (selected)
        pointed({back.x + 1, back.y + 1, back.w - 2, back.h - 2},
                theme({.05f, .48f, 1.f, focusPulse()}, {.84f, .53f, .24f, focusPulse()}),
                theme({.03f, .28f, .85f, focusPulse()}, {.60f, .33f, .13f, focusPulse()}), true);
    text(label, back.cx() + 4, back.cy() - 8.6f, 16,
         theme({.12f, .20f, .31f, .65f}, {.08f, .035f, .01f, .8f}), true, true);
    text(label, back.cx() + 4, back.cy() - 8, 16, white, true, true);
}
} // namespace mino
