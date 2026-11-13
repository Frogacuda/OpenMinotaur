#include "launcher.h"
#include "game_catalog.h"
#include "gridrunner_choices.h"
#include "about_screen.h"
#include "build_info.h"
#include <algorithm>
#include <glad/gl.h>
#include <sstream>

namespace mino {
void Launcher::prepareAbout() {
    const AboutLayout about(layout_);
    if (aboutScreen_.textWidth != about.body.w) {
        aboutScreen_.textWidth = about.body.w;
        aboutScreen_.lines.clear();
        float y = 0;
        for (const auto &section : kAboutSections) {
            std::istringstream words(section.text);
            std::string word, line;
            const auto emit = [&]() {
                aboutScreen_.lines.push_back({line, y, section.size, section.bold});
                y += section.size * 1.35f;
                line.clear();
            };
            while (words >> word) {
                const auto next = line.empty() ? word : line + " " + word;
                if (!line.empty() && textWidth(next, section.size, section.bold) > about.body.w)
                    emit();
                if (!line.empty())
                    line += ' ';
                line += word;
            }
            if (!line.empty())
                emit();
            y += section.bold ? 8 : 18;
        }
        aboutScreen_.height = y;
    }
    aboutScreen_.scroll =
        std::clamp(aboutScreen_.scroll, 0.f, std::max(0.f, aboutScreen_.height - about.body.h));
}
void Launcher::drawAbout() {
    prepareAbout();
    const AboutLayout about(layout_);
    const auto &panel = about.panel;
    rect(panel, {.78f, .81f, .84f}, {.78f, .81f, .84f});
    for (float x = 0; x < panel.w; x += 5)
        rect({x, panel.y, 2, panel.h}, {.83f, .85f, .87f}, {.83f, .85f, .87f});
    rect({12, about.body.y - 8, panel.w - 24, about.body.h + 16}, {.59f, .62f, .65f},
         {.59f, .62f, .65f}, true, false, 8);
    rect({13, about.body.y - 7, panel.w - 26, about.body.h + 14}, {1, 1, 1}, {1, 1, 1}, true, false,
         7);
    rect(about.header, {.65f, .72f, .81f}, {.36f, .46f, .59f});
    rect({0, panel.y, panel.w, 20}, {1, 1, 1, .17f}, {1, 1, 1, .025f});
    rect({0, panel.y + 42, panel.w, 1}, {.22f, .30f, .41f}, {.22f, .30f, .41f});
    text("About", about.header.cx() + .3f, panel.y + 8.3f, 22, {.12f, .19f, .28f, .55f}, true,
         true);
    text("About", about.header.cx() - .3f, panel.y + 9.8f, 22, {1, 1, 1, .35f}, true, true);
    text("About", about.header.cx(), panel.y + 9, 22, {.96f, .97f, .99f}, true, true);
    recessedButton(about.back, "Back", focused(2), true);
    if (focused(0))
        rect({panel.x + 1, about.body.y, 4, about.body.h}, {.13f, .43f, .82f}, {.13f, .43f, .82f});
    const iox::CanvasTransform canvas(layout_, surface_.width, surface_.height);
    const float sx = canvas.scaleX, sy = canvas.scaleY;
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(about.body.x * sx), int((layout_.height - about.body.y - about.body.h) * sy),
              int(about.body.w * sx), int(about.body.h * sy));
    for (const auto &line : aboutScreen_.lines) {
        const float y = about.body.y + line.y - aboutScreen_.scroll;
        if (y + line.size * 1.35f < about.body.y || y >= about.body.y + about.body.h)
            continue;
        text(line.text, about.body.x, y, line.size, {.12f, .13f, .15f}, false, line.bold);
    }
    glDisable(GL_SCISSOR_TEST);
    if (aboutScreen_.height > about.body.h) {
        const float thumb = std::max(24.f, about.body.h * about.body.h / aboutScreen_.height);
        const float y = about.body.y + (about.body.h - thumb) * aboutScreen_.scroll /
                                           (aboutScreen_.height - about.body.h);
        rect({panel.x + panel.w - 12, about.body.y, 4, about.body.h}, {.9f, .91f, .92f},
             {.9f, .91f, .92f});
        rect({panel.x + panel.w - 12, y, 4, thumb}, {.43f, .48f, .54f}, {.43f, .48f, .54f});
    }
    rect({panel.x + 1, about.donate.y - 12, panel.w - 2, 1}, {.76f, .78f, .81f},
         {.76f, .78f, .81f});
    recessedButton(about.donate, "Donate to Llamasoft", focused(1));
}
void Launcher::moveAbout(int dx, int dy) {
    if (page() == Page::About) {
        prepareAbout();
        const float end = std::max(0.f, aboutScreen_.height - AboutLayout(layout_).body.h);
        if (dx)
            focus() = dx > 0 ? 1 : 2;
        else if (focus() == 0 && dy) {
            if (dy < 0 && aboutScreen_.scroll <= 0)
                focus() = 2;
            else if (dy > 0 && aboutScreen_.scroll >= end)
                focus() = 1;
            else
                aboutScreen_.scroll = std::clamp(aboutScreen_.scroll + dy * 44.f, 0.f, end);
        } else if ((focus() == 2 && dy > 0) || (focus() == 1 && dy < 0))
            focus() = 0;
    }
}
void Launcher::activateAbout() {
    if (focus() == iox::about::body)
        focus() = iox::about::donate;
    else if (focus() == iox::about::donate)
        request_ = {Action::Donate, {}};
    else
        close();
}
} // namespace mino
