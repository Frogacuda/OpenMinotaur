// rescue_core.cpp - RescueCore: the shell around the ported Minotaur Rescue
// engine, replacing the iOS app (EAGLView, ES2Renderer, the app delegate,
// OpenFeint and the AudioUnit mixer).
//
// Per 60 Hz tick, as EAGLView::drawView did:
//   1. WaveGen (the procedural clock some shaders read),
//   2. service the game's shell-command mailbox,
//   3. spaceWars::run(), then the effect's module stacks
//      (cellGrid -> showJoy -> Mixer), which render into render targets,
//   4. draw the effect's output over the whole surface.
#include "neon/backend_setup.h"
#include "startup_screen.h"
#include "neon/controller_session.h"
#include "neon/sample_audio.h"
#include "rescue_game_host.h"
#include "core_api.h"
#include "engine/game.h"
#include "engine/neon_audio.h"
#include "engine/neon_device.h"
#include "engine/neon_modules.h"
#include "engine/neon_resources.h"
#include "engine/neon_sprite.h"
#include "engine/neon_synth.h"
#include "grid_menus.h"

#include <glad/gl.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace NEON_GAME {
namespace Neon_Internals {
WaveState *Waves = nullptr;
} // namespace Neon_Internals
} // namespace NEON_GAME

namespace mino {
namespace {

using namespace NEON_GAME::Neon_Internals;

constexpr int kLogicalW = 480, kLogicalH = 320;
constexpr double kTick = 1.0 / 60.0;

// The module owns the shared sample service. Original voice/arithmetical rules
// are chosen at construction; original game sound scheduling stays below.
using Sample = neon::audio::Sample;
using SampleMixer = neon::audio::SampleMixer;
using neon::audio::parseWav;
SampleMixer *gMixer = nullptr; // borrowed by this module's original audio entry points

// ------------------------------------------------------------- core -------

class RescueCore final : public Core {
    mino::StartupScreen startup_;

  public:
    explicit RescueCore(float &titleHistory, neon::ShaderCycleState &history)
        : shaderHistory_(history), gameHost_(&titleHistory) {
        renderState_.shaderCycles = history;
    }
    const CoreInfo &info() const override;

    bool init(Host &host, const SurfaceInfo &s) override {
        if (!startup_.begin(host, s))
            return false;
        auto bind = renderState_.bind();
        host_ = &host;
        surfW_ = s.width;
        surfH_ = s.height;
        std::vector<uint8_t> xml;
        if (!host.readAsset("NeonResources.xml", xml)) {
            host.log("minorescue: NeonResources.xml missing");
            return false;
        }
        std::string err;
        if (!res_.load(std::string(xml.begin(), xml.end()), &err)) {
            host.log("minorescue: resources: %s", err.c_str());
            return false;
        }
        renderState_.attachResources(res_);

        std::vector<Sample> samples;
        for (const SampleEntry &e : res_.samples()) {
            Sample smp;
            smp.maxInstances = std::max(1, e.maxInstances);
            smp.priority = e.priority;
            std::vector<uint8_t> wav;
            if (!host.readAsset(e.file, wav) || !parseWav(wav, smp))
                host.log("minorescue: sample %s not loaded", e.file.c_str());
            samples.push_back(std::move(smp));
        }
        mixer_.setSamples(std::move(samples));
        gMixer = &mixer_;

        waves_ = WaveState{};
        Waves = &waves_;

        dev_.setHost(&host);
        dev_.Init(0, kLogicalW, kLogicalH, s.framebuffer, s.width, s.height, 0);
        dev_.InitAllShaders();
        dev_.InitLiteralTextures(int(res_.literals().size()), res_.literals());
        dev_.SetupDefaultProjection(-1, -1, PROJ_ORTHO);
        renderState_.attachDevice(dev_);

        neon::backend::initializeDrawing(ctx_, dev_, {0, 20000}, {1, 24000},
                                         neon::backend::DrawingOrder::AllocateBothThenConfigure,
                                         neon::backend::BufferFailure::IgnoreReturn);
        renderState_.attachDrawing(ctx_);

        // The effect that hosts the game: cell 0 of testGrid.ox.
        std::vector<uint8_t> grid;
        if (!host.readAsset("testGrid.ox", grid)) {
            host.log("minorescue: testGrid.ox missing");
            return false;
        }
        std::vector<superSaver> cells;
        if (!loadGrid(grid, cells) || cells.empty() || !cells[0].hasDNA) {
            host.log("minorescue: testGrid.ox unreadable");
            return false;
        }
        effect_ = cells[0].createEffectFromDNA(&dev_, 0, &gameHost_);
        if (!effect_ || !gameHost_.game()) {
            host.log("minorescue: game effect not created: %s", cells[0].error.c_str());
            return false;
        }
        auto *last = neon::backend::lastStack(*effect_);
        ctx_.effect = effect_;
        ctx_.effectStack = last ? last->stackIndex : 0;
        loadScores();
        return true;
    }

    void resize(const SurfaceInfo &s) override {
        startup_.resize(s);
        auto bind = renderState_.bind();
        dev_.SetBackbuffer(s.framebuffer, kLogicalW, kLogicalH, s.width, s.height);
        surfW_ = s.width;
        surfH_ = s.height;
    }

    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        auto bind = renderState_.bind();
        acc_ += std::min(dt, 0.25);
        int ticks = 0;
        while (acc_ >= kTick && ticks < 4) {
            acc_ -= kTick;
            tick();
            ticks++;
        }
        if (ticks == 4)
            acc_ = 0;
        present();
    }

    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        auto bind = renderState_.bind();
        spaceWars *g = game();
        if (!g || surfW_ <= 0 || surfH_ <= 0)
            return;
        // Touch ids handed to the engine start at 1 (as the iOS dispenser did).
        unsigned id = e.id + 1;
        vec2d p(double(e.x) / double(surfW_) - 0.5, double(e.y) / double(surfH_) - 0.5);
        switch (e.phase) {
        case TouchPhase::Down:
            padMode_ = false; // touching hides the controller cursor
            g->processJoyDown(id, p);

            break;
        case TouchPhase::Move:
            g->processJoyMove(id, p);
            break;
        case TouchPhase::Up:
        case TouchPhase::Cancel:
            g->processJoyUp(id);
            break;
        }
    }

    // Controllers supply native player controls; physical touches stay separate.
    // Start / Back pause.
    void pads(const PadState *pads, int count) override {
        if (startup_.pads(pads, count))
            return;
        controllerFrame_.capture(pads, count);
        if (!controllerFrame_.connected())
            padMode_ = false;
        if (host_->keyboardNavigationAvailable())
            padMode_ = true;
        auto bind = renderState_.bind();
        for (int i = 0; i < kMaxPads; i++) {
            const PadState &p = controllerFrame_[i];
            auto axis = neon::input::movement(p);
            float x = axis.x, y = axis.y;
            bool held = p.connected && (x * x + y * y > 0.04f);
            if (p.connected) {
                padPressed_ |= p.pressed;
                if (p.pressed || held)
                    padMode_ = true;
            }
            padStick_[i][0] = x;
            padStick_[i][1] = y;
            padHeld_[i] = held;
        }
    }

    void mixAudio(float *out, int frames) override {
        mixer_.mix(out, frames);
    }

    void pause(bool paused) override {
        auto bind = renderState_.bind();
        spaceWars *g = game();
        if (paused) {
            controllerFrame_.clear();
            mixer_.stopAll();
            for (int i = 0; i < kMaxPads; ++i) {
                padStick_[i][0] = padStick_[i][1] = 0;
                padHeld_[i] = false;
            }
            padPressed_ = 0;
            if (g) {
                g->controllers.cancel();
                g->menuActions = {};
            }
            requestPause(g);
        } else if (g) {
            g->purgeTouches();
            g->controllers.cancel();
        }
    }

    void save() override {
        auto bind = renderState_.bind();
        saveScores();
    }

    bool back() override {
        if (startup_.blocking())
            return true;
        auto bind = renderState_.bind();
        spaceWars *g = game();
        if (openingOrTitle(g)) {
            save();
            host_->requestQuit();
            return true;
        }
        if (!g || g->demo || !(g->state == 4 || g->state == 6) || g->paused)
            return false;
        pauseRequest_ = true;
        return true;
    }

    void shutdown() override {
        startup_.release();
        auto bind = renderState_.bind();
        shaderHistory_ = renderState_.shaderCycles;
        saveScores();
        gMixer = nullptr;
        delete effect_;
        effect_ = nullptr;
        renderState_.disconnect();
        Waves = nullptr;
        dev_.Destroy(true);
    }

  private:
    spaceWars *game() const {
        return effect_ ? gameHost_.game() : nullptr;
    }

    static void requestPause(spaceWars *g) {
        if (g && !g->demo && (g->state == 4 || g->state == 6))
            g->paused = true;
    }

    // ------------------------------------------------- high scores -----
    // minorescue_scores.txt: "board rank score level name" lines, plus
    // "name <last name entered>".
    std::string scorePath() const {
        return host_->saveDir() + "minorescue_scores.txt";
    }

    void loadScores() {
        if (spaceWars *g = game())
            g->hs.load(scorePath());
    }

    void saveScores() {
        spaceWars *g = game();
        if (g && host_)
            g->hs.save(scorePath());
    }

    void waveGen() {
        waves_.beat = waves_.beat < 800 ? waves_.beat + 1 : 0;
    }

    void serviceMailbox(spaceWars *g) {
        host_->observeOriginalAchievements(g->goalsDone & 0x3ffu);
        int cmd = g->shellCommand;
        if (cmd < 1)
            return;
        switch (cmd) {
        case SHELL_CHECK_ONLINE:
            g->onlineOK = false;
            break;
        case SHELL_POST_SCORE:
            host_->log("minorescue: score %d (board %d)", g->postScore, g->postBoard);
            break;
        case SHELL_SHOW_BOARD:
            host_->log("minorescue: leaderboard %d requested", g->postBoard);
            break;
        case SHELL_SHOW_DASHBOARD:
        case SHELL_SCREEN_ON:
            break;
        case SHELL_EXIT:
            host_->requestQuit();
            break;
        default:
            if (cmd >= SHELL_ACHIEVEMENT && cmd < SHELL_ACHIEVEMENT + 10)
                host_->log("minorescue: achievement %d", cmd - SHELL_ACHIEVEMENT);
            break;
        }
        g->shellCommand = 0;
    }

    void padControllers(spaceWars *g) {
        std::array<neon::input::PlayerMotion, 4> input{};
        for (int i = 0; i < kMaxPads; ++i)
            input[i] = {padHeld_[i], double(padStick_[i][0]), double(padStick_[i][1])};
        bool playing = !g->demo && (g->state == 4 || g->state == 6) && !g->paused;
        g->controllers.update(input, g->joyZones, g->inputFlipped, playing, g->joyActive);
    }

    static bool openingOrTitle(spaceWars *g) {
        if (!g)
            return false;
        if (g->state >= 0 && g->state <= 2)
            return true;
        if (g->state != 3 || !g->grid)
            return false;
        // The start selector and Info share state 3; only the main menu has Exit.
        const auto buttons = mino::grid::scanButtons(g->grid);
        return mino::grid::findButton(buttons, 0x22) != nullptr;
    }

    void tick() {
        spaceWars *g = game();
        if (!g)
            return;
        waveGen();
        serviceMailbox(g);
        menu_.restore();

        uint32_t pressed = padPressed_;
        padPressed_ = 0;
        // Complete the frame before the host compositor handles an early exit.
        if (neon::input::menuButtons(pressed).cancel && openingOrTitle(g)) {
            back();
            pressed = 0;
        }
        bool playing = !g->demo && (g->state == 4 || g->state == 6);
        if (playing && !g->paused && (pressed & (PAD_START | PAD_BACK)))
            pauseRequest_ = true;
        if (pauseRequest_) {
            requestPause(g);
            pauseRequest_ = false;
        }

        g->menuActions = {};
        padControllers(g);
        if (!playing)
            padMenus(g, pressed);

        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        dev_.alphaEnable(true);
        dev_.SetAlphaBlend(1, 1);
        g->run();
        if (g->hs.dirty)
            saveScores();
        if (padMode_ && g->grid && menuState(g))
            menu_.highlight(g->grid, padMenu_.sel, ++menuPulse_,
                            [](const gridCell &c) { return c.attr >= 2 && c.attr <= 4; });
        effect_->aspect = 1.0f;
        effect_->RunEffect();
        drawn_ = true;
    }

    // ------------------------------------------------------- pad menus -----
    // Screens with buttons (the menus, the pause screen) get a cursor: the
    // d-pad / stick moves between the grid's labelled buttons, A presses the
    // selected action directly; cancel goes BACK / UNPAUSE, Start unpauses.
    // Elsewhere (title, story, game over) buttons advance; on
    // the info pages the stick scrolls and B leaves.

    static bool menuState(spaceWars *g) {
        return g->state == 3 || g->state == 7 || g->state == 0x71 || g->state == 0x72;
    }

    void padMenus(spaceWars *g, uint32_t pressed) {
        // Strongest stick / d-pad over all pads, as one direction with repeat.
        float sx = 0, sy = 0;
        for (int i = 0; i < kMaxPads; i++)
            if (padStick_[i][0] * padStick_[i][0] + padStick_[i][1] * padStick_[i][1] >
                sx * sx + sy * sy) {
                sx = padStick_[i][0];
                sy = padStick_[i][1];
            }
        const auto buttonsPressed = neon::input::menuButtons(pressed);
        cellGrid *grid = g->grid;
        if (!grid)
            return;
        g->menuActions.controller = padMode_;
        g->menuActions.scroll = sy;
        if (g->state == 0x70) {
            g->menuActions.back = !g->joyActive[0] && buttonsPressed.cancel;
            return;
        }
        if (!menuState(g)) {
            g->menuActions.advance = buttonsPressed.advance;
            g->menuActions.feedback = buttonsPressed.advance;
            return;
        }
        if (g->joyActive[0])
            return; // a real finger owns this activation

        std::vector<mino::grid::Button> buttons = mino::grid::scanButtons(grid);
        mino::grid::dropLabels(buttons, {1, 2}); // banners: the cursor uses their name buttons
        int start = 0;
        if (buttonsPressed.pause)
            start = g->state == 7 ? 0x17 : g->state == 0x71 ? 6 : g->state == 0x72 ? 3 : 0;
        int press = padMenu_.update(buttons, g->state, sx, sy, buttonsPressed.confirm,
                                    buttonsPressed.cancel, start, {3, 5, 0x17, 6}, {0x17, 6, 0x15});
        if (press) {
            g->menuActions.activateLabel = press;
            const auto *button = mino::grid::findButton(buttons, press);
            mino::grid::tapPoint(grid, *button, g->menuActions.feedbackX, g->menuActions.feedbackY);
            g->menuActions.feedback = true;
        }
    }

    void present() {
        if (!drawn_)
            return;
        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        ctx_.main.SetCurrenRenderTarget(-1);
        ctx_.aux.SetCurrenRenderTarget(-1);
        dev_.alphaEnable(false);
        dev_.SetupDefaultProjection(kLogicalW, kLogicalH, PROJ_ORTHO);
        dev_.ClearScreen(255, 0, 0, 0);
        D3DXMatrixTranslation(&ctx_.main.world, kLogicalW * -0.5f, kLogicalH * 0.5f, 0.0f);
        renderState_.beginBatch();
        sprite s;
        s.pos = vec2d(0, 0);
        s.width = kLogicalW;
        s.height = kLogicalH;
        s.col.white(1.0);
        s.frame = INT_MIN;
        s.draw();
        purge();
        dev_.PresentFrame();
    }

    neon::input::ControllerFrame controllerFrame_;
    Host *host_ = nullptr;
    NeonResManager res_;
    NeonDevice dev_;
    NeonDrawContext ctx_;
    neon::ShaderCycleState &shaderHistory_;
    RenderSession renderState_;
    minorescue::module::GameHost gameHost_;
    NeonEffect *effect_ = nullptr;
    WaveState waves_;
    SampleMixer mixer_{{16, neon::audio::MixArithmetic::ReverseQ16Saturating,
                        neon::audio::RetriggerRule::RequireSampleRestart}};
    bool pauseRequest_ = false;
    float padStick_[kMaxPads][2] = {};
    bool padHeld_[kMaxPads] = {};
    mino::grid::Highlighter<cellGrid, gridCell, &gridCell::attr> menu_;
    mino::grid::PadMenu padMenu_;
    unsigned menuPulse_ = 0;
    bool padMode_ = false;
    uint32_t padPressed_ = 0;
    double acc_ = 0;
    bool drawn_ = false;
    int surfW_ = kLogicalW, surfH_ = kLogicalH;
};

const CoreInfo kInfo = {"minorescue", "Minotaur Rescue", kLogicalW, kLogicalH, 1.5f,
                        1.5f,         "spacewars.png"};
const CoreInfo &RescueCore::info() const {
    return kInfo;
}

} // namespace

extern const CoreEntry kMinoRescueCore = {&kInfo, []() -> Core * {
                                              static float titleHistory = 0;
                                              static neon::ShaderCycleState shaderHistory;
                                              return new RescueCore(titleHistory, shaderHistory);
                                          }};

} // namespace mino

// The game's audio entry points (AudioSystem.c).
namespace NEON_GAME {
void AudioSystemPlaySample(int sample, int volume, int flags) {
    if (mino::gMixer)
        mino::gMixer->play(sample, volume, flags);
}
void AudioSystemStopSample(int sample) {
    if (mino::gMixer)
        mino::gMixer->stop(sample);
}
} // namespace NEON_GAME
