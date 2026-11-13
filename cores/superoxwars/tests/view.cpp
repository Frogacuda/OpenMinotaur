#include "backend.h"
#include "air_enemy.h"
#include "ground.h"
#include "terrain.h"
#include "scene.h"
#include "controls.h"
#include "records.h"
#include "ground_features.h"
#include "neon/sample_audio.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "../superoxwars_core.cpp"
namespace mino {
namespace {
struct SuperOxWarsMotionChecks {
    static double delivered(Core &core) {
        return static_cast<SuperOxWarsCore &>(core).world_->layerTilt;
    }
    static double physical(Core &core) {
        return static_cast<SuperOxWarsCore &>(core).world_->lateralTilt;
    }
    static bool visible(Core &core) {
        return static_cast<SuperOxWarsCore &>(core).front_.tiltChoiceVisible();
    }
    static bool enabled(Core &core) {
        return static_cast<SuperOxWarsCore &>(core).front_.records.tiltEnabled;
    }
};
} // namespace
} // namespace mino
namespace {
void finishStartup(mino::Core &core, mino::Display &display) {
    display.beginFrame();
    core.frame(0);
    SDL_Delay(1010);
    display.beginFrame();
    core.frame(1. / 60);
}

void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
struct Host : mino::Host {
    bool motionReady = false, motionEnabled = false;
    int motionRequests = 0;
    mino::MotionState acceleration{true, .5, 0, 0};
    bool enableMotion(bool enabled) override {
        ++motionRequests;
        motionEnabled = enabled && motionReady;
        return motionEnabled;
    }
    mino::MotionState motion() const override {
        return motionEnabled ? acceleration : mino::MotionState{};
    }
    std::filesystem::path assets, savePath;
    mino::Display *display = nullptr;
    bool error = false, quit = false;
    uint32_t achieved = 1u << 9;
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    uint32_t originalAchievementDisplayMask() const override {
        return achieved;
    }
    void unlockAchievement(const std::string &id) override {
        if (id.rfind("original-", 0) == 0)
            achieved |= 1u << std::stoi(id.substr(9));
    }
    std::string saveDir() const override {
        return savePath.u8string();
    }
    void log(const char *fmt, ...) override {
        va_list a;
        va_start(a, fmt);
        char message[2048];
        std::vsnprintf(message, sizeof message, fmt, a);
        std::fputs(message, stderr);
        if (std::string(message).find("gameplay error") != std::string::npos)
            error = true;
        std::fputc(10, stderr);
        va_end(a);
    }
    std::string adaptShader(const std::string &s, int stage) const override {
        return display->adaptShader(s, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {
        quit = true;
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 3, "Supply source assets and screenshot destination");
        check(SDL_Init(SDL_INIT_VIDEO), "SDL video");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        auto *window = SDL_CreateWindow("Super Ox Wars family integration", 480, 640,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        check(window, "Window");
        auto context = SDL_GL_CreateContext(window);
        check(context, "GL context");
        check(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)), "GL loader");
        mino::Display display;
        check(display.init(false), "Display");
        mino::DisplaySettings settings;
        settings.internalScale = 1;
        display.configure(480, 640, {320, 480, .75f, .75f}, settings);
        check(std::abs(display.surface().displayAspect - .75f) < .001f,
              "Touch profile uses displayed aspect independently of 320x480 render target");
        Host host;
        host.assets = std::filesystem::u8path(argv[1]);
        host.display = &display;
        host.savePath = std::filesystem::u8path(argv[2]).parent_path() / "controller-test-saves";
        std::filesystem::create_directories(host.savePath);
        {
            std::ofstream old(host.savePath / "superoxwars-controls.txt");
            old << "SOW-CONTROLS 1\n0.05 3 0.05 3\n";
        }
        // Save records without the optional touch-mode field default to mode 0.
        const auto legacy = host.savePath / "legacy-records.txt";
        {
            std::ofstream old(legacy);
            old << "SOW-RECORDS 1\n0 40 20 60 3 4 0\n";
            for (int n = 0; n < 24; ++n)
                old << "0 0 " << (n ? 1 : 3) << '\n';
        }
        superoxwars::Records previous;
        check(previous.load(legacy.u8string()) && previous.blueBest == 40 &&
                  previous.touchMode == 0 && previous.tiltEnabled,
              "Legacy record loads with original touch choice and TILT ON default");
        superoxwars::Records empty;
        empty.unlocked = 10;
        empty.starts[10] = {100, 200, 4};
        empty.achievements = 1u << 7;
        empty.touchMode = 1;
        check(empty.save((host.savePath / "superoxwars-records.txt").u8string()),
              "Initialize isolated source record fixture");
        {
            superoxwars::Backend backend;
            check(backend.init(host, display.surface()), "Native effect/resource binding");
            superoxwars::World w;
            superoxwars::Scene scene(w);
            scene.start();
            w.player->grace = 1000000;
            std::vector<superoxwars::ActorDraw> actors;
            backend.background = [&](auto &target, double width, double height) {
                backend.drawActors(target, width, height, actors);
            };
            backend.foreground = [](auto &, double, double) {};
            backend.text = [](auto &grid) {
                grid.cls();
                grid.clattr();
            };
            // Exercise the actual SOW adapter and stack emitter, not a copied UV formula.
            bool uvChecked = false;
            backend.background = [&](auto &target, double width, double height) {
                // Original atlas rectangles for glyphs 1,9,17,31,46,50; cover both rows.
                const int origins[][2] = {{19, 426}, {163, 426}, {307, 426},
                                          {55, 444}, {325, 444}, {397, 444}};
                for (const auto &origin : origins) {
                    superoxwars::ActorDraw tile;
                    tile.explicitUv = true;
                    tile.uv = {origin[0] / 512., origin[1] / 512.};
                    tile.uvSize = {16. / 512., 16. / 512.};
                    backend.drawActors(target, width, height, {tile});
                    const auto *v = target.batchStack->verts;
                    check(v[0].u0 == float(origin[0] / 512.) &&
                              v[0].v0 == float(origin[1] / 512.) &&
                              v[2].u0 == float((origin[0] + 16) / 512.) &&
                              v[1].v0 == float((origin[1] + 16) / 512.),
                          "Original 16x16 terrain UV rectangle through production adapter");
                }
                superoxwars::ActorDraw ordinary;
                ordinary.frame = 10;
                backend.drawActors(target, width, height, {ordinary});
                const auto *v = target.batchStack->verts;
                const auto *entry = backend.resources().sprite(12);
                check(target.explicitUv == nullptr && v[0].u0 == float(double(entry->x) / 512.) &&
                          v[0].v0 == float(double(entry->y) / 512.),
                      "Explicit terrain UV cannot leak into the next ordinary actor");
                uvChecked = true;
            };
            backend.execute(w);
            check(uvChecked && backend.feedback().colour[0] == .2f &&
                      backend.feedback().scaleX == 1,
                  "Game-owned feedback reaches active effect execution");
            backend.background = [&](auto &target, double width, double height) {
                w.prepare(actors);
                backend.drawActors(target, width, height, actors);
            };
            backend.foreground = [&](auto &, double, double) { w.finishForeground(); };
            int ticks = 0;
            std::array<int, 5> injected{};
            std::array<bool, superoxwars::Scene::SupportedAreas> bossCaptured{}, terrainCaptured{};
            auto capture = [&](const std::string &suffix) {
                const auto surface = display.surface();
                std::vector<uint8_t> pixels(size_t(surface.width) * surface.height * 4);
                glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
                glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels.data());
                stbi_flip_vertically_on_write(1);
                check(stbi_write_png((std::string(argv[2]) + suffix).c_str(), surface.width,
                                     surface.height, 4, pixels.data(), surface.width * 4) != 0,
                      "Campaign screenshot");
            };
            while (w.level < 10 && ticks++ < 250000) {
                w.tick({{std::sin(double(ticks) * .013) * .025, 0}});
                display.beginFrame();
                backend.execute(w);
                backend.present();
                check(w.coverageStop.empty(), w.coverageStop.c_str());
                if (scene.terrain.cursor == 200 && !terrainCaptured[size_t(w.area)]) {
                    capture("-area" + std::to_string(w.area + 1) + "-terrain.png");
                    terrainCaptured[size_t(w.area)] = true;
                }
                if (scene.bossPhase && scene.speed < .2f && !bossCaptured[size_t(w.area)]) {
                    capture("-area" + std::to_string(w.area + 1) + "-boss.png");
                    bossCaptured[size_t(w.area)] = true;
                }
                for (auto *e = w.lists[size_t(superoxwars::Domain::Air)]; e; e = e->next)
                    if (auto *g = dynamic_cast<superoxwars::GroundEnemy *>(e);
                        g && !g->dead && scene.speed < .1f) {
                        const int family = dynamic_cast<superoxwars::Eyeball *>(g) ? 0
                                           : dynamic_cast<superoxwars::Nose *>(g)  ? 1
                                           : dynamic_cast<superoxwars::Ear *>(g)   ? 2
                                           : dynamic_cast<superoxwars::Horn *>(g)  ? 3
                                           : dynamic_cast<superoxwars::Tooth *>(g) ? 4
                                                                                   : -1;
                        if (family >= 0) {
                            superoxwars::Entity hit(w);
                            hit.type = 0x2000;
                            hit.frame = 28;
                            hit.position = g->position;
                            const int hits = g->armour + 1;
                            for (int n = 0; n < hits; ++n)
                                g->hitBy(hit);
                            ++injected[size_t(family)];
                        }
                    }
            }
            std::cout << "Native route ticks=" << ticks << " level=" << w.level
                      << " injected eye/nose/ear/horn/tooth kills=";
            for (int count : injected)
                std::cout << count << '/';
            std::cout << '\n';
            check(
                w.level == 10 &&
                    std::all_of(terrainCaptured.begin(), terrainCaptured.end(),
                                [](bool v) { return v; }) &&
                    std::all_of(bossCaptured.begin(), bossCaptured.end(), [](bool v) { return v; }),
                "Native ten-area route reaches terrain, every boss and repeat progression");
            check(w.coverageStop.empty(), w.coverageStop.c_str());
            check(glGetError() == GL_NO_ERROR, "Native GL error");
            const auto surface = display.surface();
            std::vector<uint8_t> pixels(size_t(surface.width) * surface.height * 4);
            glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
            glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
            size_t lit = 0;
            for (size_t at = 0; at < pixels.size(); at += 4)
                if (pixels[at] || pixels[at + 1] || pixels[at + 2])
                    ++lit;
            check(lit > 1000, "Source game artwork missing");
            stbi_flip_vertically_on_write(1);
            check(stbi_write_png(argv[2], surface.width, surface.height, 4, pixels.data(),
                                 surface.width * 4) != 0,
                  "Screenshot write");
            backend.release();
        }
        {
            std::unique_ptr<mino::Core> core(mino::kSuperOxWarsCore.create());
            check(core->init(host, display.surface()), "Playable core initialization");
            check(host.motionRequests == 0, "Sensors wait until loading finishes");
            finishStartup(*core, display);
            check(host.achieved & (1u << 7),
                  "Original goal identity restored through common Host reporting");
            check(host.motionRequests == 1 && !host.motionEnabled,
                  "Unavailable startup sensor attempted once");
            check(!mino::SuperOxWarsMotionChecks::visible(*core),
                  "No motion capability hides the tilt toggle");
            auto press = [&](uint32_t key) {
                mino::PadState p;
                p.connected = true;
                core->pads(&p, 1);
                p.buttons = p.pressed = key;
                core->pads(&p, 1);
                p.pressed = 0;
                core->pads(&p, 1);
            };
            auto captureCore = [&](const char *suffix) {
                const auto s = display.surface();
                std::vector<uint8_t> pixels(size_t(s.width) * s.height * 4);
                glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
                glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                stbi_flip_vertically_on_write(1);
                check(stbi_write_png((std::string(argv[2]) + suffix).c_str(), s.width, s.height, 4,
                                     pixels.data(), s.width * 4) != 0,
                      "Core frontend screenshot");
            };
            display.beginFrame();
            core->frame(1. / 60); // Loading.
            display.beginFrame();
            core->frame(1. / 60);
            captureCore("-title.png");
            press(mino::PAD_A); // Title -> selector.
            mino::PadState held;
            held.connected = true;
            held.buttons = mino::PAD_A;
            core->pads(&held, 1);
            std::vector<float> heldSamples(1470);
            bool entryAudible = false;
            for (int n = 0; n < 60; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
                core->mixAudio(heldSamples.data(), 735);
                entryAudible |= std::any_of(heldSamples.begin(), heldSamples.end(),
                                            [](float v) { return v != 0; });
            }
            check(entryAudible, "Selector entry direct sample is audible while A remains held");
            // Later fresh navigation and Start edges exercise that the hold did not
            // skip this selector. Demo suppression does not apply to its UI click.
            captureCore("-selector.png");
            press(mino::PAD_DOWN);
            press(mino::PAD_A);
            for (int n = 0; n < 12; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            captureCore("-scores.png");
            press(mino::PAD_B);
            display.beginFrame();
            core->frame(1. / 60);
            press(mino::PAD_DOWN);
            press(mino::PAD_RIGHT);
            press(mino::PAD_A);
            for (int n = 0; n < 12; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            captureCore("-goals.png");
            press(mino::PAD_B);
            display.beginFrame();
            core->frame(1. / 60);
            for (int n = 0; n < 10; ++n)
                press(mino::PAD_RIGHT); // Saved loop-two start also exercises the original JOURNEY
                                        // popup.
            host.motionReady = true;
            press(mino::PAD_A); // Fresh edge starts the selected area.
            for (int n = 0; n < 3; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            check(host.motionRequests == 2 && host.motionEnabled &&
                      mino::SuperOxWarsMotionChecks::delivered(*core) == -.5,
                  "Entering play recovers a failed startup sensor open and delivers its data");
            core->pause(true);
            core->pause(false);
            check(!host.motionEnabled, "Resume defers opening to the active frame");
            display.beginFrame();
            core->frame(1. / 60);
            check(host.motionEnabled && mino::SuperOxWarsMotionChecks::delivered(*core) == -.25,
                  "Host sensor reaches the production world with original first-sample averaging");
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == -.5,
                  "Held physical tilt reaches the layer consumer");
            mino::PadState worldPad;
            worldPad.connected = true;
            worldPad.rx = .75f;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(
                mino::SuperOxWarsMotionChecks::delivered(*core) == .75 &&
                    mino::SuperOxWarsMotionChecks::physical(*core) == -.5,
                "Right stick has a direct world path independent of the physical sensor producer");
            worldPad.buttons = mino::PAD_A;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == .75,
                  "Ship precision modifier does not scale world influence");
            worldPad = {};
            worldPad.connected = true;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == -.5,
                  "Centered stick returns to physical tilt");
            host.acceleration.available = false;
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == 0,
                  "Missing sensor neutralizes tilt");
            worldPad.rx = -1;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == -1,
                  "Right stick works with no accelerometer");
            core->pause(true);
            core->pause(false);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == 0,
                  "Lifecycle pause clears held world input until a fresh report");
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == -1,
                  "Fresh controller report restores world input after resume");
            core->pads(nullptr, 0);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == 0,
                  "Controller disconnection releases world influence");
            host.acceleration = {true, -.5, 0, 0};
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::delivered(*core) == .25,
                  "Reconnection does not average a stale sample");
            host.acceleration = {true, 0, 0, 0};
            for (int n = 0; n < 2; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            worldPad = {};
            worldPad.connected = true;
            core->pads(&worldPad, 1);
            press(mino::PAD_START);
            for (int n = 0; n < 20; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            check(mino::SuperOxWarsMotionChecks::visible(*core) &&
                      mino::SuperOxWarsMotionChecks::enabled(*core),
                  "Tilt and controller expose default TILT ON");
            captureCore("-tilt-on.png");
            press(mino::PAD_DOWN);
            press(mino::PAD_DOWN);
            press(mino::PAD_A);
            display.beginFrame();
            core->frame(1. / 60);
            check(!host.motionEnabled && !mino::SuperOxWarsMotionChecks::enabled(*core) &&
                      mino::SuperOxWarsMotionChecks::visible(*core),
                  "R STICK disables physical tilt without hiding its toggle");
            host.acceleration = {true, .9, 0, 0};
            worldPad.rx = .5f;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::physical(*core) == 0 &&
                      mino::SuperOxWarsMotionChecks::delivered(*core) == .5,
                  "R STICK suppresses physical motion while retaining direct world input");
            captureCore("-right-stick.png");
            {
                superoxwars::Records stored;
                check(stored.load((host.savePath / "superoxwars-records.txt").u8string()) &&
                          !stored.tiltEnabled,
                      "Tilt preference persists in source records");
            }
            worldPad.buttons = mino::PAD_A;
            for (int n = 0; n < 20; ++n) {
                core->pads(&worldPad, 1);
                display.beginFrame();
                core->frame(1. / 60);
            }
            check(!mino::SuperOxWarsMotionChecks::enabled(*core),
                  "Held confirmation does not repeat the toggle");
            core->pads(nullptr, 0);
            display.beginFrame();
            core->frame(1. / 60);
            check(!mino::SuperOxWarsMotionChecks::visible(*core) && host.motionEnabled,
                  "Controller removal hides toggle and restores physical tilt");
            captureCore("-no-controller.png");
            worldPad = {};
            worldPad.connected = true;
            core->pads(&worldPad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(mino::SuperOxWarsMotionChecks::visible(*core) && !host.motionEnabled,
                  "Controller reconnection restores saved R STICK choice");
            const auto surface = display.surface();
            core->touch({mino::TouchPhase::Down, 99, .5f * surface.width, .58f * surface.height});
            for (int n = 0; n < 12; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            check(mino::SuperOxWarsMotionChecks::enabled(*core) && host.motionEnabled,
                  "Touch toggles TILT ON exactly once while held");
            core->touch({mino::TouchPhase::Up, 99, .5f * surface.width, .58f * surface.height});
            host.acceleration = {true, 0, 0, 0};
            display.beginFrame();
            core->frame(1. / 60);
            press(mino::PAD_B);
            std::vector<float> samples(1470);
            bool audible = false;
            for (int tick = 0; tick < 1200; ++tick) {
                mino::PadState pad;
                pad.connected = true;
                pad.lx = float(std::sin(double(tick) * .017)) * .25f;
                core->pads(&pad, 1);
                display.beginFrame();
                core->frame(1. / 60);
                core->mixAudio(samples.data(), 735);
                for (float x : samples) {
                    check(std::isfinite(x) && std::abs(x) <= 1,
                          "Finite bounded original sample mix");
                    audible |= x != 0;
                }
                check(!host.error, "Playable core reached an unfinished path");
                if (tick == 70) {
                    captureCore("-achievement.png");
                    press(mino::PAD_START);
                    for (int n = 0; n < 20; ++n) {
                        display.beginFrame();
                        core->frame(1. / 60);
                    }
                    captureCore("-pause.png");
                    press(mino::PAD_B);
                }
            }
            check(host.achieved & (1u << 6), "Eligible JOURNEY award reaches common Host");
            check(audible, "Original sample audio audible");
            core->save();
            superoxwars::Records saved;
            check(saved.load((host.savePath / "superoxwars-records.txt").u8string()),
                  "Source records serialize and reopen");
            check(saved.achievements & (1u << 9),
                  "Existing collection goals are retained in the local presentation record");
            check(saved.unlocked == 10 && saved.starts[10].blue == 100 &&
                      saved.starts[10].red == 200 && saved.starts[10].lives == 4,
                  "Saved starting state survives frontend and gameplay lifecycle");
            check(saved.touchMode == 1,
                  "Saved touch mode survives frontend and gameplay lifecycle");
            press(mino::PAD_START);
            for (int n = 0; n < 20; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            press(mino::PAD_DOWN);
            press(mino::PAD_A);
            display.beginFrame();
            core->frame(1. / 60);
            check(!host.quit, "Pause Title must not request iOX");
            captureCore("-pause-title.png");
            press(mino::PAD_A);
            display.beginFrame();
            core->frame(1. / 60);
            press(mino::PAD_A);
            display.beginFrame();
            core->frame(1. / 60);
            press(mino::PAD_START);
            for (int n = 0; n < 20; ++n) {
                display.beginFrame();
                core->frame(1. / 60);
            }
            press(mino::PAD_DOWN);
            press(mino::PAD_DOWN);
            press(mino::PAD_DOWN);
            press(mino::PAD_A);
            check(host.quit, "Pause Exit reaches the real core Host iOX request");
            host.quit = false;
            core->shutdown();
            core->shutdown();
            check(core->init(host, display.surface()), "Core reopens after shutdown");
            finishStartup(*core, display);
            core->frame(1. / 60);
            press(mino::PAD_B);
            check(host.quit, "Title B exits to iOX");
            core->shutdown();
        }
        display.shutdown();
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "Super Ox Wars: ten-area native render with test-only protection/hits; 1200 "
                     "Core controller/audio frames; original pause, record persistence, exit and "
                     "reopen passed. Original frontend and HUD integrated.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
