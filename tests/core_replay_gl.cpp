// Deterministic, same-harness before/after gameplay and GPU-name lifetime probe.
// Original-execution fidelity and physical GPU memory reclamation are separate.
#include "core_api.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include "gl_object_tracker.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "../third_party/stb/stb_image_write.h"
#include <array>
#include <cmath>
#include <cstdarg>
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
uint64_t hash(const void *data, size_t size, uint64_t h = 14695981039346656037ull) {
    const auto *p = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
struct Host final : mino::Host {
    std::filesystem::path assets, saves;
    mino::Display *display = nullptr;
    bool quit = false;
    bool readAsset(const std::string &name, std::vector<uint8_t> &data) const override {
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        data.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    std::string saveDir() const override {
        return saves.generic_string() + "/";
    }
    void log(const char *fmt, ...) override {
        va_list a;
        va_start(a, fmt);
        std::vfprintf(stderr, fmt, a);
        va_end(a);
        std::fputc('\n', stderr);
    }
    std::string adaptShader(const std::string &source, int stage) const override {
        return display->adaptShader(source, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {
        quit = true;
    }
    bool presentStartupFrame() override {
        gpu_check::Scope scope("display", "startup present");
        display->present();
        return true;
    }
};
void glCheck() {
    REQUIRE(glGetError() == GL_NO_ERROR);
}
} // namespace
int main(int argc, char **argv) {
    try {
        REQUIRE(argc == 5);
        const auto assets = std::filesystem::absolute(argv[1]),
                   output = std::filesystem::absolute(argv[2]);
        const std::string id = argv[3];
        const bool es = std::string(argv[4]) == "es";
        std::filesystem::create_directories(output);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        auto *window =
            SDL_CreateWindow("Neon replay", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        REQUIRE(window);
        auto context = SDL_GL_CreateContext(window);
        REQUIRE(context);
        REQUIRE(SDL_GL_MakeCurrent(window, context));
        REQUIRE(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)));
        gpu_check::install();
        mino::Display display;
        REQUIRE(display.init(es));
        mino::DisplaySettings settings;
        settings.internalScale = 1;
        settings.screenMode = mino::ScreenMode::HiRes;
        settings.orientation = mino::Orientation::Landscape;
        const auto *entry = mino::findCore(id);
        REQUIRE(entry);
        std::unique_ptr<mino::Core> core(entry->create());
        Host host;
        host.assets = assets / (entry->info->assetGameId ? entry->info->assetGameId : id);
        host.saves = output / "saves";
        host.display = &display;
        std::filesystem::create_directories(host.saves);
        display.configure(640, 480, core->nativeFormat(), settings);
        display.beginFrame();
        {
            gpu_check::Scope scope(id, "init");
            REQUIRE(core->init(host, display.surface()));
        }
        glCheck();
        std::array<mino::PadState, mino::kMaxPads> pads{};
        pads[0].connected = true;
        if (id != "gridrunner-classic") {
            SDL_Delay(1010);
            display.beginFrame();
            gpu_check::Scope scope(id, "startup handover");
            core->pads(pads.data(), int(pads.size()));
            core->frame(0);
        }
        std::ofstream trace(output / "trace.tsv");
        trace << "frame\twidth\theight\tpixels\taudio\n";
        uint64_t audioHash = 14695981039346656037ull;
        bool audible = false;
        uint32_t prior = 0;
        int samples = 0;
        size_t peakObjects = 0;
        for (int frame = 0; frame < 1200; ++frame) {
            uint32_t buttons = (frame == 180 || frame == 300 || frame == 420) ? mino::PAD_SOUTH : 0;
            if (frame >= 500 && frame < 570)
                buttons |= mino::PAD_SOUTH;
            // GoatUp requires held jump to remain on its scrolling course. A brief
            // menu-style pulse otherwise leaves most of this replay on the results screen.
            if (id == "goatup" && frame >= 181 && frame < 780)
                buttons |= mino::PAD_SOUTH;
            pads[0].buttons = buttons;
            pads[0].pressed = buttons & ~prior;
            prior = buttons;
            pads[0].lx = frame >= 450 ? float(std::sin(double(frame) * .03) * .65) : 0;
            pads[0].ly = frame >= 450 ? float(std::cos(double(frame) * .024) * .5) : 0;
            pads[0].rx = frame >= 450 ? .8f : 0;
            pads[0].ry = frame >= 450 ? -.4f : 0;
            if (frame == 900 || frame == 1000) {
                settings.internalScale = frame == 900 ? 2 : 1;
                display.configure(frame == 900 ? 800 : 640, frame == 900 ? 600 : 480,
                                  core->nativeFormat(), settings);
                gpu_check::Scope scope(id, "resize");
                core->resize(display.surface());
            }
            display.beginFrame();
            {
                gpu_check::Scope scope(id, "frame");
                core->pads(pads.data(), int(pads.size()));
                const auto &surface = display.surface();
                if (frame == 680)
                    core->touch(
                        {mino::TouchPhase::Down, 777, surface.width * .25f, surface.height * .75f});
                if (frame == 700)
                    core->touch(
                        {mino::TouchPhase::Move, 777, surface.width * .35f, surface.height * .65f});
                if (frame == 720)
                    core->touch(
                        {mino::TouchPhase::Up, 777, surface.width * .35f, surface.height * .65f});
                if (frame == 800)
                    core->pause(true);
                if (frame == 830)
                    core->pause(false);
                core->frame(1. / 60);
                std::array<float, 1470> audio{};
                core->mixAudio(audio.data(), 735);
                for (float sample : audio) {
                    REQUIRE(std::isfinite(sample) && std::abs(sample) <= 1);
                    audible |= sample != 0;
                }
                audioHash = hash(audio.data(), sizeof(audio), audioHash);
            }
            glCheck();
            REQUIRE(!host.quit);
            peakObjects = std::max(peakObjects, gpu_check::owned(id));
            if (frame % 30 == 0 || frame == 1199) {
                const auto &s = display.surface();
                std::vector<uint8_t> pixels(size_t(s.width) * s.height * 4);
                glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
                glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glCheck();
                trace << frame << '\t' << s.width << '\t' << s.height << '\t' << std::hex
                      << hash(pixels.data(), pixels.size()) << '\t' << audioHash << std::dec
                      << '\n';
                ++samples;
                if (frame == 120 || frame == 210 || frame == 450 || frame == 750 || frame == 1050 ||
                    frame == 1199) {
                    stbi_flip_vertically_on_write(1);
                    REQUIRE(stbi_write_png(
                        (output / ("frame-" + std::to_string(frame) + ".png")).string().c_str(),
                        s.width, s.height, 4, pixels.data(), s.width * 4));
                }
            }
            display.present();
            glCheck();
        }
        REQUIRE(audible);
        {
            gpu_check::Scope scope(id, "shutdown");
            core->shutdown();
            core->shutdown();
            core.reset();
        }
        glCheck();
        std::ofstream objects(output / "gpu-objects.tsv");
        objects << "kind\tname\towner\tphase\n";
        for (const auto &pair : gpu_check::live)
            if (pair.second.owner == id)
                objects << pair.first.first << '\t' << pair.first.second << '\t'
                        << pair.second.owner << '\t' << pair.second.phase << '\n';
        const auto residual = gpu_check::owned(id);
        display.shutdown();
        glCheck();
        const auto totalResidual = gpu_check::live.size();
        std::ofstream report(output / "result.txt");
        report << "samples=" << samples << " peak_core_names=" << peakObjects
               << " residual_core_names=" << residual << " residual_all_names=" << totalResidual
               << " tracker_errors=" << gpu_check::errors.size() << '\n';
        for (const auto &error : gpu_check::errors)
            report << error << '\n';
        std::cout << id << " " << (es ? "ES" : "desktop") << ": " << samples
                  << " samples; residual core=" << residual << " all=" << totalResidual
                  << " tracker errors=" << gpu_check::errors.size() << '\n';
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        REQUIRE(residual == 0 && totalResidual == 0 && gpu_check::errors.empty());
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
