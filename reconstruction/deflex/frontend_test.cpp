// Production frontend/controller and complete original history-script integration.
// Scripted inputs are regression coverage, not an original-execution witness.
#include "frontend.h"
#include <fstream>
#include <regex>
#include <cstdio>
#include <cmath>
#include <cstring>
using namespace deflex;
using namespace neon::input;
struct Output : GridRenderer, ActorRenderer {
    int sprites = 0, quads = 0, alphaQuads = 0;
    std::array<GridVertex, 6> firstQuad{};
    bool finite = true, graph = false, nextButton = false;
    neon::colour playColour;
    neon::colour pauseColour;
    bool pauseDrawn = false;
    neon::vec2d playSize, playPosition;
    int panels = 0;
    double firstPanelX = 0;
    std::vector<neon::vec2d> stars;
    void emit(const ActorSprite &s) override {
        ++sprites;
        graph |= s.customCorners;
        nextButton |= s.frame == 42;
        if (s.frame == 107) {
            pauseColour = s.colour;
            pauseDrawn = true;
        }
        if (s.frame == 106) {
            playColour = s.colour;
            playSize = s.size;
            playPosition = s.position;
        }
        if (s.frame == 102 && panels++ == 0)
            firstPanelX = s.position.x;
        if (s.frame == 30)
            stars.push_back(s.position);
        finite &= std::isfinite(s.position.x) && std::isfinite(s.position.y);
        for (double x : s.corners)
            finite &= std::isfinite(x);
    }
    void quad(const std::array<GridVertex, 6> &q, bool additive, int) override {
        if (!quads)
            firstQuad = q;
        ++quads;
        if (!additive)
            ++alphaQuads;
        for (auto &v : q)
            finite &= std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.u) &&
                      std::isfinite(v.v);
    }
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream file(argv[1]);
    std::string xml((std::istreambuf_iterator<char>(file)), {});
    const std::regex entry(
        R"(<Spentry[^>]*>\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),)");
    std::vector<neon::vec2d> sizes;
    for (auto i = std::sregex_iterator(xml.begin(), xml.end(), entry); i != std::sregex_iterator();
         ++i)
        sizes.push_back({double(std::stoi((*i)[4])), double(std::stoi((*i)[5]))});
    int failures = 0;
    auto check = [&](bool pass, const char *text) {
        if (!pass) {
            ++failures;
            std::fprintf(stderr, "%s\n", text);
        }
    };
    {
        Progress fadeProgress;
        Frontend fade(fadeProgress, sizes);
        auto key = [&](uint32_t b) {
            fade.pads(0, 0, nullptr, 0);
            fade.pads(b, b, nullptr, 0);
            fade.tick();
        };
        key(South);
        key(South);
        fade.world().ball().mesh.startupFrames = 10000; // Isolate visibility from ball depletion.
        auto alpha = [&]() {
            Output capture;
            fade.drawUI(capture, capture, {512, 512});
            check(capture.pauseDrawn, "Gameplay publishes pause sprite");
            return float(capture.pauseColour.a);
        };
        auto bits = [](float a) {
            uint32_t value;
            std::memcpy(&value, &a, sizeof value);
            return value;
        };
        check(bits(alpha()) == 0x3f7ae148u, "First gameplay tick uses rounded double .98 decay");
        for (int i = 0; i < 60; ++i)
            fade.tick();
        check(fade.page == Frontend::Page::Game && bits(alpha()) == 0x3e954ce1u,
              "Pause fade follows source double arithmetic then float storage over 61 ticks");
        const float before = alpha();
        fade.touch(7, 0, {.4, .4});
        for (int i = 0; i < 20; ++i)
            fade.tick();
        check(alpha() == before, "First touch in bottom strip holds visibility");
        fade.touch(8, 0, {.4, 0});
        fade.tick();
        check(alpha() == before, "Second touch does not override first-touch visibility rule");
        fade.touch(8, 2, {.4, 0});
        fade.touch(7, 1, {.4, 0});
        for (int i = 0; i < 20; ++i)
            fade.tick();
        check(alpha() == 1, "Upper touch reveals pause and clamps alpha to one");
        fade.touch(7, 2, {.4, 0});
        for (int i = 0; i < 601; ++i)
            fade.tick();
        check(bits(alpha()) == 0x36b2e790u && int(alpha() * 255) == 0,
              "Released pause icon fades below one output alpha byte");
        const auto frozen = alpha();
        key(Start); // Pause contains no gameplay pause sprite.
        Output paused;
        fade.drawUI(paused, paused, {512, 512});
        check(!paused.pauseDrawn, "Paused menu replaces gameplay pause sprite");
        for (int i = 0; i < 30; ++i)
            fade.tick();
        key(Down);
        key(South);
        check(fade.page == Frontend::Page::Scores, "Pause scores route");
        for (int i = 0; i < 20; ++i)
            fade.tick();
        fade.back();
        key(Start);
        check(fade.page == Frontend::Page::Game && alpha() == float(double(frozen) * .98),
              "Pause and scores freeze stored fade; resume continues it");
        fade.touch(9, 0, {0, -.38});
        check(fade.page == Frontend::Page::Paused,
              "Faded pause retains its touch activation route");
        fade.touch(9, 1, {0, -.36});
        fade.touch(9, 2, {0, -.36});
        check(fade.page == Frontend::Page::Paused, "Pause-opening finger cannot resume on release");
        fade.touch(99, 2, {0, -.36});
        check(fade.page == Frontend::Page::Paused,
              "Unowned release cannot activate pause controls");
        key(Down);
        key(Down);
        key(South);
        check(fade.page == Frontend::Page::Game && bits(alpha()) == 0x3f7ae148u,
              "Retry restores initial visibility");
        Output tint;
        fade.drawUI(tint, tint, {512, 512});
        const auto expected = fade.world().context.palette[4];
        check(tint.pauseColour.r == expected.r && tint.pauseColour.g == expected.g &&
                  tint.pauseColour.b == expected.b,
              "Pause uses source type-two palette colour with independent alpha");
    }
    {
        Progress p;
        Frontend ui(p, sizes);
        ui.pads(0, 0, nullptr, 0);
        ui.pads(South, South, nullptr, 0);
        ui.tick();
        ui.pads(0, 0, nullptr, 0);
        ui.pads(Down, Down, nullptr, 0);
        ui.tick();
        const auto colour = [&]() {
            Output o;
            ui.drawUI(o, o, {512, 512});
            return o.playColour.packRGBA();
        };
        const auto plain = colour();
        check(plain == ui.world().context.palette[4].packRGBA(),
              "Touch selector does not show controller focus");
        std::array<ControllerState, 4> pads{};
        pads[3].connected = true;
        ui.pads(0, 0, pads.data(), 4);
        check(colour() == ui.world().context.palette[0].packRGBA(),
              "An idle controller in any slot enables selector focus");
        ui.pads(0, 0, nullptr, 0);
        check(colour() == plain, "Disconnect restores the selector's ordinary colour");
        ui.keyboardNavigation = true;
        ui.pads(0, 0, nullptr, 0);
        check(colour() == ui.world().context.palette[0].packRGBA(),
              "Desktop keyboard enables menu focus without a gamepad");
        ui.keyboardNavigation = false;
        ui.pads(0, 0, nullptr, 0);
        check(colour() == plain, "Losing navigation input removes the selection tint");
    }
    Progress progress;
    Frontend front(progress, sizes);
    check(front.feedback().scaleX == .99f, "source title feedback uses its .99 history scale");
    Output out;
    auto neutral = [&] { front.pads(0, 0, nullptr, 0); };
    auto press = [&](uint32_t button) {
        neutral();
        front.pads(button, button, nullptr, 0);
        front.tick();
    };
    neutral();
    press(South);
    check(front.page == Frontend::Page::Selector,
          "title accepts native confirmation into original selector");
    check(front.feedback().scaleX == .995f && front.world().context.frameCount == 3,
          "selector sets its original history scale and phase 3");
    for (int i = 0; i < 20; ++i) {
        front.pads(South, 0, nullptr, 0);
        front.tick();
    }
    check(front.page == Frontend::Page::Selector, "held title press cannot also select a level");
    front.drawGrid(out, out, {512, 512}, false);
    front.drawActors(out, {512, 512}, false);
    front.drawUI(out, out, {512, 512});
    check(out.graph && out.sprites > 0 && out.quads > 0 && out.finite,
          "selector draws actual source panels, score graph, actors and proportional messages");
    check(front.unlocked() == 3, "fresh original progression opens the first four stages");
    progress.bestScores[0] = progress.bestScores[1] = progress.bestScores[2] = 1;
    check(front.unlocked() == 7, "three positive records in a group open the next four stages");
    front.world().sessionAchievements = 1u << 4;
    const auto selectorClock = front.world().context.frameCount;
    press(South);
    check(front.page == Frontend::Page::Game, "selector starts selected native stage");
    check(front.world().context.frameCount == selectorClock + 1 && front.feedback().scaleX == .996f,
          "newLevel keeps the game clock and selects gameplay feedback scale");
    check(front.world().sessionAchievements == (1u << 4),
          "newLevel preserves the original game-owner award gate");
    auto *existing = &front.world();
    for (int i = 0; i < 60; ++i)
        front.tick();
    const auto age = front.world().ball().age;
    press(Start);
    check(front.page == Frontend::Page::Paused, "native Start opens source pause controls");
    check(front.feedback().colour[0] == .15f && front.feedback().scaleX == .996f,
          "pause dims feedback without resetting its transform");
    for (int i = 0; i < 5; ++i) {
        front.pads(Start, 0, nullptr, 0);
        front.tick();
    }
    check(front.page == Frontend::Page::Paused && front.world().ball().age == age,
          "held Start does not reopen or advance paused simulation");
    press(South);
    check(front.page == Frontend::Page::Game && &front.world() == existing &&
              front.world().ball().age == age + 1,
          "Resume retains the actual run instead of recreating it");
    front.world().context.score = 50;
    front.world().context.bestScore = 100;
    progress.bestScores[0] = 100;
    front.world().level.targetsRemaining = 0;
    front.tick();
    check(front.page == Frontend::Page::Result, "last-target completion enters result flow");
    front.tick();
    front.tick();
    check(!front.resultSummary() &&
              front.world().gridView.lines[5].text == "YOUR SCORE IS 50% OF THE LOCAL BEST",
          "settled result first presents the source percentage cue, not immediate retry controls");
    press(South);
    check(front.resultSummary() && front.world().gridView.lines[5].text == "YOUR SCORE:",
          "confirmation skips the result cue into its separate summary");
    for (int i = 0; i < 8; ++i) {
        front.pads(South, 0, nullptr, 0);
        front.tick();
    }
    check(front.resultSummary() && front.page == Frontend::Page::Result,
          "held cue confirmation cannot also activate Next");
    press(South);
    check(front.page == Frontend::Page::Game && front.selected() == 1,
          "result confirmation starts next level after source score settlement");
    check((front.world().sessionAchievements & (1u << 4)) != 0,
          "award gate survives result arena retirement and next-level construction");
    front.world().context.score = 1000;
    front.world().context.levelFinished = true;
    front.world().context.levelSucceeded = false;
    front.tick();
    check(front.world().context.score == 0 && progress.bestScores[1] == 1,
          "failed-attempt points are discarded before record commits");
    front.tick();
    int cueTicks = 0;
    while (!front.resultSummary() && ++cueTicks < 300)
        front.tick();
    check(cueTicks == 237 && front.resultSummary(),
          "source result cue advances from phase 3 to summary at 240 without input");
    check(front.world().context.targets == nullptr &&
              front.world().particles.particles(ParticleLayer::World).empty(),
          "summary retires the arena instead of retaining live gameplay actors");
    front.back();
    front.back(); // result -> title -> host exit, independently tested below
    Progress fresh;
    Frontend historyFront(fresh, sizes);
    auto step = [&](uint32_t held, uint32_t pressed) {
        ControllerState pad;
        pad.connected = true;
        pad.buttons = held;
        pad.pressed = pressed;
        historyFront.pads(held, pressed, &pad, 1);
        historyFront.tick();
        historyFront.drawGrid(out, out, {512, 512}, false);
        historyFront.drawActors(out, {512, 512}, false);
        historyFront.drawUI(out, out, {512, 512});
    };
    step(0, 0);
    step(South, South);
    step(0, 0);
    step(Down, Down);
    step(0, 0);
    step(Left, Left);
    step(0, 0);
    step(South, South);
    step(0, 0);
    check(historyFront.page == Frontend::Page::Info,
          "selector bottom row reaches Info horizontally");
    step(South, South);
    step(0, 0);
    check(historyFront.page == Frontend::Page::History, "Info starts native interactive history");
    bool modes[3] = {}, placed[3] = {};
    int trialTicks = 0, lastPhase = -1;
    try {
        for (int frame = 0; frame < 20000 && historyFront.history()->phase != 12; ++frame) {
            int phase = historyFront.history()->phase;
            if (phase != lastPhase) {
                std::printf("History phase %d at scripted frame %d\n", phase, frame);
                trialTicks = 0;
                lastPhase = phase;
            }
            if (historyFront.history()->trial()) {
                check((historyFront.world().context.balls != nullptr) == (phase == 11),
                      "only C64 history uses the source main ball list; PET/VIC use overlay");
                modes[phase == 5 ? 0 : phase == 8 ? 1 : 2] = true;
                // Source controls finish their entrance before accepting placement.
                if (++trialTicks == 100)
                    step(Start, Start);
                else if (trialTicks == 60) {
                    step(East, East);
                    check(historyFront.world().input.pending.has_value() == (phase == 11),
                          "overlay PET/VIC consume a new control before tick end; C64 queues it "
                          "for next tick");
                } else
                    step(0, 0);
                if (historyFront.history()->trial()) {
                    auto &grid = historyFront.world().level.grid;
                    for (int y = 0; y <= grid.rows; ++y)
                        for (int x = 0; x <= grid.cols; ++x) {
                            const int glyph = grid.peek(x, y) & 255;
                            placed[phase == 5   ? 0
                                   : phase == 8 ? 1
                                                : 2] |= glyph == 128 || glyph == 130;
                        }
                }
            } else {
                bool activate = frame % 12 < 2;
                step(activate ? South : 0, frame % 12 == 0 ? South : 0);
            }
        }
        check(
            historyFront.history()->phase == 12 && modes[0] && modes[1] && modes[2],
            "original scripts reach PET, VIC and C64 interactive trials and their terminal state");
        check(placed[0] && placed[1] && placed[2],
              "direct canonical controller input places deflectors in every character trial");
        check(out.quads > 100 && out.finite, "console backgrounds, original glyphs and character "
                                             "actors produce finite draw commands");
        step(0, 0);
        step(Back, Back);
        step(Back, 0);
        check(historyFront.page == Frontend::Page::Info,
              "history Back returns to Info and carries held-button release state");
    } catch (const std::exception &e) {
        ++failures;
        std::fprintf(stderr, "history failed: %s\n", e.what());
    }
    Frontend analog(fresh, sizes);
    ControllerState stick;
    stick.connected = true;
    analog.pads(0, 0, &stick, 1);
    analog.pads(South, South, &stick, 1);
    analog.pads(0, 0, &stick, 1);
    stick.ly = .8f;
    analog.pads(0, 0, &stick, 1);
    stick.ly = 0;
    analog.pads(0, 0, &stick, 1);
    stick.lx = -.8f;
    analog.pads(0, 0, &stick, 1);
    stick.lx = 0;
    analog.pads(0, 0, &stick, 1);
    analog.pads(South, South, &stick, 1);
    check(analog.page == Frontend::Page::Info,
          "native stick navigates frontend without touch translation");
    Frontend continuity(fresh, sizes);
    Output priorStars;
    for (int i = 0; i < 8; ++i) {
        continuity.tick();
        priorStars.stars.clear();
        continuity.drawGrid(priorStars, priorStars, {512, 512}, false);
    }
    continuity.pads(0, 0, nullptr, 0);
    continuity.pads(South, South, nullptr, 0);
    continuity.tick();
    Output continuedStars;
    continuity.drawGrid(continuedStars, continuedStars, {512, 512}, false);
    bool preservedStar = false;
    for (auto before : priorStars.stars)
        for (auto after : continuedStars.stars)
            preservedStar |= before.x == after.x && after.y > before.y && after.y - before.y < 3;
    check(preservedStar, "title-to-selector keeps existing star positions and advances them "
                         "instead of restarting the background");
    CellGrid consoleGrid;
    GridView consoleView(consoleGrid);
    neon::DarwinRandom consoleRandom(1);
    Console console(consoleGrid, consoleView, consoleRandom);
    console.setMode(Console::Spectrum);
    check(consoleGrid.cols == 36 && consoleGrid.rows == 24 && consoleView.fontY == 108 &&
              consoleGrid.peek(2, 21) == '%' && console.x == 2 && console.y == 2 &&
              console.palette[15].packARGB() == 0xefefefef,
          "source Spectrum preset restores its font, window, boot copyright and paper colour");
    console.start("/s3/x5/y5TEST/q");
    console.run(false, true);
    check(console.mode == Console::Atari8 && consoleGrid.cols == 42 && consoleGrid.rows == 30 &&
              console.left == 1 && console.top == 3 && consoleView.fontY == 162 &&
              consoleGrid.peek(2, 4) == 'R' && consoleGrid.peek(5, 5) == 'T' &&
              console.palette[1].packARGB() == 0xff7f5300,
          "Atari boot preserves the caller script and uses original bounds, glyphs and palette");
    console.start("/s-/q");
    console.run(false, true);
    check(console.mode == Console::Spectrum,
          "console machine decrement reaches the recovered Spectrum mode");
    Progress allOpen;
    allOpen.bestScores.fill(1);
    Frontend last(allOpen, sizes);
    auto lastPress = [&](uint32_t button) {
        last.pads(0, 0, nullptr, 0);
        last.pads(button, button, nullptr, 0);
        last.tick();
    };
    lastPress(South);
    for (int i = 0; i < 51; ++i)
        lastPress(Right);
    lastPress(South);
    check(last.selected() == 51 && last.page == Frontend::Page::Game,
          "all-unlocked route reaches the final standard stage");
    last.world().context.score = 100;
    last.world().level.targetsRemaining = 0;
    last.tick();
    last.tick();
    lastPress(South);
    Output finalOutput;
    last.drawUI(finalOutput, finalOutput, {512, 512});
    check(last.resultSummary() && !finalOutput.nextButton && allOpen.achievements[8],
          "final stage omits Next and all-unlocked result records the original local achievement");
    {
        // Source rules: Run_Module 4a288 ticks the cursor outside the pause
        // gate, while rawGlyph 4408c suppresses proportional text when paused.
        CellGrid grid;
        grid.setSize(2, 4);
        GridView view(grid);
        view.stars = false;
        neon::DarwinRandom random(1);
        std::array<neon::colour, 11> palette;
        view.quick("PAUSED HUD", {0, 0}, {1, 1}, -1, 0, 0);
        view.lines[0].state = 1;
        view.lines[0].alpha = 1;
        const auto phase = view.lines[0].phase;
        Output paused;
        for (int i = 0; i < 30; ++i)
            view.draw(grid, palette.data(), random, true, paused, paused, {512, 512});
        check(paused.quads == 0 && !view.cursorVisible && view.cursorClock == 30 &&
                  view.lines[0].phase == phase && random.draws() == 0,
              "paused grid keeps cursor cadence but suppresses HUD glyphs and freezes text/stars");
        view.text(paused, "RESUME", {0, 0}, {1, 1}, 0xffffffff);
        check(paused.alphaQuads > 0,
              "host pause labels remain drawable independently of frozen source text");
        Output resumed;
        view.draw(grid, palette.data(), random, false, resumed, resumed, {512, 512});
        check(resumed.alphaQuads > 0 && view.lines[0].phase == phase + 1,
              "resuming redraws source text and advances its retained phase once");

        // outch 4605c distinguishes a flat foreground spin from 3D cell
        // rotation. Use quarter-turn geometry, independently known positions.
        view.lines[0].text.clear();
        auto &cell = grid.cell(0, 0);
        cell.bgOn = 1;
        cell.attr = 1;
        cell.bgAngle = 1.5707964f;
        Output flatBackground;
        view.draw(grid, palette.data(), random, false, flatBackground, flatBackground, {512, 512});
        check(flatBackground.quads == 1 && flatBackground.firstQuad[0].x == -256 &&
                  flatBackground.firstQuad[0].y == 256,
              "flat background ignores its atlas angle, as the original pass-zero branch does");
        cell.bgOn = 0;
        cell.fgOn = 1;
        cell.attr = 2;
        cell.fgIcon = 1;
        cell.fgAngle = 1.5707964f;
        cell.rot.z = 1;
        Output flatForeground;
        view.draw(grid, palette.data(), random, false, flatForeground, flatForeground, {512, 512});
        check(
            flatForeground.quads == 1 && std::fabs(flatForeground.firstQuad[0].x) < .001f &&
                std::fabs(flatForeground.firstQuad[0].y - 320) < .001f,
            "flat foreground spin excludes cell Z rotation and retains the non-square cell centre");
        cell.fgOn = 0;
        cell.bgOn = 1;
        cell.attr = 1;
        cell.bgAngle = 0;
        cell.rot = {1.5707963267948966, 0, 0};
        cell.offset = {2, 3, 4};
        Output displaced;
        view.draw(grid, palette.data(), random, false, displaced, displaced, {512, 512});
        check(displaced.quads == 1 && std::fabs(displaced.firstQuad[0].x + 254) < .001f &&
                  std::fabs(displaced.firstQuad[0].y - 189) < .001f &&
                  std::fabs(displaced.firstQuad[0].z - 550) < .001f,
              "3D cell rotation applies its source centre, offset and depth");
    }
    {
        Progress records;
        Frontend nav(records, sizes);
        nav.keyboardNavigation = true;
        auto move = [&](uint32_t button) {
            nav.pads(0, 0, nullptr, 0);
            nav.pads(button, button, nullptr, 0);
            nav.tick();
        };
        Output title;
        nav.drawUI(title, title, {512, 512});
        check(title.quads == 0, "title has no Info action or invisible selectable menu row");
        move(Down);
        move(Start);
        check(nav.page == Frontend::Page::Selector,
              "title ignores navigation and Start always enters level selection");
        move(Right);
        Output sliding;
        nav.drawUI(sliding, sliding, {512, 512});
        check(nav.selected() == 1 && sliding.firstPanelX < 0 && sliding.firstPanelX > -102.4,
              "native right changes the target immediately and animates the existing carousel");
        for (int i = 0; i < 40; ++i) {
            nav.pads(Right, 0, nullptr, 0);
            nav.tick();
        }
        Output settled;
        nav.drawUI(settled, settled, {512, 512});
        check(nav.selected() == 1 && std::fabs(settled.firstPanelX + 102.4) < .001,
              "held direction does not generate extra selections and the slide settles on its "
              "target");
        check(std::fabs(settled.firstQuad[0].y - 51.2f) < .001f &&
                  std::fabs(settled.playSize.x - .168 * 512) < .001 &&
                  std::fabs(settled.playSize.y - .12 * 512) < .001 &&
                  std::fabs(settled.playPosition.x - .4 * 512) < .001 &&
                  std::fabs(settled.playPosition.y - .43 * 512) < .001,
              "arrows align with panel centres and Play retains its original corner position and "
              "dimensions");
        move(Down);
        Output play;
        nav.drawUI(play, play, {512, 512});
        check(play.playColour.packRGBA() == nav.world().context.palette[0].packRGBA(),
              "entering the bottom row highlights Play by default");
        move(Left);
        check(nav.selected() == 1, "bottom row left/right never changes the selected level");
        move(South);
        check(nav.page == Frontend::Page::Info,
              "left from Play selects Info in physical horizontal order");
        move(East);
        move(Down);
        move(Left);
        move(Left);
        move(South);
        check(nav.page == Frontend::Page::Title, "two left presses from Play select Back");
        nav.touch(1, 0, {0, .43});
        nav.touch(1, 2, {0, .43});
        check(nav.page == Frontend::Page::Selector,
              "tapping the former title Info area enters the selector");
        const int before = nav.selected();
        nav.touch(2, 0, {.45, -.1});
        nav.touch(2, 2, {.45, -.1});
        nav.tick();
        check(nav.selected() == before + 1,
              "touch arrows share the native animated level-step action");
        move(Down);
        move(Left);
        move(Up);
        move(Down);
        Output returned;
        nav.drawUI(returned, returned, {512, 512});
        check(returned.playColour.packRGBA() == nav.world().context.palette[0].packRGBA(),
              "returning to the bottom row resets its default to Play");
        move(South);
        check(nav.page == Frontend::Page::Game && nav.world().context.levelIndex == before + 1,
              "Play launches the selected target during its smooth transition");
        Frontend swipe(records, sizes);
        swipe.touch(3, 0, {0, 0});
        swipe.touch(3, 2, {0, 0});
        swipe.touch(4, 0, {0, 0});
        swipe.touch(4, 1, {-.15, 0});
        swipe.tick();
        swipe.touch(4, 2, {-.15, 0});
        swipe.tick();
        Output moved;
        swipe.drawUI(moved, moved, {512, 512});
        check(swipe.page == Frontend::Page::Selector && moved.firstPanelX < 0,
              "touch still scrolls the carousel with its original drag and release inertia");
        Frontend corner(records, sizes);
        corner.touch(5, 0, {0, 0});
        corner.touch(5, 2, {0, 0});
        corner.touch(6, 0, {.4, .34});
        corner.touch(6, 2, {.4, .34});
        check(corner.page == Frontend::Page::Selector,
              "former raised Play hit area no longer launches a level");
        corner.touch(7, 0, {.4, .43});
        corner.touch(7, 2, {.4, .43});
        check(corner.page == Frontend::Page::Game,
              "Original lower-right Play touch area launches the selected level");
    }
    {
        Progress records;
        Frontend pauseMenu(records, sizes);
        auto pressOnce = [&](uint32_t button) {
            pauseMenu.pads(0, 0, nullptr, 0);
            pauseMenu.pads(button, button, nullptr, 0);
            pauseMenu.tick();
        };
        auto start = [&] {
            pressOnce(South);
            pressOnce(South);
            pressOnce(Start);
        };
        start();
        struct Buttons : Output {
            std::vector<ActorSprite> items;
            void emit(const ActorSprite &s) override {
                items.push_back(s);
                Output::emit(s);
            }
        } buttons;
        pauseMenu.drawUI(buttons, buttons, {512, 512});
        const int frames[] = {104, 113, 41, 114, 105};
        bool ordered = buttons.items.size() == 5;
        for (size_t i = 0; i < std::min(size_t(5), buttons.items.size()); ++i)
            ordered &= buttons.items[i].frame == frames[i] &&
                       (i == 0 || buttons.items[i].position.y > buttons.items[i - 1].position.y) &&
                       std::fabs(buttons.items[i].size.y - .168 * 512) < .001;
        check(
            ordered && sizes.size() >= 115 && buttons.quads == 0,
            "pause uses five consistently styled sprites in Resume/Scores/Retry/Title/Quit order");
        auto *pausedWorld = &pauseMenu.world();
        pressOnce(Down);
        pressOnce(South);
        check(pauseMenu.page == Frontend::Page::Scores && pauseMenu.paused() &&
                  &pauseMenu.world() == pausedWorld,
              "second pause action opens scores without replacing or unpausing the run");
        pressOnce(East);
        pauseMenu.pads(South, 0, nullptr, 0);
        pauseMenu.tick();
        check(pauseMenu.page == Frontend::Page::Paused,
              "returning from scores waits for held input to release");
        pauseMenu.world().context.score = 123;
        pressOnce(Down);
        pressOnce(South);
        check(pauseMenu.page == Frontend::Page::Game && pauseMenu.world().context.score == 0,
              "return focus remains on Scores and the next item retries the selected stage");
        pressOnce(Start);
        pressOnce(Down);
        pressOnce(Down);
        pressOnce(Down);
        pressOnce(South);
        check(pauseMenu.page == Frontend::Page::Title && !pauseMenu.quit,
              "Title returns to the game title without requesting an iOX exit");
        start();
        pauseMenu.touch(8, 0, {0, -.18});
        pauseMenu.touch(8, 2, {0, -.18});
        check(pauseMenu.page == Frontend::Page::Scores,
              "touch uses the moved Hi-Scores button area");
        pauseMenu.back();
        pauseMenu.touch(9, 0, {0, .18});
        pauseMenu.touch(9, 2, {0, .18});
        check(pauseMenu.page == Frontend::Page::Title && !pauseMenu.quit,
              "touch Title returns to the title screen");
        start();
        pressOnce(Up);
        pressOnce(South);
        check(pauseMenu.quit && pauseMenu.page == Frontend::Page::Paused,
              "Quit requests host/iOX exit rather than entering Title");
    }
    if (failures)
        return 1;
    std::puts("Deflex frontend: title/selector, original progression, held input, pause/resume, "
              "result/records, all tutorial scripts and three character trials passed.");
}
