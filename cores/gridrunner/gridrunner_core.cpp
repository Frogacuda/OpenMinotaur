// Platform shell for the source reconstruction and the separate VIC Classic
// game. Controls, local records and text menus are deliberate port extensions.
#include "neon/backend_setup.h"
#include "startup_screen.h"
#include "neon/controller_session.h"
#include "gridrunner_game_host.h"
#include "core_api.h"
#include "../../platform/build_info.h"
#include "controls.h"
#include "modern_profile.h"
#include "../../reconstruction/gridrunner/native_levels.h"
#include "../../reconstruction/gridrunner/native_classic.h"
#include "native_view.h"
#include "game_format.h"
#include "original_frontend.h"
#include "original_messages.h"
#include "../../reconstruction/gridrunner/native_star.h"
#include "text_menus.h"
#include "sample_audio.h"
#include "engine/game.h"
#include "neon_modules.h"
#include "neon_audio.h"
#include "neon_resources.h"
#include "highscores.h"
#include <glad/gl.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>

namespace gridrunner {
namespace Neon_Internals {
WaveState *Waves = nullptr;
}
} // namespace gridrunner
namespace mino {
namespace {
using namespace gridrunner::Neon_Internals;
using gridrunner::UiAction;
using gridrunner::UiButton;
constexpr int kW = 480, kH = 640;
constexpr double kTick = 1.0 / 60;
const CoreInfo kInfo = {"gridrunner", "Gridrunner", kW, kH, .75f, .75f, "gridrunner.png"};
const CoreInfo kClassicInfo = {
    "gridrunner-classic", "Gridrunner Classic", 640,     480,         4.f / 3, 4.f / 3,
    "gridrunner.png",     "gridrunner",         nullptr, "gridrunner"};
using NativeGrid = gr::reconstruction::ios::Campaign;

class GridrunnerCore final : public Core {
    friend struct GridrunnerUxChecks;
    mino::StartupScreen startup_;

  public:
    explicit GridrunnerCore(neon::ShaderCycleState &history, bool classic = false)
        : classic_(classic), kW(classic ? 640 : 480), kH(classic ? 480 : 640),
          shaderHistory_(history) {
        renderState_.shaderCycles = history;
    }
    const CoreInfo &info() const override {
        return classic_ ? kClassicInfo : kInfo;
    }
    NativeFormat nativeFormat() const override {
        return gridrunner::nativeFormat(classic_ ? gridrunner::GameVersion::ClassicVic20
                                                 : gridrunner::GameVersion::Enhanced);
    }
    bool init(Host &host, const SurfaceInfo &surface) override {
        // Classic had no independent launch image in the original release.
        if (!classic_ && !startup_.begin(host, surface))
            return false;
        auto bind = renderState_.bind();
        host_ = &host;
        surface_ = surface;
        std::vector<uint8_t> xml;
        if (!host.readAsset("NeonResources.xml", xml))
            return false;
        std::string error;
        if (!resources_.load(std::string(xml.begin(), xml.end()), &error)) {
            host.log("Gridrunner XML: %s", error.c_str());
            return false;
        }
        if (resources_.spriteCount() != 102 || resources_.literals().size() != 3)
            return false;
        std::vector<gridrunner::Sample> samples;
        int loaded = 0;
        for (const auto &entry : resources_.samples()) {
            gridrunner::Sample sample;
            sample.maxInstances = std::max(1, entry.maxInstances);
            sample.priority = entry.priority;
            std::vector<uint8_t> wav;
            if (host.readAsset(entry.file, wav) && gridrunner::parseWav(wav, sample))
                ++loaded;
            samples.push_back(std::move(sample));
        }
        mixer_.setSamples(std::move(samples));
        if (!classic_)
            gridrunner::migratePreviewProfile(host.saveDir());
        scores_.init(3, {"PLAYER"}, 0, 0);
        scores_.load(host.saveDir() + scoreFile());
        achievementRecord_.load(host.saveDir() + "achievements.dat");
        host.observeOriginalAchievements(achievementRecord_.unlocked);
        progress_.load(host.saveDir() + checkpointFile());
        std::ifstream preferences(host.saveDir() + preferencesFile());
        expertMode_ = false;
        int oldAnalog = 1, touch = 0, survival = 0, level = 0;
        if (preferences >> oldAnalog >> touch) {
            // Preserve the saved touch choice: normal is Mode 1, expert Mode 2.
            expertMode_ = touch != 0;
            if (preferences >> survival >> level) {
                survival_ = survival != 0;
                selectedLevel_ = std::clamp(level / 4 * 4, 0, progress_.highestLevel());
            }
        }
        if (const char *levelText = classic_ ? nullptr : std::getenv("MINO_GRIDRUNNER_LEVEL")) {
            devStart_ = true;
            devLevel_ = int(std::clamp(std::strtol(levelText, nullptr, 10), 1L, 1000L)) - 1;
        }
        device_.setHost(&host);
        if (device_.Init(0, kW, kH, surface.framebuffer, surface.width, surface.height, 0))
            return false;
        deviceReady_ = true;
        device_.InitAllShaders();
        device_.setNearestTextures(false);
        device_.InitLiteralTextures(int(resources_.literals().size()), resources_.literals());
        if (!neon::backend::validateLiterals(device_, 3))
            return false;
        prepareButtonArt();
        renderState_.connect(resources_, device_, context_);
        neon::backend::initializeDrawing(context_, device_, {0, 24000}, {0, 24000},
                                         neon::backend::DrawingOrder::ConfigureEach,
                                         neon::backend::BufferFailure::IgnoreReturn);
        Waves = &waves_;
        renderState_.beginBatch();
        renderSession().sourceTexture = -1;
        std::vector<uint8_t> dna;
        std::vector<superSaver> cells;
        if (!host.readAsset("testGrid.ox", dna) || !loadGrid(dna, cells) || cells.empty() ||
            !cells[0].hasDNA) {
            host.log("Gridrunner: original testGrid.ox is required");
            return false;
        }
        effect_.reset(cells[0].createEffectFromDNA(&device_, 0, &gameHost_));
        if (!effect_ || !gameHost_.game()) {
            host.log("Gridrunner: effect build failed: %s", cells[0].error.c_str());
            return false;
        }
        auto *last = neon::backend::visitModules(*effect_, [&](auto &, auto &module) {
            if (auto *grid = dynamic_cast<cellGrid *>(&module)) {
                gameHost_.game()->grid = grid;
                grid->cls();
                grid->clattr();
            }
        });
        context_.effect = effect_.get();
        context_.effectStack = last ? last->stackIndex : 0;
        gameHost_.game()->background = [this](sprite *spr, vec2d size) {
            drawBackground(spr, size);
        };
        gameHost_.game()->draw = [this](sprite *spr, vec2d size) { drawGame(spr, size); };
        host.log("Gridrunner restoration: %s; %d/65 samples", classic_ ? "Classic VIC" : "Enhanced",
                 loaded);
        if (devStart_)
            host.log("Gridrunner development start: %d; scores/checkpoints disabled",
                     devLevel_ + 1);
        starDraw_.prepare(stars_);
        rebuildButtons();
        return true;
    }
    void resize(const SurfaceInfo &surface) override {
        startup_.resize(surface);
        auto bind = renderState_.bind();
        surface_ = surface;
        device_.SetBackbuffer(surface.framebuffer, kW, kH, surface.width, surface.height);
        cancelInput();
    }
    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        auto bind = renderState_.bind();
        if (suspended_)
            return;
        gridrunner::Axis point{double(e.x) / std::max(1, surface_.width) - .5,
                               double(e.y) / std::max(1, surface_.height) - .5};
        if (e.phase == TouchPhase::Down && playing() &&
            gridrunner::hitButton(buttons_, point.x, point.y) < 0) {
            padMode_ = false;
            touchControl_.down(e.id, point);
            return;
        }
        if (touchControl_.contains(e.id)) {
            if (e.phase == TouchPhase::Move)
                touchControl_.move(e.id, point);
            else if (e.phase == TouchPhase::Up || e.phase == TouchPhase::Cancel)
                touchControl_.up(e.id, point, e.phase == TouchPhase::Cancel);
            return;
        }
        if (e.phase == TouchPhase::Down && !touching_) {
            padMode_ = false;
            touching_ = true;
            touchId_ = e.id;
            touchButton_ = gridrunner::hitButton(buttons_, point.x, point.y);
            touchPauseHotspot_ = !classic_ && touchButton_ < 0 && !paused_ && !scoresOpen_ &&
                                 (demo_ || (!active_ && frontend_.acceptsPlay())) &&
                                 gridrunner::pauseHotspot(point.x, point.y);
            if (touchButton_ >= 0) {
                selected_ = touchButton_;
                if (buttons_[size_t(touchButton_)].action == UiAction::Pause)
                    pauseAlpha_ = 1;
            } else if (playing())
                touchControl_.begin(point);
        } else if (touching_ && e.id == touchId_) {
            if (e.phase == TouchPhase::Move) {
                if (touchControl_.active())
                    touchControl_.move(point);
            } else if (e.phase == TouchPhase::Up || e.phase == TouchPhase::Cancel) {
                int hit = gridrunner::hitButton(buttons_, point.x, point.y);
                UiAction action = e.phase == TouchPhase::Up && hit == touchButton_ && hit >= 0
                                      ? buttons_[size_t(hit)].action
                                      : UiAction::None;
                if (e.phase == TouchPhase::Up && !paused_ && !scoresOpen_) {
                    if (classicTitle())
                        action = UiAction::Play;
                    else if (touchPauseHotspot_) {
                        if (gridrunner::pauseHotspot(point.x, point.y))
                            action = UiAction::Pause;
                    } else if (touchButton_ < 0 && hit < 0) {
                        if (demo_ ||
                            (!active_ && frontend_.acceptsPlay() && !frontend_.levelSelect()) ||
                            (active_ && frontend_.stage() == 21))
                            action = UiAction::Play;
                    }
                }
                if (touchControl_.active()) {
                    if (e.phase == TouchPhase::Up) {
                        touchControl_.move(point);
                        touchControl_.release();
                    } else
                        touchControl_.cancel();
                    action = UiAction::None;
                }
                touching_ = false;
                touchButton_ = -1;
                touchPauseHotspot_ = false;
                if (action != UiAction::None)
                    activate(action);
            }
        }
    }
    void pads(const PadState *pads, int count) override {
        if (startup_.pads(pads, count))
            return;
        controllerFrame_.capture(pads, count);
        if (!controllerFrame_.connected())
            padMode_ = false;
        if (host_->keyboardNavigationAvailable())
            padMode_ = true;
        auto bind = renderState_.bind();
        pad_ = {};
        digital_ = false;
        speedModifierHeld_ = false;
        if (suspended_)
            return;
        uint32_t pressed = 0, held = 0;
        int dx = 0, dy = 0;
        bool trigger = false;
        for (const auto &p : controllerFrame_.slots())
            if (p.connected) {
                pressed |= p.pressed;
                held |= p.buttons;
                trigger |= p.lt > .5f || p.rt > .5f;
                gridrunner::Axis d{
                    double(bool(p.buttons & PAD_RIGHT)) - double(bool(p.buttons & PAD_LEFT)),
                    double(bool(p.buttons & PAD_DOWN)) - double(bool(p.buttons & PAD_UP))};
                if (d.x || d.y) {
                    pad_ = gridrunner::unitDisk(d);
                    digital_ = true;
                } else if (!digital_ && std::hypot(p.lx, p.ly) > std::hypot(pad_.x, pad_.y))
                    pad_ = {p.lx, p.ly};
            }
        if (inputGate_) {
            if (held || pressed || trigger || std::hypot(pad_.x, pad_.y) > .25) {
                pad_ = {};
                return;
            }
            inputGate_ = false;
        }
        speedModifierHeld_ = (held & PAD_SOUTH) != 0;
        if (classicTitle()) {
            const auto actions = neon::input::menuButtons(pressed);
            if (actions.cancel)
                activate(UiAction::Exit);
            else if (actions.confirm || actions.pause)
                activate(UiAction::Play);
            return;
        }
        if (pressed || std::hypot(pad_.x, pad_.y) > .5)
            padMode_ = true;
        if (!playing()) {
            if (std::abs(pad_.x) > std::abs(pad_.y) && std::abs(pad_.x) > .5)
                dx = pad_.x < 0 ? -1 : 1;
            else if (std::abs(pad_.y) > .5)
                dy = pad_.y < 0 ? -1 : 1;
            if (dx != heldDx_ || dy != heldDy_ || repeat_ <= 0) {
                if (dx || dy)
                    selected_ = nameEntry_ ? gridrunner::moveNameFocus(buttons_, selected_, dx, dy)
                                           : gridrunner::moveFocus(buttons_, selected_, dx, dy);
                repeat_ = dx != heldDx_ || dy != heldDy_ ? .3 : .12;
            }
        }
        heldDx_ = dx;
        heldDy_ = dy;
        const auto actions = neon::input::menuButtons(pressed);
        if (actions.cancel)
            back();
        else if (actions.pause) {
            if (nameEntry_)
                finishName();
            else if (scoresOpen_)
                closeScores();
            else if (paused_)
                resume();
            else if (demo_)
                activate(UiAction::Play);
            else if (active_ && frontend_.stage() != 21)
                openPause();
            else
                activate(UiAction::Play);
        } else if (actions.confirm && !playing()) {
            if (selected_ >= 0 && selected_ < int(buttons_.size()))
                activate(buttons_[size_t(selected_)].action);
            else if (demo_ || (!active_ && frontend_.acceptsPlay()) ||
                     (active_ && frontend_.stage() == 21))
                activate(UiAction::Play);
        }
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        auto bind = renderState_.bind();
        if (classicTitle()) {
            activate(UiAction::Exit);
            return true;
        }
        if (nameEntry_) {
            activate(UiAction::NameDelete);
            return true;
        }
        if (titleLanding()) {
            activate(UiAction::Exit);
            return true;
        }
        if (!classic_ && !active_ && !scoresOpen_ && !paused_) {
            activate(UiAction::Title);
            return true;
        }
        if (scoresOpen_)
            closeScores();
        else if (paused_)
            resume();
        else
            openPause();
        return true;
    }
    void pause(bool suspended) override {
        auto bind = renderState_.bind();
        suspended_ = suspended;
        cancelInput();
        accumulator_ = 0;
        if (suspended && !paused_)
            openPause();
    }
    void mixAudio(float *output, int frames) override {
        mixer_.mix(output, frames);
    }
    void save() override {
        auto bind = renderState_.bind();
        if (!host_)
            return;
        if (nameEntry_ && entryRank_ >= 0) {
            scores_.at(entryBoard_, entryRank_).name = entryName_.empty() ? "PLAYER" : entryName_;
            scores_.dirty = true;
        }
        if (scores_.dirty && !scores_.save(host_->saveDir() + scoreFile()))
            scores_.dirty = true;
        if (progressDirty_ && progress_.save(host_->saveDir() + checkpointFile()))
            progressDirty_ = false;
        if (!achievementRecord_.save(host_->saveDir() + "achievements.dat"))
            host_->log("Gridrunner: achievement save failed; retry pending");
        std::ofstream preferences(host_->saveDir() + preferencesFile());
        preferences << int(!expertMode_) << ' ' << int(expertMode_) << ' ' << int(survival_) << ' '
                    << selectedLevel_ << '\n';
    }
    void shutdown() override {
        startup_.release();
        auto bind = renderState_.bind();
        shaderHistory_ = renderState_.shaderCycles;
        native_.reset();
        mixer_.stopAll();
        effect_.reset();
        context_.effect = nullptr;
        buttonArt_.clear();
        redButton_ = -1;
        if (deviceReady_)
            device_.Destroy(true);
        deviceReady_ = false;
        Waves = nullptr;
        renderState_.disconnect();
        renderState_.beginBatch();
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        auto bind = renderState_.bind();
        repeat_ -= std::max(0.0, dt);
        if (!suspended_) {
            accumulator_ += std::clamp(dt, 0.0, .25);
            while (accumulator_ >= kTick) {
                tick();
                if (!paused_ && !scoresOpen_ && !nameEntry_)
                    renderEffect();
                accumulator_ -= kTick;
            }
        } else
            accumulator_ = 0;
        if (effect_) {
            if (!rendered_)
                renderEffect();
            present();
        }
    }

  private:
    const char *scoreFile() const {
        return classic_ ? "ios-rules-highscores.txt" : "highscores.txt";
    }
    const char *preferencesFile() const {
        return classic_ ? "gridrunner-controls.txt" : "controls.txt";
    }
    const char *checkpointFile() const {
        return classic_ ? "gridrunner-checkpoints.txt" : "checkpoints.txt";
    }
    bool nativeEnded() const {
        return native_ && (native_->state() == NativeGrid::State::Complete ||
                           native_->state() == NativeGrid::State::GameOver);
    }
    bool playing() const {
        return active_ && !demo_ && !paused_ && !scoresOpen_ && !nameEntry_ &&
               (classic_ ? classicGame_.state() != gr::reconstruction::ios::Classic::State::GameOver
                         : !nativeEnded());
    }
    bool classicTitle() const {
        return classic_ && !active_ && !paused_ && !scoresOpen_ && !nameEntry_;
    }
    bool titleLanding() const {
        return !classic_ && !active_ && !frontend_.levelSelect() && !paused_ && !scoresOpen_ &&
               !nameEntry_;
    }
    int startLevel() const {
        return devStart_ ? devLevel_ : survival_ ? 0 : selectedLevel_;
    }
    gridrunner::GameMode mode() const {
        if (classic_)
            return gridrunner::GameMode::Pure;
        return survival_      ? gridrunner::GameMode::Survival
               : startLevel() ? gridrunner::GameMode::Casual
                              : gridrunner::GameMode::Pure;
    }
    int shownScore() const {
        return native_                  ? native_->points()
               : classic_               ? classicGame_.score()
               : devStart_ || survival_ ? 0
                                        : progress_.at(startLevel()).score;
    }
    int shownLives() const {
        return native_     ? native_->ac.lives
               : classic_  ? classicGame_.lives()
               : survival_ ? 1
               : devStart_ ? 3
                           : progress_.at(startLevel()).lives;
    }
    int best() const {
        return std::max(shownScore(), scores_.entry(int(mode()), 0).score);
    }
    void cancelInput() {
        controllerFrame_.clear();
        pad_ = {};
        speedModifierHeld_ = false;
        touchControl_.cancel();
        touching_ = false;
        touchButton_ = -1;
        touchPauseHotspot_ = false;
        inputGate_ = true;
        heldDx_ = heldDy_ = 0;
        repeat_ = 0;
    }
    void openPause() {
        paused_ = true;
        pauseAlpha_ = 0;
        pauseMenuAlpha_ = 0;
        rendered_ = false;
        selected_ = 0;
        cancelInput();
        if (native_)
            native_->clearPausedFeedback();
        mixer_.stopAll();
        mixer_.play(60, 7 << 12, 0);
        rebuildButtons();
    }
    void resume() {
        paused_ = false;
        pauseAlpha_ = 1;
        cancelInput();
        selected_ = 0;
        rebuildButtons();
    }
    void closeScores() {
        scoresOpen_ = false;
        cancelInput();
        selected_ = scoreReturnSelection_;
        rebuildButtons();
    }
    void activate(UiAction action) {
        if (paused_ && pauseMenuAlpha_ <= .9 &&
            (action == UiAction::Exit || action == UiAction::Title ||
             action == UiAction::Acceleration))
            return;
        mixer_.play(60, 7 << 12, 0);
        if (nameEntry_) {
            if (action == UiAction::NameDone)
                finishName();
            else if (action == UiAction::NameDelete) {
                if (!entryName_.empty())
                    entryName_.pop_back();
            } else if (action == UiAction::NameKey && selected_ >= 0 && selected_ < 36)
                gridrunner::appendName(entryName_,
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[selected_],
                                       HighScores::kNameLen, selected_);
            rebuildButtons();
            return;
        }
        switch (action) {
        case UiAction::Pause:
            if (paused_)
                resume();
            else
                openPause();
            break;
        case UiAction::Exit:
            recordScore();
            save();
            host_->requestQuit();
            break;
        case UiAction::Title:
            recordScore();
            save();
            mixer_.stopAll();
            native_.reset();
            active_ = demo_ = paused_ = scoresOpen_ = nameEntry_ = starting_ = startingNext_ =
                false;
            pauseMenuAlpha_ = pauseAlpha_ = 0;
            messages_.clear();
            frontend_.reset();
            if (classic_)
                classicGame_.title(best(), uint32_t(frame_));
            cancelInput();
            selected_ = 0;
            rendered_ = false;
            break;
        case UiAction::Acceleration:
            if (paused_) {
                expertMode_ = !expertMode_;
                touchControl_.cancel();
                save();
            }
            break;
        case UiAction::Scores:
            if (paused_ || titleLanding() || (!classic_ && nativeEnded())) {
                scoreReturnSelection_ = selected_;
                scoresOpen_ = true;
                scoreBoard_ = int(mode());
                cancelInput();
                selected_ = 0;
            }
            break;
        case UiAction::Back:
            closeScores();
            break;
        case UiAction::Previous:
        case UiAction::Next:
            if (scoresOpen_)
                scoreBoard_ = classic_ ? 0 : (scoreBoard_ + (action == UiAction::Next ? 1 : 2)) % 3;
            else if (!survival_ && !devStart_) {
                selectedLevel_ += action == UiAction::Next ? 4 : -4;
                if (selectedLevel_ < 0)
                    selectedLevel_ = progress_.highestLevel();
                if (selectedLevel_ > progress_.highestLevel())
                    selectedLevel_ = 0;
                save();
            }
            break;
        case UiAction::Mode:
            survival_ = !survival_;
            save();
            break;
        case UiAction::Play:
            if (!classic_ && nativeEnded() && !paused_ && !scoresOpen_) {
                native_.reset();
                active_ = false;
                frontend_.startTransition();
                starting_ = true;
                cancelInput();
                break;
            }
            if (classic_) {
                if (!active_ && !paused_ && !scoresOpen_) {
                    messages_.clear();
                    classicGame_.start();
                    active_ = true;
                    recorded_ = false;
                    pauseAlpha_ = 1;
                    cancelInput();
                }
                break;
            }
            if (demo_) {
                demo_ = false;
                active_ = false;
                native_.reset();
                mixer_.stopAll();
                frontend_.reset();
                frontend_.selectLevel();
                frontend_.playerSelected();
                cancelInput();
                selected_ = 0;
                break;
            }
            if (paused_ || scoresOpen_ || active_)
                break;
            if (!frontend_.acceptsPlay())
                break;
            frontend_.playerSelected();
            if (!frontend_.levelSelect())
                frontend_.selectLevel();
            else {
                frontend_.startTransition();
                starting_ = true;
            }
            cancelInput();
            selected_ = 0;
            break;
        default:
            break;
        }
        rebuildButtons();
    }
    void startGame() {
        startNative(false);
    }
    void startNative(bool demo) {
        messages_.clear();
        const int nativeStart = demo ? 0 : startLevel();
        native_ = std::make_unique<NativeGrid>(
            [this](int32_t frame) {
                const auto *art = resources_.sprite(frame);
                if (!art)
                    throw std::logic_error("Original native sprite dimensions unavailable");
                return gr::reconstruction::ios::Vec2{double(art->w), double(art->h)};
            },
            sessionRandom_.state(), uint32_t(frame_),
            demo || survival_ ? 1
            : devStart_       ? 3
                              : progress_.at(startLevel()).lives,
            demo, nativeStart, survival_,
            demo || survival_ || devStart_ ? 0 : progress_.at(startLevel()).score, &sessionRandom_);
        native_->survival = survival_;
        active_ = true;
        demo_ = demo;
        demoTicks_ = 0;
        starting_ = startingNext_ = false;
        recorded_ = false;
        if (!demo)
            pauseAlpha_ = 1;
        nativeDraw_.prepare(*native_);
        cancelInput();
        mixer_.play(38, 7 << 12, 0);
        host_->log("Gridrunner %s native campaign: grid %d; shared RNG %u; clock %d",
                   mino::kBuildTag, native_->level() + 1, sessionRandom_.state(), frame_);
    }
    void tickNative() {
        if (!paused_ && !scoresOpen_ && !nameEntry_) {
            if (native_ && startingNext_)
                ++native_->clock;
            if (native_ && active_ && !nativeEnded() && !startingNext_) {
                namespace source = gr::reconstruction::ios;
                std::array<source::Rgba, 33> palette{};
                for (int i = 1; i < 6; ++i) {
                    constexpr int cycles[] = {0, 8, 5, 0, 4, 2};
                    palette[size_t(i)] = source::dynamicColour(cycles[i], 1, native_->clock + 1);
                }
                for (int i = 0; i < 8; ++i)
                    palette[size_t(i + 6)] = source::unpackRgba(
                        gridrunner::data::flashes[i * 16 + ((native_->clock + 1) & 15)]);
                native_->updatePalette(palette);
                auto d = touchControl_.tick(expertMode_ ? gridrunner::TouchMode::Expert
                                                        : gridrunner::TouchMode::Normal);
                if (!touchControl_.active() && (pad_.x || pad_.y)) {
                    const auto v = gridrunner::controllerResponse(pad_, digital_, expertMode_,
                                                                  speedModifierHeld_);
                    d = {v.x * 180 / kW / 60, v.y * 180 / kH / 60};
                }
                const auto before = native_->state();
                native_->tick({d.x, d.y});
                feedbackFlash_ = native_->presentedFeedback();
                nativeDraw_.prepare(*native_);
                for (int goal : native_->achievements)
                    recordAchievement(goal);
                if (before != native_->state())
                    host_->log("Native campaign grid %d state %d: clock %u, score %d, lives %d",
                               native_->level() + 1, int(native_->state()), native_->clock,
                               native_->points(), native_->ac.lives);
                for (int sound : native_->sounds) {
                    if (sound < 0)
                        mixer_.stop(-1 - sound);
                    else
                        mixer_.play(sound, native_->soundVolume(sound), 0);
                }
                if (effect_)
                    for (float &channel : gameHost_.game()->fbColour)
                        channel = feedbackFlash_;
                if (demo_) {
                    if (++demoTicks_ > 3500 || native_->state() != NativeGrid::State::Playing)
                        finishDemo();
                } else {
                    if (before == NativeGrid::State::Playing &&
                        native_->state() == NativeGrid::State::Completing)
                        frontend_.startCompletion(native_->level());
                    if (native_->state() == NativeGrid::State::Complete &&
                        native_->advanceLevel()) {
                        if (!survival_ && !devStart_) {
                            progressDirty_ |= progress_.reached(native_->level(), native_->points(),
                                                                native_->ac.lives);
                            save();
                        }
                        frontend_.startTransition();
                        startingNext_ = true;
                        nativeDraw_.prepare(*native_);
                        cancelInput();
                        host_->log("Native campaign advanced to grid %d: score %d, lives %d",
                                   native_->level() + 1, native_->points(), native_->ac.lives);
                    }
                    if (nativeEnded()) {
                        selected_ = 0;
                        recordScore(true);
                        cancelInput();
                    }
                }
            }
            if (!nativeEnded())
                frontend_.tick(shownScore(), best(), shownLives(), mode());
            if (frontend_.levelSelect())
                frontend_.selectionInfo(startLevel(), shownScore(), shownLives(), mode());
            if (frontend_.stage() == 6)
                for (int i = 0; i < HighScores::kRows; ++i) {
                    const auto &e = scores_.entry(0, i);
                    frontend_.attractScore(i, e.name, e.score);
                }
            if (starting_ && frontend_.readyToStart())
                startGame();
            if (startingNext_ && frontend_.readyToStart()) {
                startingNext_ = false;
                cancelInput();
            } else if (!active_ && frontend_.demoRequested())
                startDemo();
        }
        rebuildButtons();
    }
    void startDemo() {
        frontend_.consumeDemoRequest();
        startNative(true);
    }
    void finishDemo() {
        demo_ = false;
        active_ = false;
        native_.reset();
        mixer_.stopAll();
        frontend_.finishDemo();
        cancelInput();
    }
    void finishName() {
        if (!nameEntry_)
            return;
        host_->log("Gridrunner name entry completed");
        scores_.at(entryBoard_, entryRank_).name = entryName_.empty() ? "PLAYER" : entryName_;
        scores_.lastName = scores_.at(entryBoard_, entryRank_).name;
        scores_.dirty = true;
        nameEntry_ = false;
        save();
        cancelInput();
        selected_ = 0;
        rebuildButtons();
    }
    void recordScore(bool askName = false) {
        if (recorded_ || !active_ || demo_ || devStart_)
            return;
        recorded_ = true;
        if (shownScore() > 0) {
            entryBoard_ = int(mode());
            entryName_ = scores_.lastName;
            entryRank_ = scores_.insert(entryBoard_, entryName_.empty() ? "PLAYER" : entryName_,
                                        shownScore(),
                                        classic_  ? classicGame_.grid()
                                        : native_ ? native_->level() + 1
                                                  : 1);
            nameEntry_ = askName && entryRank_ >= 0;
            host_->log("Gridrunner score recorded: ask=%d rank=%d nameEntry=%d", int(askName),
                       entryRank_, int(nameEntry_));
            if (nameEntry_) {
                mixer_.play(61, 7 << 12, 0);
                cancelInput();
                selected_ = entryName_.size() >= HighScores::kNameLen ? gridrunner::kNameDone : 0;
            }
        }
        save();
    }
    void tick() {
        ++frame_;
        waves_.beat = frame_;
        if (!paused_ || scoresOpen_ || nameEntry_) {
            stars_.tick();
            starDraw_.prepare(stars_);
        }
        if (!paused_)
            pauseAlpha_ *= .98;
        pauseMenuAlpha_ = paused_ ? std::min(1., pauseMenuAlpha_ + .05) : pauseMenuAlpha_ * .95;
        if (classic_) {
            if (!paused_ && !scoresOpen_ && !nameEntry_) {
                if (active_) {
                    auto d = touchControl_.tick(expertMode_ ? gridrunner::TouchMode::Expert
                                                            : gridrunner::TouchMode::Normal,
                                                gridrunner::TouchTarget::Classic);
                    if (!touchControl_.active() && (pad_.x || pad_.y)) {
                        const auto v = gridrunner::controllerResponse(pad_, digital_, expertMode_,
                                                                      speedModifierHeld_);
                        d = {v.x * .025, v.y * .025};
                    }
                    classicGame_.tick(d.x, d.y, uint32_t(frame_));
                    for (int goal : classicGame_.achievements())
                        recordAchievement(goal);
                    for (int sound : classicGame_.sounds())
                        mixer_.play(sound, 7 << 12, 0);
                    if (classicGame_.state() == gr::reconstruction::ios::Classic::State::GameOver) {
                        recordScore(true);
                        active_ = false;
                        classicGame_.title(best(), uint32_t(frame_));
                        cancelInput();
                    }
                } else
                    classicGame_.title(best(), uint32_t(frame_));
            }
            messages_.prepare(paused_ || scoresOpen_ || nameEntry_);
            rebuildButtons();
            return;
        }
        tickNative();
        messages_.prepare(paused_ || scoresOpen_ || nameEntry_);
    }
    void rebuildButtons() {
        using A = UiAction;
        buttons_.clear();
        if (nameEntry_) {
            static const char *keys[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                         "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                         "S", "T", "U", "V", "W", "X", "Y", "Z", "0",
                                         "1", "2", "3", "4", "5", "6", "7", "8", "9"};
            for (int i = 0; i < 36; ++i)
                buttons_.push_back({A::NameKey, 0, (i % 9 - 4) * .095, -.08 + (i / 9) * .1, .04,
                                    .04, 1, 1, keys[i]});
            buttons_.push_back({A::NameDelete, 0, -.24, .36, .2, .04, 1, 1, "Delete"});
            buttons_.push_back({A::NameDone, 0, .24, .36, .2, .04, 1, 1, "Done"});
        } else if (scoresOpen_) {
            buttons_ = gridrunner::scoreMenu();
        } else if (paused_) {
            buttons_ = gridrunner::pauseMenu(expertMode_, pauseMenuAlpha_, double(kW) / kH);
        } else if (!classic_ && nativeEnded()) {
            buttons_.push_back({A::Play, 0, 0, .20, .25, .05, 1, 1, "Play again"});
            buttons_.push_back({A::Scores, 0, 0, .30, .25, .05, 1, 1, "High scores"});
            buttons_.push_back({A::Exit, 0, 0, .40, .25, .05, 1, 1, "Exit"});
        } else if (!active_ && frontend_.levelSelect()) {
            buttons_.push_back({A::Play, 80, 0, .32, .07 * 97 / 24, .07});
            buttons_.push_back({A::Mode,
                                survival_      ? 94
                                : startLevel() ? 96
                                               : 95,
                                0, -.08, .07 * 97 / 24, .07});
            if (!survival_ && progress_.highestLevel() > 0) {
                buttons_.push_back({A::Previous, 82, -.43, -.08, .07 * 22 / 24, .07});
                buttons_.push_back({A::Next, 81, .43, -.08, .07 * 22 / 24, .07});
            }
        } else if (playing())
            buttons_.push_back(gridrunner::pauseMenu(expertMode_, 1, double(kW) / kH).front());
        if (selected_ < 0 || selected_ >= int(buttons_.size()))
            selected_ = buttons_.empty() ? -1 : 0;
    }
    void drawBackground(sprite *spr, vec2d size) {
        if (classic_)
            return;
        frontend_.screen().draw(
            spr, size, frame_,
            native_ ? native_->gridColour()
                    : gr::reconstruction::ios::levelData::gridColours[(startLevel() >> 2) & 7]);
        drawMessages(spr, size, false);
    }
    void recordAchievement(int goal) {
        achievementRecord_.record(goal);
        messages_.show(goal);
        host_->observeOriginalAchievements(achievementRecord_.unlocked);
        // Android explicitly plays sample 63 here. Retain the local host cue;
        // iOS resets +e0c, whose active consumer has not been identified.
        mixer_.play(63, 7 << 12, 0);
        host_->log("Gridrunner local goal %d at frame %d: %s", goal, frame_,
                   gridrunner::messageData::goals[goal]);
        if (!achievementRecord_.save(host_->saveDir() + "achievements.dat"))
            host_->log("Gridrunner: achievement save failed; retry pending");
    }
    void drawMessages(sprite *target, vec2d size, bool overlay) {
        if (messages_.glyphs().empty())
            return;
        auto *grid = gameHost_.game()->grid;
        auto route = [&](int literal) {
            auto *stack = target->batchStack;
            _input_info src{stack->id, grid->index, literal, INPUT_LITERAL},
                dst{stack->id, grid->index, 0, 0};
            stack->effect->AssignTextureInput(&src, &dst);
            grid->SetModuleSourceTextures(&stack->render);
        };
        // Proportional glyphs are on the character page, not the game's
        // sprite/STM page. Flush before switching the shared stack input.
        if (overlay)
            settex(1);
        else {
            target->setupPurge();
            target->batchCount = 0;
            target->batchPtr = target->batchStack->verts;
            route(grid->glyphLiteral);
        }
        const auto flags = target->flags;
        for (const auto &g : messages_.glyphs()) {
            target->frame = 0;
            target->flags = SPR_REGION;
            target->angle = 0;
            target->pos = {(g.x + (overlay ? .5 : 0)) * size.x,
                           (g.y + (overlay ? .5 : 0)) * size.y};
            target->width = g.w * size.x;
            target->height = g.h * size.y;
            target->regionX = g.atlasX;
            target->regionY = g.atlasY;
            target->regionW = g.width;
            target->regionH = 18 * .95;
            target->col.unpackRGBA(g.colour);
            if (overlay) {
                if (renderSession().count + 6 > kLocalVerts)
                    purge();
                target->setupSimple(renderSession().cursor);
                renderSession().cursor += 6;
                renderSession().count += 6;
            } else
                target->setupChecking();
        }
        if (!overlay) {
            target->setupPurge();
            target->batchCount = 0;
            target->batchPtr = target->batchStack->verts;
            route(grid->bgLiteral);
        }
        target->flags = flags;
    }
    void drawGame(sprite *spr, vec2d size) {
        if (classic_)
            return;
        if (!scoresOpen_) {
            gridrunner::drawNativeGrid(starDraw_, spr, size);
            if (active_ && !startingNext_)
                drawWorld(spr, size);
            else
                frontend_.draw(spr, size);
        }
        if (paused_)
            return; // Original pause sprites are composed by the always-run UI below.
        for (const auto &button : buttons_) {
            if (button.label)
                continue; // Text is composited after the game effect.
            double alpha = button.alpha;
            if (button.action == UiAction::Pause && !paused_)
                alpha = pauseAlpha_;
            const bool selected = padMode_ && !playing() && selected_ >= 0 &&
                                  selected_ < int(buttons_.size()) &&
                                  &button == &buttons_[size_t(selected_)];
            if (!button.sprite || alpha <= .1)
                continue;
            uint32_t colour = 0xffffffff;
            if (selected) {
                const uint32_t light = gridrunner::menuColour(true, frame_) >> 24;
                colour = (light << 24) | (light << 16) | (light << 8) | 0xff;
            }
            gridrunner::drawUiSprite(spr, size, button.sprite, button.x, button.y, button.rx,
                                     button.ry, fade(colour, alpha));
        }
    }
    void drawScoreStars(vec2d size) {
        sprite star;
        gridrunner::drawNativeOverlay(starDraw_, &star, size);
        purge();
    }
    struct ButtonArt {
        int frame, x, y, w, h;
        std::vector<uint8_t> normal, red;
    };
    void prepareButtonArt() {
        // Capture only the six UI regions from the already-decoded atlas.
        // Keep its premultiplication, alpha, bevel and lettering byte-for-byte.
        const auto *atlas = device_.literal(0);
        if (!atlas)
            return;
        GLint read = 0, draw = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atlas->name, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            for (int frame : {80, 81, 82, 94, 95, 96}) {
                const auto *e = resources_.sprite(frame);
                if (!e || e->page != 0)
                    continue;
                ButtonArt art{frame, e->x, atlas->height - e->y - e->h, e->w, e->h, {}, {}};
                art.normal.resize(size_t(art.w) * art.h * 4);
                glReadPixels(art.x, art.y, art.w, art.h, GL_RGBA, GL_UNSIGNED_BYTE,
                             art.normal.data());
                art.red = art.normal;
                for (size_t i = 0; i < art.red.size(); i += 4)
                    std::swap(art.red[i], art.red[i + 2]);
                buttonArt_.push_back(std::move(art));
            }
        } else
            host_->log("Gridrunner: button highlight atlas capture unavailable");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, unsigned(read));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, unsigned(draw));
        glDeleteFramebuffers(1, &fbo);
    }
    void updateButtonArt() {
        int next = -1;
        if (padMode_ && !active_ && !paused_ && !scoresOpen_ && frontend_.levelSelect() &&
            selected_ >= 0 && selected_ < int(buttons_.size()))
            next = buttons_[size_t(selected_)].sprite;
        if (next == redButton_)
            return;
        const auto *atlas = device_.literal(0);
        if (!atlas)
            return;
        GLint binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        glBindTexture(GL_TEXTURE_2D, atlas->name);
        for (const auto &art : buttonArt_) {
            const auto *pixels = art.frame == next         ? &art.red
                                 : art.frame == redButton_ ? &art.normal
                                                           : nullptr;
            if (pixels)
                glTexSubImage2D(GL_TEXTURE_2D, 0, art.x, art.y, art.w, art.h, GL_RGBA,
                                GL_UNSIGNED_BYTE, pixels->data());
        }
        glBindTexture(GL_TEXTURE_2D, unsigned(binding));
        redButton_ = next;
    }

    static uint32_t fade(uint32_t rgba, double amount) {
        return (rgba & 0xffffff00u) | uint32_t(double(rgba & 255) * std::clamp(amount, 0.0, 1.0));
    }
    void drawWorld(sprite *spr, vec2d size) {
        if (native_)
            gridrunner::drawNativeGrid(nativeDraw_, spr, size);
    }
    void renderEffect() {
        if (classic_) {
            rendered_ = true;
            return;
        }
        if (!effect_)
            return;
        updateButtonArt();
        device_.BeginFrame();
        device_.SetCurrenRT(-1);
        device_.alphaEnable(true);
        effect_->aspect = 1;
        effect_->RunEffect();
        rendered_ = true;
    }
    void drawMenuOverlay() {
        const vec2d size{double(kW), double(kH)};
        device_.alphaEnable(true);
        device_.SetAlphaBlend(0x302, 0x303);
        if (scoresOpen_ || nameEntry_ || (!classic_ && nativeEnded())) {
            sprite shade;
            shade.frame = 2;
            shade.width = kW;
            shade.height = kH;
            shade.col.unpackRGBA(scoresOpen_ || nameEntry_ || (!classic_ && nativeEnded())
                                     ? 0x000000ff
                                     : 0x0000008c);
            shade.draw();
            purge();
        }
        if (classic_ && playing() && pauseAlpha_ > .1) {
            sprite icon;
            const auto b = gridrunner::pauseMenu(expertMode_, 1, double(kW) / kH).front();
            icon.flags = SPR_CENTRE_X | SPR_CENTRE_Y;
            icon.frame = b.sprite;
            icon.pos = {(b.x + .5) * size.x, (b.y + .5) * size.y};
            icon.width = 2 * b.rx * size.x;
            icon.height = 2 * b.ry * size.y;
            icon.col.white(pauseAlpha_);
            icon.draw();
        }
        if (!classic_ && nativeEnded() && !paused_ && !scoresOpen_ && !nameEntry_) {
            const auto heading =
                native_->state() == NativeGrid::State::Complete
                    ? std::string("Grid ") + std::to_string(native_->level() + 1) + " complete"
                    : std::string("Game over");
            gridrunner::drawMenuText(size, heading.c_str(), 0, -.12, 1.1, 0xffffffff);
            const auto score = std::string("Score ") + std::to_string(native_->points());
            gridrunner::drawMenuText(size, score.c_str(), 0, 0, 1, 0xffff00ff);
        }
        if (nameEntry_) {
            drawScoreStars(size);
            gridrunner::drawMenuText(size, "New High Score", 0, -.36, 1.1, 0xffffffff);
            const auto name = gridrunner::nameSlots(entryName_, HighScores::kNameLen);
            gridrunner::drawMenuText(size, name.c_str(), 0, -.24, 1, 0xffff00ff);
            // The original imported font has no visible underscore glyph.
            // Draw explicit slot markers at its baseline, preserving spacing.
            const double cw = size.x / gridrunner::OriginalStm::cols,
                         ch = size.y / gridrunner::OriginalStm::rows;
            for (size_t i = entryName_.size(); i < size_t(HighScores::kNameLen); ++i) {
                sprite line;
                line.frame = 2;
                line.flags = SPR_CENTRE_X | SPR_CENTRE_Y;
                line.col.unpackRGBA(0xffff00ff);
                line.width = cw * .65;
                line.height = std::max(1.5, ch * .08);
                line.pos = {size.x * .5 + (double(i) - double(HighScores::kNameLen - 1) * .5) * cw,
                            size.y * .26 + ch * .4};
                line.draw();
            }
        } else if (scoresOpen_) {
            drawScoreStars(size);
            gridrunner::OriginalStm table;
            char line[80];
            const char *heading = classic_           ? "^7    CLASSIC SCORES"
                                  : scoreBoard_ == 0 ? "^7   PURE MODE SCORES"
                                  : scoreBoard_ == 1 ? "^7  CASUAL MODE SCORES"
                                                     : "^7 SURVIVAL MODE SCORES";
            table.stringAt(0, 4, heading);
            for (int rank = 0; rank < HighScores::kRows; ++rank) {
                const auto &e = scores_.entry(scoreBoard_, rank);
                std::snprintf(line, sizeof line, "^7%2d %-10.10s %8d", rank + 1, e.name.c_str(),
                              e.score);
                table.stringAt(0, 7 + rank * 2, line);
            }
            sprite font;
            table.drawOverlay(&font, size, frame_);
            purge();
        }
        if (!scoresOpen_ && !nameEntry_ && (paused_ || pauseMenuAlpha_ > .1)) {
            const auto menu = gridrunner::pauseMenu(expertMode_, pauseMenuAlpha_, double(kW) / kH);
            for (size_t i = 0; i < menu.size(); ++i) {
                const auto &b = menu[i];
                if (!paused_ && (b.action == UiAction::Pause || b.action == UiAction::Scores))
                    continue;
                if (b.alpha <= .1)
                    continue;
                const double focus = paused_ && padMode_ && int(i) == selected_
                                         ? 1. + .08 * std::sin(frame_ * 6.283185307179586 / 72)
                                         : 1.;
                sprite icon;
                icon.flags = SPR_CENTRE_X | SPR_CENTRE_Y;
                icon.frame = b.sprite;
                const bool mode = b.action == UiAction::Acceleration;
                // Crop only the obsolete label, preserving the complete joystick base.
                const double crop = mode ? double(resources_.sprite(b.sprite)->h) / 49. : 1.,
                             offset = mode ? -.08 * (1 - crop) : 0;
                icon.pos = {(b.x + .5) * size.x, (b.y + .5 + offset) * size.y};
                icon.width = 2 * b.rx * size.x * focus;
                icon.height = 2 * b.ry * size.y * focus * crop;
                icon.col.white(b.alpha);
                icon.draw();
                if (mode) {
                    purge();
                    const vec2d labelSize{size.y * .75, size.y};
                    const double labelX = (b.x + .5) * size.x / labelSize.x - .5;
                    gridrunner::drawMenuText(labelSize, expertMode_ ? "Mode 2" : "Mode 1", labelX,
                                             b.y + .065, .55, fade(0xffffffff, b.alpha));
                }
            }
        }
        for (size_t i = 0; i < buttons_.size(); ++i) {
            const auto &b = buttons_[i];
            if (!b.label)
                continue;
            gridrunner::drawMenuText(
                size, b.label, b.x, b.y, b.scale,
                gridrunner::menuColour(padMode_ && int(i) == selected_, frame_));
        }
        purge();
    }
    void present() {
        device_.BeginFrame();
        device_.SetCurrenRT(-1);
        context_.main.SetCurrenRenderTarget(-1);
        context_.aux.SetCurrenRenderTarget(-1);
        device_.alphaEnable(false);
        device_.SetupDefaultProjection(kW, kH, PROJ_ORTHO);
        device_.ClearScreen(255, 0, 0, 0);
        D3DXMatrixTranslation(&context_.main.world, kW * -.5f, kH * .5f, 0);
        renderState_.beginBatch();
        if (classic_) {
            drawClassic();
            sprite text;
            drawMessages(&text, {double(kW), double(kH)}, true);
            purge();
        } else {
            sprite output;
            output.frame = INT_MIN;
            output.width = kW;
            output.height = kH;
            output.col.white(1);
            output.draw();
            purge();
        }
        drawMenuOverlay();
        device_.PresentFrame();
    }
    void drawClassic() {
        const auto &screen = classicGame_.screen();
        const double cw = double(kW) / screen.cols, ch = double(kH) / screen.rows;
        device_.alphaEnable(true);
        device_.SetAlphaBlend(0x302, 0x303);
        settex(0);
        for (const auto &g : screen.draw()) {
            const unsigned code = g.ink & 255;
            sprite s;
            s.frame = 0;
            s.width = cw * g.scaleX;
            s.height = ch * g.scaleY;
            s.angle = g.angle;
            s.flags = SPR_CENTRE_X | SPR_CENTRE_Y | SPR_REGION;
            s.pos = {(g.x + 1) * cw, (g.y + 1) * ch};
            s.col.unpackRGBA(screen.palette[(g.ink >> 8) & 15]);
            s.regionX = 1 + (code % 28) * 18;
            s.regionY = 321 + (code / 28) * 18;
            s.regionW = 16;
            s.regionH = 16;
            if (renderSession().count + 6 > kLocalVerts)
                purge();
            s.setupSimple(renderSession().cursor);
            renderSession().cursor += 6;
            renderSession().count += 6;
        }
        purge();
    }
    // Fixed by the launch entry. Resizing/rotation can never select a game.
    const bool classic_;
    gr::reconstruction::ios::DarwinRandom sessionRandom_{1};
    std::unique_ptr<NativeGrid> native_;
    gr::reconstruction::ios::DrawFrame nativeDraw_, starDraw_;
    gridrunner::GoalMessages messages_;
    gridrunner::AchievementRecord achievementRecord_;
    bool nameEntry_ = false;
    int entryRank_ = -1, entryBoard_ = 0;
    std::string entryName_;
    const int kW, kH;
    gr::reconstruction::ios::Classic classicGame_{&sessionRandom_};
    neon::input::ControllerFrame controllerFrame_;
    Host *host_ = nullptr;
    SurfaceInfo surface_;
    neon::ShaderCycleState &shaderHistory_;
    RenderSession renderState_;
    gridrunner::module::GameHost gameHost_;
    NeonResManager resources_;
    NeonDevice device_;
    NeonDrawContext context_;
    WaveState waves_;
    std::unique_ptr<NeonEffect> effect_;
    gridrunner::OriginalFrontend frontend_;
    gridrunner::CampaignProgress progress_;
    gr::reconstruction::ios::Starfield stars_{sessionRandom_};
    gridrunner::SampleMixer mixer_;
    gridrunner::MultiTouchControl touchControl_;
    HighScores scores_;
    std::vector<UiButton> buttons_;
    gridrunner::Axis pad_;
    std::vector<ButtonArt> buttonArt_;
    int redButton_ = -1;
    double accumulator_ = 0, repeat_ = 0, pauseAlpha_ = 0, pauseMenuAlpha_ = 0;
    float feedbackFlash_ = 0;
    int frame_ = 0, selected_ = 0, touchButton_ = -1, selectedLevel_ = 0, devLevel_ = 0,
        scoreBoard_ = 0, scoreReturnSelection_ = 0, heldDx_ = 0, heldDy_ = 0, demoTicks_ = 0;
    uint32_t touchId_ = 0;
    bool expertMode_ = false, survival_ = false, active_ = false, demo_ = false, starting_ = false,
         startingNext_ = false, paused_ = false, scoresOpen_ = false;
    bool speedModifierHeld_ = false, digital_ = false, padMode_ = false, inputGate_ = true,
         touching_ = false, touchPauseHotspot_ = false, suspended_ = false, recorded_ = false,
         progressDirty_ = false, devStart_ = false, deviceReady_ = false, rendered_ = false;
};
} // namespace
static neon::ShaderCycleState &gridShaderHistory() {
    static neon::ShaderCycleState state;
    return state;
}
extern const CoreEntry kGridrunnerCore = {
    &kInfo, []() -> Core * { return new GridrunnerCore(gridShaderHistory()); }};
extern const CoreEntry kGridrunnerClassicCore = {
    &kClassicInfo, []() -> Core * { return new GridrunnerCore(gridShaderHistory(), true); }};
} // namespace mino
