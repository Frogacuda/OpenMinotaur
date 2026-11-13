#if defined(HOST_RESCUE)
#define NEON_GAME minorescue
#include "cores/minorescue/rescue_game_host.h"
using Owner = minorescue::module::GameHost;
#elif defined(HOST_GRIDRUNNER)
#define NEON_GAME gridrunner
#include "cores/gridrunner/gridrunner_game_host.h"
using Owner = gridrunner::module::GameHost;
#else
#define NEON_GAME minotron
#include "cores/minotron/minotron_game_host.h"
#include "cores/minotron/engine/neon_modules.h"
using Owner = minotron::module::GameHost;
#endif
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace NEON_GAME::Neon_Internals;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << #x << ":" << __LINE__ << "\n";                                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
int main() {
    bool rejected = false;
    try {
        (void)renderSession();
    } catch (const std::logic_error &) {
        rejected = true;
    }
    CHECK(rejected);
    auto state = std::make_unique<RenderSession>();
    auto nested = std::make_unique<RenderSession>();
    static_assert(!std::is_copy_constructible_v<RenderSession> &&
                  !std::is_move_constructible_v<RenderSession>);
    auto bind = state->bind();
    state->count = 12;
    state->shaderCycles.c1 = .4f;
    try {
        auto bindOther = nested->bind();
        CHECK(renderSession().count == 0);
        renderSession().count = 6;
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error &) {
    }
    CHECK(&renderSession() == state.get() && state->count == 12 && state->shaderCycles.c1 == .4f &&
          nested->count == 6);
    NeonDrawContext draw;
    state->attachDrawing(draw);
    state->cursor = state->base + 12;
    state->sourceTexture = 2;
    settex(2);
#if defined(HOST_RESCUE)
    CHECK(state->count == 12 &&
          state->cursor == state->base + 12); // original 0x12d28 reuses the literal
#else
    CHECK(state->count == 0 && state->cursor == state->base);
#endif
    settex(-1);
    CHECK(state->count == 0 && state->sourceTexture == 4);
#if defined(HOST_GRIDRUNNER)
    CHECK(draw.main.srcLiteral[0] == 4 && draw.main.srcFont[0] == -1);
#else
    CHECK(draw.main.srcLiteral[0] == -1 && draw.main.srcFont[0] == 4);
#endif
    const auto history = state->shaderCycles.c1;
    const auto count = state->count;
    const auto source = state->sourceTexture;
    const auto cursor = state->cursor;
    state->disconnect();
    state->disconnect();
    CHECK(!state->context && !state->resources && !state->device);
    CHECK(state->shaderCycles.c1 == history && state->count == count && state->cursor == cursor &&
          state->sourceTexture == source);
    CHECK(&renderSession() == state.get()); // detach does not end or replace the callback scope

    Owner owner;
    CHECK(!owner.hasGame() && !owner.isRunning());
    owner.releaseGame();
    auto fx = std::make_unique<NeonEffect>();
    fx->gameHost = &owner;
    auto stack = std::make_unique<NeonStack>();
    stack->effect = fx.get();
    {
        showJoy presenter;
        CHECK(presenter.Init_Module_Resources(nullptr, 0, stack.get()) == 0);
        CHECK(owner.hasGame());
        auto *original = owner.game();
        owner.prepareGame();
        CHECK(owner.game() == original);
        auto grid = std::make_unique<cellGrid>();
        auto another = std::make_unique<cellGrid>();
        owner.attachTextGrid(*grid);
        owner.attachTextGrid(*another);
        CHECK(original->grid == grid.get());
        original->running = 0;
        CHECK(!owner.isRunning());
        original->running = 1;
        CHECK(owner.isRunning());
        original->fbRot = .125f;
        original->fbX = -.5f;
        original->fbScaleY = .75f;
        original->fbColour[3] = .25f;
        original->inputFlipped = true;
        auto feedback = owner.feedback();
        CHECK(feedback.rotation == .125f && feedback.x == -.5f && feedback.scaleY == .75f &&
              feedback.colour[3] == .25f && feedback.inputFlipped);
        original->flash = .731f;
        owner.decayFlash();
#if defined(HOST_RESCUE)
        CHECK(original->flash == float(double(.731f) * .95));
        CHECK(!owner.presenterRules().purgeBefore && owner.presenterRules().blendCalls == 1 &&
              owner.presenterRules().projectionFlags == 1);
#else
        CHECK(original->flash == .731f * .95f);
        CHECK(owner.presenterRules().purgeBefore && owner.presenterRules().blendCalls == 2 &&
              owner.presenterRules().projectionFlags == 0);
#endif
        presenter.Release_Module_Resources();
        CHECK(!owner.hasGame());
        presenter.Release_Module_Resources();
        CHECK(!owner.hasGame());
    } // destructor repeats the release safely
    owner.prepareGame();
    CHECK(owner.hasGame());
    owner.releaseGame();
    CHECK(!owner.hasGame());
    for (int type : {200, 300, 400, 401, 402, 403, 404, 405, 406, 499})
        CHECK(!createModule(type));
    for (int type : {1, 5, 500, 600}) {
        std::unique_ptr<NeonModule_Base> m(createModule(type));
        CHECK(m && m->typeId == type);
    }
    std::cout
        << "Presentation ownership, resource callbacks, source rules, unsupported capabilities and nested render sessions passed\n";
}
