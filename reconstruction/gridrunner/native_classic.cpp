#include "native_classic.h"
#include "native_classic_tables.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr::reconstruction::ios {
namespace {
constexpr int obstacles[] = {7,  8,  7,  11, 12,
                             13, 14, 15, 16, 78};       // iOS 0x661c8, duplicate 7 is intentional
constexpr int lethal[] = {30, 7, 8, 9, 10, 77, 78, 79}; // 0x661f0
bool pod(int ink) {
    return unsigned(ink - 11) < 6;
}
bool snake(int ink) {
    return unsigned(ink - 77) < 3;
}
bool danger(int ink) {
    return std::find(std::begin(lethal), std::end(lethal), ink) != std::end(lethal);
}
} // namespace
VicRules vicRules(int grid) {
    const size_t i = uint32_t(grid - 1) & 31;
    return {vicData::delay[i], vicData::count[i], vicData::body[i], vicData::speed[i],
            vicData::zapSpeed[i]};
}
void VicScreen::clear() {
    cells.fill(0x76);
}
void VicScreen::put(int x, int y, int ink) {
    // Source poke is unchecked. Contain out-of-screen host accesses; GetAt
    // already returns zero there in the original. Valid play cells are exact.
    if (x >= 0 && x <= cols && y >= 0 && y <= rows)
        cells[size_t(y * stride + x)] = ink;
}
int VicScreen::get(int x, int y) const {
    return x >= 0 && x <= cols && y >= 0 && y <= rows ? cells[size_t(y * stride + x)] : 0;
}
void VicScreen::floating(float x, float y, int ink, int background, double scale, float angle) {
    const int ix = int(x), iy = int(y);
    if (ix < 0 || ix > cols || iy < 0 || iy > rows)
        return;
    const size_t i = size_t(iy * stride + ix);
    floats[i] = {background, ink, x - .5f, y - .5f, scale, scale, angle};
    cells[i] = ink | 0x10000;
}
void VicScreen::stringAt(int x, int y, const char *text, int font, uint32_t clock) {
    int colour = 0;
    for (size_t i = 0; text[i]; ++i) {
        unsigned c = static_cast<unsigned char>(text[i]);
        if ((c == '^' || c == '|') && text[i + 1] && text[i + 2]) {
            colour = c == '^' ? text[i + 1] - '0' : int(clock & 7);
            i += 2;
            c = static_cast<unsigned char>(text[i]);
        }
        // theGame::StringAt has different number mapping from stm::stringAt.
        int glyph = c == ' '                 ? 0x76
                    : (c >= '1' && c <= '9') ? int(c) + (font == 0x25 ? 15 : -28)
                                             : int(c) + font - 'A';
        put(x++, y, c == ' ' ? 0x76 : glyph | (colour << 8));
    }
}
void VicScreen::scoreAt(int x, int y, int score) {
    char text[32];
    std::snprintf(text, sizeof text, "%07d", score);
    for (int i = 0; i < 7; ++i)
        put(x + i, y, text[i] - 28);
}
void VicScreen::valueAt(int x, int y, int value) {
    put(x, y, value / 10 + 20);
    put(x + 1, y, value % 10 + 20);
}
std::vector<VicScreen::Glyph> VicScreen::draw() const {
    std::vector<Glyph> out;
    out.reserve(cells.size() * 2);
    for (int y = 0; y <= rows; ++y)
        for (int x = 0; x <= cols; ++x) {
            const size_t i = size_t(y * stride + x);
            const int ink = cells[i] & 0x10000 ? floats[i].background : cells[i];
            if ((ink & 255) != 0 && (ink & 255) != 0x76)
                out.push_back({double(x), double(y), ink});
        }
    for (size_t i = 0; i < cells.size(); ++i)
        if (cells[i] & 0x10000) {
            const auto &f = floats[i];
            if ((f.ink & 255) != 0 && (f.ink & 255) != 0x76)
                out.push_back({f.x, f.y, f.ink, f.scaleX, f.scaleY, f.angle});
        }
    return out;
}

Classic::Classic(DarwinRandom *shared) : random_(shared ? shared : &ownRandom_) {
    initOldGR();
    title();
}
// iOS 0x46358. The spawn side and slot body storage survive grid resets.
void Classic::initOldGR() {
    screen_.clear();
    rules_ = vicRules(grid_);
    zapY_ = 2;
    zapX_ = 1;
    zap_ = -1;
    zapTimer_ = 9;
    for (auto &b : bombs_)
        b.x = -1;
    ship_ = {11, 18};
    fireTimer_ = 2;
    bullet_.x = -1;
    screen_.stringAt(1, 0, "^3GRID^4RUNNER", 0x25, clock_);
    for (auto &p : pedes_)
        p.head.x = -1;
    remaining_ = rules_.count;
    spawnTimer_ = 0;
}
// iOS 0x47dc0 and oldGridrunner case 1. The fire prompt is a port
// accessibility choice, retained for controller/touch in the separate launch.
void Classic::title(int high, uint32_t clock) {
    clock_ = clock;
    high_ = std::max(high_, high);
    if (state_ != State::Title) {
        initOldGR();
        state_ = State::Title;
    }
    status();
    screen_.put(6, 2, 31);
    screen_.put(7, 2, 32);
    screen_.scoreAt(9, 2, high_);
    screen_.stringAt(3, 7, "^3BY JEFF MINTER`", 0x58, clock_);
    screen_.stringAt(3, 11, "[C\\ 1982 Y_A_K_", 0x58, clock_);
    screen_.stringAt(1, 17, "^7PRESS FIRE TO BEGIN", 0x58, clock_);
    // iOS 0x40c5c: 7 - ASR(clock << 30, 31). The signed shift yields
    // 0 or -1, so the two line glyphs are 7 and 8, never 6.
    const int line = (7 + int((clock_ >> 1) & 1)) | 0x300;
    for (int x = 3; x < 18; ++x) {
        screen_.put(x, 10, line);
        screen_.put(x, 12, line);
    }
}
void Classic::status() { // iOS 0x45d6c
    for (int x = 12; x < 15; ++x)
        screen_.put(x, 0, x + 0x705);
    screen_.scoreAt(15, 0, score_);
}
void Classic::addScore(int amount) {
    score_ += amount;
    // Original monochrome VIC mode: a new best above 50000 earns goal 0.
    if (score_ > high_ && score_ > 50000 && !(goalsDone_ & 1)) {
        goalsDone_ |= 1;
        achievements_.push_back(0);
    }
    high_ = std::max(high_, score_);
    status();
} // 0x45f90
void Classic::drawGrid() {
    for (int y = 2; y < 19; ++y)
        for (int x = 1; x < 22; ++x)
            screen_.put(x, y, 0x201);
}
void Classic::start(int grid) {
    grid_ = int((uint32_t(std::max(grid, 1) - 1) & 31) + 1);
    lives_ = 5;
    score_ = 0;
    sounds_.clear();
    achievements_.clear();
    goalsDone_ = 0;
    initOldGR();
    status();
    drawGrid();
    state_ = State::Playing;
}
void Classic::sound(int kind) {
    // iOS schedules samples by resetting counters; repeated requests in a tick
    // coalesce. Classic death (GRSound 5) intentionally has no sample case.
    static constexpr int samples[] = {0, 20, 28, 27, 21};
    if (kind < 0 || kind > 4)
        return;
    const int sample = kind == 0 ? 22 + int(random_->next() & 3) : samples[kind];
    if (std::find(sounds_.begin(), sounds_.end(), sample) == sounds_.end())
        sounds_.push_back(sample);
}
// iOS 0x46948 combines runGR/newPede. Slots are visited in ascending order.
void Classic::runGR() {
    if (remaining_ <= 0)
        return;
    if (spawnTimer_ > 0) {
        --spawnTimer_;
        return;
    }
    spawnTimer_ = rules_.delay;
    for (auto &p : pedes_)
        if (p.head.x < 0) {
            p.x.fill(-1);
            p.length = rules_.body;
            p.head = {side_ ? 1.f : 20.f, 2};
            p.speed = side_ ? rules_.speed : -rules_.speed;
            side_ = !side_;
            screen_.put(int(p.head.x), 2, 0x34d);
            break;
        }
    --remaining_;
}
// iOS 0x46524. Keep binary float arithmetic and original duplicate obstacle
// tests; changing the order or merging them changes turns and collisions.
bool Classic::doPedes() {
    bool live = false;
    for (auto &p : pedes_)
        if (p.head.x > 0) {
            int oldX = int(p.head.x), oldY = int(p.head.y);
            float oldSpeed = p.speed;
            p.head.x = p.head.x + p.speed;
            bool edge = p.head.x >= 22 || p.head.x < 1;
            if (edge) {
                p.head.x = p.head.x - p.speed;
                p.speed = -p.speed;
                p.head.y += 1.f;
            }
            if (int(p.head.x) != oldX && !edge) {
                const int c = screen_.get(int(p.head.x), int(p.head.y)) & 255;
                for (int obstacle : obstacles)
                    if (c == obstacle) {
                        p.head.x = p.head.x - p.speed;
                        p.speed = -p.speed;
                        p.head.y += 1.f;
                    }
            }
            if (p.head.y > 18) {
                p.head = {1, 13};
                if (p.speed < 0)
                    p.speed = -p.speed;
            }
            screen_.floating(p.head.x, float(int(p.head.y)) + .5f, 0x34e, 0x201, 1.2,
                             p.head.x * -.5f);
            if (int(p.head.x) != oldX || int(p.head.y) != oldY) {
                for (int j = 0; j < p.length; ++j) {
                    const size_t k = size_t(j);
                    if (p.x[k] < 0) {
                        p.x[k] = float(oldX);
                        p.y[k] = float(oldY);
                        p.direction[k] = oldSpeed;
                        screen_.put(oldX, oldY, 0x34d);
                        break;
                    }
                    const int x = int(p.x[k]), y = int(p.y[k]);
                    const float direction = p.direction[k];
                    p.x[k] = float(oldX);
                    p.y[k] = float(oldY);
                    p.direction[k] = oldSpeed;
                    screen_.put(oldX, oldY, 0x34d);
                    oldX = x;
                    oldY = y;
                    oldSpeed = direction;
                }
                screen_.put(oldX, oldY, 0x201);
            }
            live = true;
        }
    if (!live)
        spawnTimer_ = 0;
    return live;
}
// iOS 0x47070. A split can allocate a later slot, which is then visited by
// this same call. Do not snapshot or compact the slot array.
void Classic::splitSnake(int x, int y) {
    for (auto &p : pedes_)
        if (p.head.x > 0) {
            if (int(p.head.x) == x && int(p.head.y) == y) {
                addScore(400);
                screen_.put(x, y, 0x70e);
                if (p.length < 1)
                    p.head.x = -1;
                else {
                    p.head = {p.x[0], p.y[0]};
                    p.speed = p.direction[0];
                    --p.length;
                    for (int j = 0; j < p.length; ++j) {
                        p.x[j] = p.x[j + 1];
                        p.y[j] = p.y[j + 1];
                        p.direction[j] = p.direction[j + 1];
                    }
                }
                continue;
            }
            const int oldLength = p.length;
            for (int j = 0; j < p.length; ++j)
                if (int(p.x[j]) == x && int(p.y[j]) == y) {
                    addScore(100);
                    screen_.put(x, y, 0x70e);
                    if (p.length == 1) {
                        --p.length;
                        continue;
                    }
                    p.length = j;
                    for (auto &tail : pedes_)
                        if (tail.head.x < 0) {
                            // Even the last body-cell hit copies the next (inactive) slot.
                            // It remains inactive; source arrays are initialized to -1.
                            tail.head = {p.x[j + 1], p.y[j + 1]};
                            tail.speed = p.direction[j + 1];
                            tail.length = std::max(0, oldLength - j - 2);
                            for (int k = 0; k < tail.length; ++k) {
                                tail.x[k] = p.x[j + 2 + k];
                                tail.y[k] = p.y[j + 2 + k];
                                tail.direction[k] = p.direction[j + 2 + k];
                            }
                            break;
                        }
                    break;
                }
        }
}
void Classic::doBombs() { // iOS 0x461c4
    for (auto &b : bombs_)
        if (b.x > 0) {
            screen_.put(int(b.x), int(b.y), 0x201);
            b.y = b.y + .3f;
            if (b.y > 19)
                b.x = -1;
            else
                screen_.floating(b.x, b.y, 0x11e);
        }
}
void Classic::updateGrid() { // iOS 0x46a44: bottom-up, left-to-right
    for (int y = 19; y >= 2; --y)
        for (int x = 1; x < 22; ++x) {
            const int ink = screen_.get(x, y), c = ink & 255;
            if (!pod(c))
                continue;
            if (c == 11) {
                for (auto &b : bombs_)
                    if (b.x < 0) {
                        b = {x + .5f, y + .5f};
                        break;
                    }
            } else
                screen_.floating(x + .5f, y + .5f, ink - 1);
        }
}
void Classic::doZapper() { // iOS 0x46c28
    if (zap_ < 0) {
        screen_.put(0, int(zapY_), 0x76);
        screen_.put(int(zapX_), 19, 0x76);
        // These additions use a double literal in the ARM code.
        zapY_ = float(double(zapY_) + .1);
        zapX_ = float(double(zapX_) + .1);
        if (zapY_ > 18)
            zapY_ = 2;
        if (zapX_ >= 22)
            zapX_ = 1;
        screen_.floating(.5f, zapY_, 0x105, 0x76);
        screen_.floating(zapX_, 19.5f, 0x106, 0x76);
        zapTimer_ = zapTimer_ - rules_.zapSpeed;
        if (zapTimer_ < 0) {
            zapTimer_ += 9.f;
            zap_ = 1;
            updateGrid();
            sound(zapX_ < 11 ? 2 : 3);
        }
    } else {
        const int phase = int((clock_ >> 2) & 1);
        for (int y = 2; y < 19; ++y)
            screen_.put(int(zapX_), y, 0x100 | (9 + phase));
        screen_.put(int(zap_), int(zapY_), 0x201);
        zap_ = float(double(zap_) + .9);
        if (zap_ < zapX_)
            screen_.put(int(zap_), int(zapY_), 0x700 | (7 + phase));
        else {
            for (int y = 2; y < 19; ++y)
                screen_.put(int(zapX_), y, 0x201);
            screen_.put(int(zap_), int(zapY_), 0x70e);
            zap_ = -1;
        }
    }
}
void Classic::doBull() { // iOS 0x472f8, one VIC bullet, automatic fire
    if (fireTimer_ != 0)
        --fireTimer_;
    else if (bullet_.x < 0) {
        fireTimer_ = 10;
        sound(4);
        bullet_ = {ship_.x, ship_.y - 1.f};
    }
    if (bullet_.x <= 0)
        return;
    bool hit = false;
    if (snake(screen_.get(int(bullet_.x), int(bullet_.y)) & 255)) {
        sound(0);
        splitSnake(int(bullet_.x), int(bullet_.y));
        bullet_.x = -1;
        hit = true;
    } else
        screen_.put(int(bullet_.x), int(bullet_.y), 0x201);
    // Advance in double precision, then store as float. Original execution
    // confirms that the bullet expires below y=2 on both source platforms.
    bullet_.y = float(double(bullet_.y) - .8);
    if (bullet_.y < 2) {
        bullet_.x = -1;
        hit = true;
    }
    const int x = int(bullet_.x), y = int(bullet_.y), ink = screen_.get(x, y), c = ink & 255;
    if (snake(c)) {
        splitSnake(x, y);
        sound(0);
        bullet_.x = -1;
        hit = true;
    }
    if (pod(c)) {
        int next = ink + 1;
        if (c == 16) {
            addScore(10);
            sound(0);
            next = 0x201;
        }
        screen_.floating(x + .5f, y + .5f, next);
        bullet_.x = -1;
        hit = true;
    } else if (!hit)
        screen_.floating(bullet_.x, bullet_.y, 0x104);
    if (hit)
        fireTimer_ = 0;
}
void Classic::doShip(double dx, double dy) { // iOS 0x47800
    const auto die = [&](int x, int y) {
        state_ = State::Dying;
        transition_ = 1;
        screen_.put(x, y, 0x201);
    };
    if (danger(screen_.get(int(ship_.x), int(ship_.y)) & 255)) {
        die(int(ship_.x), int(ship_.y));
        return;
    }
    screen_.put(int(ship_.x), int(ship_.y), 0x201);
    if (!std::isfinite(dx) || !std::isfinite(dy))
        dx = dy = 0;
    const Point next{std::clamp(ship_.x + float(dx * 4), 1.f, float(21.f + .9)),
                     std::clamp(ship_.y + float(dy * 4), 8.f, float(18.f + .9))};
    const int c = screen_.get(int(next.x), int(next.y)) & 255;
    if (danger(c)) {
        die(int(ship_.x), int(ship_.y));
        return;
    }
    if (!pod(c))
        ship_ = next;
    // Pods 13..16 block both the position update and its drawing coordinates.
    // The original 0x47a0e ITTTT CS groups all four stores under that condition.
    screen_.floating(ship_.x, ship_.y, 0x502, 0x201, 1.2);
}
void Classic::between(bool completed) { // oldGridrunner case 10 / explodeShip tail
    initOldGR();
    status();
    for (int x = 8; x < 12; ++x)
        screen_.put(x, 2, x + 0x4c);
    screen_.put(12, 2, lives_ + 0x14);
    screen_.stringAt(6, 12, "^4GRID", 0x25, clock_);
    screen_.valueAt(13, 12, grid_);
    if (completed)
        screen_.stringAt(5, 5, "^3GRID ZAPPED", 0x58, clock_);
    transition_ = 1;
    state_ = State::BetweenGrids;
}
void Classic::explodeShip() { // iOS 0x48600
    const auto corners = [&](int ink) {
        for (int sx : {1, -1})
            for (int sy : {1, -1}) {
                const int x = int(ship_.x + sx * transition_), y = int(ship_.y + sy * transition_);
                if (x > 0 && x <= 21 && y > 1 && y <= 18)
                    screen_.put(x, y, ink);
            }
    };
    corners(0x201);
    transition_ = float(double(transition_) + .1);
    corners(0x50 + int((clock_ >> 2) % 3));
    if (transition_ > 10) {
        --lives_;
        between(false);
        if (lives_ > 0)
            sound(1);
    }
}
void Classic::tick(double dx, double dy, uint32_t clock) { // iOS 0x40c5c
    clock_ = clock;
    sounds_.clear();
    achievements_.clear();
    switch (state_) {
    case State::Title:
        title(high_, clock);
        break;
    case State::Dying:
        explodeShip();
        break;
    case State::BetweenGrids:
        transition_ += 1.f;
        if (transition_ > 250) {
            if (lives_ < 1)
                state_ = State::GameOver; // port records/name entry intercept
            else {
                drawGrid();
                status();
                state_ = State::Playing;
            }
        }
        break;
    case State::Playing:
        runGR();
        if (!doPedes() && remaining_ == 0) {
            grid_ = grid_ % 32 + 1;
            if (lives_ < 9)
                ++lives_;
            between(true);
            sound(1);
            break;
        }
        doBombs();
        doZapper();
        doBull();
        doShip(dx, dy);
        break;
    case State::GameOver:
        break;
    }
}
} // namespace gr::reconstruction::ios
