#include "stage_frontend.h"
#include <algorithm>
#include "neon/integer.h"
#include <cmath>
#include <cstdio>
namespace goatup2 {
void StageFrontend::initialize(StageRuntime &w) {
    playAspect_ = 1. / w.spriteScale(originalPlaySprite_).y;
    nextAspect_ = 1. / w.spriteScale(originalNextSprite_).y;
    previousAspect_ = 1. / w.spriteScale(originalPreviousSprite_).y;
    for (size_t i = 0; i < banks.size(); ++i)
        names[i] = generateOriginalName(w.random, uint32_t(banks[i].gid), NameMode::Bank);
}

void StageFrontend::enter(Page p, StageRuntime &w) {
    page = p;
    ticks_ = 0;
    paused = false;
    navigation_ = {};
    cancelInput(true);
    resetTicks_ = 0;
    attractRemaining_ = p == Page::Attract ? 1500 : 0;
    if (p == Page::Title || p == Page::Banks) {
        // 0x42f28 case -2 / 0x4ec98: one random banner phase on creation.
        buckAngle_ = float(w.random.range(-.39269908175, .39269908175));
        buckRadius_ = .1f;
        buckTicks_ = 0;
        slide_ = 0;
    }
    buildButtons(w);
}
void StageFrontend::setPaused(bool value, StageRuntime &w) {
    paused = value;
    navigation_ = {};
    cancelInput(true);
    if (page != Page::Playing || buttons_.empty())
        buildButtons(w);
}
void StageFrontend::cancelInput(bool preserveHeld) {
    if (preserveHeld)
        pads_.consume();
    else
        pads_.clear();
    // Touches arrive as events, unlike the repeated pad snapshots. Cancel the
    // old owner's touch: its eventual Up can be routed to a different view.
    touchHeld_ = touchPressed_ = false;
    armed_ = false;
    navigation_.moveHeld = 0;
}
void StageFrontend::pads(const neon::input::ControllerState *p, int count) {
    pads_.captureLatched(p, count);
    if (!pads_.connected())
        padUsed_ = false;
    if (keyboardNavigation)
        padUsed_ = true;
    for (const auto &state : pads_.slots())
        if (state.connected &&
            (state.pressed || std::abs(state.lx) > .5f || std::abs(state.ly) > .5f))
            padUsed_ = true;
}
void StageFrontend::touch(neon::input::TouchPhase phase, uint32_t id, neon::vec2d point) {
    using P = neon::input::TouchPhase;
    if (phase == P::Down && !touchHeld_) {
        touchHeld_ = touchPressed_ = true;
        touchId_ = id;
        touchPoint_ = point;
        padUsed_ = false;
    } else if (id == touchId_) {
        touchPoint_ = point;
        if (phase == P::Up || phase == P::Cancel)
            touchHeld_ = false;
    }
}
bool StageFrontend::anyInput() const {
    if (touchHeld_ || touchPressed_)
        return true;
    for (const auto &p : pads_.slots())
        if (p.connected && (p.buttons || p.pressed || std::abs(p.lx) > .5f || std::abs(p.ly) > .5f))
            return true;
    return false;
}
void StageFrontend::add(StageRuntime &w, int action, int image, neon::vec2d position, float radius,
                        neon::vec2d scale, double alpha) {
    if (scale.x == 0 && scale.y == 0)
        scale = w.spriteScale(image);
    buttons_.push_back({action, image, position, scale, radius, radius, alpha});
}
void StageFrontend::buildButtons(StageRuntime &w) {
    buttons_.clear();
    if (page == Page::Banks) {
        for (int i = 0; i < 4; ++i) {
            // 0x33708 widens a binary32 division, then adds the half-cell.
            const auto cell = [](int x, int y) {
                return neon::vec2d{double(float(x) / 42.f) + .5 / 42. - .5,
                                   double(float(y) / 46.f) + .5 / 46. - .5};
            };
            add(w, Choose0 + i, editing ? 117 : 21, cell(editing ? 32 : 38, 11 + 9 * i), .04f,
                editing ? neon::vec2d{.9, .9} : neon::vec2d{1, 1});
            if (editing) {
                add(w, BankRename0 + i, 98, cell(28, 11 + 9 * i), .04f, {.9, .9});
                add(w, BankLoad0 + i, 17, cell(23, 11 + 9 * i), .04f, {.9, .9});
                add(w, BankSave0 + i, 16, cell(19, 11 + 9 * i), .04f, {.9, .9});
            }
        }
        add(w, ToggleEdit, 18, {.43, -.41}, .04f);
    } else if (page == Page::Preview) {
        add(w, Return, 42, {.43, -.4}, .04f);
        if (campaignPlayable(bank) && (!editing || bank > 0) &&
            (editing || banks[size_t(bank)].stages[size_t(level)].seed))
            add(w, Play, originalPlaySprite_ + int(editing), {0, .39}, .07f, {playAspect_, 1});
        add(w, Next, originalNextSprite_, {.42, .39}, .04f, {nextAspect_, 1});
        add(w, Previous, originalPreviousSprite_, {-.42, .39}, .04f, {previousAspect_, 1});
    } else if (page == Page::Playing) {
        add(w, Resume, 1, {0, -.35}, .08f, {1, 1}, 1);
        {
            add(w, Reset, 166, {0, -.17}, .08f, w.spriteScale(166) * 3., 0);
            add(w, Title, 186, {0, -.025}, .08f, w.spriteScale(166) * 3., 0);
            double controlY = .12;
            if (touchAvailable_) {
                add(w, Hand, touchMode == 0 ? handSprites_[0] : handSprites_[1], {0, controlY},
                    .05f, {4.8, 1}, 0);
                controlY += .11;
            }
            if (motionAvailable_) {
                add(w, Tilt, tiltEnabled ? 187 : 188, {0, controlY}, .045f, {5, 1}, 0);
                controlY += .11;
            }
            add(w, Exit, 6, {0, controlY + .015}, .06f, {2.4, 1}, 0);
        }
    } else if (page == Page::Credits)
        add(w, Return, 185, {-.35, .34}, .04f);
}
void StageFrontend::capabilities(bool touch, bool motion, StageRuntime &w) {
    if (touch == touchAvailable_ && motion == motionAvailable_)
        return;
    touchAvailable_ = touch;
    motionAvailable_ = motion;
    buildButtons(w);
    navigation_.sel = Resume;
}
void StageFrontend::activatePulse(int action) {
    for (auto &b : buttons_)
        if (b.action == action)
            b.radius = b.baseRadius * 2.f;
}
int StageFrontend::tick(StageRuntime &w, uint32_t) {
    const bool activeInput = anyInput();
    const auto input = pads_.consume();
    ++ticks_;
    if (resetTicks_ > 0)
        --resetTicks_;
    if (page == Page::Title || page == Page::Banks) {
        if (buckTicks_ < 100)
            buckRadius_ = float(double(buckRadius_) * 1.01);
        ++buckTicks_;
    }
    if (page == Page::Title) {
        if (slide_ < 1)
            slide_ = std::min(1.f, slide_ + .08f);
    }
    for (auto &b : buttons_) {
        if (b.radius > b.baseRadius)
            b.radius = std::max(b.baseRadius, float(double(b.radius) * .9));
        if (page == Page::Playing) {
            if (b.action == Resume) {
                if (paused)
                    b.alpha = 1;
                else
                    b.alpha = double(float(b.alpha * .98));
            } else if (!paused)
                b.alpha *= .95;
            else {
                if (b.action == Exit || b.action == Title)
                    b.alpha = std::min(.6, b.alpha + .05);
                if (b.action == Hand || b.action == Tilt)
                    b.alpha = std::min(.5, b.alpha + .05);
                // 0x4a458 resets packed FF..FF colour before reading alpha:
                // the .5 comparison is not a fade cap on an active Reset.
                if (b.action == Reset)
                    b.alpha = w.lives > 0 ? 1. : b.alpha * .95;
            }
        }
    }
    if (page == Page::Attract) {
        if (attractRemaining_ > 0)
            --attractRemaining_;
        return activeInput || attractRemaining_ < 1 ? Return : None;
    }
    if (!armed_) {
        if (!activeInput)
            armed_ = true;
        touchPressed_ = false;
        return None;
    }
    if (page == Page::Title && activeInput) {
        touchPressed_ = false;
        for (const auto &p : input)
            if (p.connected && (p.pressed & neon::input::East))
                return Exit;
        return Begin;
    }
    if (page == Page::Title && ticks_ > 500)
        return IdleCycle;
    if (page == Page::Credits) {
        if (activeInput)
            ticks_ += 10;
        if (ticks_ >= 901 || activeInput)
            return Return;
        return None;
    }
    if (page == Page::Loading)
        return None;
    std::vector<neon::input::MenuItem> items;
    for (const auto &b : buttons_) {
        if (page == Page::Playing && !paused && b.action != Resume)
            continue;
        const double x = (b.position.x + .5) * 320, y = (b.position.y + .5) * 480;
        neon::input::MenuItem m;
        m.label = b.action;
        m.cx = x;
        m.cy = y;
        m.x0 = int(x - std::abs(b.scale.x * b.baseRadius) * 320);
        m.x1 = int(x + std::abs(b.scale.x * b.baseRadius) * 320);
        m.y0 = int(y - std::abs(b.scale.y * b.baseRadius) * 480);
        m.y1 = int(y + std::abs(b.scale.y * b.baseRadius) * 480);
        items.push_back(m);
    }
    float x = 0, y = 0;
    bool accept = false, back = false;
    for (const auto &p : input)
        if (p.connected) {
            if (std::abs(p.lx) > std::abs(x))
                x = p.lx;
            if (std::abs(p.ly) > std::abs(y))
                y = p.ly;
            if (p.buttons & neon::input::Left)
                x = -1;
            if (p.buttons & neon::input::Right)
                x = 1;
            if (p.buttons & neon::input::Up)
                y = -1;
            if (p.buttons & neon::input::Down)
                y = 1;
            accept |= (p.pressed & neon::input::South) != 0;
            back |= (p.pressed & neon::input::East) != 0;
        }
    if (page == Page::Playing && !paused) {
        x = y = 0;
        accept = back = false;
    }
    if (x || y || accept || back)
        padUsed_ = true;
    if (back && page == Page::Banks)
        return Return;
    int action = navigation_.update(items, int(page) * 2 + int(paused), x, y, accept, back, 0,
                                    {Play, Resume, Choose0}, {Return, Resume});
    if (touchPressed_) {
        for (auto i = buttons_.rbegin(); i != buttons_.rend(); ++i) {
            if (page == Page::Playing && !paused && i->action != Resume)
                continue;
            if (i->action == Reset && w.lives < 1)
                continue;
            if (std::abs(touchPoint_.x - i->position.x) <
                    std::abs(i->scale.x) * double(i->radius * 1.2f) &&
                std::abs(touchPoint_.y - i->position.y) <
                    std::abs(i->scale.y) * double(i->radius * 1.2f)) {
                action = i->action;
                break;
            }
        }
    }
    touchPressed_ = false;
    if (action != None) {
        auto found = std::find_if(buttons_.begin(), buttons_.end(),
                                  [&](const auto &b) { return b.action == action; });
        if (found != buttons_.end()) {
            if (((action == Exit || action == Title) && found->alpha <= .5) ||
                ((action == Hand || action == Tilt) && found->alpha <= .4) ||
                (action == Reset && found->alpha <= .8))
                return None;
        }
        activatePulse(action);
    }
    return action;
}
void StageFrontend::sprite(StageRuntime &w, int image, neon::vec2d p, float radius,
                           neon::colour colour, neon::vec2d scale, double angle) {
    if (scale.x == 0 && scale.y == 0)
        scale = w.spriteScale(image);
    draws_.push_back({p, scale * double(radius + radius), colour, angle, image + 2});
}
void StageFrontend::prepare(StageRuntime &w, StageHud &hud) {
    prepareText(w, hud);
    prepareDraws(w);
}
void StageFrontend::prepareText(StageRuntime &w, StageHud &hud) {
    hud.clearRows.clear();
    hud.text.clear();
    auto text = [&](int x, int y, std::string s, int colour) {
        hud.text.push_back({x, y, std::move(s), w.terrainPalette.at(size_t(colour))});
    };
    char line[100];
    if (page == Page::Title) {
        text(6, 25, "     THE TAIL OF GOATUP 2", 4);
        text(6, 28, "       TOUCH TO BEGIN", 3);
    }
    if (page == Page::Banks) {
        text(15, 2, "SELECT GAME", 2);
        text(12, 5, editing ? "MODE: EDIT GAMES" : "MODE: PLAY GAMES", 0);
        for (int i = 0; i < 4; ++i) {
            const int row = 10 + 9 * i;
            if (editing) {
                text(1, row, "GAME " + std::to_string(i + 1) + ":", 0);
                text(2, row + 3, names[size_t(i)], 0);
                continue;
            }
            text(1, row, names[size_t(i)], 0);
            int count = 0, total = 0;
            for (const auto &s : banks[size_t(i)].stages)
                if (s.seed) {
                    ++count;
                    total = neon::integers::add32(total, s.highScore);
                }
            text(2, row + 2, "A GAME OF " + std::to_string(count) + " LEVELS", 2);
            text(2, row + 3, "TOTAL SCORE: " + std::to_string(total), 3);
        }
    } else if (page == Page::Preview) {
        const auto &stage = banks.at(size_t(bank)).stages.at(size_t(level));
        text(1, 1, names.at(size_t(bank)), 0);
        text(33, 1, "LEVEL " + std::to_string(level + 1), 2);
        text((42 - int(previewName.size())) / 2, 3, previewName, 0);
        std::snprintf(line, sizeof line, "LEVEL HISCORE: %d", stage.highScore);
        text((42 - int(std::char_traits<char>::length(line))) / 2, 5, line, 3);
        auto time = StageHud::timeText("BEST TIME", stage.bestTime ? stage.bestTime : 108000);
        text((42 - int(time.size())) / 2, 7, time, 4);
    } else if (page == Page::Credits)
        credits(w, hud);
    else if (page == Page::Attract && attractRemaining_ < 1500) {
        text(1, 40, "****************************************", 0);
        text(1, 41, "*         --== GOATUP 2 ==--           *", 0);
        text(1, 42, "*                                      *", 0);
        text(1, 43, "*           TOUCH TO BEGIN             *", 0);
        text(1, 44, "****************************************", 0);
    }
}
void StageFrontend::prepareDraws(StageRuntime &w) {
    draws_.clear();
    foreground_.clear();
    if (page == Page::Title || page == Page::Banks) {
        const float angle = float(buckAngle_ * std::sin(float(double(w.sourceTime) * .1)));
        // 0x42f28 -2 prepends portrait after buck: portrait draws first.
        if (page == Page::Title) {
            const float y = .68f + (.32f - .68f) * slide_;
            auto scale = w.spriteScale(118);
            scale.x *= 1.3;
            sprite(w, 118, {0, double(y)}, .2f, {1, 1, 1, 1}, scale);
        }
        sprite(w, 110, {0, page == Page::Title ? -.2 : 0.}, buckRadius_,
               {0, 1, 0, page == Page::Title ? 1. : .2}, {}, double(angle));
    }
    for (const auto &b : buttons_) {
        auto colour = neon::colour{1, 1, 1, b.alpha};
        if (padUsed_ && (page != Page::Playing || paused) && navigation_.sel == b.action) {
            colour = w.terrainPalette[0];
            colour.a = b.alpha;
        }
        float angle = 0;
        if (b.action == Reset && paused && w.lives > 0)
            colour = (ticks_ & 16) ? neon::colour{1, 0, 112. / 255, b.alpha}
                                   : neon::colour{1, 1, 0, b.alpha};
        if (b.action == Reset && resetTicks_ > 0) {
            angle =
                float(w.random.range(-.2617993878, .2617993878) * double(std::sin(float(ticks_))));
        }
        if (b.action == Tilt) {
            sprite(w, tiltEnabled ? 187 : 188, b.position, b.radius, colour, b.scale);
            foreground_.push_back(draws_.back());
            draws_.pop_back();
            continue;
        }
        if (b.action == Hand) {
            auto copy = b;
            copy.sprite = handSprites_[touchMode == 0 ? 0 : 1];
            sprite(w, copy.sprite, copy.position, copy.radius, colour, copy.scale);
            foreground_.push_back(draws_.back());
            draws_.pop_back();
            continue;
        }
        if (b.alpha > .1) {
            sprite(w, b.sprite, b.position, b.radius, colour, b.scale, double(angle));
            if (page == Page::Playing || b.action == Play || b.action == Previous ||
                b.action == Next) {
                foreground_.push_back(draws_.back());
                draws_.pop_back();
            }
        }
    }
}
void StageFrontend::credits(StageRuntime &w, StageHud &hud) {
    auto line = [&](int x, int row, const char *text, int palette) {
        hud.text.push_back({x, row, text, w.terrainPalette[size_t(palette)]});
    };
    line(10, 4, "     --------------", 0);
    line(10, 5, "     -  GOATUP 2  -", 0);
    line(10, 6, "     --------------", 0);
    if (ticks_ > 30)
        line(1, 9, "   CREATED BY", 14);
    if (ticks_ > 40) {
        line(1, 12, "           JEFF MINOTAUR", 2);
        line(1, 14, "         AND", 2);
        line(1, 16, "      GILESGOAT", 2);
    }
    if (ticks_ > 160)
        line(15, 19, "            LEVELS BY", 14);
    if (ticks_ > 170) {
        line(15, 22, "  STINKY OX", 3);
        line(15, 24, "    PRECIOUS PONY", 3);
        line(15, 26, "      PVB", 3);
    }
    if (ticks_ > 220)
        line(15, 28, "         AND YOU!", 4);
    if (ticks_ > 300) {
        line(1, 32, "         (c) 2013 LLAMASOFT LTD", 1);
        line(1, 34, "    SOUNDS SOURCED AT FREESOUND.ORG", 1);
    }
    line(1, 37, "         -- TOUCH THE SHEEP ICON", 8);
    line(1, 39, "         -- FOR MORE LLAMASOFT GAMES", 8);
}
} // namespace goatup2
