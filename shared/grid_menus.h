// grid_menus.h - shared helpers for the Neon engine's text-grid menus,
// used by every core (each engine copy has its own cellGrid type, so these
// are templates over it):
//
//   * scanning the grid's labelled buttons (a label is what a tap on a cell
//     reports) and moving a controller cursor between them,
//   * highlighting the selected button the way the games mark a chosen
//     level / volume (a red cell background, here pulsing),
//   * the local high score table and name entry screens.
//
// The grid needs: cols, rowsN (largest indices), colOffset, rowOffset,
// rows[y][x] cells with .label and .bg (a colour with lerp / unpackRGBA /
// packRGBA / a), labelMode, curLabel, visible, cls(), clattr(), printAt(),
// cPrintAt(), hlin_a(flag, c0, c1, x0, x1, y, alpha).
#pragma once

#include "highscores.h"
#include "neon/menu_navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace mino {
namespace grid {

// Colours the games use for menu cells (RGBA).
constexpr uint32_t kSelOn = 0xf210007fu;  // chosen level / volume
constexpr uint32_t kSelOff = 0x0000307fu; // the other boxes

// ------------------------------------------------------------- buttons ----

using Button = neon::input::MenuItem;

template <class Grid> int screenCol(const Grid *g, int x) {
    return (x - g->colOffset + g->cols + 1) % (g->cols + 1);
}
template <class Grid> int screenRow(const Grid *g, int y) {
    return (y - g->rowOffset + g->rowsN + 1) % (g->rowsN + 1);
}

// Every label on screen as a button (only cells getLabel can reach: the
// last row and column cannot be tapped).
template <class Grid> std::vector<Button> scanButtons(const Grid *g) {
    std::vector<Button> out;
    std::vector<int> count;
    auto each = [&](auto &&fn) {
        for (int y = 0; y <= g->rowsN; y++)
            for (int x = 0; x <= g->cols; x++) {
                int l = g->rows[y][x].label;
                int sx = screenCol(g, x), sy = screenRow(g, y);
                if (l > 0 && sx < g->cols && sy < g->rowsN)
                    fn(l, sx, sy);
            }
    };
    each([&](int l, int sx, int sy) {
        size_t i = 0;
        while (i < out.size() && out[i].label != l)
            i++;
        if (i == out.size()) {
            out.push_back(Button{});
            out[i].label = l;
            count.push_back(0);
        }
        Button &b = out[i];
        b.cx += sx;
        b.cy += sy;
        b.x0 = std::min(b.x0, sx);
        b.x1 = std::max(b.x1, sx);
        b.y0 = std::min(b.y0, sy);
        b.y1 = std::max(b.y1, sy);
        count[i]++;
    });
    for (size_t i = 0; i < out.size(); i++) {
        out[i].cx = out[i].cx / count[i] + 0.5;
        out[i].cy = out[i].cy / count[i] + 0.5;
    }
    std::vector<double> best(out.size(), 1e9);
    each([&](int l, int sx, int sy) {
        for (size_t i = 0; i < out.size(); i++) {
            if (out[i].label != l)
                continue;
            double dx = sx + 0.5 - out[i].cx, dy = sy + 0.5 - out[i].cy;
            if (dx * dx + dy * dy < best[i]) {
                best[i] = dx * dx + dy * dy;
                out[i].tx = sx;
                out[i].ty = sy;
            }
        }
    });
    return out;
}

// The screen point (-0.5..0.5, as touches arrive) that getLabel maps to b.
template <class Grid> void tapPoint(const Grid *g, const Button &b, double &x, double &y) {
    x = (b.tx + 0.5) / double(g->cols) - 0.5;
    y = (b.ty + 0.5) / double(g->rowsN) - 0.5;
}

// Drops labels the cursor should skip (big touch areas that also have a
// smaller button of their own).
inline void dropLabels(std::vector<Button> &v, std::initializer_list<int> labels) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const Button &b) {
                               return std::find(labels.begin(), labels.end(), b.label) !=
                                      labels.end();
                           }),
            v.end());
}

// `s` (optionally starting with a ~RGBA colour code) with `pad` spaces each
// side, so a short word makes a reasonably sized button.
inline std::string padded(const char *s, int pad = 2) {
    if (!s)
        return std::string();
    std::string code, text = s;
    if (text.size() >= 5 && text[0] == '~') {
        code = text.substr(0, 5);
        text = text.substr(5);
    }
    return code + std::string(size_t(pad), ' ') + text + std::string(size_t(pad), ' ');
}

using neon::input::findButton;

// The nearest button in direction (dx, dy) from `from`: the gap between the
// boxes along the move plus a penalty for being out of line. Returns `from`
// if there is none.
using neon::input::moveSelection;

// --------------------------------------------------------- highlighter ----
// Lights the selected button's cells after the game has run and puts them
// back before it runs again (unless the game redrew them meanwhile), so the
// game never sees the change. BgFlag is the cell's "draw a background"
// field; cells for which skip() is true (sprite icons) are left alone.

template <class Grid, class Cell, int Cell::*BgFlag> class Highlighter {
  public:
    using Colour = decltype(Cell::bg);

    template <class Skip> void highlight(Grid *g, int label, unsigned frame, Skip skip) {
        saved_.clear();
        if (!g || label <= 0)
            return;
        Colour red;
        red.unpackRGBA(kSelOn);
        float k = 0.65f + 0.15f * std::sin(float(frame) * 0.15f);
        for (int y = 0; y <= g->rowsN; y++)
            for (int x = 0; x <= g->cols; x++) {
                Cell &c = g->rows[y][x];
                if (c.label != label || skip(c))
                    continue;
                Colour v = c.*BgFlag == 0 ? red : c.bg.lerp(red, k);
                v.a = std::max(double(v.a), double(red.a) * double(k));
                saved_.push_back(Saved{&c, c.*BgFlag, c.bg, v});
                if (c.*BgFlag == 0)
                    c.*BgFlag = 999;
                c.bg = v;
            }
    }

    void restore() {
        for (const Saved &s : saved_) {
            if (s.cell->bg.packRGBA() != s.written.packRGBA())
                continue;
            s.cell->*BgFlag = s.flag;
            s.cell->bg = s.bg;
        }
        saved_.clear();
    }

  private:
    struct Saved {
        Cell *cell;
        int flag;
        Colour bg;
        Colour written;
    };
    std::vector<Saved> saved_;
};

// ---------------------------------------------------------- pad menus -----
// Controller input for a screen of buttons: direction (with key repeat)
// moves the selection and returns the activation ID handled by the game.
// Physical touch hit-testing selects the same ID independently.

using neon::input::PadMenu;

// ------------------------------------------------------- score screens ----
// Labels on these screens (each game handles them in its own states).
constexpr int kLabelNextTable = 2;    // the top banner (touch)
constexpr int kLabelTableName = 0x31; // the table's name (the cursor's button for it)
constexpr int kLabelBack = 6;         // table: BACK; name entry: DELETE
constexpr int kLabelEnd = 3;          // name entry: END
constexpr int kLabelSpace = 0x30;
constexpr int kLabelKey0 = 0x40; // + index into kNameKeys
constexpr char kNameKeys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-!?";
constexpr int kNameKeyCount = 40;

template <class Grid> void scoreBanners(Grid *g, uint32_t c, int topLabel) {
    g->labelMode = true;
    float alpha = 1.0f;
    for (int i = 0; i < 5; i++) {
        g->curLabel = topLabel;
        g->hlin_a(999, c, 0, 0, 0x13, i, alpha);
        g->hlin_a(999, 0, c, 0x14, 0x27, i, alpha);
        g->curLabel = 0;
        g->hlin_a(999, c, 0, 0, 0x13, 0x17 - i, alpha);
        g->hlin_a(999, 0, c, 0x14, 0x27, 0x17 - i, alpha);
        alpha -= 0.15f;
    }
    g->labelMode = false;
}

// The table: a banner to switch tables (when there are several), ten rows,
// one centred BACK. `title` may carry a ~colour code; `levelHead` names the
// last column ("LEVEL"). Row `newRow` (or -1) is highlighted.
template <class Grid>
void drawScoreTable(Grid *g, const HighScores &hs, int board, const char *title, uint32_t banner,
                    int newRow, const char *levelHead = "LEVEL") {
    g->visible = true;
    g->cls();
    g->clattr();
    bool several = hs.boards() > 1;
    scoreBanners(g, banner, several ? kLabelNextTable : 0);
    g->labelMode = several;
    g->curLabel = kLabelNextTable;
    if (several)
        g->cPrintAt(0, "~0389 -- CHANGE TABLE --");
    g->curLabel = kLabelTableName;
    g->cPrintAt(2, padded(title).c_str());
    g->labelMode = true;
    g->curLabel = kLabelBack;
    for (int y = 0x14; y <= 0x16; y++) {
        g->hlin_a(999, 0x400080ffu, 0x000020ffu, 0xc, 0x13, y, 1.0f);
        g->hlin_a(999, 0x000020ffu, 0x400080ffu, 0x14, 0x1b, y, 1.0f);
    }
    g->cPrintAt(0x15, "~9099BACK");
    g->labelMode = false;
    char buf[80];
    std::snprintf(buf, sizeof buf, "~0389RANK  NAME            SCORE  %s", levelHead);
    g->cPrintAt(6, buf);
    for (int i = 0; i < HighScores::kRows; i++) {
        const HighScores::Entry &e = hs.entry(board, i);
        const char *c = i == newRow ? "~0999" : (i == 0 ? "~9959" : "~9999");
        std::snprintf(buf, sizeof buf, "%s%2d.  %-10s  %9d  %4d", c, i + 1, e.name.c_str(), e.score,
                      e.level);
        g->printAt(3, 8 + i, buf);
    }
}

// Name entry: score, the name so far, a keyboard and SPACE / DELETE / END.
template <class Grid> void drawNameEntry(Grid *g, const std::string &name, int score, int level) {
    g->visible = true;
    g->cls();
    g->clattr();
    g->cPrintAt(1, "~9909NEW HIGH SCORE!");
    char buf[80];
    std::snprintf(buf, sizeof buf, "~9999%d    LEVEL %d", score, level);
    g->cPrintAt(3, buf);
    std::string field = name + (int(name.size()) < HighScores::kNameLen ? "_" : "");
    std::snprintf(buf, sizeof buf, "~9959%-11s", field.c_str());
    g->printAt(14, 6, buf);
    g->hlin_a(999, kSelOff, kSelOff, 13, 25, 6, 1.0f);
    g->labelMode = true;
    for (int k = 0; k < kNameKeyCount; k++) {
        int x = 1 + (k % 10) * 4, y = 9 + (k / 10) * 2;
        g->curLabel = kLabelKey0 + k;
        g->hlin_a(999, kSelOff, kSelOff, x, x + 2, y, 1.0f);
        char key[8];
        std::snprintf(key, sizeof key, "~9999%c", kNameKeys[k]);
        g->printAt(x + 1, y, key);
    }
    struct {
        int label, x;
        const char *text;
    } buttons[3] = {{kLabelSpace, 2, "~9999SPACE"},
                    {kLabelBack, 15, "~9999DELETE"},
                    {kLabelEnd, 28, "~0999END"}};
    for (auto &b : buttons) {
        g->curLabel = b.label;
        for (int y = 18; y <= 20; y++)
            g->hlin_a(999, kSelOff, kSelOff, b.x, b.x + 9, y, 1.0f);
        g->printAt(b.x + (10 - int(std::strlen(b.text) - 5)) / 2, 19, b.text);
    }
    g->labelMode = false;
}

// A tap on the name entry screen: 0 nothing, 1 the name changed, 2 END.
inline int nameEntryKey(int label, std::string &name) {
    int len = int(name.size());
    if (label >= kLabelKey0 && label < kLabelKey0 + kNameKeyCount) {
        if (len < HighScores::kNameLen)
            name += kNameKeys[label - kLabelKey0];
        return 1;
    }
    if (label == kLabelSpace) {
        if (len > 0 && len < HighScores::kNameLen)
            name += ' ';
        return 1;
    }
    if (label == kLabelBack) {
        if (len > 0)
            name.pop_back();
        return 1;
    }
    return label == kLabelEnd ? 2 : 0;
}

// The name to store: trailing spaces trimmed, never empty.
inline std::string finalName(std::string name) {
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    return name.empty() ? std::string("MINOTAUR") : name;
}

} // namespace grid
} // namespace mino
