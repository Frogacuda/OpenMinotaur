#include "launcher.h"
#include "game_catalog.h"
#include "gridrunner_choices.h"
#include "about_screen.h"
#include "build_info.h"
#include <algorithm>
#include "game_preview.h"

namespace mino {
void Launcher::drawGridrunnerChoices() {
    drawHome();
    rect({0, LauncherLayout::statusBarHeight, layout_.width,
          layout_.height - LauncherLayout::statusBarHeight - 28},
         {0, 0, 0, .62f}, {0, 0, 0, .62f});
    const GridrunnerChoiceLayout choice(layout_.portrait);
    const auto &panel = choice.panel;
    rect({panel.x + 3, panel.y + 5, panel.w, panel.h}, {0, 0, 0, .55f}, {0, 0, 0, .55f});
    rect(panel, {.88f, .9f, .93f}, {.88f, .9f, .93f});
    rect(choice.header, {.62f, .7f, .8f}, {.22f, .35f, .52f}, false, true);
    text("Gridrunner", choice.header.cx(), choice.header.y + 12, 22, {1, 1, 1}, true, true);
    const auto &back = choice.back;
    if (focused(GridrunnerChoiceLayout::count))
        rect({back.x - 2, back.y - 2, back.w + 4, back.h + 4}, {.96f, .72f, .18f},
             {.96f, .72f, .18f}, true);
    rect(back, {.44f, .57f, .73f}, {.18f, .31f, .49f}, true, true);
    text("Back", back.cx(), back.y + 7, 16, {1, 1, 1}, true, true);
    for (size_t i = 0; i < GridrunnerChoiceLayout::count; ++i) {
        const auto &r = choice.rows[i];
        const auto &preview = choice.previews[i];
        const auto &game = kGridrunnerChoices[i];
        const bool available = findCore(game.id) != nullptr;
        const bool selected = focused(int(i));
        rect(r, selected ? Colour{.87f, .93f, 1} : Colour{1, 1, 1},
             selected ? Colour{.87f, .93f, 1} : Colour{1, 1, 1});
        if (selected)
            rect({r.x, r.y, 4, r.h}, {.13f, .43f, .82f}, {.13f, .43f, .82f});
        const float textWidth = preview.x - r.x - 24;
        text(game.title, r.x + 16, r.y + 15, layout_.portrait ? 21.f : 23.f, {.04f, .05f, .06f},
             false, true);
        wrappedText(game.description, r.x + 16, r.y + 49, textWidth, layout_.portrait ? 16.f : 14.f,
                    {.27f, .28f, .3f});
        const char *status = !available                                   ? "Not available"
                             : icons_[size_t(gameScreen_.game)].installed ? "Play"
                                                                          : "Resources unavailable";
        text(status, r.x + 16, r.y + r.h - 26, 14,
             available ? Colour{.16f, .36f, .64f} : Colour{.47f, .48f, .5f}, false, true);
        rect(preview, {.02f, .02f, .025f}, {.02f, .02f, .025f});
        if (gridrunnerPreviews_[i]) {
            const float aspect = i == 1 ? 4.f / 3 : .75f;
            const float height = std::min(preview.h, preview.w / aspect), width = height * aspect;
            quad({preview.cx() - width / 2, preview.cy() - height / 2, width, height}, 0, 0, 1, 1,
                 {1, 1, 1}, {1, 1, 1});
            draw(gridrunnerPreviews_[i]);
        } else if (gridrunnerAtlas_)
            drawGridrunnerPreview(preview, i == 1);
        else {
            text("Screenshot", preview.cx(), preview.cy() - 15, 13, {.68f, .7f, .73f});
            text("pending", preview.cx(), preview.cy() + 3, 13, {.68f, .7f, .73f});
        }
        const MenuRect arrow{r.x + r.w - 29, r.cy() - 12, 24, 24};
        const MenuRect ring{arrow.x - 2, arrow.y - 2, 28, 28};
        quad(ring, 0, 0, 1, 1, {.95f, .96f, .97f}, {.72f, .75f, .79f});
        roundEdge(ring);
        draw(white_, true, false, false, 0, true);
        quad(arrow, 0, 0, 1, 1, available ? Colour{.38f, .66f, 1} : Colour{.64f, .67f, .71f},
             available ? Colour{.06f, .31f, .74f} : Colour{.4f, .44f, .49f});
        roundEdge(arrow);
        draw(white_, true, true, false, 0, true);
        text(">", arrow.cx(), arrow.y + 1, 21, {1, 1, 1}, true, true);
        rect({r.x, r.y + r.h - 1, r.w, 1}, {.76f, .78f, .81f}, {.76f, .78f, .81f});
    }
}
void Launcher::drawGridrunnerPreview(const MenuRect &box, bool classic) {
    static const GamePreview previews[] = {gridrunnerTitlePreview(false),
                                           gridrunnerTitlePreview(true)};
    const auto &p = previews[classic ? 1 : 0];
    const float aspect = classic ? 4.f / 3 : .75f;
    const float height = std::min(box.h, box.w / aspect), width = height * aspect;
    const MenuRect r{box.cx() - width / 2, box.cy() - height / 2, width, height};
    const float cw = width / p.columns, ch = height / p.rows;
    for (int y = 0; y <= p.rows; ++y)
        for (int x = 0; x <= p.columns; ++x) {
            const unsigned cell = p.cells[size_t(y * (p.columns + 1) + x)], glyph = cell & 255;
            if (!glyph || glyph == 0x76)
                continue;
            const auto rgba = p.palette[(cell >> 8) & 31];
            const Colour c{float(rgba >> 24) / 255, float((rgba >> 16) & 255) / 255,
                           float((rgba >> 8) & 255) / 255, float(rgba & 255) / 255};
            const float u = float(1 + (glyph % 28) * 18) / 512,
                        v = float(321 + (glyph / 28) * 18) / 512;
            quad({r.x + (x + .5f) * cw, r.y + (y + .5f) * ch, cw, ch}, u, v, u + 16.f / 512,
                 v + 16.f / 512, c, c);
        }
    for (const auto &s : p.sprites)
        quad({r.x + (s.cx + .5f - s.rx) * width, r.y + (s.cy + .5f - s.ry) * height,
              2 * s.rx * width, 2 * s.ry * height},
             float(s.x) / 512, float(s.y) / 512, float(s.x + s.w) / 512, float(s.y + s.h) / 512,
             {1, 1, 1}, {1, 1, 1});
    draw(gridrunnerAtlas_);
}
void Launcher::activateHome() {
    const int selected = navigation_.homeFocus();
    if (selected == iox::home::options)
        openApp(Page::Options, iox::options::firstRow);
    else if (selected == iox::home::achievements) {
        noteScroll_ = 0;
        openApp(Page::Achievements);
    } else if (selected == iox::home::about) {
        aboutScreen_.scroll = 0;
        openApp(Page::About);
    } else {
        gameScreen_.game = selected;
        if (std::string(kGameCatalog[size_t(selected)].id) == "gridrunner")
            openApp(Page::Gridrunner);
        else if (icons_[size_t(selected)].playable && icons_[size_t(selected)].installed)
            request_ = {Action::Launch, kGameCatalog[size_t(selected)].id};
        else
            open(Page::Game);
    }
}
void Launcher::activateGame() {
    if (page() == Page::Gridrunner) {
        if (focus() == GridrunnerChoiceLayout::count) {
            close();
            return;
        }
        const auto &choice = kGridrunnerChoices[focus()];
        if (!findCore(choice.id))
            showMessage(choice.title, "This title is not available in this build.");
        else if (!icons_[size_t(gameScreen_.game)].installed)
            showMessage(choice.title, "Bundled game resources are unavailable.");
        else
            request_ = {Action::Launch, choice.id};
    } else
        close();
}
} // namespace mino
