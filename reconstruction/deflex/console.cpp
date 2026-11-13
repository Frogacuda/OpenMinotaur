#include "console.h"
#include <stdexcept>
#include <algorithm>
namespace deflex {
int Console::number() {
    int n = 0;
    while (at_ < script_.size() && script_[at_] >= '0' && script_[at_] <= '9')
        n = n * 10 + script_[at_++] - '0';
    return n;
}
void Console::poke(int a, int b, int glyph, uint32_t rgba) {
    if (a < 0 || b < 0 || a > grid.cols || b > grid.rows)
        return;
    auto &c = grid.cell(b, a);
    c.ch = int8_t(glyph);
    c.fgOn = glyph != 32;
    c.col.unpackRGBA(rgba);
    c.scale = c.decay = 1;
}
void Console::clear() {
    for (int b = top; b <= bottom; ++b)
        for (int a = left; a <= right; ++a) {
            auto &c = grid.cell(b, a);
            c = GridCell{};
            c.ch = 32;
            c.bg = palette[fill];
            c.bgOn = 999;
        }
    x = left;
    y = top;
}
void Console::borderClear() {
    grid.rowOffset = grid.colOffset = 0;
    for (int b = 0; b <= grid.rows; ++b)
        for (int a = 0; a <= grid.cols; ++a) {
            auto &c = grid.cell(b, a);
            c.ch = 32;
            c.fgOn = 0;
            c.bgOn = 0;
            if ((b < top || (b > bottom && b < grid.rows)) ||
                (b >= top && b <= bottom && a < grid.cols && (a < left || a > right))) {
                c.bgOn = 999;
                c.bg = palette[border];
            }
        }
    clear();
}
void Console::smash(neon::DarwinRandom &randomSource) {
    for (int a = 0; a < grid.cols; ++a)
        for (int b = 0; b < grid.rows; ++b)
            grid.cell(b, a).smash(randomSource);
}
void Console::newline() {
    x = left;
    if (++y <= bottom)
        return;
    y = bottom;
    // Source scroll copies only visible colours, glyph and enable counts.
    for (int b = top; b < bottom; ++b)
        for (int a = left; a <= right; ++a) {
            auto &c = grid.cell(b, a);
            const auto &next = grid.cell(b + 1, a);
            c.col = next.col;
            c.bg = next.bg;
            c.ch = next.ch;
            c.fgOn = next.fgOn;
            c.bgOn = next.bgOn;
        }
    for (int a = left; a <= right; ++a) {
        auto &c = grid.cell(bottom, a);
        c.fgOn = 0;
        c.bg = palette[fill];
    }
}
void Console::put(unsigned char glyph) {
    if (x < 0 || x > grid.cols || y < 0 || y > grid.rows)
        throw std::out_of_range("Deflex console cursor");
    auto &c = grid.cell(y, x);
    c.col = palette[foreground];
    c.bg = palette[background];
    c.scale = scale;
    c.decay = decay;
    if (decay == 1) {
        c.bgState = c.fgState = -1;
        c.rot = c.rotVel = c.offset = c.vel = {};
        c.fgSeq = c.bgSeq = nullptr;
        c.fadeFlags = 0;
    } else {
        c.bgFrom = c.bg;
        c.fgFrom = c.col;
        c.fadeTo = palette[fill];
        c.fgState = 0;
    }
    c.ch = glyph;
    c.bgOn = 999;
    c.fgOn = glyph != 32;
    if (++x > right)
        newline();
}
void Console::rectangle() {
    int oppositeX = right - (x - left), oppositeY = bottom - (y - top);
    for (int b = y + 1; b < oppositeY; ++b)
        for (int a : {x, oppositeX}) {
            auto &c = grid.cell(b, a);
            c.fgOn = 0;
            c.ch = 32;
            c.bg = palette[background];
        }
    for (int a = x; a <= oppositeX; ++a)
        for (int b : {y, oppositeY}) {
            auto &c = grid.cell(b, a);
            c.fgOn = 0;
            c.ch = 32;
            c.bg = palette[background];
        }
}
void Console::setMode(int selected) {
    if (selected < Vic || selected > Pet)
        throw std::invalid_argument("Deflex console mode");
    mode = selected;
    if (mode == Pet) {
        static constexpr uint32_t p[] = {0xff000000, 0xff080808, 0xff202020, 0xff404040,
                                         0xff606060, 0xff808080, 0xffc0c0c0, 0xffe0e0e0,
                                         0xff000000, 0xff181818, 0xff303030, 0xff505050,
                                         0xff707070, 0xff909090, 0xffb0b0b0, 0xffd0d0d0};
        for (int i = 0; i < 16; ++i)
            palette[i].unpackARGB(p[i]);
    } else if (mode == Vic || mode == Spectrum || mode == Atari8) {
        static constexpr uint32_t p[] = {0xff000000, 0xffff0000, 0xff0000ff, 0xff00ff00,
                                         0xffff00ff, 0xffffff00, 0xff00ffff, 0xffffffff,
                                         0xff000000, 0xffff0000, 0xff0000ff, 0xff00ff00,
                                         0xffff00ff, 0xffffff00, 0xff00ffff, 0xffffffff};
        for (int i = 0; i < 16; ++i)
            palette[i].unpackARGB(p[i]);
        // goSpec/goA8 share these literal entries with goVic; only the
        // explicitly changed colours below differ in the original presets.
        if (mode == Spectrum)
            palette[15].unpackARGB(0xefefefef);
        if (mode == Atari8) {
            palette[1].unpackARGB(0xff7f5300);
            palette[9] = palette[1];
            palette[2].unpackARGB(0xffd3b35f);
            palette[10] = palette[2];
        }
    } else if (mode == C64) {
        static constexpr uint32_t p[] = {0,          0xff434b98, 0xffa5519b, 0xff9d4252,
                                         0xff5cae68, 0xff84d6c9, 0xff636363, 0xffadadad,
                                         0xff00546a, 0xff39679b, 0xff757bc3, 0xffce7b8a,
                                         0xff99e5a3, 0xffc8c179, 0xff8a8a8a, 0xffffffff};
        for (int i = 0; i < 16; ++i)
            palette[i].unpackARGB(p[i]);
    } else
        throw std::invalid_argument("Deflex tutorial console mode");
    struct Preset {
        int columns, rows, left, top, right, bottom, font, ink, paper, border;
    };
    static constexpr Preset presets[] = {{28, 24, 2, 2, 25, 21, 0, 1, 7, 5},
                                         {44, 28, 2, 2, 41, 25, 54, 11, 3, 11},
                                         {36, 24, 2, 2, 33, 21, 108, 0, 15, 7},
                                         {42, 30, 1, 3, 40, 26, 162, 2, 1, 0},
                                         {44, 28, 2, 2, 41, 25, 0, 7, 1, 1}};
    const auto &p = presets[mode];
    grid.setSize(p.columns, p.rows);
    left = p.left;
    top = p.top;
    right = p.right;
    bottom = p.bottom;
    view.fontY = p.font;
    foreground = p.ink;
    background = fill = p.paper;
    border = p.border;
    borderClear();
    auto saved = script_;
    auto savedAt = at_;
    start(mode == Vic        ? "/d0*** MINOTAUR BASIC ***/n/n3583 BYTES FREE/n/nREADY./n/q"
          : mode == C64      ? "       **** MINOTAUR BASIC v2 ****/n/n 64K RAM SYSTEM  38911 BASIC "
                               "BYTES FREE/n/nREADY./n/q"
          : mode == Spectrum ? "/x2/y21% 2011 Minotaur Research Ltd/x2/y2/q"
          : mode == Atari8   ? "/x2/y4READY./n/q"
                             : "*** MINOTAUR BASIC ***/n/n 7167 BYTES FREE/n/nREADY./n/q");
    run(false, true);
    script_ = std::move(saved);
    at_ = savedAt;
    cursor = true;
}
void Console::run(bool held, bool immediate) {
    if (!busy())
        return;
    if (!held) {
        waitRelease_ = false;
        if (!immediate) {
            if (wait_ > 0 && --wait_ != 0)
                return;
            if (--delay_ > 0)
                return;
        }
    } else {
        if (waitRelease_)
            return;
        immediate = true;
    }
    bool more = true;
    while (more && busy()) {
        if (at_ >= script_.size())
            throw std::runtime_error("unterminated Deflex console script");
        unsigned char c = script_[at_++];
        while (c == '/') {
            if (at_ >= script_.size())
                throw std::runtime_error("unterminated Deflex console command");
            char cmd = script_[at_++];
            switch (cmd) {
            case 'q':
                script_.clear();
                at_ = 0;
                loops_.clear();
                delay_ = wait_ = 0;
                return;
            case 'r':
                at_ = 0;
                loops_.clear();
                break;
            case 'n':
                newline();
                if (!immediate)
                    return;
                break;
            case '*':
                clear();
                break;
            case '$':
                smash(random);
                break;
            case ',':
                if (held) {
                    waitRelease_ = true;
                    return;
                }
                break;
            case '(':
                loops_.push_back({0, number()});
                loops_.back().at = at_;
                break;
            case ')':
                if (loops_.empty())
                    throw std::runtime_error("unmatched console loop");
                if (--loops_.back().count > 0)
                    at_ = loops_.back().at;
                else
                    loops_.pop_back();
                break;
            case '[':
                rectangle();
                break;
            case 'd':
                delayReload = number();
                delay_ = 0;
                if (!immediate)
                    return;
                break;
            case 'w':
                wait_ = number();
                if (held)
                    wait_ = 0;
                if (!immediate)
                    return;
                break;
            case 'f':
                decay = float(number()) * .0009765625f;
                break;
            case 'l':
                scale = float(number());
                break;
            case 'h':
                cursor = number() == 0;
                break;
            case 'b':
            case 'c': {
                int &v = cmd == 'b' ? background : foreground;
                if (script_[at_] == '+') {
                    ++at_;
                    v = (v + 1) & 15;
                } else if (script_[at_] == '-') {
                    ++at_;
                    v = (v - 1) & 15;
                } else
                    v = number() & 15;
                break;
            }
            case 'x':
            case 'y': {
                int &v = cmd == 'x' ? x : y;
                int lo = cmd == 'x' ? left : top, hi = cmd == 'x' ? right : bottom;
                if (script_[at_] == '+') {
                    ++at_;
                    v = std::min(v + 1, hi);
                } else if (script_[at_] == '-') {
                    ++at_;
                    v = std::max(v - 1, lo);
                } else
                    v = number();
                break;
            }
            case 's': {
                int next;
                if (script_[at_] == '+') {
                    ++at_;
                    next = (mode + 1) % 5;
                } else if (script_[at_] == '-') {
                    ++at_;
                    next = (mode + 4) % 5;
                } else
                    next = number();
                setMode(next);
                break;
            }
            default:
                break; // original unassigned commands consume their command byte
            }
            if (at_ >= script_.size())
                throw std::runtime_error("unterminated Deflex console script");
            c = script_[at_++];
        }
        delay_ = delayReload;
        if (held)
            delay_ = wait_ = 0;
        put(c);
        if (!immediate)
            more = delayReload < 1;
    }
}
} // namespace deflex
