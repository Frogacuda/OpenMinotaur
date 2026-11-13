#include "frontend.h"
#include "original_tables.h"
#include "core_api.h"
#include <algorithm>
#include <cstdio>
namespace superoxwars {
namespace {
uint32_t pack(Colour c) {
    auto b = [](double v) { return uint32_t(std::clamp(v, 0., 1.) * 255.); };
    return b(c.r) << 24 | b(c.g) << 16 | b(c.b) << 8 | b(c.a);
}
struct AttractLine {
    int page, time, row;
    const char *text;
};
// 4a428: each line appears after its elapsed-frame threshold. Source copy,
// spelling and font substitutions (e.g. @/'{') are intentionally retained.
const AttractLine Lines[] = {{2, 101, 7, "ON THE DISTANT PLANET"},
                             {2, 151, 9, "'PARINT' THE PEOPLE REVERE"},
                             {2, 201, 11, "TWO OXEN."},
                             {2, 301, 14, "EACH OX REPRESENTS A"},
                             {2, 351, 16, "VALUABLE CHARACTERISTIC"},
                             {2, 401, 18, "OF THE SOCIETY."},
                             {2, 501, 21, "EACH PERSON CHOOSES"},
                             {2, 551, 23, "ONE OX TO GUIDE THEIR"},
                             {2, 601, 25, "SPIRIT."},
                             {3, 101, 7, "        CAPRICHOSO"},
                             {3, 201, 14, "     THE CAPRICIOUS OX"},
                             {3, 251, 17, "REPRESENTS THE JOYFUL"},
                             {3, 301, 19, "CREATIVITY OF THE PEOPLE."},
                             {3, 401, 22, "CAPRICHOSO IS REPRESENTED"},
                             {3, 451, 24, "AS A BLACK OX WITH A"},
                             {3, 501, 26, "BLUE STAR."},
                             {4, 101, 7, "        GARANTIDO"},
                             {4, 201, 14, "    THE DEPENDABLE OX"},
                             {4, 251, 17, "REPRESENTS THE STEADFAST"},
                             {4, 301, 19, "LOYALTY OF THE PEOPLE."},
                             {4, 401, 22, "GARANTIDO IS REPRESENTED"},
                             {4, 451, 24, "AS A WHITE OX WITH A"},
                             {4, 501, 26, "RED HEART."},
                             {5, 101, 7, "NOW 'PARINT' WAS INVADED BY"},
                             {5, 151, 9, "THE MARCAB EMPIRE AND THE"},
                             {5, 201, 11, "PEOPLE ENSLAVED."},
                             {5, 301, 14, "A FINAL WARRIOR MUST UNITE"},
                             {5, 351, 16, "THE OXENS' POWER TO"},
                             {5, 401, 18, "VANQUISH THE OPPRESSORS."},
                             {5, 501, 21, "TAKE OFF AND STRIKE WITH"},
                             {5, 551, 23, "BLAZING STAR AND SHINING"},
                             {5, 601, 25, "HEART OF OXEN!"},
                             {6, 51, 5, "   --- HELPFUL HINTS ---"},
                             {6, 101, 8, "STICK WITH ONE COLOUR FOR A"},
                             {6, 151, 10, "WHILE TO GAIN MAXIMUM"},
                             {6, 201, 12, "BENEFIT FROM THAT COLOUR."},
                             {6, 301, 15, "TRY PLAYING A LEVEL IN ONE"},
                             {6, 351, 17, "COLOUR, THEN PLAY THROUGH"},
                             {6, 401, 19, "AGAIN WITH OPPOSITE."},
                             {6, 501, 22, "YOU GET HIGH SCORE TABLES"},
                             {6, 551, 24, "FOR EACH COLOUR AS WELL"},
                             {6, 601, 26, "AS FOR COMBINED TOTAL."},
                             {7, 51, 5, "      --- LORE ---"},
                             {7, 101, 8, "THE MORE ONE OX DOMINATES"},
                             {7, 151, 10, "THE OTHER, THE MORE THAT"},
                             {7, 201, 12, "OX CONFERS HIS POWER."},
                             {7, 301, 15, "COLLECT JEWELS OF THE OX"},
                             {7, 351, 17, "TO STRONGLY INCREASE YOUR"},
                             {7, 401, 19, "POWER."},
                             {7, 501, 22, "NO-MISS-SHOOTING OF THE"},
                             {7, 551, 24, "FLYING ENEMY GROUPS"},
                             {7, 601, 26, "BRINGS GREAT REWARDS."},
                             {8, 51, 5, "  --- SUPER OX WARS ---"},
                             {8, 101, 8, "CREATED BY LLAMASOFT"},
                             {8, 151, 10, "  JEFF MINOTAUR"},
                             {8, 201, 12, "  GILESGOAT"},
                             {8, 301, 15, "FOLLOW US ON TWITTER!"},
                             {8, 351, 17, "  `LLAMASOFT{OX"},
                             {8, 501, 22, "LEARN THE REAL STORY OF"},
                             {8, 551, 24, "THE TWO OXEN! VISIT"},
                             {8, 601, 26, "  BOIBUMBA.COM!"}};
} // namespace
namespace {
// llabel::run 5bd24. Labels share the Decor list and its pause gate; title
// changes mark type 7000 for retirement, while bonus labels have a finite age.
struct Label final : GameEntity {
    int remaining = 0;
    Label(World &w, int sprite, Vec2 at, float size, Vec2 dimensions, int life = 0)
        : GameEntity(w), remaining(life) {
        type = 0x7000;
        frame = sprite;
        position = retained = at;
        radius = size;
        scale = dimensions;
    }
    void run() override {
        if (remaining > 0 && --remaining == 0)
            dead = true;
    }
};
void retireLabels(World &w) {
    for (auto *e = w.lists[size_t(Domain::Decor)]; e; e = e->next)
        if (e->type == 0x7000)
            e->dead = true;
}
Vec2 cellCentre(int x, int y) {
    return {double(float(float(x) / 30.f)) - .5 + double(float(.5f / 30.f)),
            double(float(float(y) / 38.f)) - .5 + double(float(.5f / 38.f))};
}
void label(World &w, int frame, int x, int y, float radius, Vec2 scale, int life = 0) {
    w.add(Domain::Decor, new Label(w, frame, cellCentre(x, y), radius, scale, life));
}
// 4107c, 4502c, 452a0, 45418: original entities, in the foreground UI
// list. Canonical controller commands invoke the same activation method.
struct Button final : Entity {
    // Pause stores its fade as float; Quit fades the inherited double alpha.
    Frontend &owner;
    int action;
    float baseRadius;
    double opacity = 1;
    bool down = true;
    Button(World &w, Frontend &f, int a, int sprite, Vec2 at, float size, Vec2 dimensions)
        : Entity(w), owner(f), action(a), baseRadius(size) {
        type = a < 5 ? 0x1001 : 0x1000;
        frame = sprite;
        position = at;
        radius = size;
        scale = dimensions;
        if (a >= 6)
            opacity = 0;
    }
    void run() override {
        if (dead)
            return;
        if (action == 8 && !owner.tiltChoiceVisible()) {
            hidden = true;
            return;
        }
        if (owner.screen == Frontend::Screen::Scores || owner.screen == Frontend::Screen::Goals) {
            hidden = true;
            return;
        }
        hidden = false;
        if (action == 8)
            frame = owner.records.tiltEnabled ? 92 : 93;
        if (action == 6)
            position.y = owner.tiltChoiceVisible() ? .30 : .16;
        if (action < 5) {
            // 421e0 retires type 1001 at start; 45418 culls it immediately.
            if (owner.phase != Frontend::Phase::Select) {
                dead = true;
                return;
            }
            if (radius > baseRadius)
                radius = float(double(radius) * .9);
        } else if (action == 5) {
            if (world.demo || (world.player && world.player->state == Ship::State::GameOver)) {
                colour.a = 0;
                return;
            }
            bool near = false;
            for (size_t n = 0; n < world.pointerHeld.size(); ++n)
                if (world.pointerHeld[n]) {
                    const Vec2 d = world.pointers[n] - position;
                    near |= std::abs(d.x) < double(radius) * std::abs(scale.x) * 1.5 &&
                            std::abs(d.y) < double(radius) * std::abs(scale.y) * 1.5;
                }
            if (world.paused)
                opacity = 1;
            else if (near)
                opacity = double(std::min(1.f, float(opacity) + .1f));
            else if (opacity > 0)
                opacity = float(double(opacity) * .98);
        } else {
            if (world.paused)
                opacity = std::min(.6, opacity + .05);
            else
                opacity *= .95;
        }
        bool held = false;
        if (!owner.touchBlocked)
            for (size_t n = 0; n < world.pointerHeld.size(); ++n)
                if (world.pointerHeld[n]) {
                    const Vec2 d = world.pointers[n] - position;
                    const float r = world.tabletTouch ? radius : radius * 1.2f;
                    if (std::abs(d.x) < double(r) * std::abs(scale.x) &&
                        std::abs(d.y) < double(r) * std::abs(scale.y)) {
                        held = true;
                        break;
                    }
                }
        if (held && !down && (action < 6 || opacity > .5f))
            owner.activate(world, action);
        down = held;
        if (action == 5) {
            owner.pauseOpacity = float(opacity);
        }
        if (action == 6)
            owner.exitOpacity = float(opacity);
        const bool selected =
            owner.menuFocus &&
            ((world.paused && action >= 5 && owner.selection >= 0 &&
              owner.selection < owner.pauseCount() &&
              action == owner.pauseAction(owner.selection)) ||
             (owner.phase == Frontend::Phase::Select && action == owner.selection));
        colour = selected ? world.palette[0] : Colour{};
        colour.a = opacity;
    }
    void prepare(std::vector<ActorDraw> &out) override {
        if (colour.a > .1)
            Entity::prepare(out);
    }
};
void retireButtons(World &w) {
    for (auto *e = w.lists[size_t(Domain::Always)]; e; e = e->next)
        if (dynamic_cast<Button *>(e))
            e->dead = true;
}
} // namespace
void Frontend::line(int x, int row, std::string text, uint32_t colour) {
    writes_.push_back({x, row, std::move(text), colour, false});
}
void Frontend::centre(int row, std::string text, uint32_t colour) {
    writes_.push_back({0, row, std::move(text), colour, true});
}
void Frontend::clear(int first, int last) {
    // A later original clear also erases writes issued earlier in this update.
    // The adapter applies clears before the surviving writes to retained cells.
    writes_.erase(std::remove_if(writes_.begin(), writes_.end(),
                                 [=](const TextLine &l) { return l.y >= first && l.y <= last; }),
                  writes_.end());
    for (int row = first; row <= last; ++row)
        clearRows.push_back(row);
}
void Frontend::enter(Screen next) {
    if (screen == Screen::Scores || screen == Screen::Goals || next == Screen::Scores ||
        next == Screen::Goals)
        pendingClear_ = true;
    screen = next;
    screenTicks = 0;
    selection = 0;
}
void Frontend::goal(int id) {
    goals_.push_back(id);
}
void Frontend::attach(World &w, Scene &scene) {
    w.ownerTick = [this, &w, &scene] { run(w, scene); };
    w.bankRun = [this, &w] {
        records.bank(w, fromBeginning);
        banked_ = true;
    };
    phase = Phase::ResetAttract;
}
void Frontend::makePause(World &w) {
    w.add(Domain::Always, new Button(w, *this, 5, 1, {0, -.35}, .08f, {1, 1}));
    w.add(Domain::Always, new Button(w, *this, 7, 91, {0, -.06}, .06f, {2.4, 25. / 53.}));
    w.add(Domain::Always, new Button(w, *this, 6, 6, {0, .16}, .06f, {2.4, 1}));
    w.add(Domain::Always, new Button(w, *this, 8, 92, {0, .08}, .06f, {2.4, 25. / 53.}));
}
void Frontend::capabilities(bool motion, bool controller) {
    const int action = pauseAction(std::clamp(selection, 0, pauseCount() - 1));
    const bool visible = tiltChoiceVisible();
    motionAvailable_ = motion;
    controllerConnected_ = controller;
    if (visible != tiltChoiceVisible())
        touchBlocked = true;
    if (screen == Screen::Paused) {
        selection = 0;
        for (int i = 0; i < pauseCount(); ++i)
            if (pauseAction(i) == action)
                selection = i;
    }
}
void Frontend::makeSelector(World &w) {
    if (records.unlocked > 0) {
        w.add(Domain::Always, new Button(w, *this, 1, 4, {.42, -.08}, .07f, {22. / 25., 1}));
        w.add(Domain::Always, new Button(w, *this, 2, 5, {-.42, -.08}, .07f, {22. / 25., 1}));
    }
    w.add(Domain::Always, new Button(w, *this, 0, 3, {0, .22}, .07f, {97. / 25., 1}));
    w.add(Domain::Always, new Button(w, *this, 3, 7, {-.3, -.3}, .05f, {97. / 25., 1}));
    w.add(Domain::Always, new Button(w, *this, 4, 8, {.3, -.3}, .05f, {97. / 25., 1}));
}
void Frontend::startAttract(World &w, Scene &scene) {
    // 418c4 attract setup and 421e0 +c8=-999. Keep the game owner, records,
    // sound queue and award session alive while 48f94 rebuilds gameplay lists.
    records.captureTotals(w);
    retireButtons(w);
    w.demo = true;
    w.paused = false;
    w.blueScore = w.redScore = w.score = 0;
    scene.start();
    w.ticks = 0;
    w.presentation.beginAttract();
    makePause(w);
    phase = Phase::Attract;
    enter(Screen::Title);
    attractIndex = attractTick = 0;
    lastPage_ = -1;
    banked_ = false;
    enterSelect_ = false;
    touchBlocked = true;
    clearGrid = true;
    clearGoals();
}
void Frontend::startPlay(World &w, Scene &scene) {
    records.captureTotals(w);
    w.demo = false;
    w.paused = false;
    fromBeginning = selectedLevel == 0;
    // 421e0 Start branch sets the starting scores/lives, then 48f94 rebuilds
    // the scene. Type 1001 selector entities retire on the next UI update.
    scene.start(selectedLevel);
    const auto &start = records.starts[size_t(selectedLevel)];
    w.blueScore = start.blue;
    w.redScore = start.red;
    w.score = start.blue + start.red;
    w.lives = start.lives;
    w.ticks = 0;
    w.presentation.beginPlay();
    phase = Phase::Play;
    enter(Screen::Playing);
    banked_ = false;
    touchBlocked = true;
    clearGrid = true;
    clearGoals();
    for (auto *e = w.lists[size_t(Domain::Always)]; e; e = e->next)
        if (auto *b = dynamic_cast<Button *>(e); b && b->action == 5)
            b->opacity = 1;
}
void Frontend::activate(World &w, int action) {
    if (action == 5) {
        if (w.demo || !w.player || w.player->state == Ship::State::GameOver)
            return;
        w.uiSound(7);
        w.paused = !w.paused;
        enter(w.paused ? Screen::Paused : Screen::Playing);
        return;
    }
    if (action == 6) {
        if (w.paused) {
            w.uiSound(7);
            quitRequested = true;
        }
        return;
    }
    if (action == 7) {
        if (w.paused) {
            w.uiSound(7);
            phase = Phase::ResetAttract;
        }
        return;
    }
    if (action == 8) {
        if (w.paused && tiltChoiceVisible()) {
            records.tiltEnabled = !records.tiltEnabled;
            w.uiSound(7);
        }
        return;
    }
    if (phase != Phase::Select)
        return;
    w.uiSound(7);
    w.ticks = 0;
    if (action == 0) {
        phase = Phase::Start;
        return;
    }
    if (action == 1 || action == 2) {
        selectedLevel =
            (selectedLevel + (action == 1 ? 1 : records.unlocked)) % (records.unlocked + 1);
        pulseDirection = action == 1 ? 1 : -1;
        levelPulse = 2;
        for (auto *e = w.lists[size_t(Domain::Always)]; e; e = e->next)
            if (auto *b = dynamic_cast<Button *>(e); b && b->action == action)
                b->radius = b->baseRadius * 2.f;
    } else if (action == 3 || action == 4) {
        scoreReturn = Screen::Select;
        enter(action == 3 ? Screen::Scores : Screen::Goals);
        w.paused = true;
        clear(3, 33);
    }
}
void Frontend::command(World &w, Scene &, uint32_t edge) {
    const bool accept = (edge & (mino::PAD_A | mino::PAD_START)) != 0,
               back = (edge & (mino::PAD_B | mino::PAD_BACK)) != 0;
    if (screen == Screen::Loading)
        return;
    if (screen == Screen::Scores || screen == Screen::Goals) {
        if (back || accept) {
            enter(scoreReturn);
            w.paused = false;
            clear(3, 33);
        }
        return;
    }
    if (screen == Screen::Error) {
        if (back || accept) {
            w.coverageStop.clear();
            phase = Phase::ResetAttract;
        }
        return;
    }
    if (w.demo && phase == Phase::Attract) {
        if (back)
            quitRequested = true;
        else if (accept)
            enterSelect_ = true;
        return;
    }
    if (phase == Phase::Select) {
        if (back) {
            phase = Phase::ResetAttract;
            return;
        }
        if (edge & (mino::PAD_UP | mino::PAD_DOWN))
            selection = selection == 0 ? 3 : 0;
        if (selection >= 3) {
            if (edge & (mino::PAD_LEFT | mino::PAD_RIGHT))
                selection = 7 - selection;
        } else {
            if (edge & mino::PAD_LEFT)
                activate(w, 2);
            if (edge & mino::PAD_RIGHT)
                activate(w, 1);
        }
        if (accept)
            activate(w, selection);
        return;
    }
    if (screen == Screen::GameOver) {
        if ((back || accept) && w.ticks > 120)
            phase = Phase::ResetAttract;
        return;
    }
    if (w.paused) {
        const int count = pauseCount();
        selection = std::clamp(selection, 0, count - 1);
        if (edge & (mino::PAD_LEFT | mino::PAD_UP))
            selection = (selection + count - 1) % count;
        if (edge & (mino::PAD_DOWN | mino::PAD_RIGHT))
            selection = (selection + 1) % count;
        if (back || (edge & mino::PAD_START))
            activate(w, 5);
        else if (edge & mino::PAD_A)
            activate(w, pauseAction(selection));
    } else if (back || (edge & mino::PAD_START))
        activate(w, 5);
}
void Frontend::tick(World &w) { // source UI owner clock, also callable in model checks
    ++screenTicks;
    if (levelPulse > 1)
        levelPulse = std::max(1.f, levelPulse * .9f);
    if (std::none_of(w.pointerHeld.begin(), w.pointerHeld.end(), [](bool p) { return p; }))
        touchBlocked = false;
}
void Frontend::hud(World &w) {
    line(0, 1, "CAPRICHOSO", 0x0000ffff);
    line(21, 1, "GARANTIDO", 0xff0000ff);
    line(13, 1, "TOTAL", 0xffff00ff);
    char s[32];
    std::snprintf(s, sizeof s, " %8d", w.blueScore);
    line(0, 2, s, 0xcfcfffff);
    std::snprintf(s, sizeof s, " %8d", w.redScore);
    line(21, 2, s, 0xffcfcfff);
    std::snprintf(s, sizeof s, " %8d", w.score);
    line(10, 2, s,
         w.blueScore > w.redScore   ? 0x4040ffff
         : w.redScore > w.blueScore ? 0xff4040ff
                                    : 0xffffffff);
    clear(36, 36);
    line(1, 36,
         std::string(original::AreaNames[size_t(w.area)]) + "-AREA " + std::to_string(w.level + 1),
         0x00ff00ff);
    for (int n = 0; n < std::min(8, w.lives - 1); ++n)
        line(28 - n, 36, "[");
}
void Frontend::scores(const World &w, bool staged) {
    const int elapsed = staged ? attractTick : 10000;
    const int rows[] = {10, 15, 20, 25}, times[] = {201, 401, 601, 801},
              values[] = {records.blueBest, records.redBest, records.blueBest + records.redBest,
                          records.pureBest};
    const char *labels[] = {"CAPRICHOSO'S BEST", "GARANTIDO'S BEST", "COMBINED BEST", "PURE BEST"};
    const uint32_t colours[] = {0x8080ffff, 0xff8080ff, 0xffff80ff, 0x80ff80ff};
    if (elapsed >= 101)
        centre(7, "BEST LOCAL SCORES", pack(w.palette[0]));
    for (int n = 0; n < 4; ++n) {
        if (elapsed >= times[n])
            centre(rows[n], labels[n], colours[n]);
        if (elapsed >= times[n] + 50)
            centre(rows[n] + 2, std::to_string(values[n]), pack(w.palette[0]));
    }
}
void Frontend::attract(World &w) {
    // 4a428: countdown-driven pages. Retained writes intentionally repeat
    // after each threshold; leaving a page explicitly clears rows 3..29.
    const int page = original::AttractOrder[size_t(attractIndex)];
    if (lastPage_ != page) {
        lastPage_ = page;
        retireLabels(w);
        clear(3, 29);
    }
    if (page == 0) {
        clear(3, 3);
        line(13, 5, "PLAY", pack(w.palette[0]));
        line(12, 7, "SUPER", pack(w.palette[13]));
        line(11, 13, "OX WARS!", pack(w.palette[13]));
        bool found = false;
        for (auto *e = w.lists[size_t(Domain::Decor)]; e; e = e->next)
            if (!e->dead && e->type == 0x7000)
                found = true;
        if (!found)
            label(w, 47, 14, 10, .07f, {130. / 86. * 1.4, 1});
        if (w.ticks & 64)
            centre(15, "*** GAME OVER ***", pack(w.palette[0]));
        else
            clear(15, 15);
    } else if (page == 1)
        scores(w, true);
    else {
        for (const auto &l : Lines)
            if (l.page == page && attractTick >= l.time) {
                uint32_t col = 0xffc0ffff;
                if (page == 3 && l.row == 7)
                    col = pack(w.palette[8]);
                else if (page == 4 && l.row == 7)
                    col = pack(w.palette[1]);
                else if (page == 3 && l.row == 14)
                    col = 0x2080ffff;
                else if (page == 4 && l.row == 14)
                    col = 0xff303fff;
                else if (page == 6)
                    col = pack(w.palette[l.row == 5 ? 3 : l.row < 15 ? 2 : l.row < 22 ? 5 : 4]);
                else if (page == 7)
                    col = pack(w.palette[l.row == 5 ? 3 : l.row < 15 ? 2 : l.row < 22 ? 7 : 6]);
                else if (page == 8)
                    col = pack(w.palette[l.row == 5                   ? 3
                                         : l.row == 8 || l.row == 15  ? 0
                                         : l.row == 10 || l.row == 12 ? 6
                                         : l.row == 17                ? 7
                                         : l.row == 26                ? 1
                                                                      : 4]);
                else if (page == 5 && l.row >= 21)
                    col = pack(w.palette[4]);
                line(2, l.row, l.text, col);
            }
        if ((page == 3 || page == 4) && attractTick >= 101) {
            bool found = false;
            for (auto *e = w.lists[size_t(Domain::Decor)]; e; e = e->next)
                if (!e->dead && e->type == 0x7000)
                    found = true;
            if (!found)
                label(w, page == 3 ? 49 : 50, 14, 10, .07f, {68. / 77. * 1.4, 1});
        }
    }
    if (++attractTick > original::AttractDurations[size_t(page)]) {
        attractTick = 0;
        attractIndex = (attractIndex + 1) % 9;
        retireLabels(w);
        clear(3, 29);
        lastPage_ = -1;
    }
}
void Frontend::selector(World &w) {
    for (int row : {12, 14, 19, 21, 23})
        clear(row, row);
    centre(12, "YOU ENTER", pack(w.palette[0]));
    centre(14, std::string("'") + original::AreaNames[size_t(selectedLevel % 10)] + "-AREA'",
           pack(w.palette[2]));
    const auto &start = records.starts[size_t(selectedLevel)];
    centre(19,
           start.lives < 2 ? "START WITH JUST ONE LIFE!"
                           : "START WITH " + std::to_string(start.lives) + " LIVES",
           pack(w.palette[2]));
    if (selectedLevel > 0) {
        centre(21, "START WITH SCORES");
        centre(23, "~4499" + std::to_string(start.blue) + " ~9999+ ~9449" +
                       std::to_string(start.red) + " ~9999] " +
                       std::to_string(start.blue + start.red));
    }
    if (w.ticks > 1000)
        phase = Phase::ResetAttract; // source timeout cancels selection
}
void Frontend::finish(World &w) {
    // 421e0's game-over presentation awards follow 4eed4's record banking.
    const int large = std::max(w.blueCollected, w.redCollected),
              small = std::min(w.blueCollected, w.redCollected);
    const double ratio = large ? double(float(small) / float(large)) : 1.;
    if (large > 1000 && ratio <= .1)
        w.achieveGoal(1);
    if (large > 10000 && ratio <= .1)
        w.achieveGoal(w.blueCollected > w.redCollected ? 8 : 9);
    records.achievements |= w.sessionAchievements;
}
void Frontend::run(World &w, Scene &scene) {
    writes_.clear();
    clearRows.clear();
    clearGrid = pendingClear_;
    pendingClear_ = false;
    ++textRevision;
    tick(w);
    if (phase == Phase::ResetAttract) {
        startAttract(w, scene);
        return;
    }
    if (phase == Phase::Start) {
        startPlay(w, scene);
        return;
    }
    const bool hostView = screen == Screen::Scores || screen == Screen::Goals;
    if (hostView && w.freshPress) {
        for (size_t n = 0; n < w.pointerHeld.size(); ++n)
            if (w.pointerHeld[n]) {
                const Vec2 d = w.pointers[n] - cellCentre(14, 33);
                if (std::abs(d.x) < .1 && std::abs(d.y) < 1. / 38.) {
                    enter(scoreReturn);
                    w.paused = false;
                    clear(3, 33);
                    w.uiSound(7);
                    touchBlocked = true;
                    return;
                }
            }
    }
    hud(w);
    const int priorBonus = scene.bonusTimer;
    const bool priorBoss = w.bossPresent;
    scene.beginTick();
    if (priorBoss && !w.paused && !w.demo)
        centre(5, original::BossMessages[size_t(w.area)], pack(w.palette[0]));
    if (!w.demo && w.player && w.player->state == Ship::State::GameOver) {
        if (screen != Screen::GameOver) {
            if (!banked_)
                scene.endRun();
            finish(w);
            enter(Screen::GameOver);
            if (w.blueCollected != w.redCollected)
                label(w, w.blueCollected > w.redCollected ? 49 : 50, 15, 10, .07f,
                      {68. / 77. * 1.4, 1});
        }
        centre(15, "*** GAME OVER ***", pack(w.palette[0]));
        line(2, 18, "CAPRICHOSO STARS COLLECTED:", 0x4080ffff);
        line(20, 20, std::to_string(w.blueCollected), pack(w.palette[8]));
        line(2, 23, "GARANTIDO HEARTS COLLECTED:", 0xff408fff);
        line(20, 25, std::to_string(w.redCollected), pack(w.palette[1]));
        const int big = std::max(w.blueCollected, w.redCollected),
                  small = std::min(w.blueCollected, w.redCollected);
        if (big == small)
            line(2, 28, "YOU DO NOT FAVOUR EITHER OX.", pack(w.palette[0]));
        else
            line(2, 28,
                 std::string("YOU SUPPORT ") +
                     (w.blueCollected > w.redCollected ? "CAPRICHOSO " : "GARANTIDO ") +
                     std::to_string(int((1. - double(float(small) / float(big))) * 100.)) + "&",
                 pack(w.palette[w.blueCollected > w.redCollected ? 8 : 1]));
        const bool dismiss = w.ticks > 1000 || (w.freshPress && w.ticks > 120);
        if (dismiss)
            w.demo = true; // source publishes demo before the later campaign block
        scene.finishTick();
        if (dismiss)
            startAttract(w, scene);
        return;
    }
    scene.finishTick();
    if (!w.demo) {
        if (priorBonus == 0 && scene.bonusTimer > 0) {
            clear(5, 5);
            const int duration = std::max(scene.blueBonus.steps, scene.redBonus.steps) * 5 + 120;
            auto heading = [](const Scene::Bonus &b) {
                return b.heading +
                       (b.heading.rfind("SORRY", 0) == 0 ? "" : ": " + std::to_string(b.initial));
            };
            w.message(8, 0x0030ff00, duration, scene.blueBonus.detail, 5);
            w.message(10, 8, duration, heading(scene.blueBonus), 5, 5);
            w.message(13, 0xff100000, duration, scene.redBonus.detail, 5);
            w.message(15, 1, duration, heading(scene.redBonus), 5, 5);
            label(w, 20, 2, 9, .04f, {1, 1}, duration);
            w.lists[size_t(Domain::Decor)]->colour = unpack(0x5080ffff);
            label(w, 21, 2, 14, .04f, {1, 1}, duration);
            w.lists[size_t(Domain::Decor)]->colour = unpack(0x8050ffff);
        }
        // 421e0 clears this tally row and prints the zero-filled cc864 buffer.
        // No producer exists in the supplied complete exports; do not invent a
        // remaining-points display in place of the original empty string.
        if (priorBonus > 0 &&
            (scene.bonusState == 3 || scene.bonusState == 1 || scene.bonusTimer == 0))
            clear(20, 20);
        return;
    }
    if (w.player && w.player->state == Ship::State::GameOver && w.ticks > 50) {
        // Original demo reinitialization preserves page/selector state.
        scene.start();
        clearGrid = true;
    }
    if (hostView) {
        clear(3, 33);
        if (screen == Screen::Scores)
            scores(w, false);
        else {
            centre(5, "GOALS", pack(w.palette[0]));
            for (int n = 0; n < 10; ++n)
                centre(8 + n * 2,
                       std::string(records.achievements & (1u << n) ? "[ " : "  ") +
                           original::GoalNames[size_t(n)],
                       records.achievements & (1u << n) ? 0xffff80ffu : 0x808080ffu);
        }
        centre(33, "BACK", pack(w.palette[0]));
        return;
    }
    if (phase == Phase::Attract) {
        attract(w);
        if (enterSelect_ || (w.freshPress && !touchBlocked)) {
            enterSelect_ = false;
            clear(4, 29);
            retireLabels(w);
            phase = Phase::Select;
            // Demo keeps running under the selector, as in 421e0.
            w.ticks = 0;
            touchBlocked = true;
            selection = 0;
            enter(Screen::Select);
            makeSelector(w);
            w.uiSound(10);
        }
    } else if (phase == Phase::Select)
        selector(w);
}
void Frontend::prepare(const World &, const Scene &, std::vector<TextLine> &text,
                       std::vector<ActorDraw> &draws) const {
    text = writes_;
    draws.clear(); // UI and labels now execute through their original lists.
}
} // namespace superoxwars
