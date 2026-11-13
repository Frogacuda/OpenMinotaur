#include "frontend.h"
#include "info_rows.h"
#include "neon/controller_session.h"
#include <cmath>
#include <cstdio>

namespace goatup {
using namespace Neon_Internals;
namespace {
void box(cellGrid &g, int x, int y, int w, int h) {
    g.printAt(x, y, "[");
    g.printAt(x + w, y, "]");
    g.printAt(x, y + h, "{");
    g.printAt(x + w, y + h, "}");
    for (int n = 1; n < w; ++n) {
        g.printAt(x + n, y, "\\");
        g.printAt(x + n, y + h, "|");
    }
    for (int n = 1; n < h; ++n) {
        g.printAt(x, y + n, "?");
        g.printAt(x + w, y + n, ">");
    }
}
void image(std::vector<ActorDraw> &out, int catalog, Vec2 at, Vec2 size) {
    ActorDraw d;
    d.frame = catalog - 2;
    d.position = at;
    d.scale = size;
    d.radius = .5f;
    d.colour.white(1);
    out.push_back(d);
}
} // namespace
neon::colour Frontend::palette(int type) const {
    const auto c = neon::paletteColour(type, 1, uint32_t(paletteTicks),
                                       {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
void Frontend::capabilities(bool touch, bool tilt) {
    touchAvailable = touch;
    tiltAvailable = tilt;
    control = preferredControl;
    if ((control < 2 && !touch) || (control == 2 && !tilt))
        control = touch ? 0 : tilt ? 2 : 0;
}
void Frontend::title() {
    screen = Screen::Title;
    page = 0;
    selection = 2;
    infoFromPause = false;
}
Frontend::Action Frontend::command(uint32_t edges, bool paused) {
    using namespace mino;
    if (screen == Screen::Loading || screen == Screen::Resetting || screen == Screen::Boot ||
        screen == Screen::Playing)
        return Action::None;
    if (edges & (PAD_B | PAD_BACK)) {
        if (screen == Screen::Title)
            return Action::Exit;
        if (infoFromPause) {
            screen = Screen::Playing;
            infoFromPause = false;
        } else
            title();
        return Action::Changed;
    }
    if (screen == Screen::Title) {
        if (edges & PAD_UP)
            selection = selection == 2 ? 1 : selection == 1 ? 3 : 2;
        if (edges & PAD_DOWN)
            selection = selection >= 3 ? 1 : selection == 1 ? 2 : 3;
        if (edges & PAD_LEFT)
            selection = selection == 4 ? 3 : selection;
        if (edges & PAD_RIGHT)
            selection = selection == 3 ? 4 : selection;
        if (edges & PAD_START)
            return Action::Start;
        if (edges & PAD_A) {
            if (selection == 2)
                return Action::Start;
            if (selection == 1) {
                if (!touchAvailable && !tiltAvailable)
                    return Action::None;
                do {
                    control = (control + 1) % 3;
                } while (control == 2 ? !tiltAvailable : !touchAvailable);
                preferredControl = control;
                return Action::Changed;
            }
            page = selection == 4 ? 2 : 1;
            screen = Screen::Info;
            selection = 0;
            return Action::Changed;
        }
    } else {
        if (edges & (PAD_LEFT | PAD_UP))
            selection = 0;
        if (edges & (PAD_RIGHT | PAD_DOWN))
            selection = 1;
        if (edges & (PAD_A | PAD_START)) {
            if (selection) {
                page = page == 6 ? 1 : page + 1;
            } else if (infoFromPause) {
                screen = Screen::Playing;
                infoFromPause = false;
            } else
                title();
            return Action::Changed;
        }
    }
    return Action::None;
}
Frontend::Action Frontend::touch(double x, double y, bool paused) {
    const int col = int((x + .5) * 29), row = int((y + .5) * 37);
    if (screen == Screen::Title) {
        if (col >= 1 && col <= 26 && row >= 26 && row <= 34)
            selection = 2;
        else if (col >= 1 && col <= 26 && row >= 19 && row <= 25)
            selection = 1;
        else if (col >= 1 && col <= 15 && row >= 12 && row <= 18)
            selection = 3;
        else if (col >= 16 && col <= 26 && row >= 12 && row <= 18)
            selection = 4;
        else
            return Action::None;
    } else if (screen == Screen::Info) {
        if (row < 32 || row > 34)
            return Action::None;
        if (col >= 1 && col <= 11)
            selection = 0;
        else if (col >= 16 && col <= 26)
            selection = 1;
        else
            return Action::None;
    } else
        return Action::None;
    return command(mino::PAD_A, paused);
}
void Frontend::drawText(cellGrid &g, const World &w) {
    g.fontY = 108;
    g.setSize(28, 36);
    g.cls();
    g.clattr();
    g.labelMode = false;
    // The title adaptation changes only the selected frame. Label colours
    // and all unselected frames retain their original palette/cycling calls.
    const auto border = [&](int item, int normal) {
        const double pulse = .75 + .25 * std::sin(double(ticks) * 6.283185307179586 / 90.);
        return controllerFocus && selection == item ? neon::colour{pulse, .08, .08, 1}
                                                    : palette(normal);
    };
    char value[96];
    if (screen == Screen::Boot) {
        // goSpec(28,36), immediately replaced by charScreen at tick 20.
        for (int row = 0; row <= 36; ++row)
            g.hlin_a(1, 0xffffffffu, 0, 28, row, 1);
        g.printColour = {0, 0, 0, 1};
        g.printAt(2, 33, "% 2011 Minotaur Research Ltd");
        return;
    }
    if (screen == Screen::Title) {
        g.printColour = palette(0);
        g.cPrintAt(9, "G O A T   U P");
        g.cPrintAt(10, "=============");
        g.printColour = border(2, 0);
        box(g, 1, 26, 25, 8);
        g.printColour = palette(0);
        g.printAt(2, 27, "    -- START GAME --");
        const char *labels[] = {"LOCAL BEST:", "LAST GAME:"};
        for (int n = 0; n < 2; ++n) {
            g.printColour = palette(4);
            g.printAt(2, 30 + n * 2, labels[n]);
            g.printColour.white(1);
            std::snprintf(value, sizeof value, "%d", n == 1 ? lastScore : best);
            g.printAt(15, 30 + n * 2, value);
        }
        g.printColour = border(3, 5);
        box(g, 1, 12, 14, 6);
        g.printColour = palette(2);
        g.printAt(3, 14, "MORE");
        g.printAt(3, 16, "INFO");
        g.printColour = border(4, 3);
        box(g, 16, 12, 10, 6);
        g.printColour = palette(3);
        g.printAt(18, 15, "STATS");
        g.printColour = border(1, 2);
        box(g, 1, 19, 25, 6);
        g.printColour = palette(2);
        if (!touchAvailable && !tiltAvailable) {
            g.cPrintAt(21, "Touch/Tilt Control:");
            g.printColour = palette(8);
            g.cPrintAt(23, "Not Present");
        } else if (control == 2) {
            g.printAt(2, 20, "TILT CONTROL");
            g.printColour = palette(8);
            g.printAt(2, 22, "      TILT TO MOVE");
            if (touchAvailable) {
                g.printColour = palette(5);
                g.printAt(2, 24, "      TOUCH TO JUMP");
            }
        } else {
            g.printAt(2, 20, control ? "TOUCH CONTROL TYPE: B" : "TOUCH CONTROL TYPE: A");
            g.printColour = palette(8);
            g.printAt(control ? 10 : 2, 24, "(-----MOVE-----)");
            g.printColour = palette(5);
            g.printAt(control ? 2 : 18, 24, "( JUMP )");
        }
    } else if (screen == Screen::Info) {
        g.printColour = palette(0);
        for (const auto &row : InfoRows)
            if (row.page == page) {
                if (row.palette >= 0)
                    g.printColour = palette(row.palette);
                else if (row.palette == -2)
                    g.printColour.white(1);
                if (row.x < 0)
                    g.cPrintAt(row.y, row.text);
                else
                    g.printAt(row.x, row.y, row.text);
            }
        if (page == 2) {
            g.cPrintAt(2, "GAME STATS");
            g.cPrintAt(3, "==========");
            const auto stat = [&](int row, const char *label, int n, int colour = 5) {
                std::snprintf(value, sizeof value, "%s%d", label, n);
                g.printColour = palette(colour);
                g.cPrintAt(row, value);
            };
            const char *labels[] = {"LOCAL BEST:", "LAST GAME:"};
            for (int i = 0; i < 2; ++i) {
                g.printColour = palette(4);
                g.printAt(2, 8 + i * 2, labels[i]);
                g.printColour.white(1);
                std::snprintf(value, sizeof value, "%d", i == 1 ? lastScore : best);
                g.printAt(15, 8 + i * 2, value);
            }
            stat(12, "Unique items found: ", w.actors.uniqueRunBonuses);
            stat(14, "Highest platform: ", w.game.highestPlatform);
            stat(16, "Goats kissed: ", w.actors.runBillyGoats);
            stat(18, "Most kids: ", w.kidState.mostInRun);
            g.printColour = palette(8);
            g.printAt(2, 20, "LIFETIME");
            stat(22, "Unique items found: ", w.actors.records.uniqueBonuses, 4);
            stat(24, "Highest platform: ", highest, 4);
            stat(26, "Goats kissed: ", w.actors.records.billyGoats, 4);
            stat(28, "Most kids: ", w.actors.records.mostKids, 4);
        }
        g.printColour = border(0, 5);
        box(g, 1, 32, 10, 2);
        g.printColour = palette(5);
        g.printAt(2, 33, infoFromPause ? "BACK" : "MAIN MENU");
        g.printColour = border(1, 5);
        box(g, 16, 32, 10, 2);
        g.printColour = palette(5);
        g.printAt(17, 33, "MORE INFO");
    }
}
void Frontend::drawArt(std::vector<ActorDraw> &out) const {
    if (screen == Screen::Title)
        image(out, 3, {0, -.35}, {.3, .2});
    if (screen == Screen::Info && page >= 4) {
        image(out, page == 4 ? 589 : page == 5 ? 590 : 591, {0, -.31}, {.22, .16});
        image(out, page == 4 ? 604 : page == 5 ? 605 : 606, {0, 0}, {.7, .38});
    }
}
} // namespace goatup
