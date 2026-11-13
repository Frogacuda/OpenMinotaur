#include "launcher.h"
#include "achievement_catalog.h"
#include "game_catalog.h"
#include <glad/gl.h>
#include <sstream>
#include <cmath>
namespace mino {
MenuRect Launcher::achievementBody() const {
    return {39, 143, layout_.width - 58, layout_.height - 227};
}
int Launcher::achievementButtonCount() const {
    return page() == Page::AchievementIndex ? 10 : 5;
}
MenuRect Launcher::achievementButton(int i) const {
    const float top = LauncherLayout::statusBarHeight;
    if (page() == Page::AchievementIndex) {
        if (i == 9)
            return {12, top + 8, 78, 28};
        const float pitch = (layout_.height - top - 44 - 28) / 9;
        return {0, top + 44 + i * pitch, layout_.width, pitch};
    }
    if (i == 0)
        return achievementBody();
    if (i == 4)
        return {12, top + 8, 78, 28};
    return {layout_.width * (i - .5f) / 3 - 44, layout_.height - 72, 88, 36};
}
void Launcher::prepareAchievements() {
    const auto body = achievementBody();
    if (notePreparedGame_ != noteGame_ || noteWidth_ != body.w) {
        notePreparedGame_ = noteGame_;
        noteWidth_ = body.w;
        noteLines_.clear();
        float y = 0;
        const bool before = notesWriting_;
        notesWriting_ = true;
        for (const auto &award : iox::achievementNotes) {
            if (std::string(award.game) != kGameCatalog[size_t(noteGame_)].id)
                continue;
            std::istringstream words(std::string(award.name) + " - " + award.description);
            std::string line, word;
            bool first = true;
            auto emit = [&]() {
                noteLines_.push_back({line, y, award.id, first});
                first = false;
                y += 28;
                line.clear();
            };
            while (words >> word) {
                const auto candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && textWidth(candidate, 22) > body.w - 12)
                    emit();
                if (!line.empty())
                    line += ' ';
                line += word;
            }
            if (!line.empty())
                emit();
            y += 28;
        }
        noteHeight_ = y;
        notesWriting_ = before;
    }
    noteScroll_ = std::clamp(noteScroll_, 0.f, std::max(0.f, noteHeight_ - body.h));
}
void Launcher::drawAchievements() {
    const float top = LauncherLayout::statusBarHeight, w = layout_.width,
                bottom = layout_.height - 28;
    const Colour ink{.25f, .12f, .055f};
    rect({0, top, w, bottom - top}, {1.f, .985f, .70f}, {.99f, .94f, .57f});
    // Paper grain and ruling stay anchored to the page while only the ink scrolls.
    for (float x = 0; x < w; x += 3)
        quad({x, top, 1, bottom - top}, 0, 0, 1, 1, {.65f, .51f, .22f, .018f},
             {.65f, .51f, .22f, .018f});
    draw(white_);
    const bool index = page() == Page::AchievementIndex;
    if (!index) {
        for (float y = 169; y < bottom; y += 28)
            quad({0, y, w, 1}, 0, 0, 1, 1, {.53f, .49f, .28f, .38f}, {.53f, .49f, .28f, .38f});
        quad({28, top, 1, bottom - top}, 0, 0, 1, 1, {.70f, .28f, .19f, .48f},
             {.70f, .28f, .19f, .48f});
        quad({31, top, 1, bottom - top}, 0, 0, 1, 1, {.70f, .28f, .19f, .28f},
             {.70f, .28f, .19f, .28f});
        draw(white_);
    }
    rect({0, top, w, 44}, {.48f, .34f, .25f}, {.22f, .13f, .085f});
    rect({0, top, w, 20}, {1, 1, 1, .13f}, {1, 1, 1, 0});
    rect({0, top + 43, w, 3}, {0, 0, 0, .3f}, {0, 0, 0, 0});
    text(index ? "Games" : "Achievements", w / 2, top + 9, 22, {1, .96f, .86f}, true, true);
    const int backSlot = index ? 9 : 4;
    const auto back = achievementButton(backSlot);
    recessedButton(back, index ? "Back" : "Home", focused(backSlot), true, true);
    notesWriting_ = true;
    if (index) {
        for (int i = 0; i < 9; ++i) {
            const auto r = achievementButton(i);
            if (focused(i))
                rect(r, {.80f, .60f, .19f, .25f}, {.80f, .60f, .19f, .25f});
            text(kGameCatalog[size_t(i)].title, 16, r.cy() - 14, 23, ink, false);
            text(">", w - 20, r.cy() - 14, 23, ink, true);
            rect({0, r.y + r.h - 1, w, 1}, {.58f, .51f, .27f, .4f}, {.58f, .51f, .27f, .4f});
        }
    } else {
        prepareAchievements();
        const auto body = achievementBody();
        text(kGameCatalog[size_t(noteGame_)].title, w / 2, 73, 25, ink, true);
        text("To-Do list", body.x, 108, 25, ink, false);
        const iox::CanvasTransform canvas(layout_, surface_.width, surface_.height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, int((layout_.height - body.y - body.h) * canvas.scaleY), surface_.width,
                  int(body.h * canvas.scaleY));
        for (const auto &line : noteLines_) {
            const float y = body.y + line.y - noteScroll_;
            if (y + 28 < body.y || y >= body.y + body.h)
                continue;
            text(line.text, body.x, y, 22, ink, false);
            if (line.first && achievementRecords_ &&
                achievementRecords_->entries().count(
                    {kGameCatalog[size_t(noteGame_)].id, "original-" + std::to_string(line.id)})) {
                const MenuRect badge{6, y + 6, 20, 20};
                rect(badge, {.25f, .73f, .21f}, {.05f, .43f, .06f}, true);
                const auto stroke = [&](float ax, float ay, float bx, float by) {
                    const float dx = bx - ax, dy = by - ay, length = std::sqrt(dx * dx + dy * dy),
                                nx = -dy / length * 1.7f, ny = dx / length * 1.7f;
                    auto v = [&](float x, float y) {
                        const auto p = drawTransform_.apply({x, y, 0, 0});
                        return Vertex{p.x, p.y, 0, 0, 1, 1, 1, 1};
                    };
                    const auto a = v(ax + nx, ay + ny), b = v(bx + nx, by + ny),
                               c = v(bx - nx, by - ny), d = v(ax - nx, ay - ny);
                    vertices_.insert(vertices_.end(), {a, b, c, a, c, d});
                    draw(white_);
                };
                stroke(10, y + 16, 15, y + 21);
                stroke(15, y + 21, 23, y + 10);
            }
        }
        if (noteLines_.empty())
            text("No achievements.", body.x, body.y, 22, ink, false);
        glDisable(GL_SCISSOR_TEST);
        rect({0, bottom - 48, w, 48}, {1, .97f, .66f, .96f}, {.91f, .83f, .44f, .98f});
        rect({0, bottom - 48, w, 1}, {.61f, .48f, .24f, .5f}, {.61f, .48f, .24f, .5f});
        const char *labels[] = {"Previous", "Games", "Next"};
        for (int i = 1; i <= 3; ++i) {
            const auto r = achievementButton(i);
            if (focused(i)) {
                // A stable, overlapping red pencil loop, used only by Notes.
                const float rx = textWidth(labels[i - 1], 23) * .5f + 12, ry = 16;
                const auto point = [&](float t, float side) {
                    const float wobble =
                        1 + .025f * std::sin(3 * t + .7f) + .018f * std::sin(7 * t);
                    const float halfWidth = 1.25f + .30f * std::sin(5 * t + 1);
                    const float x = (rx * wobble + side * halfWidth) * std::cos(t);
                    const float y = (ry * wobble + side * halfWidth) * std::sin(t);
                    const auto p =
                        drawTransform_.apply({r.cx() + x + .07f * y, r.cy() + y - .045f * x, 0, 0});
                    return Vertex{p.x,  p.y,   0,     0,
                                  .72f, .055f, .035f, std::abs(side) > 1.1f ? 0.f : .88f};
                };
                constexpr int segments = 104;
                for (int n = 0; n < segments; ++n) {
                    const float t0 = -.35f + n * 6.68f / segments,
                                t1 = -.35f + (n + 1) * 6.68f / segments;
                    for (int band = 0; band < 3; ++band) {
                        const float edges[] = {-1.65f, -.8f, .8f, 1.65f};
                        const auto a = point(t0, edges[band]), b = point(t1, edges[band]),
                                   c = point(t1, edges[band + 1]), d = point(t0, edges[band + 1]);
                        vertices_.insert(vertices_.end(), {a, b, c, a, c, d});
                    }
                }
                draw(white_);
            }
            text(labels[i - 1], r.cx(), r.cy() - 14, 23, {.50f, .28f, .10f}, true);
        }
    }
    notesWriting_ = false;
}
void Launcher::activateAchievements() {
    if (page() == Page::AchievementIndex) {
        if (focus() == 9) {
            close();
            return;
        }
        noteGame_ = focus();
        noteScroll_ = 0;
        close();
        focus() = 0;
        return;
    }
    if (focus() == 4) {
        close();
        return;
    }
    if (focus() == 2) {
        open(Page::AchievementIndex, noteGame_);
        return;
    }
    if (focus() == 1 || focus() == 3) {
        noteGame_ = (noteGame_ + (focus() == 1 ? 8 : 1)) % 9;
        noteScroll_ = 0;
    }
}
void Launcher::moveAchievements(int dx, int dy) {
    if (page() == Page::AchievementIndex) {
        if (dy)
            focus() = std::clamp(focus() + dy, 0, 9);
        return;
    }
    if (focus() == 0) {
        prepareAchievements();
        const float end = std::max(0.f, noteHeight_ - achievementBody().h);
        if (dx)
            focus() = dx < 0 ? 1 : 3;
        else if (dy > 0 && noteScroll_ >= end)
            focus() = 2;
        else if (dy < 0 && noteScroll_ <= 0)
            focus() = 4;
        else
            noteScroll_ = std::clamp(noteScroll_ + dy * 28.f, 0.f, end);
    } else if (focus() == 4) {
        if (dy > 0)
            focus() = 0;
    } else {
        if (dx)
            focus() = std::clamp(focus() + dx, 1, 3);
        if (dy < 0)
            focus() = 0;
    }
}
} // namespace mino
