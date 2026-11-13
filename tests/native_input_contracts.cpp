#ifdef NDEBUG
#undef NDEBUG
#endif
#define NEON_GAME minotron
#include "cores/minotron/engine/game.h"
#include "cores/minotron/engine/neon_modules.h"
#undef NEON_GAME
#define NEON_GAME minorescue
#include "cores/minorescue/engine/game.h"
#include "cores/minorescue/engine/neon_modules.h"
#undef NEON_GAME
#include "core_api.h"
#include "grid_menus.h"
#include "cores/minotron/mode_preference.h"
#include <sstream>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
namespace mt = minotron::Neon_Internals;
namespace mr = minorescue::Neon_Internals;
using neon::input::ControllerState;

// Saved reference controller-to-iCade adapter, used only as a regression oracle.
static unsigned oldWord(const ControllerState &p) {
    using namespace mino;
    unsigned b = 0;
    if (!p.connected)
        return b;
    if (p.lx < -.4f || (p.buttons & PAD_LEFT))
        b |= 8;
    if (p.lx > .4f || (p.buttons & PAD_RIGHT))
        b |= 2;
    if (p.ly < -.4f || (p.buttons & PAD_UP))
        b |= 1;
    if (p.ly > .4f || (p.buttons & PAD_DOWN))
        b |= 4;
    if (p.rx < -.4f || (p.buttons & PAD_X))
        b |= 0x10;
    if (p.rx > .4f || (p.buttons & PAD_B))
        b |= 0x100;
    if (p.ry < -.4f || (p.buttons & PAD_Y))
        b |= 0x40;
    if (p.ry > .4f || (p.buttons & PAD_A))
        b |= 0x80;
    if (p.buttons & (PAD_START | PAD_RB | PAD_LB) || p.rt > .5f || p.lt > .5f)
        b |= 0x200;
    return b;
}
static void minotronControls() {
    ControllerState pads[2];
    pads[0].connected = pads[1].connected = true;
    int cases = 0;
    for (float axis : {-.40001f, -.4f, -.39999f, 0.f, .39999f, .4f, .40001f})
        for (uint32_t buttons = 0; buttons < 16384; ++buttons) {
            pads[0].buttons = buttons;
            pads[0].lx = pads[0].ry = axis;
            pads[0].ly = pads[0].rx = -axis;
            pads[0].lt = (buttons & 1) ? .5f : .50001f;
            pads[1].buttons = (buttons & 2) ? mino::PAD_LEFT | mino::PAD_UP : 0;
            unsigned old = oldWord(pads[0]) | oldWord(pads[1]);
            auto c = minotron::module::controllerInput(pads, 2);
            assert(c.move.left == bool(old & 8) && c.move.right == bool(old & 2));
            assert(c.move.up == bool(old & 1) && c.move.down == bool(old & 4));
            assert(c.aim.left == bool(old & 0x10) && c.aim.right == bool(old & 0x100));
            assert(c.aim.up == bool(old & 0x40) && c.aim.down == bool(old & 0x80));
            assert(c.fire == bool(old & 0xff0) && c.advance == bool(old & 0xffa));
            ++cases;
        }
    pads[0].connected = pads[1].connected = false;
    assert(!minotron::module::controllerInput(pads, 2).any());
    std::cout
        << cases
        << " Minotron mappings match the virtual-touch reference, including opposite directions and thresholds\n";
}
static void rescueControls() {
    int compared = 0;
    for (int players = 1; players <= 4; ++players)
        for (bool flipped : {false, true}) {
            mr::controlBlock reference, direct;
            reference.demo = direct.demo = false;
            reference.joyZones = direct.joyZones = players;
            reference.inputFlipped = direct.inputFlipped = flipped;
            bool down[4]{};
            uint32_t random = 19;
            for (int tick = 0; tick < 600; ++tick) {
                bool enabled = tick % 150 < 130;
                if (tick % 180 == 70) {
                    reference.purgeTouches();
                    direct.purgeTouches();
                }
                // Mix a real finger with controllers, including first-claim wins
                // and the rule that a held rejected controller must release first.
                if (tick % 100 == 5) {
                    reference.processJoyDown(91, {-.3, -.3});
                    direct.processJoyDown(91, {-.3, -.3});
                }
                if (tick % 100 == 45) {
                    reference.processJoyUp(91);
                    direct.processJoyUp(91);
                }
                std::array<neon::input::PlayerMotion, 4> input{};
                for (int i = 0; i < 4; ++i) {
                    random = random * 1664525u + 1013904223u;
                    float x = float(int((random >> 16) % 201) - 100) / 100.f;
                    random = random * 1664525u + 1013904223u;
                    float y = float(int((random >> 16) % 201) - 100) / 100.f;
                    bool held = x * x + y * y > .04f && ((tick + i * 7) % 53 < 39);
                    input[i] = {held, double(x), double(y)};
                    // Reference virtual-touch driver, through the real original
                    // touch methods. This reference never receives direct input.
                    const double origin[4][2] = {
                        {-.25, -.25}, {.25, .25}, {-.25, .25}, {.25, -.25}};
                    bool want = enabled && held;
                    unsigned id = 0x10000u + unsigned(i);
                    if (want && !down[i]) {
                        reference.processJoyDown(id, {origin[i][0], origin[i][1]});
                        down[i] = true;
                    } else if (!want && down[i]) {
                        reference.processJoyUp(id);
                        down[i] = false;
                    }
                    if (down[i])
                        reference.processJoyMove(
                            id, {origin[i][0] + double(x) * .12, origin[i][1] + double(y) * .12});
                }
                direct.controllers.update(input, players, flipped, enabled, direct.joyActive);
                for (int player = 0; player < 4; ++player) {
                    const auto a = reference.playerControl(player),
                               b = direct.playerControl(player);
                    if (a.active != b.active) {
                        std::cerr << "Rescue mismatch players=" << players << " flipped=" << flipped
                                  << " tick=" << tick << " slot=" << player
                                  << " reference=" << a.active << " direct=" << b.active << "\n";
                        for (int k = 0; k < 4; ++k)
                            std::cerr << k << " held=" << input[k].active << " oldDown=" << down[k]
                                      << " touch=" << direct.joyActive[k] << "\n";
                        assert(false);
                    }
                    if (a.active) {
                        assert(std::abs(a.x - b.x) < 1.e-15 && std::abs(a.y - b.y) < 1.e-15);
                    }
                    if (direct.joyActive[player])
                        assert(direct.joyTouchId[player] == 91);
                    if (direct.touchActive[player])
                        assert(direct.touchId[player] == 91);
                    ++compared;
                }
            }
            direct.controllers.cancel();
            for (int p = 0; p < 4; ++p)
                assert(!direct.controllers.motion(p).active);
        }
    std::cout
        << compared
        << " Rescue player/tick outputs match the previous touch driver; real fingers remain separate\n";
}
static void menus() {
    // Use real game state machines. Controller activation must not allocate
    // any touch or joystick slot, and the queued activation is consumed once.
    {
        auto renderer = std::make_unique<mt::RenderSession>();
        mt::ScopedRenderSession binding(*renderer);
        auto game = std::make_unique<mt::theGame>();
        auto grid = std::make_unique<mt::cellGrid>();
        std::vector<uint32_t> save(0xd00 / 4);
        game->grid = grid.get();
        game->saveData = reinterpret_cast<uint8_t *>(save.data());
        game->state = 3;
        game->running = false;
        game->waitRelease = false;
        game->infoScreen();
        game->menuActions.activateLabel = 18;
        game->run();
        assert(game->state == 112);
        for (int i = 0; i < 4; ++i)
            assert(!game->touchActive[i] && !game->joyActive[i]);
        assert(game->menuActions.activateLabel == 0);
        game->run();
        game->menuActions.back = true;
        game->run();
        assert(game->state == 3);
        game->waitRelease = false;
        game->menuActions.activateLabel = 16;
        game->run();
        assert(game->state == 0x71);
        game->run();
        game->menuActions.activateLabel = 6;
        game->run();
        assert(game->state == 3);
        game->state = 0x72;
        game->waitRelease = false;
        game->entryName = "";
        game->entryBoard = 0;
        game->entryScore = 123456;
        game->menuActions.activateLabel = mino::grid::kLabelKey0;
        game->run();
        assert(game->entryName == "A");
        game->run();
        game->menuActions.activateLabel = 6;
        game->run();
        assert(game->entryName.empty());
        game->run();
        game->menuActions.activateLabel = 3;
        game->run();
        assert(game->state == 0x71);
        game->state = 7;
        game->paused = true;
        game->running = false;
        game->waitRelease = false;
        game->menuActions.activateLabel = 23;
        game->run();
        assert(game->state == 4 && !game->paused);
        game->grid = nullptr;
    }
    {
        auto game = std::make_unique<mr::spaceWars>();
        auto grid = std::make_unique<mr::cellGrid>();
        grid->setSize(40, 24);
        game->grid = grid.get();
        game->state = 2;
        game->waitRelease = false;
        game->menuActions.advance = true;
        game->run();
        assert(game->state == 3);
        game->run();
        game->menuActions.activateLabel = 18;
        game->run();
        assert(game->state == 0x70);
        for (int i = 0; i < 4; ++i)
            assert(!game->touchActive[i] && !game->joyActive[i]);
        game->menuActions.controller = true;
        game->menuActions.scroll = 1;
        game->run();
        game->menuActions.controller = true;
        game->menuActions.scroll = 1;
        game->run();
        assert(grid->scrollY > 0);
        game->menuActions.back = true;
        game->run();
        assert(game->state == 3);
        game->running = false;
        game->waitRelease = false;
        game->menuActions.activateLabel = 16;
        game->run();
        assert(game->state == 0x71);
        game->run();
        game->menuActions.activateLabel = 6;
        game->run();
        assert(game->state == 3);
        game->state = 0x72;
        game->waitRelease = false;
        game->entryName = "";
        game->entryBoard = 0;
        game->entryScore = 123456;
        game->menuActions.activateLabel = mino::grid::kLabelKey0;
        game->run();
        assert(game->entryName == "A");
        game->run();
        game->menuActions.activateLabel = 6;
        game->run();
        assert(game->entryName.empty());
        game->run();
        game->menuActions.activateLabel = 3;
        game->run();
        assert(game->state == 0x71);
        game->state = 7;
        game->paused = true;
        game->pausedState = 4;
        game->waitRelease = false;
        game->running = false;
        game->menuActions.activateLabel = 23;
        game->run();
        assert(game->state == 4 && !game->paused);
        game->grid = nullptr;
    }
    auto buttons = neon::input::menuButtons(neon::input::South | neon::input::Back);
    assert(buttons.confirm && buttons.cancel && !buttons.pause);
    neon::input::MenuBindings remapped;
    remapped.confirm = neon::input::East;
    assert(neon::input::menuButtons(neon::input::East, remapped).confirm);
    std::cout
        << "Direct title/info/scroll/scores/name-entry/pause actions and configurable menu bindings passed\n";
}
static void rescueStartBoundary() {
    // The virtual-touch reference retains the menu finger into gameplay. It makes the
    // new ship thrust at zero displacement, emitting one exhaust particle and
    // consuming a random draw before any gameplay control. Reproduce that
    // coupling as a negative control, rather than retaining a fake finger.
    struct Result {
        int next = 0, particles = 0;
        mr::vec2d position;
    };
    auto start = [&](bool legacyFinger, bool resume) {
        auto game = std::make_unique<mr::spaceWars>();
        auto grid = std::make_unique<mr::cellGrid>();
        grid->setSize(40, 24);
        game->grid = grid.get();
        game->state = 3;
        game->waitRelease = false;
        int activation = 3;
        if (resume) {
            game->demo = false;
            game->newGame();
            game->state = 7;
            game->paused = true;
            game->pausedState = 4;
            game->pauseScreen();
            activation = 23;
        } else {
            game->demo = true;
            game->startGameScreen();
        }
        auto buttons = mino::grid::scanButtons(grid.get());
        auto *button = mino::grid::findButton(buttons, activation);
        assert(button);
        double x, y;
        mino::grid::tapPoint(grid.get(), *button, x, y);
        if (legacyFinger)
            game->processJoyDown(0x20000, {x, y});
        else {
            game->menuActions.activateLabel = activation;
            game->menuActions.feedback = true;
            game->menuActions.feedbackX = x;
            game->menuActions.feedbackY = y;
        }
        mr::neonSrand(123);
        game->run();
        assert(game->state == 4);
        Result out;
        out.next = mr::neonRand();
        out.position = game->players->pos;
        for (auto *p = game->particles; p; p = p->next)
            ++out.particles;
        game->grid = nullptr;
        return out;
    };
    for (bool resume : {false, true}) {
        auto direct = start(false, resume);
        const int nextAfterExtraDraw = mr::neonRand();
        auto legacy = start(true, resume);
        assert(legacy.next == nextAfterExtraDraw);
        assert(legacy.particles == direct.particles + 1);
        assert(legacy.position.x == direct.position.x && legacy.position.y == direct.position.y);
    }
    std::cout
        << "Rescue START/UNPAUSE negative controls: old menu finger added one zero-thrust particle/random draw; removed\n";
}
static void earlyPauseRoutes() {
    {
        auto renderer = std::make_unique<mt::RenderSession>();
        mt::ScopedRenderSession binding(*renderer);
        auto game = std::make_unique<mt::theGame>();
        auto grid = std::make_unique<mt::cellGrid>();
        grid->setSize(40, 24);
        game->grid = grid.get();
        std::vector<uint32_t> save(0xd00 / 4);
        game->saveData = reinterpret_cast<uint8_t *>(save.data());
        game->state = 7;
        game->paused = true;
        game->waitRelease = false;
        game->pauseScreen();
        auto buttons = mino::grid::scanButtons(grid.get());
        const auto *title = mino::grid::findButton(buttons, 34);
        const auto *exit = mino::grid::findButton(buttons, 33);
        const auto *resume = mino::grid::findButton(buttons, 23);
        assert(title && exit && resume && resume->cy < title->cy && title->cy < exit->cy);
        game->menuActions.activateLabel = 33;
        game->run();
        assert(game->shellCommand == 32 && game->state == 7);
        game->shellCommand = 0;
        game->waitRelease = false;
        game->menuActions.activateLabel = 34;
        game->run();
        assert(game->state == 1 && game->demo);
        game->grid = nullptr;
    }
    {
        auto game = std::make_unique<mr::spaceWars>();
        auto grid = std::make_unique<mr::cellGrid>();
        grid->setSize(40, 24);
        game->grid = grid.get();
        game->state = 7;
        game->paused = true;
        game->waitRelease = false;
        game->pauseScreen();
        auto buttons = mino::grid::scanButtons(grid.get());
        const auto *title = mino::grid::findButton(buttons, 34);
        const auto *exit = mino::grid::findButton(buttons, 33);
        const auto *resume = mino::grid::findButton(buttons, 23);
        assert(title && exit && resume && resume->cy < title->cy && title->cy < exit->cy);
        game->menuActions.activateLabel = 33;
        game->run();
        assert(game->shellCommand == mr::SHELL_EXIT && game->state == 7);
        game->shellCommand = 0;
        game->waitRelease = false;
        game->menuActions.activateLabel = 34;
        game->run();
        assert(game->state == 1);
        game->grid = nullptr;
    }
    std::cout << "Early-game pause layouts preserve distinct Unpause/Title/Exit routes\n";
}
static void modePreferences() {
    minotron::module::ModePreference pref;
    assert(pref.initial(false) == 2 && pref.initial(true) == 0 && pref.automatic());
    pref.selected();
    assert(!pref.automatic() && !pref.available());
    std::stringstream unused;
    pref.write(unused);
    assert(unused.str().empty());
    for (int mode = 0; mode < 4; ++mode) {
        assert(pref.played(mode));
        std::stringstream saved;
        pref.write(saved);
        minotron::module::ModePreference loaded;
        loaded.read(saved);
        assert(loaded.initial(false) == mode && loaded.initial(true) == mode &&
               !loaded.automatic());
        assert(!loaded.played(mode));
    }
    for (const char *bad : {"-1", "4", "broken"}) {
        std::stringstream in(bad);
        pref.read(in);
        assert(!pref.available() && pref.automatic() && pref.initial(true) == 0);
    }
}
int main() {
    modePreferences();
    minotronControls();
    std::cout << std::flush;
    rescueControls();
    std::cout << std::flush;
    menus();
    rescueStartBoundary();
    earlyPauseRoutes();
}
