#include "frontend.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
namespace deflex {
using namespace neon::input;
namespace {
#include "frontend_data.inc"
enum PauseItem { PauseResume, PauseScores, PauseRetry, PauseTitle, PauseQuit };
struct PauseButton {
    int frame;
    double y, width;
};
constexpr PauseButton PauseButtons[] = {{104, -.36, .168 * 1.6},
                                        {113, -.18, .168 * 1.6 * 112 / 82},
                                        {41, 0, .168 * 1.4},
                                        {114, .18, .168 * 1.4},
                                        {105, .36, .168 * 1.4}};
constexpr double PauseHeight = .168;
constexpr double SelectorArrowY = -.1; // centre of the level panels, not their top
constexpr double SelectorPlayY = .43;  // Original lower-right position
} // namespace
Frontend::Sparkle::Sparkle(neon::DarwinRandom &rng, int i) {
    colour.unpackRGBA(EolColours[i]);
    const float t = float(i) * .25f;
    radius = float(double(t) * .2 + .1);
    size = float(double(t) * .3 + .4);
    phase = {rng.range(0, 6.2831853072), rng.range(0, 6.2831853072)};
    speed = {rng.range(.0015, .003), rng.range(.0015, .003)};
    frame = 38;
    angle = float(rng.range(-3.141592654, 3.141592654));
    spin = float(rng.range(-.003141592654, .003141592654));
}
void Frontend::Sparkle::run() {
    phase += speed;
    pos = {double(std::sin(float(phase.x)) * radius), double(std::cos(float(phase.y)) * radius)};
    angle += spin;
}
void Frontend::Sparkle::draw(ActorRenderer &out, neon::vec2d view, neon::DarwinRandom &rng,
                             int threshold) {
    colour.a = (rng.next() & 1023) < unsigned(threshold) ? 1 : 0.07421875;
    drawEntity(out, view);
}
Frontend::Frontend(Progress &p, const std::vector<neon::vec2d> &s, uint32_t seed)
    : progress_(p), sizes_(s) {
    volume_ = std::clamp(p.volume, 0, 8);
    world_ = std::make_unique<World>(0, p, s, seed, WorldMode::Attract);
    world().context.volume = volume_;
    // run state 1 sets the first presentation phase to zero. World advances
    // before preparing the palette, so stage the unsigned predecessor here.
    world().context.frameCount = UINT32_MAX;
}
int Frontend::unlocked() const {
    int limit = 3;
    for (int group = 0; group < 13; ++group) {
        int count = 0;
        for (int i = 0; i < 4; ++i)
            count += progress_.bestScores[size_t(group * 4 + i)] > 0;
        if (count >= 3)
            limit = (group + 1) * 4 + 3;
    }
    return std::min(limit, 51);
}
void Frontend::cancel() {
    touches_ = {};
    world().input.touches = {};
    world().input.nativeHeld = {};
    dragging_ = false;
    release_ = true;
    held_ = menuDirections_ = 0;
    historyPressed_ = historyOK_ = false;
}
neon::FeedbackState Frontend::feedback() const {
    neon::FeedbackState state;
    state.scaleX = state.scaleY = feedbackScale_;
    state.colour[0] = state.colour[1] = paused() ? .15f : .65f;
    state.colour[2] = paused() ? .15f : .75f;
    state.colour[3] = 1;
    return state;
}
void Frontend::scene(WorldMode mode, int level) {
    const auto seed = world().random.state();
    const auto frame = world().context.frameCount;
    auto next = std::make_unique<World>(level, progress_, sizes_, seed, mode);
    next->sessionAchievements = world().sessionAchievements;
    next->gridView.takeBackground(world().gridView);
    history_.reset();
    world_ = std::move(next);
    world_->context.volume = volume_;
    world_->context.frameCount = frame; // reset/newLevel do not reset the game clock.
}
void Frontend::enter(Page next) {
    page = next;
    ticks_ = 0;
    focus_ = 0;
    cancel();
    if (next == Page::Title) {
        feedbackScale_ = .99f;
        scene(WorldMode::Attract);
        world().context.frameCount = UINT32_MAX;
        sparkles_.clear();
    } else if (next == Page::Selector) {
        feedbackScale_ = .995f;
        scene(WorldMode::Scenery);
        world().context.frameCount = 2; // selector setup publishes phase 3 on this tick.
        sparkles_.clear();
        for (int i = 0; i < 4; ++i)
            sparkles_.emplace_back(world().random, i);
        selectorPosition_ = float(selected_);
        selectorVelocity_ = 0;
        selectorLevelRow_ = true;
        selectorAnimating_ = false;
        selectorText();
    } else if (next == Page::Game) {
        pauseOpacity_ = 1;
        feedbackScale_ = .996f;
        scene(WorldMode::Standard, selected_);
        sparkles_.clear();
    } else if (next == Page::History) {
        const auto seed = world().random.state();
        const auto frame = world().context.frameCount;
        history_ = std::make_unique<History>(progress_, sizes_, seed);
        history_->world.gridView.takeBackground(world_->gridView);
        history_->world.context.volume = volume_;
        history_->world.context.frameCount = frame;
        history_->world.sessionAchievements = world_->sessionAchievements;
        world_.reset();
    } else if (next == Page::Result) {
        feedbackScale_ = .995f;
        world().level.grid.fadeLines(1, 4);
        world().showHUD = false;
        resultPrepared_ = resultSummary_ = false;
        resultTicks_ = 0;
        focus_ = world().context.levelSucceeded && selected_ < unlocked() ? 2 : 1;
        world().gridView.quick(world().context.levelSucceeded ? "LEVEL COMPLETED"
                                                              : "LEVEL FAILED. BALL EVAPORATED.",
                               {0, -.25}, {1, 1.5}, 160, 0, 1, .25f, 0);
    } else if (next == Page::Info) {
        feedbackScale_ = 1;
        scene(WorldMode::Scenery);
        sparkles_.clear();
        infoScroll_ = 0;
    } else if (next == Page::Scores)
        infoScroll_ = 0;
}
void Frontend::selectorText() {
    auto &view = world().gridView;
    auto quick = [&](const std::string &s, int row, neon::vec2d p, neon::vec2d scale, int seq) {
        // Original messages retain their animation while the backing strings change.
        if (view.lines[size_t(row)].text.empty())
            view.quick(s, p, scale, -1, row, seq, .25f, 0);
        else
            view.lines[size_t(row)].text = s;
    };
    const auto &record = level::kLevelRecords[size_t(selected_)];
    const bool open = selected_ <= unlocked();
    quick("TOTAL SCORE:", 0, {0, -.31}, {2, 1.5}, 5);
    quick(std::to_string(std::accumulate(progress_.bestScores.begin(), progress_.bestScores.end(),
                                         int64_t(0))),
          1, {0, -.21}, {3, 1.5}, 4);
    quick(open && progress_.bestScores[size_t(selected_)] > 0 ? "BEST THIS LEVEL:" : " ", 2, {0, 0},
          {2, 1.5}, 5);
    quick(open && progress_.bestScores[size_t(selected_)] > 0
              ? std::to_string(progress_.bestScores[size_t(selected_)])
              : " ",
          3, {0, .1}, {3, 1.5}, 4);
    quick(open ? record.name() : "????????????", 6, {0, -.44}, {1.4, 1.3}, 1);
    quick(open ? record.messages[1].text : "COMPLETE EARLIER LEVELS", 7, {-.47, .4}, {1, 1.2}, 1);
    view.lines[7].align = 0;
    quick(open ? record.messages[2].text : "TO OPEN THIS LEVEL", 9, {-.47, .46}, {1, 1.2}, 1);
    view.lines[9].align = 0;
    quick("BEST LOCAL SCORE", 10, {-.4, .24}, {.9, 1}, 4);
    view.lines[10].align = 0;
    quick("LAST ATTEMPT", 11, {-.4, .3}, {.9, 1}, 4);
    view.lines[11].align = 0;
}
void Frontend::stepLevel(int direction) {
    const int next = std::clamp(selected_ + direction, 0, 51);
    if (next == selected_)
        return;
    selected_ = next;
    selectorVelocity_ = 0;
    selectorAnimating_ = true;
    dragging_ = false;
    selectorText();
}
void Frontend::activate() {
    switch (page) {
    case Page::Title:
        enter(Page::Selector);
        break;
    case Page::Selector:
        if (!selectorLevelRow_ && focus_ == 1) {
            infoOrigin_ = Page::Selector;
            enter(Page::Info);
        } else if (!selectorLevelRow_ && focus_ == 2)
            enter(Page::Title);
        else if (selected_ <= unlocked())
            enter(Page::Game);
        break;
    case Page::Paused:
        if (focus_ == PauseResume) {
            page = Page::Game;
            cancel();
        } else if (focus_ == PauseRetry)
            enter(Page::Game);
        else if (focus_ == PauseTitle)
            enter(Page::Title);
        else if (focus_ == PauseQuit)
            quit = true;
        else if (focus_ == PauseScores) {
            returnPage_ = Page::Paused;
            enter(Page::Scores);
        }
        break;
    case Page::Result:
        if (!resultPrepared_)
            break;
        if (!resultSummary_) {
            showResultSummary();
            break;
        }
        if (focus_ == 0)
            enter(Page::Selector);
        else if (focus_ == 1)
            enter(Page::Game);
        else {
            if (selected_ >= unlocked())
                break;
            ++selected_;
            enter(Page::Game);
        }
        break;
    case Page::Info:
        if (focus_ == 0)
            enter(Page::History);
        else if (focus_ == 1) {
            returnPage_ = Page::Info;
            enter(Page::Scores);
        } else if (focus_ == 2) {
            volume_ = (volume_ + 1) % 9;
            progress_.volume = volume_;
            world().context.volume = volume_;
        } else
            enter(infoOrigin_);
        break;
    case Page::Scores:
        back();
        break;
    case Page::History:
        historyOK_ = true;
        break;
    default:
        break;
    }
    world().sample(5, volume_);
}
void Frontend::back() {
    if (page == Page::Game)
        enter(Page::Paused);
    else if (page == Page::Paused) {
        page = Page::Game;
        cancel();
    } else if (page == Page::Title)
        quit = true;
    else if (page == Page::Selector || page == Page::Result)
        enter(Page::Title);
    else if (page == Page::History)
        enter(Page::Info);
    else if (page == Page::Info)
        enter(infoOrigin_);
    else {
        Page p = returnPage_;
        if (p == Page::Paused) {
            page = p;
            focus_ = PauseScores;
            cancel();
        } else
            enter(p);
    }
}
void Frontend::pads(uint32_t held, uint32_t pressed, const ControllerState *pads, int count) {
    controllerFocus = keyboardNavigation;
    if (pads)
        for (int i = 0; i < std::clamp(count, 0, MaxControllers); ++i)
            controllerFocus |= pads[i].connected;
    uint32_t directions = 0;
    if (pads)
        for (int i = 0; i < std::clamp(count, 0, MaxControllers); ++i) {
            const auto a = movement(pads[i]);
            if (a.x < -.5f)
                directions |= Left;
            if (a.x > .5f)
                directions |= Right;
            if (a.y < -.5f)
                directions |= Up;
            if (a.y > .5f)
                directions |= Down;
        }
    pressed |= directions & ~menuDirections_;
    menuDirections_ = directions;
    held |= directions;
    held_ = held;
    if (release_) {
        if (held == 0)
            release_ = false;
        return;
    }
    if (page == Page::History) {
        world().input.captureControllers(pads, count);
        historyPressed_ |= (pressed & South) != 0;
        historyOK_ |= (pressed & (Start | North)) != 0;
        if (pressed & Back)
            back();
        return;
    }
    if (page == Page::Game) {
        if (pressed & (Start | Back)) {
            enter(Page::Paused);
            return;
        }
        world().input.captureControllers(pads, count);
        return;
    }
    if (pressed & (East | Back)) {
        back();
        return;
    }
    if (page == Page::Paused && (pressed & Start)) {
        page = Page::Game;
        cancel();
        return;
    }
    if (page == Page::Title) {
        if (pressed & (South | Start))
            activate();
        return;
    }
    if (page == Page::Selector) {
        if (pressed & Up)
            selectorLevelRow_ = true;
        if ((pressed & Down) && selectorLevelRow_) {
            selectorLevelRow_ = false;
            focus_ = 0; // Play on each arrival at the bottom row.
        }
        if (selectorLevelRow_) {
            if (pressed & Left)
                stepLevel(-1);
            if (pressed & Right)
                stepLevel(1);
        } else {
            // Physical order is Back / Info / Play; action IDs remain 2/1/0.
            if (pressed & Left)
                focus_ = (focus_ + 1) % 3;
            if (pressed & Right)
                focus_ = (focus_ + 2) % 3;
        }
        if (pressed & West) {
            infoOrigin_ = page;
            enter(Page::Info);
            return;
        }
        if (pressed & (South | Start))
            activate();
        return;
    }
    int countItems = page == Page::Paused   ? 5
                     : page == Page::Info   ? 4
                     : page == Page::Result ? (selected_ < unlocked() ? 3 : 2)
                                            : 1;
    if (page == Page::Scores) {
        if (pressed & Up)
            infoScroll_ = std::max(0, infoScroll_ - 1);
        if (pressed & Down)
            infoScroll_ = std::min(43, infoScroll_ + 1);
    } else {
        if (pressed & Up || (page == Page::Result && (pressed & Left)))
            focus_ = (focus_ + countItems - 1) % countItems;
        if (pressed & Down || (page == Page::Result && (pressed & Right)))
            focus_ = (focus_ + 1) % countItems;
    }
    if (pressed & (South | Start))
        activate();
}
void Frontend::touch(uint32_t id, int phase, neon::vec2d p) {
    // Page transitions cancel touch ownership. A release from the preceding
    // page must not activate a button under the same finger on this page.
    if (phase != 0 && firstSlot(4, [&](int i) {
                          return touches_.touchActive[i] && touches_.touchId[i] == id;
                      }) < 0)
        return;
    if (phase == 0)
        beginTouch(touches_, id, p, TouchTracking::Basic);
    else if (phase == 1)
        moveTouch(touches_, id, p, TouchTracking::Basic);
    else
        endTouch(touches_, id);
    for (int i = 0; i < 4; ++i)
        world().input.touches[size_t(i)] = {touches_.touchActive[i], touches_.touchPos[i]};
    if (page == Page::Game) {
        if (phase == 0 && std::fabs(p.x) < .1 && std::fabs(p.y + .38) < .06)
            enter(Page::Paused);
        return;
    }
    if (page == Page::History) {
        historyPressed_ |= phase == 0;
        historyOK_ |=
            phase == 2 && history_->trial() && std::fabs(p.x) < .12 && std::fabs(p.y - .43) < .08;
        return;
    }
    // Arrow taps use the same native step as the pad. A swipe already in
    // progress keeps ownership even if it passes over an arrow.
    if (page == Page::Selector && !dragging_ && std::fabs(p.x) > .40 &&
        std::fabs(p.y - SelectorArrowY) < .045) {
        if (phase == 2) {
            selectorLevelRow_ = true;
            stepLevel(p.x < 0 ? -1 : 1);
        }
        return;
    }
    if (page == Page::Selector && p.y > -.4 && p.y < .25) {
        if (phase == 0) {
            selectorLevelRow_ = true;
            selectorAnimating_ = false;
            dragging_ = true;
            currentDrag_ = lastDrag_ = p.x;
            previousDrag_ = 0;
        } else if (phase == 1)
            currentDrag_ = p.x;
        else
            dragging_ = false;
        return;
    }
    if (phase != 2)
        return;
    if (page == Page::Title) {
        activate();
    } else if (page == Page::Selector && ((p.x > .2 && std::fabs(p.y - SelectorPlayY) <= .06) ||
                                          (p.x <= .2 && p.y > .29 && p.y < .39))) {
        selectorLevelRow_ = false;
        focus_ = p.x < -.3 ? 2 : p.x > .2 ? 0 : 1;
        activate();
    } else if (page == Page::Paused) {
        for (int i = 0; i < 5; ++i) {
            const auto &button = PauseButtons[i];
            if (std::fabs(p.x) <= button.width * .5 &&
                std::fabs(p.y - button.y) <= PauseHeight * .5) {
                focus_ = i;
                activate();
                break;
            }
        }
    } else if (page == Page::Result && p.y > .3) {
        focus_ = p.x < -.2 ? 0 : p.x > .2 ? 2 : 1;
        if (selected_ >= unlocked() && focus_ == 2)
            focus_ = 1;
        activate();
    } else if (page == Page::Info) {
        focus_ = std::clamp(int((p.y + .18) / .16), 0, 3);
        activate();
    } else if (page == Page::Scores)
        back();
}
void Frontend::settleResult() {
    auto &w = world();
    resultPrepared_ = true;
    resultTicks_ = 3;
    progress_.lastScores[size_t(selected_)] = w.context.score;
    std::string notice = " ";
    if (w.context.score > w.context.bestScore)
        notice = "WOOHOO! BEST LOCAL LEVEL SCORE!";
    else if (w.context.levelSucceeded) {
        // The retail expression divides two floats. An empty/corrupt record
        // with a zero-point completion has no defined percentage; display zero.
        const float ratio =
            w.context.bestScore > 0 ? float(w.context.score) / float(w.context.bestScore) : 0;
        notice = "YOUR SCORE IS " + std::to_string(int(ratio * 100.f)) + "% OF THE LOCAL BEST";
    }
    w.gridView.quick(notice, {-.005, -.15}, {1, 1.5}, -1, 5, 1, .25f, 0);
    w.resultCue();
}
void Frontend::showResultSummary() {
    const int score = world().context.score;
    const bool succeeded = world().context.levelSucceeded;
    // Source reset retires the completed arena before constructing the final
    // buttons/sparkles. Preserve result values, not the old gameplay lists.
    scene(WorldMode::Scenery, selected_);
    world().context.score = score;
    world().context.levelSucceeded = succeeded;
    resultSummary_ = true;
    cancel();
    world().context.frameCount = 239; // state 10's final screen starts at phase 240.
    if (succeeded)
        world().sample(int(world().random.next() % 3) + 11, int(float(volume_) * 1.2f));
    if (unlocked() == 51)
        world().achievement(8);
    focus_ = succeeded && selected_ < unlocked() ? 2 : 1;
    for (int i = 0; i < 4; ++i)
        sparkles_.emplace_back(world().random, i);
    auto &v = world().gridView;
    v.quick("YOUR SCORE:", {0, -.25}, {2, 1.5}, -1, 5, 1, .25f, 0);
    v.quick(std::to_string(score), {0, -.15}, {3, 1.5}, -1, 1, 2, .25f, 0);
    v.quick("BEST SCORE:", {0, 0}, {2, 1.5}, -1, 2, 1, .25f, 0);
    v.quick(std::to_string(progress_.bestScores[size_t(selected_)]), {0, .1}, {3, 1.5}, -1, 3, 2,
            .25f, 0);
}
void Frontend::tick() {
    ++ticks_;
    if (page == Page::Game) {
        // pauseButton::run: only the first touch controls visibility. A held
        // touch in the lower strip preserves alpha; release resumes decay.
        if (touches_.touchActive[0]) {
            if (touches_.touchPos[0].y < .3)
                pauseOpacity_ = float(std::min(1., double(pauseOpacity_) + .1));
        } else if (pauseOpacity_ > 0)
            pauseOpacity_ = float(double(pauseOpacity_) * .98);
    }
    if (page == Page::History) {
        history_->tick((held_ & South) || touches_.touchActive[0], historyPressed_, historyOK_);
        historyPressed_ = historyOK_ = false;
    } else if (page == Page::Result) {
        // Original state processing precedes palette/list execution. A target
        // that retires during this tick cannot settle the result until the next.
        if (!resultSummary_) {
            if (!resultPrepared_) {
                if (world().scoreSettled())
                    settleResult();
                world().context.frameCount = resultPrepared_ ? 2 : 0;
            } else if (++resultTicks_ >= 240)
                showResultSummary();
            else
                world().context.frameCount = uint32_t(resultTicks_ - 1);
        }
        if (resultSummary_) {
            world().beginTick();
            for (auto &sparkle : sparkles_)
                sparkle.run();
        } else
            world().tick(false);
    } else if (page == Page::Game || page == Page::Title || page == Page::Paused ||
               (page == Page::Scores && returnPage_ == Page::Paused)) {
        world().tick(paused());
        if (page == Page::Game && world().context.levelFinished)
            enter(Page::Result);
    } else {
        world().beginTick(false);
        if (page == Page::Selector) {
            if (selectorAnimating_) {
                // Native selection eases the existing carousel to its target.
                // Touch continues to use the source drag/inertia path below.
                selectorPosition_ += (float(selected_) - selectorPosition_) * .25f;
                if (std::fabs(float(selected_) - selectorPosition_) < .001f) {
                    selectorPosition_ = float(selected_);
                    selectorAnimating_ = false;
                }
            } else {
                if (dragging_) {
                    double delta = currentDrag_ - lastDrag_;
                    selectorVelocity_ = float(-(delta + previousDrag_) * 2.2);
                    previousDrag_ = delta;
                    lastDrag_ = currentDrag_;
                    selected_ = int(double(selectorPosition_) + (currentDrag_ + .5) * 5) - 2;
                } else
                    selectorVelocity_ = float(double(selectorVelocity_) * .95);
                float next = selectorPosition_ + selectorVelocity_;
                const float maximum = std::min(float(unlocked() + 2), 51.49f);
                if (next < 0) {
                    if (selectorVelocity_ < 0)
                        selectorVelocity_ *= -.5f;
                } else if (next > maximum) {
                    if (selectorVelocity_ > 0)
                        selectorVelocity_ *= -.5f;
                } else
                    selectorPosition_ = next;
                int old = selected_;
                selected_ = std::clamp(selected_, std::max(0, int(selectorPosition_) - 2),
                                       std::min(51, int(selectorPosition_) + 3));
                if (old != selected_ || dragging_)
                    selectorText();
            }
        }
        for (auto &s : sparkles_)
            s.run();
    }
    for (auto s : world().sounds)
        sounds.push_back(s);
    world().sounds.clear();
}
void Frontend::drawGrid(GridRenderer &q, ActorRenderer &r, neon::vec2d v, bool paused) {
    if (history_)
        history_->drawGrid(q, r, v, paused);
    else
        world().drawGrid(q, r, v, paused);
}
void Frontend::drawActors(ActorRenderer &r, neon::vec2d v, bool paused) {
    if (history_)
        history_->drawActors(r, v, paused);
    else {
        world().drawActors(r, v, false, paused);
        world().drawActors(r, v, true, paused);
    }
    for (auto i = sparkles_.rbegin(); i != sparkles_.rend(); ++i)
        i->draw(r, v, world().random, page == Page::Result ? 32 : 2);
}
void Frontend::sprite(ActorRenderer &r, neon::vec2d v, int frame, neon::vec2d p, neon::vec2d size,
                      int item) {
    ActorSprite s;
    s.frame = frame;
    s.position = p * v;
    s.size = size * v;
    s.colour.white(1);
    if (item >= 0)
        s.colour = world().context.palette[controllerFocus && focus_ == item &&
                                                   (page != Page::Selector || !selectorLevelRow_)
                                               ? 0
                                               : 4];
    r.emit(s);
}
void Frontend::drawSelector(ActorRenderer &r, neon::vec2d view) {
    const int base = int(selectorPosition_);
    const float fraction = selectorPosition_ - float(base);
    const double width = view.x / 5;
    const int maximum =
        std::max(1, *std::max_element(progress_.bestScores.begin(), progress_.bestScores.end()));
    for (int i = base - 2; i <= base + 3; ++i) {
        if (i < 0 || i >= 52)
            continue;
        const double x = double(float(-fraction * float(width) + float(view.x * -.5))) +
                         double(i - (base - 2)) * width;
        ActorSprite panel;
        panel.frame = 102;
        panel.position = {x + width * .5, view.y * -.4 + view.x * .3};
        panel.size = {width, view.x * .6};
        panel.colour = i == selected_ ? world().context.palette[2] : neon::colour{.3, .2, .25, 1};
        r.emit(panel);
        auto graph = [&](int offset, neon::colour c) {
            const float scale = float((view.x * .6 - 10) / double(maximum));
            float previous = i < 1 ? 0 : float(progress_.bestScores[size_t(i - 1)]);
            float score = float(progress_.bestScores[size_t(i)]);
            if (offset) {
                previous = i < 1 ? 0 : float(progress_.lastScores[size_t(i - 1)]);
                score = float(progress_.lastScores[size_t(i)]);
            }
            const double bottom = double(float(view.x * .6 + view.y * -.4));
            double y0 = bottom - double(previous * scale), y1 = bottom - double(score * scale);
            ActorSprite line;
            line.frame = 103;
            line.colour = c;
            line.customCorners = true;
            line.corners = {x, y0, x, y0 - 10, x + width, y1 - 10, x + width, y1};
            r.emit(line);
        };
        graph(0, world().context.palette[1]);
        graph(1, world().context.palette[3]);
        LevelDefinition d(level::kLevelRecords[i]);
        ActorSprite icon;
        icon.frame = i <= unlocked() ? d.integer(0xce0) + 2 : 108;
        icon.position = {x + width * .5, view.y * -.1};
        icon.size = {view.x * .1, view.y * .12};
        if (i <= unlocked())
            icon.colour.unpackRGBA(d.word(0xce4));
        else
            icon.colour = world().context.palette[7];
        r.emit(icon);
    }
}
void Frontend::drawSelectorArrows(GridRenderer &out, neon::vec2d view) {
    // Solid triangles sample inside chars.png's opaque glyph 127. No extra
    // texture or platform drawing path is needed; the second triangle is empty.
    constexpr float u = 503.5f / 512, v = 43.5f / 512;
    for (int direction : {-1, 1}) {
        const double centre = direction * .45;
        const std::array<neon::vec2d, 3> points{
            {{centre + direction * .018, SelectorArrowY},
             {centre - direction * .012, SelectorArrowY - .022},
             {centre - direction * .012, SelectorArrowY + .022}}};
        auto colour = world().context.palette[controllerFocus && selectorLevelRow_ ? 0 : 4];
        if ((direction < 0 && selected_ == 0) || (direction > 0 && selected_ == 51))
            colour.a *= .35;
        std::array<GridVertex, 6> vertices;
        for (int i = 0; i < 3; ++i)
            vertices[size_t(i)] = {float(points[size_t(i)].x * view.x),
                                   float(-points[size_t(i)].y * view.y),
                                   610,
                                   u,
                                   v,
                                   colour.packARGB()};
        vertices[3] = vertices[4] = vertices[5] = vertices[2];
        out.quad(vertices, false, 1);
    }
}
void Frontend::drawUI(GridRenderer &q, ActorRenderer &r, neon::vec2d view) {
    auto text = [&](const std::string &s, double y, double scale = 1.2, int item = -1) {
        uint32_t col = 0xffffffff;
        if (item >= 0)
            col = world().context.palette[controllerFocus && focus_ == item ? 0 : 4].packRGBA();
        world().gridView.text(q, s, {0, y}, {scale, scale * 1.3}, col);
    };
    switch (page) {
    case Page::Title:
        sprite(r, view, 29, {0, -.2}, {.2, .1});
        break;
    case Page::Selector:
        drawSelector(r, view);
        drawSelectorArrows(q, view);
        sprite(r, view, 106, {.4, SelectorPlayY}, {.168, .12}, 0);
        // Compact host navigation leaves the original two instruction lines clear.
        world().gridView.text(
            q, "BACK", {-.4, .34}, {.65, .85},
            world()
                .context.palette[controllerFocus && !selectorLevelRow_ && focus_ == 2 ? 0 : 4]
                .packRGBA());
        world().gridView.text(
            q, "INFO", {0, .34}, {.65, .85},
            world()
                .context.palette[controllerFocus && !selectorLevelRow_ && focus_ == 1 ? 0 : 4]
                .packRGBA());
        break;
    case Page::Game: {
        ActorSprite pause;
        pause.frame = 107;
        pause.position = neon::vec2d{0, -.38} * view;
        pause.size = neon::vec2d{.168, .12} * view;
        pause.colour = world().context.palette[4];
        pause.colour.a = pauseOpacity_;
        r.emit(pause);
    } break;
    case Page::Paused:
        for (int i = 0; i < 5; ++i)
            sprite(r, view, PauseButtons[i].frame, {0, PauseButtons[i].y},
                   {PauseButtons[i].width, PauseHeight}, i);
        break;
    case Page::Result:
        if (resultSummary_) {
            sprite(r, view, 43, {-.4, .43}, {.168, .168}, 0);
            sprite(r, view, 41, {0, .43}, {.168, .168}, 1);
            if (selected_ < unlocked())
                sprite(r, view, 42, {.4, .43}, {.168, .168}, 2);
        }
        break;
    case Page::Info:
        text("DEFLEX", -.37, 2);
        text("THE STORY OF DEFLEX", -.18, 1.1, 0);
        text("LOCAL HI-SCORES", -.02, 1.1, 1);
        text("AUDIO LEVEL: " + std::to_string(volume_), .14, 1.1, 2);
        text("BACK", .3, 1.1, 3);
        break;
    case Page::Scores:
        text("LOCAL HI-SCORES", -.43, 1.5);
        for (int i = 0; i < 9 && infoScroll_ + i < 52; ++i) {
            const int n = infoScroll_ + i;
            std::string s = std::to_string(n + 1) + " " + level::kLevelRecords[n].name() + "  " +
                            std::to_string(progress_.bestScores[size_t(n)]);
            text(s, -.32 + i * .075, .65);
        }
        text("BACK", .43);
        break;
    case Page::History:
        break;
    }
}
} // namespace deflex
