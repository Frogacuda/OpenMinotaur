// Exercise the production shell's input, state transitions and renderer.
#include <array>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "../cores/gridrunner/gridrunner_core.cpp"
#include "display.h"
#include <SDL3/SDL.h>

namespace mino {
namespace {
struct GridrunnerUxChecks {
    static inline int failures = 0;
    static void require(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    static void expect(bool value, const char *message) {
        if (!value) {
            ++failures;
            std::cerr << "FAIL " << message << '\n';
        }
    }
    struct TestHost final : Host {
        std::filesystem::path assets, saves;
        Display *display = nullptr;
        bool quit = false;
        bool readAsset(const std::string &name, std::vector<uint8_t> &bytes) const override {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file)
                return false;
            bytes.assign(std::istreambuf_iterator<char>(file), {});
            return true;
        }
        std::string saveDir() const override {
            return saves.generic_string() + "/";
        }
        void log(const char *, ...) override {}
        std::string adaptShader(const std::string &text, int stage) const override {
            return display->adaptShader(text, stage);
        }
        void lockAudio() override {}
        void unlockAudio() override {}
        void requestQuit() override {
            quit = true;
        }
        bool presentStartupFrame() override {
            display->present();
            return true;
        }
    };
    static void run(const std::filesystem::path &assets, const std::filesystem::path &scratch,
                    bool classic, bool es) {
        Display display;
        require(display.init(es), "Display init");
        TestHost host;
        host.assets = assets;
        host.saves = scratch / (classic ? "classic" : "modern");
        host.display = &display;
        std::filesystem::create_directories(host.saves);
        neon::ShaderCycleState history;
        GridrunnerCore core(history, classic);
        DisplaySettings settings;
        settings.internalScale = 1;
        display.configure(640, 640, core.nativeFormat(), settings);
        display.beginFrame();
        require(core.init(host, display.surface()), "Core init");
        if (!classic)
            SDL_Delay(1020);
        PadState pad{};
        pad.connected = true;
        const auto frame = [&] {
            display.beginFrame();
            core.frame(1. / 60);
            display.present();
            require(glGetError() == GL_NO_ERROR, "Rendering GL error");
        };
        const auto send = [&](uint32_t buttons, float x = 0, float y = 0) {
            const auto prior = pad.buttons;
            pad.buttons = buttons;
            pad.pressed = buttons & ~prior;
            pad.released = prior & ~buttons;
            pad.lx = x;
            pad.ly = y;
            core.pads(&pad, 1);
            frame();
        };
        const auto press = [&](uint32_t buttons) {
            send(0);
            send(buttons);
            send(0);
        };
        const auto touch = [&](TouchPhase phase, unsigned id, double x, double y) {
            core.touch({phase, id, float((x + .5) * core.surface_.width),
                        float((y + .5) * core.surface_.height)});
        };
        const auto tap = [&](double x, double y) {
            touch(TouchPhase::Down, 51, x, y);
            frame();
            touch(TouchPhase::Up, 51, x, y);
            frame();
        };
        const auto mark = [&](const char *label) {
            std::cout << (classic ? "classic" : "modern") << ' ' << label
                      << " active=" << core.active_ << " pause=" << core.paused_
                      << " scores=" << core.scoresOpen_ << " name=" << core.nameEntry_
                      << " focus=" << core.selected_ << " pauseAlpha=" << core.pauseAlpha_ << '\n';
        };
        send(0);
        for (int n = 0; !classic && !core.frontend_.acceptsPlay() && n < 1000; ++n)
            frame();
        require(classic ? core.classicTitle() : core.titleLanding(), "Initial title state");
        mark("title");
        send(PAD_START);
        if (!classic) {
            for (int n = 0; !core.frontend_.levelSelect() && n < 100; ++n)
                send(PAD_START);
            expect(core.frontend_.levelSelect() && !core.active_,
                   "Modern title enters level selection");
            for (int n = 0; n < 12; ++n)
                send(PAD_START);
            expect(!core.active_, "Held Start must not bypass level selection");
            press(PAD_START);
        }
        for (int n = 0; !core.playing() && n < 180; ++n)
            send(0);
        require(core.playing(), "Started gameplay");
        mark("playing");
        expect(core.pauseAlpha_ > .1, "Gameplay pause icon must be revealed at game start");
        if (classic) {
            // Compare the final framebuffer with the gameplay paused glyph
            // hidden/revealed, keeping the simulation and other art fixed.
            auto bind = core.renderState_.bind();
            std::vector<unsigned char> hidden(size_t(core.surface_.width) * core.surface_.height *
                                              4),
                visible(hidden.size());
            const auto read = [&](auto &bytes) {
                glBindFramebuffer(GL_FRAMEBUFFER, core.surface_.framebuffer);
                glReadPixels(0, 0, core.surface_.width, core.surface_.height, GL_RGBA,
                             GL_UNSIGNED_BYTE, bytes.data());
            };
            core.pauseAlpha_ = 0;
            core.present();
            read(hidden);
            core.pauseAlpha_ = 1;
            core.present();
            read(visible);
            int changed = 0;
            for (size_t pixel = 0; pixel < hidden.size(); pixel += 4)
                if (!std::equal(hidden.begin() + pixel, hidden.begin() + pixel + 3,
                                visible.begin() + pixel))
                    ++changed;
            expect(changed > 100, "Classic gameplay Pause must actually reach the framebuffer");
        }
        for (int n = 0; n < 250; ++n)
            frame();
        expect(core.pauseAlpha_ < .1, "Gameplay pause icon fades when untouched");
        tap(0, -.2);
        expect(core.paused_, "Touch pause route");
        for (int n = 0; n < 25; ++n)
            frame();
        mark("paused");
        press(PAD_DOWN);
        expect(core.selected_ == 1, "D-pad selects scores");
        press(PAD_A);
        expect(core.scoresOpen_ && core.paused_, "Scores retain paused game");
        mark("scores");
        const int board = core.scoreBoard_;
        send(0);
        send(0, 1, 0);
        send(0);
        press(PAD_A);
        expect(core.scoreBoard_ == (classic ? 0 : (board + 1) % 3), "Score-board navigation");
        press(PAD_B);
        expect(!core.scoresOpen_ && core.paused_, "Back from scores returns to pause");
        send(0);
        send(PAD_B);
        expect(!core.paused_ && core.playing(), "Back from pause resumes");
        expect(core.pauseAlpha_ > .1, "Resume reveals the fading pause control");
        send(PAD_B);
        for (int n = 0; n < 12; ++n)
            send(PAD_B);
        expect(core.playing() && !host.quit, "A held Back cannot reopen pause or exit");
        send(0);
        press(PAD_START);
        for (int n = 0; n < 25; ++n)
            frame();
        send(0);
        send(0, 0, 1);
        send(0);
        expect(core.selected_ == 1, "Analog pause navigation");
        press(PAD_DOWN);
        press(PAD_DOWN);
        press(PAD_DOWN);
        expect(core.selected_ == 4, "Pause mode focus");
        const bool expert = core.expertMode_;
        press(PAD_A);
        expect(core.expertMode_ != expert && core.paused_, "Mode toggle remains in pause");
        press(PAD_UP);
        press(PAD_UP);
        expect(core.selected_ == 2, "Title focus");
        press(PAD_A);
        expect(classic ? core.classicTitle() : core.titleLanding(), "Pause Title destination");
        mark("returned-title");
        press(PAD_B);
        expect(host.quit, "B on title exits to iOX");
        host.quit = false;

        // Seed a qualifying result through the production score insertion path.
        core.active_ = true;
        core.demo_ = false;
        core.recorded_ = false;
        if (classic)
            core.classicGame_.start();
        else
            core.startNative(false);
        core.scores_.lastName = "ABCDEFGHIJ";
        if (!classic)
            core.native_->score(500);
        // Classic's run score is private to its reconstruction, so use the
        // same inserted-record state to exercise the common name-entry owner.
        if (classic) {
            core.entryBoard_ = 0;
            core.entryRank_ = core.scores_.insert(0, "ABCDEFGHIJ", 500, 1);
            core.entryName_ = core.scores_.lastName;
            core.nameEntry_ = true;
            core.recorded_ = true;
            core.active_ = false;
            core.cancelInput();
            core.selected_ = gridrunner::kNameDone;
            core.rebuildButtons();
        } else {
            auto *ship = core.native_->currentShip();
            require(ship, "Game-over ship");
            ship->state = gr::reconstruction::ios::Ship::State::Departing;
            ship->arrival = 0;
            core.native_->ac.completed = false;
            core.native_->ac.lives = 1;
            frame();
            require(core.nativeEnded(), "Production game-over transition");
        }
        require(core.nameEntry_, "Qualifying score entry");
        mark("name-entry");
        expect(core.selected_ == gridrunner::kNameDone, "A full stored name focuses Done");
        press(PAD_LEFT);
        expect(core.selected_ == gridrunner::kNameDelete, "Left selects Delete from Done");
        press(PAD_A);
        expect(core.entryName_.size() == 9, "Delete removes a name character");
        press(PAD_RIGHT);
        expect(core.selected_ == gridrunner::kNameDone, "Right selects Done");
        press(PAD_A);
        expect(!core.nameEntry_, "Done closes name entry");

        // Enter a new run and exercise touch capture cancellation, lifecycle
        // pause, and the explicit pause-menu Exit destination.
        core.activate(UiAction::Title);
        send(0);
        for (int n = 0; !classic && !core.frontend_.acceptsPlay() && n < 100; ++n)
            frame();
        press(PAD_A);
        if (!classic) {
            for (int n = 0; !core.frontend_.levelSelect() && n < 100; ++n)
                send(0);
            press(PAD_A);
        }
        for (int n = 0; !core.playing() && n < 180; ++n)
            send(0);
        require(core.playing(), "Restart for touch/lifecycle trace");
        touch(TouchPhase::Down, 71, 0, -.2);
        touch(TouchPhase::Cancel, 71, 0, -.2);
        frame();
        expect(!core.paused_, "Cancelled pause touch cannot activate");
        touch(TouchPhase::Down, 72, -.3, .2);
        frame();
        core.pause(true);
        core.pause(false);
        send(0);
        expect(core.paused_ && !core.touchControl_.active(),
               "Lifecycle pause clears gameplay fingers");
        for (int n = 0; n < 25; ++n)
            frame();
        tap(0, .145);
        expect(host.quit, "Touch Exit in pause returns to iOX");
        core.save();
        core.shutdown();
        display.shutdown();
    }
};
} // namespace
} // namespace mino

int main(int argc, char **argv) {
    try {
        std::cout << std::unitbuf;
        using Check = mino::GridrunnerUxChecks;
        Check::require(argc == 3 || argc == 4,
                       "Supply Gridrunner assets and scratch directory, optionally es");
        const bool es = argc == 4 && std::string(argv[3]) == "es";
        Check::require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
        auto *window = SDL_CreateWindow("Gridrunner UX trace", 640, 640,
                                        SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        Check::require(window, "Window init");
        auto context = SDL_GL_CreateContext(window);
        Check::require(context, "GL init");
        Check::require(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)),
                       "GL entry points");
        for (bool classic : {false, true})
            Check::run(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]),
                       classic, es);
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return Check::failures ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
