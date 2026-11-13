#include "startup_screen.h"
#include "backend.h"
#include "world.h"
#include "frontend.h"
#include <chrono>
#include <cmath>
#include "original_tables.h"
#include "neon/controller_session.h"
#include "neon/sample_audio.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace mino {
namespace {
const CoreInfo Info = {"goatup", "GoatUp", 320, 480, .75f, .75f, "goat.png"};
class GoatUpCore final : public Core {
    friend struct GoatUpTouchChecks;
    mino::StartupScreen startup_;
    Host *host_ = nullptr;
    SurfaceInfo surface_;
    goatup::Backend backend_;
    goatup::CollectionRecord records_;
    std::unique_ptr<goatup::World> world_;
    neon::input::ControllerFrame pads_;
    neon::audio::SampleMixer mixer_{{32, neon::audio::MixArithmetic::ReverseQ16Saturating,
                                     neon::audio::RetriggerRule::RewindFirst}};
    std::atomic<bool> suspended_{false};
    struct Finger {
        uint32_t id = 0;
        bool active = false;
        double x = 0, y = 0, origin = 0, previous = 0, delta = 0;
        int moves = 0;
        double startX = 0;
    };
    std::array<Finger, 4> fingers_{};
    double accumulator_ = 0;
    bool rendered_ = false, canSave_ = true;
    int menu_ = 0, best_ = 0;
    float pauseAlpha_ = 0;
    bool revealPause_ = true, pauseTouchInside_ = false, gameOverArmed_ = false;
    bool anyTouch() const {
        return std::any_of(fingers_.begin(), fingers_.end(),
                           [](const Finger &finger) { return finger.active; });
    }
    void tickPauseButton() {
        if (revealPause_) {
            pauseAlpha_ = 1;
            revealPause_ = false;
        }
        if (world_->game.gameOver)
            return;
        if (world_->paused) {
            constexpr float radius = float(.065f * 1.2);
            for (int i = 3; i >= 0; --i) {
                const bool inside =
                    std::any_of(fingers_.begin(), fingers_.end(), [&, i](const Finger &finger) {
                        return finger.active && std::abs(finger.x) < double(radius) * 2.4 &&
                               std::abs(finger.y - PauseY[size_t(i)]) < radius;
                    });
                const bool entered = inside && !pauseMenuInside_[size_t(i)];
                pauseMenuInside_[size_t(i)] = inside;
                if (entered) {
                    menu_ = i;
                    activate();
                    return;
                }
            }
            return;
        }
        static constexpr float radius = float(.08f * 1.2);
        const bool inside = std::any_of(fingers_.begin(), fingers_.end(), [](const Finger &finger) {
            return finger.active && std::abs(finger.x) < double(radius) * 2.4 &&
                   std::abs(finger.y + .33) < radius;
        });
        const auto &first = fingers_.front();
        pauseAlpha_ = first.active && first.y < double(-.2f)
                          ? std::min(1.f, float(double(pauseAlpha_) + .1))
                          : float(double(pauseAlpha_) * .98);
        if (inside && !pauseTouchInside_)
            openPause();
        pauseTouchInside_ = inside;
    }
    std::array<bool, 4> pauseMenuInside_{};
    static constexpr std::array<double, 4> PauseY = {-.20, -.045, .11, .265};
    goatup::Frontend front_;
    std::array<float, 4> buttonPhase_{}, buttonRate_{};
    void openPause() {
        world_->paused = true;
        menu_ = 0;
        buttonPhase_.fill(0);
        pauseMenuInside_.fill(false);
        // Preserve the original three pause-animation random draws. The added
        // Title derives its own rate/phase without advancing gameplay RNG.
        for (int i = 0; i < 3; ++i)
            buttonRate_[size_t(i)] = float(world_->random.range(.05, .2));
        buttonRate_[3] =
            .05f +
            std::fmod(buttonRate_[0] + buttonRate_[1] * .618f + buttonRate_[2] * .414f, .15f);
        buttonPhase_[3] = 1.57079632679f;
    }
    void title() {
        front_.best = best_;
        front_.title();
        front_.paletteTicks = 0;
        world_->paused = false;
        fingers_ = {};
        world_->effects.reset();
        world_->denizens.reset();
        world_->kids.reset();
        world_->feedback = {};
        world_->feedback.scaleX = world_->feedback.scaleY = .99f;
        world_->effects.initializeTitleStars(records_.bonuses, records_.uniqueBonuses,
                                             world_->actors.spriteSize);
    }
    void returnFromGameOver() {
        world_->effects.reset();
        world_->denizens.reset();
        world_->kids.reset();
        world_->feedback = {};
        world_->feedback.scaleX = world_->feedback.scaleY = .99f;
        front_.screen = goatup::Frontend::Screen::Resetting;
        front_.bootTicks = 0;
        front_.paletteTicks = 0;
        world_->paused = false;
        fingers_ = {};
    }
    void frontendAction(goatup::Frontend::Action action) {
        if (action == goatup::Frontend::Action::Exit) {
            save();
            host_->requestQuit();
        } else if (action == goatup::Frontend::Action::Start) {
            restart();
            front_.screen = goatup::Frontend::Screen::Playing;
            mixer_.play(5, 8 << 12, 0);
        } else if (action == goatup::Frontend::Action::Changed) {
            host_->enableMotion(front_.control == 2 && front_.tiltAvailable);
            constexpr int noises[] = {0, 1, 2, 3, 4, 4};
            const auto a = world_->random.next(), b = world_->random.next();
            mixer_.play(noises[(a & 3) + (b & 1)], 8 << 12, 0);
        }
    }
    uint32_t menuDirections_ = 0;
    double previousAccel_ = 0, smoothedAccel_ = 0;
    bool motionValid_ = false;
    bool failed_ = false;
    std::vector<goatup::ActorDraw> backgroundCache_, foregroundCache_;
    void restart(bool gameplay = true) {
        mixer_.stopAll();
        fingers_ = {};
        // Original rand is process-global: newLevel/reset do not reseed it.
        // A fresh process starts at 1; title activity and earlier games in this
        // module continue to affect the next run's platform/actor stream.
        const uint32_t stream = world_ ? world_->random.state() : 1u;
        world_ = std::make_unique<goatup::World>(records_, stream, gameplay);
        auto &w = *world_;
        w.game.stopSound = [this](int n) { mixer_.stop(n); };
        w.actors.sound = [this](int n, int shift) { mixer_.play(n, 8 << shift, 0); };
        w.actors.spriteSize = [this](int frame) {
            const auto *entry = backend_.resources().sprite(frame);
            if (!entry)
                throw std::runtime_error("Missing original GoatUp sprite");
            return goatup::Vec2{double(entry->w), double(entry->h)};
        };
        rendered_ = false;
        accumulator_ = 0;
        menu_ = 0;
        failed_ = false;
        pauseAlpha_ = 0;
        revealPause_ = true;
        pauseTouchInside_ = false;
        gameOverArmed_ = false;
        backgroundCache_.clear();
        foregroundCache_.clear();
        front_.screen = goatup::Frontend::Screen::Playing;
        front_.paletteTicks = 0;
    }
    void overlay(goatup::Neon_Internals::cellGrid &grid) {
        if (!world_)
            return;
        front_.best = best_;
        if (front_.screen != goatup::Frontend::Screen::Playing) {
            front_.drawText(grid, *world_);
            return;
        }
        const auto &w = *world_;
        grid.setSize(28, 36);
        grid.cls();
        grid.clattr();
        for (const auto &row : w.hudRows) {
            grid.printColour = row.colour;
            if (row.x < 0)
                grid.cPrintAt(row.row, row.text.c_str());
            else
                grid.printAt(row.x, row.row, row.text.c_str());
        }
    }
    void activate() {
        if (world_->paused) {
            if (menu_ == 0)
                world_->paused = false;
            else if (menu_ == 1) {
                front_.screen = goatup::Frontend::Screen::Info;
                front_.page = 2;
                front_.selection = 0;
                front_.infoFromPause = true;
            } else if (menu_ == 2)
                title();
            else {
                save();
                host_->requestQuit();
            }
        }
    }
    void commands(uint32_t pressed) {
        if (!world_ || failed_ || !pressed)
            return;
        if (front_.screen != goatup::Frontend::Screen::Playing) {
            frontendAction(front_.command(pressed, world_->paused));
            return;
        }
        if (world_->game.gameOver) {
            if (world_->game.ticks > 120 && gameOverArmed_ &&
                (pressed & (PAD_A | PAD_START | PAD_B | PAD_BACK)))
                returnFromGameOver();
            if (pressed & (PAD_A | PAD_START | PAD_B | PAD_BACK))
                gameOverArmed_ = false;
            return;
        }
        if (pressed & (PAD_START | PAD_B | PAD_BACK)) {
            if (world_->paused)
                world_->paused = false;
            else
                openPause();
            return;
        }
        if (!world_->paused)
            return;
        constexpr int last = 3;
        if (pressed & PAD_UP)
            menu_ = menu_ == 0 ? last : menu_ - 1;
        if (pressed & PAD_DOWN)
            menu_ = menu_ == last ? 0 : menu_ + 1;
        if (pressed & PAD_A)
            activate();
    }
    goatup::ClimbInput input() {
        goatup::ClimbInput result;
        for (const auto &p : pads_.slots())
            if (p.connected) {
                const auto move = neon::input::movement(p);
                result.jump |= bool(p.buttons & PAD_A);
                if (move.x != 0 && result.method == goatup::ClimbInput::Method::None) {
                    result.method = goatup::ClimbInput::Method::DirectAxis;
                    result.horizontal = move.x;
                }
            }
        // Jump regions use the immutable touch-down X, independently of the
        // drag origin that steering reversals and wall bounces can rebase.
        for (const auto &finger : fingers_)
            if (finger.active) {
                if (jumpFinger(finger))
                    result.jump = true;
                else if (result.method == goatup::ClimbInput::Method::None) {
                    result.method = goatup::ClimbInput::Method::RelativeTouch;
                    result.horizontal = finger.origin - finger.x;
                    result.dragDelta = finger.delta;
                    result.dragHasHistory = finger.moves > 1;
                }
            }
        if (front_.control == 2 && motionValid_ &&
            result.method == goatup::ClimbInput::Method::None) {
            result.method = goatup::ClimbInput::Method::Accelerometer;
            result.horizontal = smoothedAccel_;
        }
        return result;
    }
    bool jumpFinger(const Finger &finger) const {
        return front_.control == 2 ||
               (front_.control == 1 ? finger.startX <= -.2 : finger.startX >= .2);
    }

  public:
    const CoreInfo &info() const override {
        return Info;
    }
    bool init(Host &host, const SurfaceInfo &surface) override {
        if (!startup_.begin(host, surface))
            return false;
        host_ = &host;
        surface_ = surface;
        records_ = {};
        best_ = 0;
        front_ = {};
        canSave_ = true;
        menuDirections_ = 0;
        pads_ = {};
        if (!backend_.init(host, surface, false))
            return false;
        std::vector<neon::audio::Sample> samples;
        std::vector<uint8_t> bytes;
        for (const auto &entry : backend_.resources().samples()) {
            neon::audio::Sample sample;
            sample.maxInstances = entry.maxInstances;
            sample.priority = entry.priority;
            if (!host.readAsset(entry.file, bytes) || !neon::audio::parseWav(bytes, sample))
                return false;
            samples.push_back(std::move(sample));
        }
        mixer_.setSamples(std::move(samples));
        front_.control = 0;
        front_.tiltAvailable = host.enableMotion(true);
        previousAccel_ = smoothedAccel_ = 0;
        motionValid_ = false;
        const auto path = std::filesystem::u8path(host.saveDir()) / "goatup-records.txt";
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            std::string tag;
            int version = 0;
            goatup::CollectionRecord loaded;
            int best = 0;
            file >> tag >> version >> best >> loaded.uniqueBonuses >> loaded.billyGoats >>
                loaded.mostKids;
            for (auto &word : loaded.bonuses)
                file >> word;
            canSave_ = bool(file) && tag == "goatup-records" && version == 1 && best >= 0 &&
                       loaded.uniqueBonuses >= 0 && loaded.uniqueBonuses <= 512 &&
                       loaded.billyGoats >= 0 && loaded.mostKids >= 0;
            int last = 0, height = 0, control = 0;
            if (canSave_) {
                file >> std::ws;
                // The appended fields are optional only as a complete group:
                // Older save records end after the bonus mask; absent fields keep their defaults.
                if (!file.eof()) {
                    canSave_ = bool(file >> last >> height >> control) && last >= 0 &&
                               height >= 0 && control >= 0 && control <= 2;
                    if (canSave_) {
                        file >> std::ws;
                        canSave_ = file.eof();
                    }
                }
            }
            if (canSave_) {
                records_ = loaded;
                best_ = best;
                front_.lastScore = last;
                front_.highest = height;
                records_.highestPlatform = height;
                front_.preferredControl = control;
            } else
                host.log("GoatUp records unreadable; preserving file");
        }
        front_.capabilities(host.touchControlsAvailable(), front_.tiltAvailable);
        host.enableMotion(front_.control == 2 && front_.tiltAvailable);
        backend_.background = [this](auto &target, double w, double h) {
            backgroundCache_.clear();
            if (front_.screen == goatup::Frontend::Screen::Playing)
                world_->prepareBackground(backgroundCache_);
            else
                world_->effects.drawStars(backgroundCache_);
            backend_.drawActors(target, w, h, backgroundCache_);
            if (front_.screen == goatup::Frontend::Screen::Playing)
                backend_.drawSurface(target, w, h, world_->tower);
        };
        backend_.foreground = [this](auto &target, double w, double h) {
            foregroundCache_.clear();
            if (front_.screen == goatup::Frontend::Screen::Playing) {
                world_->prepareForeground(foregroundCache_);
                const auto button = [&](int frame, double y, int item) {
                    goatup::ActorDraw d;
                    d.frame = frame;
                    d.radius = .065f;
                    d.position = {0, y};
                    d.scale = {2.4, 1};
                    d.colour.white(1);
                    if (world_->paused && item >= 0 && item < 4) {
                        if (front_.controllerFocus && menu_ == item) {
                            d.frame = 609 + item;
                            d.colour.white(.75 +
                                           .25 * std::sin(front_.ticks * 6.283185307179586 / 90.));
                        }
                        constexpr int phaseForButton[] = {0, 2, 3, 1};
                        d.angle = float(std::sin(buttonPhase_[size_t(phaseForButton[item])]) * .1);
                    } else {
                        d.radius = .08f;
                        d.colour.white(pauseAlpha_);
                    }
                    foregroundCache_.push_back(d);
                };
                if (world_->paused) {
                    const int frames[] = {207, 607, 608, 208};
                    for (int i = 0; i < 4; ++i)
                        button(frames[i], PauseY[size_t(i)], i);
                } else if (!world_->game.gameOver)
                    button(206, -.33, -1);
            } else
                front_.drawArt(foregroundCache_);
            backend_.drawActors(target, w, h, foregroundCache_);
        };
        backend_.text = [this](auto &grid) { overlay(grid); };
        backend_.feedbackState.scaleX = backend_.feedbackState.scaleY = .99f;
        restart(false);
        front_.screen = goatup::Frontend::Screen::Loading;
        host.log("GoatUp: original title/info presentation and full course, with native controller "
                 "menus.");
        return true;
    }
    void pads(const PadState *states, int count) override {
        if (startup_.pads(states, count))
            return;
        pads_.capture(states, count);
        front_.controllerFocus = pads_.connected() || host_->keyboardNavigationAvailable();
        if (suspended_)
            return;
        uint32_t edges = 0, directions = 0;
        for (const auto &p : pads_.consume()) {
            edges |= p.pressed;
            const auto axis = neon::input::movement(p);
            if (axis.y < -.5f)
                directions |= PAD_UP;
            if (axis.y > .5f)
                directions |= PAD_DOWN;
            if (axis.x < -.5f)
                directions |= PAD_LEFT;
            if (axis.x > .5f)
                directions |= PAD_RIGHT;
        }
        // Same canonical axis/D-pad path as the other modules. Keep the latch
        // across screen changes so opening a menu does not invent another edge.
        edges |= directions & ~menuDirections_;
        menuDirections_ = directions;
        commands(edges);
    }
    void touch(const TouchEvent &event) override {
        if (startup_.touch(event))
            return;
        if (suspended_ || !world_ || failed_)
            return;
        const double x = double(event.x) / surface_.width - .5,
                     y = double(event.y) / surface_.height - .5;
        if (event.phase == TouchPhase::Cancel) {
            fingers_ = {};
            return;
        }
        if (front_.screen != goatup::Frontend::Screen::Playing) {
            if (event.phase == TouchPhase::Up)
                for (auto &finger : fingers_)
                    if (finger.active && finger.id == event.id)
                        finger.active = false;
            if (event.phase == TouchPhase::Down)
                frontendAction(front_.touch(x, y, world_->paused));
            return;
        }
        const bool freshTouch = !anyTouch() && event.phase == TouchPhase::Down;
        if (event.phase == TouchPhase::Down) {
            for (auto &finger : fingers_)
                if (!finger.active) {
                    finger = {event.id, true, x, y, x, x, 0, 0, x};
                    break;
                }
        } else
            for (auto &finger : fingers_)
                if (finger.active && finger.id == event.id) {
                    if (event.phase == TouchPhase::Up)
                        finger.active = false;
                    else {
                        finger.delta = finger.previous - x;
                        finger.previous = x;
                        finger.x = x;
                        finger.y = y;
                        ++finger.moves;
                    }
                    break;
                }
        if (world_->game.gameOver && freshTouch) {
            if (world_->game.ticks > 120 && gameOverArmed_)
                returnFromGameOver();
            gameOverArmed_ = false;
        }
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        if (suspended_ || !world_ || failed_)
            return;
        if (front_.screen == goatup::Frontend::Screen::Loading) {
            front_.screen = goatup::Frontend::Screen::Boot;
            front_.bootTicks = 0;
            accumulator_ = 0;
        }
        const int previousControl = front_.control;
        front_.capabilities(host_->touchControlsAvailable(), front_.tiltAvailable);
        if (previousControl != front_.control)
            host_->enableMotion(front_.control == 2 && front_.tiltAvailable);
        accumulator_ += std::clamp(dt, 0., .1);
        try {
            while (accumulator_ >= 1. / 60) {
                accumulator_ -= 1. / 60;
                // EAGLView::drawView averages this and the previous raw sample;
                // theGoat then halves it, applies its dead band, and uses .19.
                const auto motion = host_->motion();
                motionValid_ = motion.available;
                smoothedAccel_ =
                    motion.available
                        ? goatup::ClimbInput::tiltAxisFromGravity((motion.x + previousAccel_) * .5)
                        : 0.;
                previousAccel_ = motion.available ? motion.x : 0.;
                ++front_.ticks;
                if (front_.screen == goatup::Frontend::Screen::Resetting) {
                    front_.screen = goatup::Frontend::Screen::Boot;
                    front_.bootTicks = 0;
                    front_.paletteTicks = 0;
                } else if (front_.screen == goatup::Frontend::Screen::Boot &&
                           ++front_.bootTicks >= 20) {
                    front_.title();
                    world_->effects.initializeTitleStars(records_.bonuses, records_.uniqueBonuses,
                                                         world_->actors.spriteSize);
                }
                if (front_.screen == goatup::Frontend::Screen::Playing) {
                    world_->tick(input());
                    front_.paletteTicks = world_->game.ticks;
                    front_.highest = std::max(front_.highest, world_->game.highestPlatform);
                    if (world_->game.gameOver) {
                        if (world_->game.ticks == 1) {
                            front_.lastScore = world_->actors.score;
                            best_ = std::max(best_, front_.lastScore);
                            save();
                        }
                        constexpr auto confirm = PAD_A | PAD_START | PAD_B | PAD_BACK;
                        const auto &slots = pads_.slots();
                        if (!anyTouch() &&
                            std::none_of(slots.begin(), slots.end(), [&](const PadState &p) {
                                return p.connected && (p.buttons & confirm);
                            }))
                            gameOverArmed_ = true;
                        if (world_->game.ticks > 4000)
                            returnFromGameOver();
                    } else
                        tickPauseButton();
                } else if (!world_->paused) {
                    // Original owner +290 is the palette clock and stops in
                    // pause. The host focus pulse has its own always-run clock.
                    ++front_.paletteTicks;
                    world_->effects.tickStars();
                }
                // Stats opened from pause leaves the gameplay star state frozen.
                if (world_->paused)
                    for (size_t n = 0; n < buttonPhase_.size(); ++n)
                        buttonPhase_[n] = float(buttonPhase_[n] + buttonRate_[n]);
                if (world_->goat.touchRebaseRequested)
                    for (auto &finger : fingers_)
                        if (finger.active && !jumpFinger(finger)) {
                            finger.origin = finger.x;
                            break;
                        }

                host_->observeOriginalAchievements(world_->actors.achievements);
                backend_.feedbackState = world_->feedback;
                backend_.execute(world_->random);
                rendered_ = true;
            }
            if (!rendered_) {
                backend_.feedbackState = world_->feedback;
                backend_.execute(world_->random);
                rendered_ = true;
            }
        } catch (const std::exception &e) {
            failed_ = true;
            host_->log("GoatUp runtime error: %s", e.what());
            host_->requestQuit();
            return;
        }
        backend_.present();
    }
    void resize(const SurfaceInfo &surface) override {
        startup_.resize(surface);
        surface_ = surface;
        backend_.resize(surface);
        fingers_ = {};
    }
    void mixAudio(float *out, int frames) override {
        if (suspended_)
            std::fill(out, out + frames * 2, 0.f);
        else
            mixer_.mix(out, frames);
    }
    void pause(bool paused) override {
        suspended_ = paused;
        fingers_ = {};
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        commands(PAD_B);
        return true;
    }
    void save() override {
        if (!host_ || !canSave_)
            return;
        const auto path = std::filesystem::u8path(host_->saveDir()) / "goatup-records.txt";
        auto temp = path;
        temp += ".tmp";
        {
            std::ofstream file(temp);
            file << "goatup-records 1\n"
                 << best_ << ' ' << records_.uniqueBonuses << ' ' << records_.billyGoats << ' '
                 << records_.mostKids << '\n';
            for (auto word : records_.bonuses)
                file << word << ' ';
            file << '\n'
                 << front_.lastScore << ' ' << front_.highest << ' ' << front_.preferredControl
                 << '\n';
            file.flush();
            if (!file) {
                host_->log("GoatUp records write failed");
                return;
            }
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            host_->log("GoatUp records replacement failed");
#else
        std::error_code error;
        std::filesystem::rename(temp, path, error);
        if (error)
            host_->log("GoatUp records replacement failed: %s", error.message().c_str());
#endif
    }
    void shutdown() override {
        startup_.release();
        save();
        if (host_)
            host_->enableMotion(false);
        mixer_.stopAll();
        backend_.release();
        world_.reset();
        host_ = nullptr;
    }
};
Core *create() {
    return new GoatUpCore;
}
} // namespace
extern const CoreEntry kGoatUpCore = {&Info, create};
} // namespace mino
