// Run once normally and once with --lose-context, in fresh processes. Compare
// traces: the recovery must retain game/audio state and the next rendered frames.
#define MINOTAUR_SHELL_TEST
#include "../platform/main.cpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
uint64_t hashBytes(const void *data, size_t count, uint64_t h = 14695981039346656037ull) {
    auto *p = static_cast<const uint8_t *>(data);
    while (count--) {
        h ^= *p++;
        h *= 1099511628211ull;
    }
    return h;
}
void retainedBindings(App &app, bool lose, bool es) {
    using namespace neon::render;
    NeonDevice device;
    device.setHost(&app.host);
    REQUIRE(!device.Init(0, 32, 32, 0, 32, 32, 0));
    device.InitAllShaders();
    NeonBaseRender draw;
    draw.Init(&device);
    REQUIRE(!draw.GetVertexBufferWO(0, 3, -1, false));
    draw.initialised = 1;
    draw.primitive = GL_TRIANGLES;
    draw.shader = 2;
    draw.renderTarget = device.GetRTTexture(32, 16);
    REQUIRE(draw.renderTarget >= 0);
    const int retired = device.CreateVertexBufferWO(0, 3, -1);
    REQUIRE(retired >= 0);
    device.DestroyVertexuffer(retired, 0);
    draw.constEnabled[1] = true;
    draw.constEnabled[5] = true;
    for (int i = 0; i < 4; ++i) {
        draw.constants[4 + i] = float(i + 1) * .25f;
        draw.constants[20 + i] = .5f;
    }
    device.BeginFrame();
    draw.Render(0);
    device.PresentFrame();
    SDL_Event event{};
    event.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
    lifecycleWatch(&app, &event);
    if (lose) {
        auto old = app.gl;
        REQUIRE(SDL_GL_MakeCurrent(app.window, nullptr));
        REQUIRE(SDL_GL_DestroyContext(old));
        REQUIRE(createContext(app, es));
        app.gl = old;
    }
    event.type = lose ? SDL_EVENT_RENDER_DEVICE_RESET : SDL_EVENT_DID_ENTER_FOREGROUND;
    lifecycleWatch(&app, &event);
    REQUIRE(app.graphicsDirty);
    REQUIRE(synchronizeGraphics(app));
    setPaused(app, false);
    int w = 0, h = 0;
    device.GetCurrentRtSize(&w, &h);
    REQUIRE(w == 32 && h == 16);
    NeonVertex unused{};
    REQUIRE(device.UpdateBufferFromMem(retired, &unused, 1) == -1);
    draw.constEnabled[1] = false;
    draw.shader = 999;
    for (int i = 0; i < 4; ++i)
        draw.constants[4 + i] = .9f;
    device.BeginFrame();
    draw.Render(0);
    device.PresentFrame();
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    const int location = glGetUniformLocation(unsigned(program), "c1");
    REQUIRE(location >= 0);
    float value[4]{};
    glGetUniformfv(unsigned(program), location, value);
    for (int i = 0; i < 4; ++i)
        REQUIRE(value[i] == float(i + 1) * .25f);
    REQUIRE(glGetError() == GL_NO_ERROR);
    device.Destroy(true);
}
int main(int argc, char **argv) {
    try {
        REQUIRE(argc >= 3);
        const std::string id = argv[1], out = argv[2];
        const bool lose = argc > 3 && std::string(argv[3]) == "--lose-context";
#ifndef MINO_STANDALONE_GAME
        const bool launchLoss = argc > 3 && std::string(argv[3]) == "--launch-loss";
        const bool launchControl = argc > 3 && std::string(argv[3]) == "--launch-control";
#endif
        const bool es = argc > 4 && std::string(argv[4]) == "--gles";
        REQUIRE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD));
        App app;
        app.host.app = &app;
        app.prefDir = out + "/";
        app.savesRoot = app.prefDir + "saves/";
        std::filesystem::create_directories(app.savesRoot);
        app.host.achievementPath = app.prefDir + "achievements.txt";
#ifndef MINO_STANDALONE_GAME
        app.settings.load(app.prefDir + "settings.ini");
        if (argc > 5) {
            const std::string mode = argv[5];
            if (mode == "--crt" || mode == "--crt2")
                app.settings.set("global", "screen_mode", mode == "--crt2" ? "crt2" : "crt");
        }
        app.core = &app.launcher;
        app.gameId = "launcher";
#else
        REQUIRE(findCore(id));
        app.core = findCore(id)->create();
        app.gameId = id;
#endif
        app.window = SDL_CreateWindow("Application resume checks", 640, 480,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        REQUIRE(app.window);
        REQUIRE(createContext(app, es));
        REQUIRE(app.display.init(app.glES));
#ifdef MINO_STANDALONE_GAME
        REQUIRE(startGame(app, id));
        SDL_DestroyAudioStream(app.audio);
        app.audio = nullptr;
        SDL_Delay(1010);
#else
        app.launcher.setPaths(app.prefDir);
        reconfigure(app, false);
        REQUIRE(app.launcher.init(app.host, app.display.surface()));
        app.coreReady = true;
        if (id == "renderer") {
            retainedBindings(app, lose, es);
            app.launcher.shutdown();
            app.display.shutdown();
            SDL_GL_DestroyContext(app.gl);
            SDL_DestroyWindow(app.window);
            SDL_Quit();
            std::cout
                << "Selected target, retained program and disabled shader constants survived recovery\n";
            return 0;
        }
#ifdef MINO_COLLECTION_TITLE
        if (id == "title")
            REQUIRE(startCollectionTitle(app));
        else
#endif
            if (id != "launcher") {
            bool injected = false;
            const auto interrupt = [&](float) {
                if (!(launchLoss || launchControl) || injected)
                    return;
                injected = true;
                SDL_Event event{};
                event.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
                lifecycleWatch(&app, &event);
                SDL_PushEvent(&event);
                if (launchLoss) {
                    auto old = app.gl;
                    REQUIRE(SDL_GL_MakeCurrent(app.window, nullptr));
                    REQUIRE(SDL_GL_DestroyContext(old));
                    REQUIRE(createContext(app, es));
                    app.gl = old;
                }
            };
            REQUIRE(startGame(app, id, launchLoss || launchControl, interrupt));
            if (launchLoss || launchControl)
                REQUIRE(injected);
            REQUIRE(synchronizeGraphics(app));
            setPaused(app, false);
            SDL_DestroyAudioStream(app.audio);
            app.audio = nullptr;
            SDL_Delay(1010);
        }
#endif
        std::ofstream trace(out + "/trace.tsv");
        uint32_t previous = 0;
        for (int frame = 0; frame < 400; ++frame) {
            if (frame == 200 || frame == 280) {
                auto *owner = app.core;
                SDL_Event event{};
                event.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
                lifecycleWatch(&app, &event);
                REQUIRE(app.paused && app.graphicsCheckpointed);
                auto old = app.gl;
                REQUIRE(SDL_GL_MakeCurrent(app.window, nullptr));
                if (lose) {
                    REQUIRE(SDL_GL_DestroyContext(old));
                    REQUIRE(createContext(app, es));
                    app.gl = old;
                } else
                    REQUIRE(SDL_GL_MakeCurrent(app.window, old));
                event.type = lose ? SDL_EVENT_RENDER_DEVICE_RESET : SDL_EVENT_DID_ENTER_FOREGROUND;
                lifecycleWatch(&app, &event);
                REQUIRE(app.graphicsDirty);
                REQUIRE(synchronizeGraphics(app));
                setPaused(app, false);
                REQUIRE(app.core == owner && !app.paused && !app.graphicsCheckpointed && !app.quit);
            }
            uint32_t buttons = 0;
            if (id != "title" && (frame == 10 || frame == 40 || frame == 70))
                buttons = PAD_A;
            if (id == "goatup" && frame >= 71 && frame < 180)
                buttons = PAD_A;
            PadState pad{};
            pad.connected = true;
            pad.buttons = buttons;
            pad.pressed = buttons & ~previous;
            previous = buttons;
            if (frame > 90 && frame < 180) {
                pad.lx = .4f;
                pad.ly = -.3f;
            }
            app.core->pads(&pad, 1);
            app.display.beginFrame();
            app.core->frame(1. / 60);
            std::array<float, 1470> audio{};
            app.core->mixAudio(audio.data(), 735);
            if (frame >= 190) {
                const auto &surface = app.display.surface();
                std::vector<uint8_t> pixels(size_t(surface.width) * surface.height * 4);
                glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
                glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels.data());
                trace << frame << '\t' << std::hex << hashBytes(pixels.data(), pixels.size())
                      << '\t' << hashBytes(audio.data(), sizeof audio);
            }
            app.display.present();
            REQUIRE(glGetError() == GL_NO_ERROR);
            REQUIRE(!app.quit);
            if (frame >= 190) {
                std::vector<uint8_t> pixels(640 * 480 * 4);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                if (id == "title") {
                    size_t lit = 0;
                    for (size_t i = 0; i < pixels.size(); i += 4)
                        lit += pixels[i] || pixels[i + 1] || pixels[i + 2];
                    REQUIRE(lit > pixels.size() / 400);
                    REQUIRE(!app.returnRequested);
                }
                trace << '\t' << hashBytes(pixels.data(), pixels.size()) << std::dec << '\n';
            }
        }
        stopGame(app);
#ifndef MINO_STANDALONE_GAME
        app.launcher.shutdown();
#endif
        app.display.shutdown();
        SDL_GL_DestroyContext(app.gl);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        std::cout << id << ": repeated "
                  << (lose ? "context replacement" : "context detach/reattach")
                  << " kept core owner and rendered successfully\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
