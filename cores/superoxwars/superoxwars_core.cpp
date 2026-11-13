#include "startup_screen.h"
#include "backend.h"
#include "scene.h"
#include "controls.h"
#include "frontend.h"
#include "original_tables.h"
#include "neon/controller_session.h"
#include "neon/sample_audio.h"
#include <filesystem>
#include <algorithm>
#include <atomic>
namespace mino {
namespace {
const CoreInfo Info = {"superoxwars", "Super Ox Wars", 320, 480, .75f, .75f, "Default.png"};
class SuperOxWarsCore final : public Core {
    friend struct SuperOxWarsMotionChecks;
    mino::StartupScreen startup_;
    using Screen = superoxwars::Frontend::Screen;
    Host *host_ = nullptr;
    SurfaceInfo surface_;
    superoxwars::Backend backend_;
    std::unique_ptr<superoxwars::World> world_;
    std::unique_ptr<superoxwars::Scene> scene_;
    neon::input::ControllerFrame pads_;
    neon::audio::SampleMixer audio_{{32, neon::audio::MixArithmetic::ReverseQ16Saturating,
                                     neon::audio::RetriggerRule::RewindFirst}};
    superoxwars::Frontend front_;
    const superoxwars::AnalogControls controls_;
    std::string recordsPath_;
    std::atomic<bool> suspended_{false};
    bool rendered_ = false;
    double accumulator_ = 0;
    uint32_t directions_ = 0, sessionAchievements_ = 0;
    struct Finger {
        uint32_t id = 0;
        bool held = false;
    };
    std::array<Finger, 4> fingers_{};
    std::vector<superoxwars::ActorDraw> actors_, interface_, foreground_;
    std::vector<superoxwars::TextLine> text_;
    std::array<std::string, 2> goalText_;
    bool clearMessages_ = true;
    uint64_t appliedTextRevision_ = ~uint64_t(0);
    float previousAccelX_ = 0;
    bool motionStarted_ = false;
    bool motionAvailable_ = false, motionWanted_ = true;
    void refreshCapabilities() {
        bool connected = false;
        for (const auto &pad : pads_.slots())
            connected |= pad.connected;
        front_.capabilities(motionAvailable_, connected);
        front_.menuFocus = connected || host_->keyboardNavigationAvailable();
    }
    bool tabletTouch() const {
        // Select the emulated iPad/phone touch coefficients by the upright
        // game format. Low-resolution filters must not change this profile.
        const double aspect = surface_.displayAspect > 0 ? surface_.displayAspect
                                                         : double(surface_.width) / surface_.height;
        return aspect > .7;
    }
    void persist() {
        if (!front_.records.save(recordsPath_))
            host_->log("Super Ox Wars records could not be saved");
    }
    void replaceWorld(bool demo, int level = 0) {
        audio_.stopAll();
        scene_.reset();
        world_ = std::make_unique<superoxwars::World>();
        world_->demo = demo;
        world_->sessionAchievements = sessionAchievements_;
        world_->blueCollected = front_.records.blueCollected;
        world_->redCollected = front_.records.redCollected;
        world_->relativeControls = front_.records.touchMode == 0;
        world_->tabletTouch = tabletTouch();
        scene_ = std::make_unique<superoxwars::Scene>(*world_);
        world_->sound = [this](int sample, int gain) { audio_.play(sample, gain, 0); };
        world_->achievement = [this](int id) {
            front_.goal(id);
            sessionAchievements_ |= 1u << id;
            front_.records.achievements |= 1u << id;
            persist();
        };
        world_->checkpoint = [this](int level) {
            front_.records.checkpoint(*world_, level);
            front_.records.captureTotals(*world_);
            persist();
        };
        front_.attach(*world_, *scene_);
        front_.run(*world_, *scene_);
        clearMessages_ = true;
        appliedTextRevision_ = ~uint64_t(0);
        fingers_ = {};
        accumulator_ = 0;
        rendered_ = false;
    }
    void commands(uint32_t edge) {
        if (!world_)
            return;
        if (edge & (PAD_A | PAD_START))
            front_.records.achievements |= host_->originalAchievementDisplayMask() & 0x3ffu;
        const bool tilt = front_.records.tiltEnabled;
        front_.command(*world_, *scene_, edge);
        if (tilt != front_.records.tiltEnabled)
            persist();
        if (front_.quitRequested) {
            host_->requestQuit();
            front_.quitRequested = false;
        }
    }
    superoxwars::PlayerInput input() {
        superoxwars::PlayerInput out;
        const auto movement = neon::input::movement(pads_[0]);
        const auto &profile = (pads_[0].buttons & PAD_A) ? controls_.focus : controls_.normal;
        out.analogMovement = profile.apply({movement.x, movement.y});
        out.worldInfluence =
            pads_[0].connected ? controls_.worldInfluence(neon::input::worldInfluenceAxis(pads_[0]))
                               : 0;
        return out;
    }
    void overlay(superoxwars::Neon_Internals::cellGrid &grid) {
        // 418c4 -> 37200(28,32,1,3) -> 34918(30,38).
        if (grid.cols != 30 || grid.rowsN != 38)
            grid.setSize(30, 38);
        grid.fontY = 0xda;
        for (auto &scale : grid.passScale)
            scale = {.98, 1};
        for (size_t n = 0; n < world_->palette.size(); ++n) {
            const auto c = world_->palette[n];
            grid.palette[n] = {c.r, c.g, c.b, c.a};
        }
        grid.scrollX = world_->presentation.textScroll.x;
        grid.scrollY = world_->presentation.textScroll.y;
        grid.noProp = front_.screen == Screen::Paused;
        if (clearMessages_) {
            grid.clm();
            clearMessages_ = false;
        }
        for (int id; (id = front_.popGoal()) >= 0;) {
            const int slot = grid.lines[11]->text ? 1 : 0, row = 11 + slot;
            goalText_[size_t(slot)] = superoxwars::original::GoalNames[size_t(id)];
            grid.QuickMsg(goalText_[size_t(slot)].c_str(), {0, slot ? .1 : 0}, {1.2, 2.4}, 4, row,
                          3, 1.f, .5f);
            auto *l = grid.lines[row];
            l->inScale = {4, 4};
            l->away.y = slot ? double(.35f) : double(.4f);
            l->from.y = slot ? double(-.55f) : -.5;
        }
        if (appliedTextRevision_ != front_.textRevision) {
            appliedTextRevision_ = front_.textRevision;
            if (front_.clearGrid)
                grid.cls();
            for (int row : front_.clearRows)
                grid.cll(row);
            grid.rowX[5] = (front_.screen == Screen::Title &&
                            superoxwars::original::AttractOrder[size_t(front_.attractIndex)] == 0)
                               ? front_.playRowOffset
                               : 0;
            for (const auto &l : text_) {
                if (l.centred)
                    grid.cPrintAt(l.y, l.text.c_str(), l.colour);
                else {
                    grid.printColour.unpackRGBA(l.colour);
                    grid.printAt(l.x, l.y, l.text.c_str());
                }
            }
            for (const auto &l : world_->messageLines) {
                grid.cll(l.row);
                if (l.text.empty())
                    continue;
                const auto c = l.colour;
                grid.printColour = {c.r, c.g, c.b, c.a};
                if (l.x < 0)
                    grid.cPrintAt(l.row, l.text.c_str());
                else
                    grid.printAt(l.x, l.row, l.text.c_str());
            }
        }
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
        front_ = superoxwars::Frontend{};
        recordsPath_.clear();
        if (!host.saveDir().empty())
            recordsPath_ =
                (std::filesystem::u8path(host.saveDir()) / "superoxwars-records.txt").u8string();
        if (!front_.records.load(recordsPath_))
            host.log("Super Ox Wars invalid records; using defaults");
        front_.records.achievements |= host.originalAchievementDisplayMask() & 0x3ffu;
        host.observeOriginalAchievements(front_.records.achievements);
        pads_.clear();
        directions_ = sessionAchievements_ = 0;
        suspended_ = false;
        previousAccelX_ = 0;
        motionStarted_ = false;
        motionAvailable_ = false;
        motionWanted_ = true;
        if (!backend_.init(host, surface))
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
        audio_.setSamples(std::move(samples));
        backend_.background = [this](auto &sprite, double width, double height) {
            world_->prepare(actors_);
            backend_.drawActors(sprite, width, height, actors_);
        };
        backend_.foreground = [this](auto &sprite, double width, double height) {
            world_->prepareForeground(foreground_);
            backend_.drawActors(sprite, width, height, foreground_);
            backend_.drawActors(sprite, width, height, interface_);
            world_->finishForeground();
        };
        backend_.text = [this](auto &grid) { overlay(grid); };
        replaceWorld(true);
        front_.enter(Screen::Loading);
        return true;
    }
    void resize(const SurfaceInfo &s) override {
        startup_.resize(s);
        surface_ = s;
        backend_.resize(s);
        if (world_)
            world_->tabletTouch = tabletTouch();
        rendered_ = false;
    }
    void pads(const PadState *states, int count) override {
        if (startup_.pads(states, count))
            return;
        pads_.capture(states, count);
        refreshCapabilities();
        uint32_t edges = 0, d = 0;
        for (const auto &pad : pads_.consume()) {
            edges |= pad.pressed;
            const auto a = neon::input::movement(pad);
            if (a.x < -.5f)
                d |= PAD_LEFT;
            if (a.x > .5f)
                d |= PAD_RIGHT;
            if (a.y < -.5f)
                d |= PAD_UP;
            if (a.y > .5f)
                d |= PAD_DOWN;
        }
        edges |= d & ~directions_;
        directions_ = d;
        if (host_ && !suspended_)
            commands(edges);
    }
    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        if (!world_ || suspended_)
            return;
        const superoxwars::Vec2 p{double(e.x) / surface_.width - .5,
                                  double(e.y) / surface_.height - .5};
        if (e.phase == TouchPhase::Cancel) {
            fingers_ = {};
            world_->pointerHeld = {};
            return;
        }
        if (e.phase == TouchPhase::Down) {
            for (size_t n = 0; n < fingers_.size(); ++n)
                if (!fingers_[n].held) {
                    fingers_[n] = {e.id, true};
                    world_->pointerDown(n, p);
                    break;
                }
        } else
            for (size_t n = 0; n < fingers_.size(); ++n)
                if (fingers_[n].held && fingers_[n].id == e.id) {
                    if (e.phase == TouchPhase::Up) {
                        fingers_[n].held = false;
                        world_->pointerUp(n);
                    } else
                        world_->pointerMove(n, p);
                    break;
                }
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        if (suspended_ || !world_)
            return;
        // Start sensors only after the launch/loading transition has finished.
        // Android can reject an open while its activity changes orientation.
        const bool wantMotion = front_.records.tiltEnabled || !front_.controllerConnected();
        if (!motionStarted_ || wantMotion != motionWanted_) {
            if (wantMotion) {
                motionAvailable_ = host_->enableMotion(true);
                if (!motionAvailable_)
                    host_->log("Super Ox Wars: accelerometer unavailable; retrying on the next "
                               "gameplay entry or resume");
            } else
                host_->enableMotion(false);
            previousAccelX_ = 0;
            motionWanted_ = wantMotion;
            motionStarted_ = true;
            refreshCapabilities();
        }
        if (front_.screen == Screen::Loading)
            front_.enter(Screen::Title);
        const auto motion = wantMotion ? host_->motion() : MotionState{};
        if (motion.available) {
            world_->lateralTilt = double(-float((double(previousAccelX_) + motion.x) * .5));
            previousAccelX_ = float(motion.x);
        } else {
            world_->lateralTilt = 0;
            previousAccelX_ = 0;
        }
        accumulator_ += std::clamp(dt, 0., .1);
        try {
            bool stepped = false;
            while (accumulator_ >= 1. / 60) {
                accumulator_ -= 1. / 60;
                const auto oldScreen = front_.screen;
                const bool oldTilt = front_.records.tiltEnabled;
                world_->tick(input());
                // A sensor open during activity startup may fail temporarily.
                // Retry on entering play, without polling/opening absent hardware
                // every frame. A successful existing subscription is unchanged.
                if (wantMotion && oldScreen != front_.screen && front_.screen == Screen::Playing &&
                    !motion.available)
                    motionStarted_ = false;
                if (oldScreen != front_.screen || oldTilt != front_.records.tiltEnabled)
                    persist();
                if (!world_->coverageStop.empty()) {
                    host_->log("Super Ox Wars gameplay error: %s", world_->coverageStop.c_str());
                    front_.enter(Screen::Error);
                    world_->paused = true;
                }
                front_.prepare(*world_, *scene_, text_, interface_);
                backend_.execute(*world_);
                host_->observeOriginalAchievements(world_->sessionAchievements);
                stepped = true;
            }
            if (!stepped && !rendered_) {
                front_.prepare(*world_, *scene_, text_, interface_);
                backend_.execute(*world_);
            }
            backend_.present();
            rendered_ = true;
        } catch (const std::exception &e) {
            host_->log("Super Ox Wars gameplay error: %s", e.what());
            world_->unsupported(e.what());
            front_.enter(Screen::Error);
        }
    }
    void mixAudio(float *out, int frames) override {
        if (suspended_)
            std::fill(out, out + frames * 2, 0.f);
        else
            audio_.mix(out, frames);
    }
    void pause(bool p) override {
        suspended_ = p;
        accumulator_ = 0;
        fingers_ = {};
        pads_.clear();
        previousAccelX_ = 0;
        motionStarted_ = false;
        if (world_)
            world_->pointerHeld = {};
        if (host_ && p)
            host_->enableMotion(false);
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        if (host_)
            commands(PAD_B);
        return true;
    }
    void save() override {
        if (host_ && world_) {
            front_.records.captureTotals(*world_);
            persist();
        }
    }
    void shutdown() override {
        startup_.release();
        if (host_) {
            save();
            host_->enableMotion(false);
        }
        audio_.stopAll();
        backend_.release();
        scene_.reset();
        world_.reset();
        host_ = nullptr;
    }
};
Core *create() {
    return new SuperOxWarsCore;
}
} // namespace
extern const CoreEntry kSuperOxWarsCore = {&Info, create};
} // namespace mino
