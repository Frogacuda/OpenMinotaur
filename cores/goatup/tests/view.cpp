// Native rendering integration fixture, not a playable/game-fidelity witness.
#include "backend.h"
#include "climber.h"
#include "denizens.h"
#include "world.h"
#include "frontend.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
namespace mino {
extern const CoreEntry kGoatUpCore;
}

namespace {
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
struct Host final : mino::Host {
    std::filesystem::path assets, saves;
    mino::Display *display = nullptr;
    bool coreError = false, quit = false;
    bool hasSensor = false, motionEnabled = false, hasTouch = true;
    mutable int motionReads = 0;
    bool enableMotion(bool enabled) override {
        motionEnabled = enabled && hasSensor;
        return motionEnabled;
    }
    mino::MotionState motion() const override {
        ++motionReads;
        return {motionEnabled, .4, 0, -1};
    }
    bool touchControlsAvailable() const override {
        return hasTouch;
    }
    bool presentStartupFrame() override {
        display->present();
        return true;
    }
    bool readAsset(const std::string &name, std::vector<uint8_t> &bytes) const override {
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        bytes.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    std::string saveDir() const override {
        return saves.string() + "/";
    }
    void log(const char *fmt, ...) override {
        va_list a;
        va_start(a, fmt);
        char line[2048];
        std::vsnprintf(line, sizeof line, fmt, a);
        va_end(a);
        std::fprintf(stderr, "%s\n", line);
        if (std::string(line).find("GoatUp runtime error") != std::string::npos)
            coreError = true;
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
void capture(const std::filesystem::path &path, const mino::SurfaceInfo &surface) {
    std::vector<uint8_t> pixels(size_t(surface.width) * surface.height * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
    glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const auto readError = glGetError();
    if (readError != GL_NO_ERROR)
        throw std::runtime_error("Readback error " + std::to_string(readError) + " at " +
                                 path.string());
    size_t lit = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i + 1] || pixels[i + 2])
            ++lit;
    check(lit > size_t(surface.width) * surface.height / 10, "Missing tower rendering");
    stbi_flip_vertically_on_write(1);
    check(stbi_write_png(path.string().c_str(), surface.width, surface.height, 4, pixels.data(),
                         surface.width * 4) != 0,
          "PNG write");
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 3, "Supply GoatUp asset directory and scratch output directory");
        std::filesystem::path scratch = std::filesystem::u8path(argv[2]);
        std::filesystem::create_directories(scratch);
        check(SDL_Init(SDL_INIT_VIDEO), "SDL video");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        auto *window = SDL_CreateWindow("GoatUp world rendering", 480, 640,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        check(window, "SDL window");
        auto gl = SDL_GL_CreateContext(window);
        check(gl, "GL context");
        check(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)), "GL loader");
        mino::Display display;
        check(display.init(false), "Display init");
        mino::DisplaySettings settings;
        settings.internalScale = 1;
        settings.aspect = mino::AspectMode::Original;
        display.configure(480, 640, {320, 480, 2.f / 3, 2.f / 3}, settings);
        Host host;
        host.assets = std::filesystem::u8path(argv[1]);
        host.display = &display;
        goatup::Backend backend;
        check(backend.init(host, display.surface()), "Original GoatUp effect/resource init");
        goatup::TowerSurface tower;
        goatup::Climber goat;
        neon::DarwinRandom random(1);
        int generated = 0, backgrounds = 0;
        goatup::Effects effects(random);
        effects.initializeStars();
        std::vector<goatup::ActorDraw> actors;
        goatup::ClimbContext game(tower);
        goatup::CollectionRecord records;
        goatup::DenizenContext actorContext{game, goat, random, effects, records};
        actorContext.cyclingBonusColour.white(1);
        // The renderer fixture records feedback without an audio device or host
        // save service. The real core must bind these before exposing gameplay.
        actorContext.sound = [](int, int) {};
        actorContext.achievement = [](int) {};
        actorContext.bonusCaption = [](int) {};
        actorContext.popup = [&](int points, auto at, double vx, double vy, float radius) {
            effects.floatingScore(points, at, vx, vy, radius);
        };
        actorContext.spriteSize = [&](int frame) {
            const auto *entry = backend.resources().sprite(frame);
            check(entry, "Missing NPC frame");
            return goatup::Vec2{double(entry->w), double(entry->h)};
        };
        goatup::Denizens denizens(actorContext);
        goatup::PlatformGenerator generation(random, game);
        generation.create = [&](const auto &birth) { denizens.create(birth); };
        generation.enterZone = [] {};
        tower.generate = [&](auto &p) {
            ++generated;
            generation.generate(p);
        };
        backend.background = [&](auto &target, double w, double h) {
            ++backgrounds;
            actors.clear();
            effects.drawStars(actors);
            backend.drawActors(target, w, h, actors);
            tower.prepare();
            backend.drawSurface(target, w, h, tower);
        };
        backend.foreground = [&](auto &target, double w, double h) {
            actors.clear();
            effects.drawParticles(actors);
            denizens.prepareDraw(actors);
            effects.prepareUiDraw(actors);
            backend.drawActors(target, w, h, actors);
            // Source _draw orders the player AFTER NPCs and UI actors.
            goat.prepareDraw();
            target.flags = 3;
            target.frame = goat.frame + 2;
            target.angle = goat.angle;
            target.col.white(1);
            target.pos = {goat.position.x * w, goat.position.y * h};
            target.width = double(float(goat.radius + goat.radius)) * w * goat.projectedScale.x;
            target.height = double(float(goat.radius + goat.radius)) * h * goat.projectedScale.y;
            target.setupChecking();
        };
        display.beginFrame();
        backend.execute(random);
        backend.present();
        check(backgrounds == 1 && generated > 0,
              "Source background callback runs at the effect slot");
        capture(scratch / "opening-tower.png", display.surface());
        const int before = generated, bg = backgrounds;
        const auto draws = random.draws();
        for (int i = 0; i < 3; ++i) {
            display.beginFrame();
            backend.present();
        }
        check(generated == before && backgrounds == bg && random.draws() == draws,
              "Redraw does not generate content or consume random draws");
        tower.scroll = 2;
        tower.angle = .15f;
        goat.heading = .3f;
        display.beginFrame();
        backend.execute(random);
        backend.present();
        capture(scratch / "rotated-tower.png", display.surface());
        for (int i = 0; i < 3; ++i) {
            denizens.tick();
            effects.tickUi(false, tower.angle, actorContext.cyclingBonusColour);
            display.beginFrame();
            backend.execute(random);
            backend.present();
        }
        bool billy = false;
        for (const auto &contact : denizens.contacts())
            if (contact.radius == .034f && contact.position.y > -.4 && contact.position.y < .4) {
                denizens.hit(contact.ordinal, 1, goat.position);
                billy = true;
                break;
            }
        check(billy && effects.hearts() == 20,
              "Generated Billy uses its live constructor and collection family");
        denizens.tick();
        effects.tickUi(false, tower.angle, actorContext.cyclingBonusColour);
        display.beginFrame();
        backend.execute(random);
        backend.present();
        capture(scratch / "actor-feedback.png", display.surface());
        // Controlled second-zone start: render and tick the real composed
        // family without requiring the short input script to beat zone one.
        goatup::CollectionRecord zoneRecords;
        goatup::World zoneWorld(zoneRecords, 1);
        zoneWorld.actors.sound = [](int, int) {};
        zoneWorld.game.stopSound = [](int) {};
        zoneWorld.actors.spriteSize = actorContext.spriteSize;
        zoneWorld.tower.scroll = 25;
        backend.background = [&](auto &target, double w, double h) {
            actors.clear();
            zoneWorld.prepareBackground(actors);
            backend.drawActors(target, w, h, actors);
            backend.drawSurface(target, w, h, zoneWorld.tower);
        };
        backend.foreground = [&](auto &target, double w, double h) {
            actors.clear();
            zoneWorld.prepareForeground(actors);
            backend.drawActors(target, w, h, actors);
        };
        display.beginFrame();
        backend.execute(zoneWorld.random);
        backend.present();
        for (int tick = 0; tick < 120; ++tick) {
            zoneWorld.tick({goatup::ClimbInput::Method::None, 0, true});
            backend.feedbackState = zoneWorld.feedback;
            display.beginFrame();
            backend.execute(zoneWorld.random);
            backend.present();
        }
        check(zoneWorld.game.lastTheme == 1 && zoneWorld.game.highestGenerated >= 69,
              "Composed second-zone factory did not execute");
        capture(scratch / "zone2-live.png", display.surface());
        zoneWorld.actors.poison = 1;
        zoneWorld.tick({});
        backend.feedbackState = zoneWorld.feedback;
        display.beginFrame();
        backend.execute(zoneWorld.random);
        backend.present();
        capture(scratch / "zone2-poison.png", display.surface());
        check(glGetError() == GL_NO_ERROR, "Native renderer errors");
        for (int theme = 2; theme < 9; ++theme) {
            goatup::CollectionRecord campaignRecords;
            goatup::World campaign(campaignRecords, 1);
            campaign.actors.sound = [](int, int) {};
            campaign.game.stopSound = [](int) {};
            campaign.actors.spriteSize = actorContext.spriteSize;
            campaign.tower.scroll = float(double(theme * 64) * .4 - .6);
            backend.background = [&](auto &target, double w, double h) {
                actors.clear();
                campaign.prepareBackground(actors);
                backend.drawActors(target, w, h, actors);
                backend.drawSurface(target, w, h, campaign.tower);
            };
            backend.foreground = [&](auto &target, double w, double h) {
                actors.clear();
                campaign.prepareForeground(actors);
                backend.drawActors(target, w, h, actors);
            };
            display.beginFrame();
            backend.execute(campaign.random);
            backend.present();
            for (int tick = 0; tick < 240; ++tick) {
                campaign.tick({goatup::ClimbInput::Method::DirectAxis, .15, true});
                backend.feedbackState = campaign.feedback;
                display.beginFrame();
                backend.execute(campaign.random);
                backend.present();
                if (tick == 80)
                    capture(scratch / ("campaign-zone-" + std::to_string(theme + 1) + ".png"),
                            display.surface());
            }
            check(glGetError() == GL_NO_ERROR, "Campaign render error");
        }
        // Render the retained original text, including the previously omitted
        // lifetime half of GOAT OVER, through the real cell-grid effect slot.
        goatup::CollectionRecord overRecord;
        overRecord.uniqueBonuses = 321;
        overRecord.highestPlatform = 499;
        overRecord.billyGoats = 123;
        overRecord.mostKids = 27;
        goatup::World over(overRecord, 17);
        over.actors.sound = [](int, int) {};
        over.game.stopSound = [](int) {};
        over.actors.spriteSize = actorContext.spriteSize;
        over.game.gameOver = true;
        over.tick({});
        backend.background = [&](auto &target, double w, double h) {
            actors.clear();
            over.prepareBackground(actors);
            backend.drawActors(target, w, h, actors);
            backend.drawSurface(target, w, h, over.tower);
        };
        backend.foreground = [&](auto &target, double w, double h) {
            actors.clear();
            over.prepareForeground(actors);
            backend.drawActors(target, w, h, actors);
        };
        backend.text = [&](auto &grid) {
            grid.setSize(28, 36);
            grid.fontY = 108;
            grid.cls();
            grid.clattr();
            for (const auto &row : over.hudRows) {
                grid.printColour = row.colour;
                if (row.x < 0)
                    grid.cPrintAt(row.row, row.text.c_str());
                else
                    grid.printAt(row.x, row.row, row.text.c_str());
            }
        };
        backend.feedbackState = {};
        display.beginFrame();
        backend.execute(over.random);
        backend.present();
        capture(scratch / "goat-over-lifetime.png", display.surface());
        backend.release();
        backend.release();
        host.saves = scratch / "core-save";
        std::filesystem::create_directories(host.saves);
        std::unique_ptr<mino::Core> core(mino::kGoatUpCore.create());
        check(core->init(host, display.surface()), "Playable core init");
        display.beginFrame();
        core->frame(1. / 60);
        capture(scratch / "frontend-loading.png", display.surface());
        SDL_Delay(1050);
        for (int n = 0; n < 23; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(scratch / "frontend-title.png", display.surface());
        mino::PadState pad;
        pad.connected = true;
        uint32_t previous = 0;
        bool audible = false;
        const auto press = [&](uint32_t button) {
            pad.buttons = pad.pressed = button;
            core->pads(&pad, 1);
            pad.buttons = pad.pressed = 0;
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
        };
        press(mino::PAD_UP);
        press(mino::PAD_A);
        capture(scratch / "frontend-touch-control-b.png", display.surface());
        press(mino::PAD_A);
        capture(scratch / "frontend-touch-control-a.png", display.surface());
        press(mino::PAD_UP);
        press(mino::PAD_A);
        capture(scratch / "frontend-info-1.png", display.surface());
        press(mino::PAD_RIGHT);
        capture(scratch / "frontend-info-right-focus.png", display.surface());
        press(mino::PAD_LEFT);
        capture(scratch / "frontend-info-left-focus.png", display.surface());
        for (int page = 2; page <= 6; ++page) {
            press(mino::PAD_RIGHT);
            press(mino::PAD_A);
            capture(scratch / ("frontend-info-" + std::to_string(page) + ".png"),
                    display.surface());
        }
        pad.buttons = pad.pressed = mino::PAD_B;
        core->pads(&pad, 1);
        pad.pressed = 0;
        for (int tick = 0; tick < 8; ++tick) {
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
        }
        check(!host.quit, "Held Back leaked through info-to-title transition");
        pad.buttons = 0;
        core->pads(&pad, 1);
        press(mino::PAD_A);
        std::vector<float> sound(1470);
        for (int tick = 0; tick < 240; ++tick) {
            pad.lx = tick > 35 && tick < 70 ? .4f : 0;
            pad.buttons = tick == 90 || tick == 110                 ? mino::PAD_START
                          : (tick < 90 || tick > 110 && tick < 180) ? mino::PAD_A
                                                                    : 0;
            pad.pressed = pad.buttons & ~previous;
            previous = pad.buttons;
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            core->mixAudio(sound.data(), 735);
            for (float sample : sound) {
                check(std::isfinite(sample) && sample >= -1 && sample <= 1, "Invalid mixed audio");
                audible |= sample != 0;
            }
            if (tick == 60)
                capture(scratch / "playable-climb.png", display.surface());
            if (tick == 0)
                capture(scratch / "playable-pause-opacity-start.png", display.surface());
            if (tick == 85)
                capture(scratch / "playable-pause-opacity-faded.png", display.surface());
            if (tick == 95)
                capture(scratch / "playable-pause.png", display.surface());
        }
        check(!host.coreError, "Playable loop reached an untranslated dependency");
        check(audible, "Original gameplay samples never reached the audio mixer");
        core->save();
        core->save();
        core->shutdown();
        check(std::filesystem::exists(host.saves / "goatup-records.txt"),
              "Local records were not written");
        check(core->init(host, display.surface()), "Playable core reload");
        check(glGetError() == GL_NO_ERROR, "Reload initialization GL error");
        SDL_Delay(1050);
        for (int n = 0; n < 23; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        core->touch({mino::TouchPhase::Down, 99, float(display.surface().width * .5),
                     float(display.surface().height * .78)});
        core->touch({mino::TouchPhase::Up, 99, float(display.surface().width * .5),
                     float(display.surface().height * .78)});
        pad = {};
        pad.connected = true;
        core->pads(&pad, 1);
        core->touch({mino::TouchPhase::Down, 1, float(display.surface().width * .5),
                     float(display.surface().height * .45)});
        core->touch({mino::TouchPhase::Move, 1, float(display.surface().width * .6),
                     float(display.surface().height * .45)});
        core->touch({mino::TouchPhase::Move, 1, float(display.surface().width * .7),
                     float(display.surface().height * .45)});
        core->touch({mino::TouchPhase::Down, 2, float(display.surface().width * .85),
                     float(display.surface().height * .85)});
        for (int i = 0; i < 45; ++i) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(scratch / "playable-touch.png", display.surface());
        core->touch({mino::TouchPhase::Cancel, 1, 0, 0});
        core->back();
        display.beginFrame();
        core->frame(1. / 60);
        pad.ly = 1;
        for (int i = 0; i < 8; ++i) {
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
        }
        pad.ly = 0;
        core->pads(&pad, 1);
        press(mino::PAD_A);
        capture(scratch / "frontend-paused-stats.png", display.surface());
        press(mino::PAD_B);
        for (int n = 0; n < 2; ++n) {
            pad.ly = 0;
            core->pads(&pad, 1);
            pad.ly = 1;
            core->pads(&pad, 1);
        }
        pad.ly = 0;
        core->pads(&pad, 1);
        press(mino::PAD_A);
        check(host.quit, "Analog pause navigation did not reach Exit after returning from Stats");
        core->shutdown();
        check(!host.coreError && glGetError() == GL_NO_ERROR, "Core lifecycle/render error");
        host.hasSensor = true;
        host.quit = false;
        check(core->init(host, display.surface()), "Motion-capable core init");
        check(!host.motionEnabled, "Touch A should not leave the accelerometer powered on");
        SDL_Delay(1050);
        for (int n = 0; n < 23; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_UP);
        press(mino::PAD_A); // A -> B, left jump region.
        press(mino::PAD_START);
        core->touch({mino::TouchPhase::Down, 31, float(display.surface().width * .4),
                     float(display.surface().height * .6)});
        core->touch({mino::TouchPhase::Move, 31, float(display.surface().width * .55),
                     float(display.surface().height * .6)});
        core->touch({mino::TouchPhase::Down, 32, float(display.surface().width * .15),
                     float(display.surface().height * .15)});
        for (int n = 0; n < 60; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(scratch / "playable-touch-b.png", display.surface());
        core->touch({mino::TouchPhase::Cancel, 31, 0, 0});
        core->back();
        press(mino::PAD_DOWN);
        press(mino::PAD_DOWN);
        press(mino::PAD_A);
        press(mino::PAD_UP);
        press(mino::PAD_A); // B -> C, now the device has a sensor.
        check(host.motionEnabled, "Selecting C did not acquire the accelerometer");
        capture(scratch / "frontend-control-c.png", display.surface());
        press(mino::PAD_START);
        core->touch({mino::TouchPhase::Down, 8, float(display.surface().width * .5),
                     float(display.surface().height * .5)});
        const auto reads = host.motionReads;
        for (int n = 0; n < 120; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        check(host.motionReads >= reads + 120, "Core did not read canonical accelerometer samples");
        capture(scratch / "playable-tilt.png", display.surface());
        core->touch({mino::TouchPhase::Cancel, 8, 0, 0});
        core->shutdown();
        check(!host.motionEnabled && !host.coreError && glGetError() == GL_NO_ERROR,
              "Motion lifecycle did not release cleanly");

        // Capability-gated control selection must never turn A on the control
        // box into Start. Keep this independent of the rendered screenshot.
        goatup::Frontend menu;
        menu.capabilities(true, false);
        menu.title();
        menu.command(mino::PAD_UP, false);
        check(menu.selection == 1, "Control row navigation");
        check(menu.command(mino::PAD_A, false) == goatup::Frontend::Action::Changed &&
                  menu.control == 1 && menu.selection == 1,
              "Control A must cycle touch layout");
        menu.command(mino::PAD_DOWN, false);
        check(menu.selection == 2 &&
                  menu.command(mino::PAD_A, false) == goatup::Frontend::Action::Start,
              "Start row must be selectable");
        menu.capabilities(false, false);
        menu.title();
        menu.command(mino::PAD_UP, false);
        check(menu.command(mino::PAD_A, false) == goatup::Frontend::Action::None,
              "Absent control hardware must not start a game");
        menu.capabilities(false, true);
        check(menu.control == 2, "Sensor-only configuration selects available tilt");
        menu.capabilities(true, false);
        check(menu.control == 1 && menu.preferredControl == 1,
              "Touch B returns after a temporary capability change");
        host.hasSensor = host.hasTouch = false;
        host.saves = scratch / "no-touch-or-tilt";
        std::filesystem::create_directories(host.saves);
        check(core->init(host, display.surface()), "Controller-only core init");
        SDL_Delay(1050);
        for (int n = 0; n < 23; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_UP);
        press(mino::PAD_A);
        capture(scratch / "frontend-no-touch-or-tilt.png", display.surface());
        press(mino::PAD_DOWN);
        press(mino::PAD_A);
        core->back();
        display.beginFrame();
        core->frame(1. / 60);
        capture(scratch / "pause-controller-only.png", display.surface());
        core->shutdown();
        host.saves = scratch / "completed-records";
        std::filesystem::create_directories(host.saves);
        const auto recordsPath = host.saves / "goatup-records.txt";
        {
            std::ofstream file(recordsPath);
            file << "goatup-records 1\n900 0 0 0\n";
            for (int n = 0; n < 16; ++n)
                file << "0 ";
            file << "\n700 0 1\n";
        }
        const auto savedTail = [&] {
            std::ifstream file(recordsPath);
            std::string tag;
            int version = 0, best = 0, unique = 0, billies = 0, kids = 0;
            file >> tag >> version >> best >> unique >> billies >> kids;
            unsigned word = 0;
            for (int n = 0; n < 16; ++n)
                file >> word;
            std::array<int, 3> tail{};
            file >> tail[0] >> tail[1] >> tail[2];
            check(bool(file), "Invalid regression save");
            return tail;
        };
        host.hasTouch = false;
        host.hasSensor = false;
        host.quit = false;
        check(core->init(host, display.surface()), "Completed-record fixture init");
        core->save();
        check(savedTail()[0] == 700 && savedTail()[2] == 1,
              "Temporary touch loss changed stored mode or completed score");
        SDL_Delay(1050);
        for (int n = 0; n < 23; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        pad = {};
        pad.connected = true;
        previous = 0;
        press(mino::PAD_START);
        for (int n = 0; n < 1000; ++n) {
            pad.buttons = n < 100 ? mino::PAD_A : 0;
            pad.pressed = pad.buttons & ~previous;
            previous = pad.buttons;
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            if (n == 90) {
                core->save();
                check(savedTail()[0] == 700,
                      "Lifecycle save overwrote Last Game with an unfinished score");
            }
        }
        check(savedTail()[0] != 700 && savedTail()[0] >= 0,
              "Completed run did not update Last Game");
        press(mino::PAD_A);
        for (int n = 0; n < 22; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_B);
        check(host.quit, "Game-over transition did not return to a working title");
        core->shutdown();

        host.saves = scratch / "invalid-records";
        std::filesystem::create_directories(host.saves);
        const auto invalidPath = host.saves / "goatup-records.txt";
        const std::string invalid = "goatup-records 1\n0 -1 0 0\n0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
        {
            std::ofstream f(invalidPath);
            f << invalid;
        }
        check(core->init(host, display.surface()), "Invalid records should not prevent startup");
        core->save();
        core->shutdown();
        {
            std::ifstream f(invalidPath);
            const std::string unchanged((std::istreambuf_iterator<char>(f)), {});
            check(unchanged == invalid, "Invalid original record was overwritten");
        }

        display.shutdown();
        SDL_GL_DestroyContext(gl);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "Original GoatUp resource/effect path, background generation boundary and "
                     "retained redraw rendered successfully\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
