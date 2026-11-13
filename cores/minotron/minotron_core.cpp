// minotron_core.cpp - MinotronCore: the shell around the ported engine,
// replacing the iOS app (EAGLView, ES2Renderer, the app delegate and the
// AudioUnit mixer).
//
// Per 60 Hz tick, as EAGLView::drawView did:
//   1. WaveGen (the procedural clock some shaders read),
//   2. service the game's shell-command mailbox,
//   3. YakEffect: theGame::run(), then run the effect's module stacks
//      (cellGrid -> showJoy -> Mixer), which render into render targets,
//   4. draw the effect's output over the whole surface (DrawEffect).
#include "neon/backend_setup.h"
#include "startup_screen.h"
#include "neon/controller_session.h"
#include "neon/sample_audio.h"
#include "core_api.h"
#include "minotron_game_host.h"
#include "minotron_menu_input.h"
#include "mode_preference.h"
#include "engine/game.h"
#include "engine/neon_audio.h"
#include "neon_device.h"
#include "engine/neon_modules.h"
#include "neon_resources.h"
#include "engine/neon_sprite.h"
#include "engine/neon_synth.h"
#include "grid_menus.h"

#include <glad/gl.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
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
constexpr int kSaveSize = 0xd00;
constexpr double kTick = 1.0 / 60.0;

// The module owns the shared sample service. Original voice/arithmetical rules
// are chosen at construction; original game sound scheduling stays below.
using Sample = neon::audio::Sample;
using SampleMixer = neon::audio::SampleMixer;
using neon::audio::parseWav;
SampleMixer *gMixer = nullptr; // borrowed by this module's original audio entry points

// ------------------------------------------------------------- core -------

class MinotronCore final : public Core {
    mino::StartupScreen startup_;

  public:
    explicit MinotronCore(neon::ShaderCycleState &history) : shaderHistory_(history) {
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
            host.log("minotron: NeonResources.xml missing");
            return false;
        }
        std::string err;
        if (!res_.load(std::string(xml.begin(), xml.end()), &err)) {
            host.log("minotron: resources: %s", err.c_str());
            return false;
        }
        renderState_.attachResources(res_);

        // Audio (InitAudioSystem + the <Sample> list).
        std::vector<Sample> samples;
        for (const SampleEntry &e : res_.samples()) {
            Sample smp;
            smp.maxInstances = std::max(1, e.maxInstances);
            smp.priority = e.priority;
            std::vector<uint8_t> wav;
            if (!host.readAsset(e.file, wav) || !parseWav(wav, smp))
                host.log("minotron: sample %s not loaded", e.file.c_str());
            samples.push_back(std::move(smp));
        }
        mixer_.setSamples(std::move(samples));
        gMixer = &mixer_;

        // StartWaveThread / SetWaveFrequency(60).
        waves_ = WaveState{};
        Waves = &waves_;

        // ES2Renderer resizeFromLayer: device, shaders, textures, projection.
        dev_.setHost(&host);
        dev_.Init(0, kLogicalW, kLogicalH, s.framebuffer, s.width, s.height, 0);
        dev_.InitAllShaders();
        dev_.InitLiteralTextures(int(res_.literals().size()), res_.literals());
        dev_.SetupDefaultProjection(-1, -1, PROJ_ORTHO);
        renderState_.attachDevice(dev_);

        // CDrawingObject::ResetObject: the drawing context's render objects.
        neon::backend::initializeDrawing(ctx_, dev_, {0, 20000}, {1, 24000},
                                         neon::backend::DrawingOrder::AllocateBothThenConfigure,
                                         neon::backend::BufferFailure::IgnoreReturn);
        renderState_.attachDrawing(ctx_);

        // The effect that hosts the game: cell 0 of testGrid.ox.
        std::vector<uint8_t> grid;
        if (!host.readAsset("testGrid.ox", grid)) {
            host.log("minotron: testGrid.ox missing");
            return false;
        }
        std::vector<superSaver> cells;
        if (!loadGrid(grid, cells) || cells.empty() || !cells[0].hasDNA) {
            host.log("minotron: testGrid.ox unreadable");
            return false;
        }
        effect_ = cells[0].createEffectFromDNA(&dev_, 0, &gameHost_);
        if (!effect_ || !gameHost_.hasGame()) {
            host.log("minotron: game effect not created: %s", cells[0].error.c_str());
            return false;
        }
        auto *last = neon::backend::lastStack(*effect_);
        ctx_.effect = effect_;
        ctx_.effectStack = last ? last->stackIndex : 0;

        loadSave();
        std::ifstream modeFile(host.saveDir() + "minotron-mode.txt");
        modePreference_.read(modeFile);
        controllerConnected_ = false;
        if (auto *g = game())
            g->gameMode = modePreference_.initial(false);
        if (theGame *g = game())
            g->hs.load(scorePath());
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
        theGame *g = game();
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

    void pads(const PadState *pads, int count) override {
        controllerConnected_ = false;
        for (int i = 0; pads && i < std::clamp(count, 0, kMaxPads); ++i)
            controllerConnected_ |= pads[i].connected;
        if (startup_.pads(pads, count))
            return;
        controllerFrame_.capture(pads, count);
        if (!controllerFrame_.connected())
            padMode_ = false;
        if (host_->keyboardNavigationAvailable())
            padMode_ = true;
        controller_ = minotron::module::controllerInput(controllerFrame_);
        menuStick_[0] = menuStick_[1] = 0;
        for (const auto &p : controllerFrame_.slots()) {
            if (!p.connected)
                continue;
            if (p.pressed & PAD_BACK)
                pauseRequest_ = true;
            if (p.pressed & PAD_START)
                back();
            padPressed_ |= p.pressed;
            auto axis = neon::input::movement(p);
            if (axis.x * axis.x + axis.y * axis.y >
                menuStick_[0] * menuStick_[0] + menuStick_[1] * menuStick_[1]) {
                menuStick_[0] = axis.x;
                menuStick_[1] = axis.y;
            }
            if (p.pressed || axis.x * axis.x + axis.y * axis.y > .04f)
                padMode_ = true;
        }
    }

    void mixAudio(float *out, int frames) override {
        mixer_.mix(out, frames);
    }

    void pause(bool paused) override {
        theGame *g = game();
        if (paused) {
            controllerFrame_.clear();
            mixer_.stopAll();
            controller_ = {};
            padPressed_ = 0;
            menuStick_[0] = menuStick_[1] = 0;
            if (g) {
                g->controller = {};
                g->menuActions = {};
            }
            requestPause(g);
        } else if (g) {
            g->purgeTouches();
        }
    }

    void save() override {
        writeSave();
        saveScores();
        writeModePreference();
    }

    bool back() override {
        if (startup_.blocking())
            return true;
        theGame *g = game();
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
        writeSave();
        saveScores();
        writeModePreference();
        gMixer = nullptr;
        delete effect_;
        effect_ = nullptr;
        gameHost_.releaseGame();
        renderState_.disconnect();
        Waves = nullptr;
        shaderHistory_ = renderState_.shaderCycles;
        dev_.Destroy(true);
    }

  private:
    minotron::module::ModePreference modePreference_;
    bool controllerConnected_ = false;
    void writeModePreference() {
        if (!host_ || !modePreference_.available())
            return;
        std::ofstream file(host_->saveDir() + "minotron-mode.txt");
        modePreference_.write(file);
    }
    theGame *game() const {
        return effect_ ? gameHost_.game() : nullptr;
    }

    // The game's own pause (as a touch on the score bar): only while playing.
    static void requestPause(theGame *g) {
        if (g && !g->demo && (g->state == 4 || g->state == 6))
            g->paused = true;
    }

    // ------------------------------------------------- high scores -----
    std::string scorePath() const {
        return host_->saveDir() + "minotron_scores.txt";
    }
    void saveScores() {
        theGame *g = game();
        if (g && host_)
            g->hs.save(scorePath());
    }

    // --------------------------------------------------------- pad menus -----
    // Screens with buttons (title / level select / info, pause, high scores,
    // name entry) get a cursor; controller and touch activate the same action IDs.
    static bool menuState(const theGame *g) {
        return g->state == 3 || g->state == 7 || g->state == 0x71 || g->state == 0x72;
    }

    void padMenus(theGame *g, uint32_t pressed) {
        const auto buttonsPressed = neon::input::menuButtons(pressed);
        cellGrid *grid = g->grid;
        if (!grid)
            return;
        if (g->joyActive[0])
            return; // a real finger owns this activation
        std::vector<mino::grid::Button> buttons = mino::grid::scanButtons(grid);
        mino::grid::dropLabels(
            buttons, {1, 2, 0x28}); // Banners and selector background are not navigation targets.
        int start = 0;
        if (buttonsPressed.pause)
            start = g->state == 7 ? 23 : g->state == 0x71 ? 6 : g->state == 0x72 ? 3 : 0;
        int press =
            padMenu_.update(buttons, g->state, menuStick_[0], menuStick_[1], buttonsPressed.confirm,
                            buttonsPressed.cancel, start, {3, 9, 5, 23, 6}, {23, 6, 21});
        if (!press)
            return;
        if (g->state == 3)
            infoPageButtons_.menuActivated(press);
        g->menuActions.activateLabel = press;
    }

    // ---------------------------------------------------------- save -----

    void loadSave() {
        save_.assign(kSaveSize, 0);
        std::string path = host_->saveDir() + "minotron.sav";
        bool ok = false;
        if (FILE *f = std::fopen(path.c_str(), "rb")) {
            ok = std::fread(save_.data(), 1, save_.size(), f) == save_.size();
            std::fclose(f);
        }
        if (!ok) {
            // application:didFinishLaunching defaults: 3 lives on level 1 of
            // every mode, volume 5, robot 0, adaptive controls.
            std::fill(save_.begin(), save_.end(), 0);
            for (int mode = 0; mode < 4; mode++)
                sdi(mode * 800) = 3;
            sdi(0xc80) = 5;
            sdi(0xc84) = 0;
            sdi(0xc94) = 0;
            sdi(0xc9c) = int(0xbaaa0001u);
            sdi(0xc98) = 0;
        }
    }

    void writeSave() {
        if (save_.size() != size_t(kSaveSize) || !host_)
            return;
        std::string path = host_->saveDir() + "minotron.sav";
        if (FILE *f = std::fopen(path.c_str(), "wb")) {
            std::fwrite(save_.data(), 1, save_.size(), f);
            std::fclose(f);
        }
    }

    int &sdi(int off) {
        return *reinterpret_cast<int *>(save_.data() + off);
    }

    // --------------------------------------------------------- tick ------

    void waveGen() {
        waves_.beat = waves_.beat < 800 ? waves_.beat + 1 : 0;
    }

    void serviceMailbox(theGame *g) {
        host_->observeOriginalAchievements(uint32_t(g->goalsDone) & 0x3ffu);
        int cmd = g->shellCommand;
        if (cmd < 1)
            return;
        switch (cmd) {
        case 1:
            g->onlineOK = false;
            break;
        case 2:
            host_->log("minotron: score %d (board %d)", g->postScoreValue, g->postScoreCategory);
            break;
        case 16:
            g->saveData = save_.data();
            break;
        case 17:
            writeSave();
            break;
        case 32:
            host_->requestQuit();
            break; // (port) EXIT on the title screen
        default:
            if (cmd >= 4 && cmd <= 13)
                host_->log("minotron: achievement %d", cmd);
            break;
        }
        g->shellCommand = 0;
    }

    static bool openingOrTitle(theGame *g) {
        if (!g)
            return false;
        if (g->state >= 0 && g->state <= 2)
            return true;
        if (g->state != 3 || !g->grid)
            return false;
        // The start selector and Info share state 3; only the main menu has Exit.
        const auto buttons = mino::grid::scanButtons(g->grid);
        return mino::grid::findButton(buttons, 0x2b) != nullptr;
    }

    void tick() {
        theGame *g = game();
        if (!g)
            return;
        if (modePreference_.automatic() && openingOrTitle(g)) {
            const int mode = modePreference_.initial(controllerConnected_);
            if (g->gameMode != mode) {
                g->gameMode = mode;
                if (g->state == 3) {
                    g->newGame();
                    g->printTitleScreen();
                }
            }
        }
        waveGen();
        serviceMailbox(g);
        if (pauseRequest_) {
            requestPause(g);
            pauseRequest_ = false;
        }
        menu_.restore();
        uint32_t pressed = padPressed_;
        padPressed_ = 0;
        // Complete the first frame even when B requests an immediate launcher return.
        const bool leaving = neon::input::menuButtons(pressed).cancel && openingOrTitle(g);
        if (leaving) {
            back();
            pressed = 0;
        }
        g->menuActions = {};
        bool menus = menuState(g);
        const bool allow = infoPageButtons_.filter(controller_.any());
        g->controller = menus || leaving ? minotron::module::ControllerInput{} : controller_;
        g->menuActions.scroll = menuStick_[1];
        g->menuActions.controller = padMode_;
        if (g->state == 112) {
            g->controller.advance = allow && controller_.fire;
            g->menuActions.back = allow && neon::input::menuButtons(pressed).cancel;
        }
        if (menus)
            padMenus(g, pressed);

        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        dev_.alphaEnable(true);
        // YakEffect
        dev_.SetAlphaBlend(1, 1);
        const int modeBefore = g->gameMode;
        g->run();
        if (g->gameMode != modeBefore)
            modePreference_.selected();
        if (g->state == 8 && !g->demo && modePreference_.played(g->gameMode))
            writeModePreference();
        if (g->hs.dirty)
            saveScores();
        if (padMode_ && g->grid && menuState(g))
            menu_.highlight(g->grid, padMenu_.sel, ++menuPulse_,
                            [](const gridCell &) { return false; });
        bgc_ = g->bgColour;
        effect_->aspect = 1.0f;
        effect_->RunEffect();
        drawn_ = true;
    }

    // DrawEffect: the effect's output as one sprite over the whole surface.
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

    neon::ShaderCycleState &shaderHistory_;
    RenderSession renderState_;
    neon::input::ControllerFrame controllerFrame_;
    Host *host_ = nullptr;
    NeonResManager res_;
    NeonDevice dev_;
    NeonDrawContext ctx_;
    minotron::module::GameHost gameHost_;
    NeonEffect *effect_ = nullptr;
    WaveState waves_;
    SampleMixer mixer_;
    std::vector<uint8_t> save_;
    minotron::module::ControllerInput controller_;
    minotron::module::InfoPageButtonGate infoPageButtons_;
    mino::grid::Highlighter<cellGrid, gridCell, &gridCell::bgOn> menu_;
    mino::grid::PadMenu padMenu_;
    unsigned menuPulse_ = 0;
    bool padMode_ = false;
    uint32_t padPressed_ = 0;
    float menuStick_[2] = {};
    bool pauseRequest_ = false;
    uint32_t bgc_ = 0;
    double acc_ = 0;
    bool drawn_ = false;
    int surfW_ = kLogicalW, surfH_ = kLogicalH;
};

const CoreInfo kInfo = {"minotron", "Minotron: 2112", kLogicalW, kLogicalH, 1.5f,
                        1.5f,       "minotron.png"};
const CoreInfo &MinotronCore::info() const {
    return kInfo;
}

} // namespace

extern const CoreEntry kMinotronCore = {&kInfo, []() -> Core * {
                                            static neon::ShaderCycleState history;
                                            return new MinotronCore(history);
                                        }};

} // namespace mino

// The game's audio entry point (AudioSystem.c).
namespace NEON_GAME {
void AudioSystemPlaySample(int sample, int volume, int flags) {
    if (mino::gMixer)
        mino::gMixer->play(sample, volume, flags);
}
} // namespace NEON_GAME
