#include "minotron_game_host.h"
#include "minotron_menu_input.h"
#include "engine/neon_modules.h"
#include "neon_resources.h"
#include "grid_menus.h"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace minotron::Neon_Internals;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "Failed: " #x " at " << __LINE__ << "\n";                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)

int main() {
    bool unboundRejected = false;
    try {
        (void)renderSession();
    } catch (const std::logic_error &) {
        unboundRejected = true;
    }
    CHECK(unboundRejected);
    auto render = std::make_unique<RenderSession>();
    auto otherRender = std::make_unique<RenderSession>();
    ScopedRenderSession outerBinding(*render);
    renderSession().count = 6;
    renderSession().cursor += 6;
    renderSession().shaderCycles.c1 = 0.4f;
    try {
        ScopedRenderSession nestedBinding(*otherRender);
        CHECK(renderSession().count == 0);
        CHECK(renderSession().cursor == otherRender->vertices.data());
        CHECK(renderSession().shaderCycles.c1 == 0);
        renderSession().count = 12;
        throw std::runtime_error("test unwinding");
    } catch (const std::runtime_error &) {
    }
    CHECK(&renderSession() == render.get());
    CHECK(renderSession().count == 6);
    CHECK(renderSession().cursor == render->vertices.data() + 6);
    CHECK(renderSession().shaderCycles.c1 == 0.4f);
    CHECK(otherRender->count == 12);
    render->beginBatch();
    CHECK(render->count == 0 && render->cursor == render->vertices.data());
    NeonResManager resources;
    CHECK(resources.load(
        "<NeonResources><Literals><Literal>test.png</Literal></Literals>"
        "<Spritemaps><Spritemap entries='1'><Spentry>0,0,0,8,8</Spentry></Spritemap></Spritemaps>"
        "<SpritePages><Spritepage>16,16</Spritepage></SpritePages></NeonResources>"));
    render->resources = &resources;
    sprite quad;
    quad.flags = 0;
    quad.frame = 0;
    quad.pos = {10, 20};
    quad.width = 8;
    quad.height = 6;
    quad.drawRegion({0, 0}, {1, 1});
    CHECK(render->count == 6 && render->cursor == render->vertices.data() + 6);
    CHECK(render->vertices[0].x == 10 && render->vertices[0].y == -20);
    CHECK(render->vertices[2].x == 18 && render->vertices[2].y == -26);
    CHECK(otherRender->vertices[0].x == 0 && otherRender->count == 12);
    render->resources = nullptr;
    render->beginBatch();
    // Reproduce the menu transition through real grid hit-testing and game
    // state updates: confirm remains held for several ticks after opening.
    for (int label : {18, 4, 20}) {
        minotron::module::GameHost owner;
        owner.prepareGame();
        auto grid = std::make_unique<cellGrid>();
        auto *game = owner.game();
        game->grid = grid.get();
        std::vector<uint32_t> save(0xd00 / 4, 0);
        game->saveData = reinterpret_cast<uint8_t *>(save.data());
        game->state = 3;
        game->infoScreen();
        auto buttons = mino::grid::scanButtons(grid.get());
        auto *button = mino::grid::findButton(buttons, label);
        CHECK(button);
        double x, y;
        mino::grid::tapPoint(grid.get(), *button, x, y);
        minotron::module::InfoPageButtonGate gate;
        gate.menuActivated(label);
        game->processJoyDown(1, {x, y});
        game->menuInput();
        game->processJoyUp(1);
        CHECK(game->state == 112);
        CHECK(game->infoPage == (label == 18 ? 2 : label == 4 ? 0 : 1));
        for (int tick = 0; tick < 4; tick++) {
            game->controller.advance = gate.filter(true);
            game->run();
            CHECK(game->state == 112);
        }
        game->controller.advance = false;
        (void)gate.filter(false);
        game->run();
        CHECK(game->state == 112);
        auto *priorSprite = game->spr;
        const auto priorSize = game->drawSize;
        NeonVertex markerVertices[6]{};
        sprite marker;
        marker.batchPtr = markerVertices;
        game->spr = &marker;
        game->drawSize = {480, 320};
        game->drawInfoBack();
        CHECK(marker.frame == 0xde && marker.col.a == .5 && marker.batchCount == 6);
        CHECK(marker.pos.x > .36 * 480 && marker.pos.y > .27 * 320);
        game->spr = priorSprite;
        game->drawSize = priorSize;
        float scrollAxis = 1;
        auto scrollTick = [&] {
            game->menuActions.controller = true;
            game->menuActions.scroll = scrollAxis;
            game->run();
        };
        for (int tick = 0; tick < 800; tick++)
            scrollTick();
        CHECK(game->state == 112);
        const double bottom = grid->scrollY;
        // Page content and fractional position stop changing at its lower bound.
        std::vector<int> bottomText;
        for (int row = 0; row <= grid->rowsN; row++)
            for (int col = 0; col <= grid->cols; col++)
                bottomText.push_back(grid->rows[row][col].ch);
        for (int tick = 0; tick < 20; tick++)
            scrollTick();
        CHECK(grid->scrollY == bottom);
        size_t at = 0;
        for (int row = 0; row <= grid->rowsN; row++)
            for (int col = 0; col <= grid->cols; col++)
                CHECK(bottomText[at++] == grid->rows[row][col].ch);
        scrollAxis = -1;
        for (int tick = 0; tick < 800; tick++)
            scrollTick();
        CHECK(grid->scrollY == 0);
        CHECK(game->state == 112);
        scrollAxis = 1;
        scrollTick();
        CHECK(grid->scrollY > 0);
        const double stopped = grid->scrollY;
        scrollAxis = 0;
        for (int tick = 0; tick < 20; tick++)
            scrollTick();
        CHECK(grid->scrollY == stopped);
        game->controller.advance = gate.filter(true);
        game->run();
        CHECK(game->state == 3);
        game->grid = nullptr;
    }
    // Uses the real game, presenter and effect teardown, with no window/GL calls.
    minotron::module::GameHost host;
    CHECK(!host.hasGame());
    CHECK(!host.isRunning());
    host.releaseGame(); // failed/partial initialization is safe
    for (int session = 0; session < 8; session++) {
        {
            NeonEffect effect;
            effect.gameHost = &host;
            auto *stack = new NeonStack();
            effect.stacks = stack;
            stack->effect = &effect;
            auto *presenter = new showJoy();
            stack->modules = presenter;
            CHECK(!host.hasGame());
            presenter->Init_Module_Resources(nullptr, 0, stack);
            CHECK(host.hasGame());
            auto *game = host.game();
            host.prepareGame();
            CHECK(host.game() == game);
            game->running = false;
            presenter->initialised = 1;
            CHECK(presenter->Run_Module() == 0); // inactive game never draws
            game->running = true;
            CHECK(host.isRunning());
            game->inputFlipped = true;
            game->fbRot = 0.25f;
            game->fbX = -0.3f;
            game->fbY = 0.6f;
            game->fbScaleX = 0.9f;
            game->fbScaleY = 1.1f;
            for (int i = 0; i < 4; i++)
                game->fbColour[i] = 0.15f * i;
            const auto feedback = host.feedback();
            CHECK(feedback.inputFlipped);
            CHECK(feedback.rotation == game->fbRot && feedback.x == game->fbX &&
                  feedback.y == game->fbY);
            CHECK(feedback.scaleX == game->fbScaleX && feedback.scaleY == game->fbScaleY);
            for (int i = 0; i < 4; i++)
                CHECK(feedback.colour[i] == game->fbColour[i]);
            game->flash = 1.25f;
            host.decayFlash();
            CHECK(host.flash() == 1.25f * 0.95f);
            auto grid = std::make_unique<cellGrid>();
            auto other = std::make_unique<cellGrid>();
            host.attachTextGrid(*grid);
            host.attachTextGrid(*other);
            CHECK(game->grid == grid.get()); // first grid wins, as before
            game->bgLayer = -1;
            CHECK(!host.hasBackground());
            game->bgLayer = 0;
            CHECK(host.hasBackground());
            game->grid = nullptr; // the test's grid lifetime is shorter than effect
            presenter->Release_Module_Resources();
            CHECK(!host.hasGame());
            presenter->Init_Module_Resources(nullptr, 0, stack);
            CHECK(host.hasGame());
            // Effect teardown releases game; the presenter's destructor repeats
            // release. Neither path owns/deletes the module host.
        }
        CHECK(!host.hasGame());
        CHECK(!host.isRunning());
        CHECK(!host.hasBackground());
    }
    // No-showJoy and malformed DNA paths must not construct a game.
    NeonDevice device;
    superSaver absent;
    CHECK(absent.createEffectFromDNA(&device, 0, &host) == nullptr);
    superSaver malformed;
    malformed.hasDNA = true;
    malformed.S = {int(0xbaaa0001u), 0, 3, 17};
    CHECK(malformed.createEffectFromDNA(&device, 0, &host) == nullptr);
    CHECK(!host.hasGame());
    {
        NeonEffect effect;
        effect.gameHost = &host;
    }
    CHECK(!host.hasGame());
    std::cout
        << "Minotron host: lifecycle, feedback, grid ownership and partial DNA checks passed\n";
}
