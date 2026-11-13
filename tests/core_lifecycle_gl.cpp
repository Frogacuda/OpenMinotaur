#include <cstdlib>
// Optional real-GL integration check. Reads imported assets, writes only to the
// explicitly supplied scratch directory, and opens a hidden SDL window.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "../third_party/stb/stb_image_write.h"
#include "core_api.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include "gl_object_tracker.h"
#include <array>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace {
struct Host final : mino::Host {
    std::filesystem::path assets, saves;
    mino::Display *display = nullptr;
    enum Failure {
        None,
        MissingResources,
        MalformedResources,
        MissingGrid,
        CorruptGrid,
        EmptyCell,
        UnsupportedModule,
        ThrowGrid
    } failure = None;
    bool requestedQuit = false, keyboard = false;
    bool keyboardNavigationAvailable() const override {
        return keyboard;
    }
    bool touchControlsAvailable() const override {
        return std::getenv("MINO_CAPTURE_MINOS_CONTROLS") != nullptr;
    }
    int motionStarts = 0, motionStops = 0;
    mutable int motionReads = 0;
    bool motionEnabled = false;
    bool enableMotion(bool on) override {
        on ? ++motionStarts : ++motionStops;
        motionEnabled = on;
        return on;
    }
    mino::MotionState motion() const override {
        ++motionReads;
        return {motionEnabled, .4, 0, -.8};
    }
    mutable int gridReads = 0;
    std::string capturePrefix = "startup";
    int startupFrames = 0;
    Uint64 startupAt = 0;
    bool slowLoad = false, fallbackStartup = false;
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        if (name == "testGrid.ox")
            ++gridReads;
        if (name == "NeonResources.xml" && failure == MissingResources)
            return false;
        if (name == "NeonResources.xml" && failure == MalformedResources) {
            out = {'<', '!'};
            return true;
        }
        if (name == "testGrid.ox" && failure == CorruptGrid) {
            out = {1, 2, 3};
            return true;
        }
        if (name == "testGrid.ox" && failure == ThrowGrid)
            throw std::runtime_error("injected grid read exception");
        if (name == "testGrid.ox" && failure == EmptyCell) {
            // Valid container with no cell DNA: the parser still produces its
            // 256 cell slots; setup must reject cell zero, not construct it.
            out.assign(12 + 4 + 1024 * 4, 0);
            out[0] = 1;
            out[5] = 4;
            return true;
        }
        if (slowLoad && name == "NeonResources.xml")
            SDL_Delay(1100);
        if (name == "DEFAULT_0" && failure != None) {
            if (failure == MissingGrid)
                return false;
            out = {1, 2, 3};
            return true;
        }
        if (name == "testGrid.ox" && failure == MissingGrid)
            return false;
        if (name == "testGrid.ox" && failure == UnsupportedModule) {
            // One valid presenter stack followed by a recognized unavailable module.
            // This reaches partial construction after the module has prepared its game.
            const std::vector<int> structure = {1, 3, 1, 500, -1, 3, 1, 200, -1};
            std::vector<int> params(1024 + 128);
            params[0] = 0;
            params[256] = 8;
            params[512] = 128;
            params[768] = 128;
            out.clear();
            auto word = [&](uint32_t x) {
                for (int k = 0; k < 4; ++k)
                    out.push_back(uint8_t(x >> (k * 8)));
            };
            word(uint32_t(structure.size()));
            word(uint32_t(params.size()));
            word(128);
            for (int x : structure)
                word(uint32_t(x));
            for (int x : params)
                word(uint32_t(x));
            for (int i = 0; i < 128; ++i)
                word(0);
            return true;
        }
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    std::string saveDir() const override {
        return saves.generic_string() + "/";
    }
    void log(const char *fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
        std::fputc('\n', stderr);
    }
    std::string adaptShader(const std::string &s, int stage) const override {
        return display->adaptShader(s, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {
        requestedQuit = true;
    }
    bool presentStartupFrame() override {
        if (fallbackStartup)
            return false;
        const auto &s = display->surface();
        std::vector<uint8_t> pixels(size_t(s.width) * s.height * 4);
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
        glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindFramebuffer(GL_FRAMEBUFFER, unsigned(previous));
        size_t lit = 0;
        for (size_t i = 0; i < pixels.size(); i += 4)
            lit += pixels[i] > 32 || pixels[i + 1] > 32 || pixels[i + 2] > 32;
        REQUIRE(lit > 100); // catches an unbound/blank startup texture
        stbi_flip_vertically_on_write(1);
        const auto path = saves / (capturePrefix + "-" + std::to_string(startupFrames++) + ".png");
        REQUIRE(stbi_write_png(path.string().c_str(), s.width, s.height, 4, pixels.data(),
                               s.width * 4));
        display->present();
        startupAt = SDL_GetTicks();
        return true;
    }
};
void checkGl() {
    const auto error = glGetError();
    if (error)
        throw std::runtime_error("GL error " + std::to_string(error));
}
} // namespace
int main(int argc, char **argv) {
    try {
        REQUIRE(argc == 3 || argc == 4);
        const auto assets = std::filesystem::absolute(argv[1]);
        const auto scratch = std::filesystem::absolute(argv[2]);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        auto *window = SDL_CreateWindow("Neon lifecycle checks", 640, 480,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        REQUIRE(window);
        auto context = SDL_GL_CreateContext(window);
        REQUIRE(context);
        REQUIRE(SDL_GL_MakeCurrent(window, context));
        REQUIRE(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)));
        gpu_check::install();
        mino::Display display;
        REQUIRE(display.init(false));
        mino::DisplaySettings settings;
        settings.internalScale = 1;
        Host host;
        host.display = &display;
        std::vector<std::string> ids = {"minotron", "minorescue", "gridrunner",
                                        "gridrunner-classic"};
        if (mino::findCore("minos"))
            ids.push_back("minos");
        if (mino::findCore("deflex"))
            ids.push_back("deflex");
        for (const char *id : {"goatup", "goatup2", "fiveaday", "superoxwars"})
            if (mino::findCore(id))
                ids.push_back(id);
        if (argc == 4)
            ids = {argv[3]};
        for (int pass = 0; pass < 2; ++pass)
            for (const auto &id : ids) {
                const auto *entry = mino::findCore(id);
                REQUIRE(entry);
                host.assets = assets / (entry->info->assetGameId ? entry->info->assetGameId : id);
                host.saves = scratch / id;
                std::filesystem::create_directories(host.saves);
                // Reuse the SAME core through early/late failures and repeated release.
                // Cross-launch history remains its existing owner's responsibility.
                std::unique_ptr<mino::Core> core(entry->create());
                for (auto failure : {Host::MissingResources, Host::MalformedResources,
                                     Host::MissingGrid, Host::CorruptGrid, Host::EmptyCell,
                                     Host::UnsupportedModule, Host::ThrowGrid, Host::None}) {
                    host.failure = failure;
                    host.requestedQuit = false;
                    host.keyboard = false;
                    host.startupFrames = 0;
                    host.gridReads = 0;
                    host.capturePrefix = "startup";
                    host.motionStarts = host.motionStops = host.motionReads = 0;
                    host.motionEnabled = false;
                    settings.screenMode = pass ? mino::ScreenMode::CRT : mino::ScreenMode::HiRes;
                    settings.orientation =
                        pass ? mino::Orientation::PortraitRight : mino::Orientation::Landscape;
                    settings.internalScale = 1;
                    display.configure(640, 480, core->nativeFormat(), settings);
                    REQUIRE(display.error().empty());
                    display.beginFrame();
                    host.slowLoad = pass == 1 && id == "minotron" && failure == Host::None;
                    host.fallbackStartup = pass == 1 && id == "minorescue";
                    bool initialized = false, threw = false;
                    try {
                        initialized = core->init(host, display.surface());
                    } catch (const std::runtime_error &e) {
                        if (std::string(e.what()) != "injected grid read exception")
                            throw;
                        threw = true;
                    }
                    checkGl();
                    REQUIRE(threw == (failure == Host::ThrowGrid));
                    REQUIRE(initialized == (failure == Host::None));
                    if (failure != Host::MissingResources && failure != Host::MalformedResources)
                        REQUIRE(host.gridReads > 0);
                    if (initialized) {
                        if (id == "gridrunner-classic") {
                            REQUIRE(host.startupFrames == 0);
                            display.beginFrame();
                            core->frame(1. / 60);
                            display.present();
                            checkGl();
                        } else {
                            REQUIRE(host.startupFrames == (host.fallbackStartup ? 0 : 1));
                            const auto pixels = [&]() {
                                const auto &s = display.surface();
                                std::vector<uint8_t> p(size_t(s.width) * s.height * 4);
                                glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
                                glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE,
                                             p.data());
                                return p;
                            };
                            const auto initial = pixels();
                            if (host.fallbackStartup)
                                host.startupAt = SDL_GetTicks();
                            if (!host.slowLoad) {
                                // Simulated time and held controls cannot shorten the real hold.
                                mino::PadState held;
                                held.connected = true;
                                held.buttons = held.pressed = mino::PAD_SOUTH;
                                core->pads(&held, 1);
                                core->back();
                                display.beginFrame();
                                core->frame(100.);
                                REQUIRE(pixels() == initial);
                                checkGl();
                                REQUIRE(!host.requestedQuit);
                                if (id == "minotron" && pass == 0) {
                                    settings.internalScale = 2;
                                    display.configure(800, 600, core->nativeFormat(), settings);
                                    core->resize(display.surface());
                                    display.beginFrame();
                                    core->frame(100.);
                                    display.present();
                                    checkGl();
                                    settings.internalScale = 1;
                                    display.configure(640, 480, core->nativeFormat(), settings);
                                    core->resize(display.surface());
                                    display.beginFrame();
                                    core->frame(100.);
                                    REQUIRE(pixels() == initial);
                                    checkGl();
                                }
                                display.present();
                                const auto elapsed = SDL_GetTicks() - host.startupAt;
                                if (elapsed < 1010)
                                    SDL_Delay(Uint32(1010 - elapsed));
                                core->pads(
                                    &held,
                                    1); // held acceptance must remain gated after the deadline
                                held.buttons = held.pressed = 0;
                                core->pads(&held, 1);
                            }
                            const auto before = SDL_GetTicks();
                            display.beginFrame();
                            core->frame(0);
                            REQUIRE(SDL_GetTicks() - before < 500);
                            // A slow init already used up the minimum; there is no added second.
                            REQUIRE(pixels() != initial);
                            checkGl();
                            display.present();
                            checkGl();
                        }
                        std::array<mino::PadState, mino::kMaxPads> pads{};
                        pads[0].connected = true;
                        if (id == "minotron" || id == "minorescue") {
                            pads[0].buttons = pads[0].pressed = mino::PAD_EAST;
                            core->pads(pads.data(), int(pads.size()));
                            display.beginFrame();
                            core->frame(1. / 60);
                            display.present();
                            REQUIRE(host.requestedQuit);
                            host.requestedQuit = false;
                            pads[0].buttons = pads[0].pressed = 0;
                            core->pads(pads.data(), int(pads.size()));
                        }
                        bool heardAudio = false;
                        for (int frame = 0; frame < 660; ++frame) {
                            pads[0].pressed = 0;
                            pads[0].buttons = 0;
                            pads[0].lx = 0;
                            pads[0].ly = 0;
                            if (frame == 180 || frame == 300 || frame == 420) {
                                pads[0].pressed = pads[0].buttons =
                                    id == "gridrunner-classic" && frame == 180 ? mino::PAD_START
                                                                               : mino::PAD_SOUTH;
                            }
                            if (frame >= 520 && frame < 580) {
                                pads[0].lx = .65f;
                                pads[0].ly = -.45f;
                            }
                            pads[0].connected =
                                (frame < 545 || frame >= 551) && (frame < 625 || frame >= 645);
                            host.keyboard = frame >= 635 && frame < 645;
                            if (id == "minos" && std::getenv("MINO_CAPTURE_MINOS_PHONE") &&
                                frame >= 603)
                                pads[0].connected = false;
                            core->pads(pads.data(), int(pads.size()));
                            const auto &surface = display.surface();
                            if (frame == 500)
                                core->touch({mino::TouchPhase::Down, 1000, surface.width * .25f,
                                             surface.height * .75f});
                            if (frame == 515)
                                core->touch({mino::TouchPhase::Move, 1000, surface.width * .35f,
                                             surface.height * .65f});
                            if (frame == 570)
                                core->touch({mino::TouchPhase::Up, 1000, surface.width * .35f,
                                             surface.height * .65f});
                            if (frame == 590)
                                core->pause(true);
                            if (frame == 600)
                                core->pause(false);
                            if (frame == 620) {
                                settings.internalScale = 2;
                                display.configure(800, 600, core->nativeFormat(), settings);
                                core->resize(display.surface());
                            }
                            display.beginFrame();
                            core->frame(1. / 60);
                            checkGl();
                            if (std::getenv("MINO_CAPTURE_CELLGRID") && id == "minos" &&
                                pass == 0 && (frame == 60 || frame == 120 || frame == 170)) {
                                const auto &target = display.surface();
                                std::vector<uint8_t> pixels(size_t(target.width) * target.height *
                                                            4);
                                glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
                                glReadPixels(0, 0, target.width, target.height, GL_RGBA,
                                             GL_UNSIGNED_BYTE, pixels.data());
                                stbi_flip_vertically_on_write(1);
                                const auto path =
                                    host.saves / ("title-" + std::to_string(frame) + ".png");
                                REQUIRE(stbi_write_png(path.string().c_str(), target.width,
                                                       target.height, 4, pixels.data(),
                                                       target.width * 4));
                            }
                            if ((id == "gridrunner" || id == "gridrunner-classic" ||
                                 id == "minotron" || id == "minorescue" || id == "minos") &&
                                frame == 610) {
                                std::vector<uint8_t> pixels(size_t(surface.width) * surface.height *
                                                            4);
                                glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
                                glReadPixels(0, 0, surface.width, surface.height, GL_RGBA,
                                             GL_UNSIGNED_BYTE, pixels.data());
                                stbi_flip_vertically_on_write(1);
                                const auto path =
                                    host.saves / ("pause-" + std::to_string(pass) + ".png");
                                REQUIRE(stbi_write_png(path.string().c_str(), surface.width,
                                                       surface.height, 4, pixels.data(),
                                                       surface.width * 4));
                            }
                            if (frame == 630 || frame == 640) {
                                const auto target = display.surface();
                                std::vector<uint8_t> image(size_t(target.width) * target.height *
                                                           4);
                                glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
                                glReadPixels(0, 0, target.width, target.height, GL_RGBA,
                                             GL_UNSIGNED_BYTE, image.data());
                                stbi_flip_vertically_on_write(1);
                                const auto path =
                                    host.saves /
                                    (std::string(frame == 630 ? "menu-touch-" : "menu-keyboard-") +
                                     std::to_string(pass) + ".png");
                                REQUIRE(stbi_write_png(path.string().c_str(), target.width,
                                                       target.height, 4, image.data(),
                                                       target.width * 4));
                            }
                            if (id == "minos" && std::getenv("MINO_CAPTURE_MINOS_CONTROLS") &&
                                frame >= 611 && frame <= 619) {
                                if ((frame & 1) == 1) {
                                    core->touch(
                                        {mino::TouchPhase::Down, 2000,
                                         float(
                                             surface.width *
                                             (std::getenv("MINO_CAPTURE_MINOS_PHONE") ? .5 : .26)),
                                         float(surface.height * .62)});
                                } else
                                    core->touch(
                                        {mino::TouchPhase::Up, 2000,
                                         float(
                                             surface.width *
                                             (std::getenv("MINO_CAPTURE_MINOS_PHONE") ? .5 : .26)),
                                         float(surface.height * .62)});
                                std::vector<uint8_t> pixels(size_t(surface.width) * surface.height *
                                                            4);
                                glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
                                glReadPixels(0, 0, surface.width, surface.height, GL_RGBA,
                                             GL_UNSIGNED_BYTE, pixels.data());
                                stbi_flip_vertically_on_write(1);
                                const auto path =
                                    host.saves / ("pause-touch-" + std::to_string(frame) + "-" +
                                                  std::to_string(pass) + ".png");
                                REQUIRE(stbi_write_png(path.string().c_str(), surface.width,
                                                       surface.height, 4, pixels.data(),
                                                       surface.width * 4));
                            }
                            display.present();
                            checkGl();
                            if (frame < 590 || frame >= 600) {
                                // Advance audio at its actual host rate alongside the
                                // fixed game clock, rather than leaving all voices
                                // occupied throughout this accelerated GL test.
                                std::array<float, 1470> block{};
                                core->mixAudio(block.data(), 735);
                                for (float value : block) {
                                    REQUIRE(std::isfinite(value) && std::fabs(value) <= 1.f);
                                    heardAudio |= value != 0;
                                }
                            }
                        }
                        if (id == "gridrunner" || id == "gridrunner-classic") {
                            const auto preferences =
                                host.saves /
                                (id == "gridrunner" ? "controls.txt" : "gridrunner-controls.txt");
                            auto input = [&](uint32_t buttons) {
                                pads[0] = {};
                                pads[0].connected = true;
                                pads[0].buttons = buttons;
                                pads[0].pressed = buttons;
                                core->pads(pads.data(), int(pads.size()));
                                display.beginFrame();
                                core->frame(1. / 60);
                                display.present();
                                checkGl();
                            };
                            auto press = [&](uint32_t button) {
                                input(button);
                                input(0);
                            };
                            for (int i = 0; i < 20; ++i)
                                input(0); // original Exit/control fade has completed
                            core->save();
                            int before = -1, touchBefore = -1;
                            {
                                std::ifstream f(preferences);
                                REQUIRE(bool(f >> before >> touchBefore));
                            }
                            press(mino::PAD_DOWN);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_SOUTH);
                            core->save();
                            int after = -1, touchAfter = -1;
                            {
                                std::ifstream f(preferences);
                                REQUIRE(bool(f >> after >> touchAfter));
                            }
                            REQUIRE(after == !before && touchAfter == !touchBefore);
                            press(mino::PAD_UP);
                            press(mino::PAD_UP);
                            press(mino::PAD_UP);
                            press(mino::PAD_SOUTH); // local scores
                            press(mino::PAD_EAST);  // return, preserving menu focus and held-input
                                                    // gate
                            press(mino::PAD_UP);
                            press(mino::PAD_SOUTH); // original pause icon resumes
                            press(mino::PAD_START);
                            for (int i = 0; i < 20; ++i)
                                input(0);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_SOUTH); // Title
                            for (int n = 0; n < 80; ++n)
                                input(0);
                            REQUIRE(!host.requestedQuit);
                            press(mino::PAD_EAST);
                            REQUIRE(host.requestedQuit); // title B exits to iOX
                        }
                        if (id == "minotron" || id == "minorescue") {
                            uint32_t previous = 0;
                            auto input = [&](uint32_t buttons) {
                                pads[0] = {};
                                pads[0].connected = true;
                                pads[0].buttons = buttons;
                                pads[0].pressed = buttons & ~previous;
                                previous = buttons;
                                core->pads(pads.data(), int(pads.size()));
                                display.beginFrame();
                                core->frame(1. / 60);
                                display.present();
                                checkGl();
                            };
                            auto press = [&](uint32_t button) {
                                input(button);
                                input(0);
                            };
                            for (int i = 0; i < 3; ++i)
                                input(0);
                            press(mino::PAD_DOWN);
                            input(mino::PAD_SOUTH); // Title, not the separate Exit
                            for (int i = 0; i < 60; ++i)
                                input(mino::PAD_SOUTH);
                            input(0);
                            REQUIRE(!host.requestedQuit);
                            press(mino::PAD_EAST);
                            REQUIRE(host.requestedQuit); // B on the returned title/opening
                            host.requestedQuit = false;
                            press(mino::PAD_SOUTH);
                            press(mino::PAD_SOUTH); // main menu then start selector
                            press(mino::PAD_SOUTH);
                            for (int i = 0; i < 100; ++i)
                                input(0);
                            press(mino::PAD_START);
                            for (int i = 0; i < 3; ++i)
                                input(0);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_DOWN);
                            press(mino::PAD_SOUTH);
                            for (int i = 0; i < 3; ++i)
                                input(0);
                            REQUIRE(host.requestedQuit); // pause Exit goes directly to iOX
                        }
                        REQUIRE(heardAudio);
                        if (id == "minos")
                            REQUIRE(host.motionStarts == 2 && host.motionStops == 1 &&
                                    host.motionReads > 500 && host.motionEnabled);
                        core->touch({mino::TouchPhase::Cancel, 1000, 0, 0});
                        core->back();
                        core->save();
                        std::array<float, 512> audio{};
                        core->mixAudio(audio.data(), 256);
                    }
                    core->shutdown();
                    core->shutdown();
                    checkGl();
                    if (id == "minos")
                        REQUIRE(!host.motionEnabled && host.motionStarts == host.motionStops);
                    if (failure == Host::None) {
                        // Reopen the same owner after a completed playable session,
                        // not only after a partial-init failure. No UI reset policy
                        // is inferred from this resource-lifecycle check.
                        host.requestedQuit = false;
                        host.startupFrames = 0;
                        host.gridReads = 0;
                        host.capturePrefix = "reopen-startup";
                        REQUIRE(core->init(host, display.surface()));
                        REQUIRE(host.gridReads > 0);
                        checkGl();
                        core->shutdown();
                        core->shutdown();
                        checkGl();
                    }
                }
                std::cout
                    << id << " pass " << pass + 1
                    << (pass ? " (CRT Royale, right cabinet)" : " (Hi-res, horizontal)")
                    << ": same-core resource/parser/partial-effect/exception recovery, mixed input, disconnect/reconnect, pause, resize, audio, present, same-owner success reopen and repeated shutdown passed\n";
            }
        display.shutdown();
        checkGl();
        // Real driver calls are forwarded unchanged. This checks balanced name
        // acquisition/release across the complete failure/retry/reopen sequence,
        // not the driver's physical memory reclamation or true GPU OOM behavior.
        for (const auto &entry : gpu_check::live)
            std::cerr << "Residual GL kind=" << entry.first.first << " name=" << entry.first.second
                      << '\n';
        for (const auto &error : gpu_check::errors)
            std::cerr << error << '\n';
        REQUIRE(gpu_check::live.empty() && gpu_check::errors.empty());
        std::cout
            << "Real GL name balance: no residual names or tracking errors after all failures, retries and reopens\n";
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
