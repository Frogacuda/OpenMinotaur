// Thin host binding. Original game/frontend mechanisms live in reconstruction/deflex.
#include "startup_screen.h"
#include "backend.h"
#include "frontend.h"
#include "neon/sample_audio.h"
#include "neon/controller_session.h"
#include <fstream>
#include <filesystem>
#include <atomic>
namespace mino {
namespace {
// Original Info.plist supports LandscapeLeft/Right, native iPhone 480x320.
const CoreInfo Info = {"deflex", "Deflex", 480, 320, 1.5f, 1.5f, "deflex.png", "deflex", "deflex"};
class DeflexCore final : public Core {
    mino::StartupScreen startup_;
    Host *host_ = nullptr;
    SurfaceInfo surface_;
    deflex::Backend backend_;
    deflex::Progress progress_;
    std::vector<neon::vec2d> sizes_;
    std::unique_ptr<deflex::Frontend> frontend_;
    neon::input::ControllerFrame pads_;
    neon::audio::SampleMixer mixer_{{32, neon::audio::MixArithmetic::ReverseQ16Saturating,
                                     neon::audio::RetriggerRule::RewindFirst}};
    double accumulator_ = 0;
    std::atomic<bool> suspended_{false};
    bool rendered_ = false;
    void requestedExit() {
        if (frontend_ && frontend_->quit)
            host_->requestQuit();
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
        if (!backend_.init(host, surface, false))
            return false;
        for (int i = 0; i < backend_.resources().spriteCount(); ++i) {
            auto *s = backend_.resources().sprite(i);
            sizes_.push_back({double(s->w), double(s->h)});
        }
        std::vector<neon::audio::Sample> samples;
        std::vector<uint8_t> bytes;
        for (const auto &entry : backend_.resources().samples()) {
            neon::audio::Sample s;
            s.maxInstances = entry.maxInstances;
            s.priority = entry.priority;
            if (!host.readAsset(entry.file, bytes) || !neon::audio::parseWav(bytes, s))
                return false;
            samples.push_back(std::move(s));
        }
        mixer_.setSamples(std::move(samples));
        std::ifstream records(std::filesystem::u8path(host.saveDir()) / "deflex-records.txt");
        for (int i = 0; i < 52 && records; ++i)
            records >> progress_.bestMoves[size_t(i)] >> progress_.bestScores[size_t(i)];
        for (auto &a : progress_.achievements)
            if (records) {
                int n;
                records >> n;
                if (records)
                    a = n != 0;
            }
        // Optional trailing fields preserve compatibility with shorter save records.
        for (auto &score : progress_.lastScores)
            if (records)
                records >> score;
        if (records)
            records >> progress_.volume;
        reportAchievements();
        frontend_ = std::make_unique<deflex::Frontend>(progress_, sizes_);
        backend_.sceneFeedback = [this] { return frontend_->feedback(); };
        backend_.gridScene = [this](auto &q, auto &r, auto view, bool p) {
            if (frontend_)
                frontend_->drawGrid(q, r, view, p);
        };
        backend_.actorsScene = [this](auto &r, auto view, bool p) {
            if (frontend_)
                frontend_->drawActors(r, view, p);
        };
        backend_.overlay = [this](auto &q, auto &r, auto view) {
            if (frontend_)
                frontend_->drawUI(q, r, view);
        };
        host.log("Deflex: landscape title, 52-stage graphical selector, pause/results/local "
                 "records, and interactive PET/VIC-20/C64 history.");
        return true;
    }
    void resize(const SurfaceInfo &s) override {
        startup_.resize(s);
        surface_ = s;
        backend_.resize(s);
        if (frontend_)
            frontend_->cancel();
    }
    void pads(const PadState *states, int count) override {
        if (startup_.pads(states, count))
            return;
        pads_.capture(states, count);
        if (suspended_ || !frontend_)
            return;
        uint32_t held = 0, pressed = 0;
        for (const auto &p : pads_.slots())
            if (p.connected) {
                held |= p.buttons;
                pressed |= p.pressed;
            }
        frontend_->keyboardNavigation = host_->keyboardNavigationAvailable();
        frontend_->pads(held, pressed, states, count);
        requestedExit();
    }
    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        if (suspended_ || !frontend_)
            return;
        const neon::vec2d p{double(e.x) / surface_.width - .5, double(e.y) / surface_.height - .5};
        int phase = e.phase == TouchPhase::Down ? 0 : e.phase == TouchPhase::Move ? 1 : 2;
        if (e.phase == TouchPhase::Cancel) {
            frontend_->cancel();
            return;
        }
        frontend_->touch(e.id, phase, p);
        requestedExit();
    }
    void reportAchievements() {
        // Preserve all 16 legacy slots; only 0..9 currently have identified callers.
        uint32_t bits = 0;
        for (unsigned i = 0; i < progress_.achievements.size(); ++i)
            if (progress_.achievements[i])
                bits |= uint32_t(1) << i;
        host_->observeOriginalAchievements(bits);
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        if (suspended_ || !frontend_)
            return;
        accumulator_ += std::clamp(dt, 0., .1);
        while (accumulator_ >= 1. / 60) {
            accumulator_ -= 1. / 60;
            frontend_->tick();
            reportAchievements();
            for (const auto &sound : frontend_->sounds)
                mixer_.play(sound.sample, sound.volume << 12, 0);
            frontend_->sounds.clear();
            backend_.execute(&frontend_->world(), frontend_->paused());
            rendered_ = true;
        }
        if (!rendered_) {
            backend_.execute(&frontend_->world(), frontend_->paused());
            rendered_ = true;
        }
        backend_.present();
        requestedExit();
    }
    void mixAudio(float *out, int frames) override {
        if (suspended_)
            std::fill(out, out + frames * 2, 0.f);
        else
            mixer_.mix(out, frames);
    }
    void pause(bool suspended) override {
        suspended_ = suspended;
        if (frontend_)
            frontend_->cancel();
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        if (frontend_) {
            frontend_->back();
            requestedExit();
        }
        return true;
    }
    void save() override {
        if (!host_ || !frontend_)
            return;
        std::ofstream out(std::filesystem::u8path(host_->saveDir()) / "deflex-records.txt");
        for (int i = 0; i < 52; ++i)
            out << progress_.bestMoves[size_t(i)] << ' ' << progress_.bestScores[size_t(i)] << '\n';
        for (bool a : progress_.achievements)
            out << int(a) << ' ';
        out << '\n';
        for (int score : progress_.lastScores)
            out << score << ' ';
        out << '\n' << progress_.volume << '\n';
    }
    void shutdown() override {
        startup_.release();
        if (host_)
            save();
        backend_.gridScene = {};
        backend_.sceneFeedback = {};
        backend_.actorsScene = {};
        backend_.overlay = {};
        backend_.release();
        frontend_.reset();
        mixer_.stopAll();
        host_ = nullptr;
    }
};
Core *create() {
    return new DeflexCore;
}
} // namespace
extern const CoreEntry kDeflexCore = {&Info, create};
} // namespace mino
