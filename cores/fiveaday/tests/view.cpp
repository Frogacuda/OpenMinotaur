#include "backend.h"
#include "records.h"
#include "achievements.h"
#include "highscores.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cstdarg>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
namespace mino {
extern const CoreEntry kFiveADayCore;
}
namespace {
void finishStartup(mino::Core &core, mino::Display &display) {
    display.beginFrame();
    core.frame(0);
    SDL_Delay(1010);
    display.beginFrame();
    core.frame(1. / 60);
}

void check(bool ok, const char *text) {
    if (!ok)
        throw std::runtime_error(text);
}
struct Host : mino::Host {
    std::filesystem::path assets, saves;
    std::string blocked;
    mino::Display *display = nullptr;
    bool error = false, quit = false, motionAvailable = false, motionEnabled = false;
    mutable int motionReads = 0;
    mino::Achievements awards;
    void unlockAchievement(const std::string &id) override {
        awards.unlock("fiveaday", id);
    }
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        if (name == blocked)
            return false;
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    std::string saveDir() const override {
        return saves.u8string();
    }
    void log(const char *fmt, ...) override {
        va_list a;
        va_start(a, fmt);
        char text[2048];
        std::vsnprintf(text, sizeof text, fmt, a);
        va_end(a);
        std::cerr << text << '\n';
        if (std::string(text).find("checkpoint error") != std::string::npos)
            error = true;
    }
    std::string adaptShader(const std::string &source, int stage) const override {
        return display->adaptShader(source, stage);
    }
    bool enableMotion(bool enabled) override {
        motionEnabled = enabled && motionAvailable;
        return motionEnabled;
    }
    mino::MotionState motion() const override {
        ++motionReads;
        return {motionEnabled, .35, -.2, 1};
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {
        quit = true;
    }
};
void capture(const std::filesystem::path &path, mino::SurfaceInfo surface, bool fullScene = true) {
    std::vector<uint8_t> pixels(size_t(surface.width) * surface.height * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
    glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    check(glGetError() == GL_NO_ERROR, "GL readback");
    size_t lit = 0;
    for (size_t at = 0; at < pixels.size(); at += 4)
        if (pixels[at] || pixels[at + 1] || pixels[at + 2])
            ++lit;
    check(fullScene ? lit > size_t(surface.width) * surface.height / 10 : lit > 0,
          "Missing game presentation");
    stbi_flip_vertically_on_write(1);
    check(stbi_write_png(path.string().c_str(), surface.width, surface.height, 4, pixels.data(),
                         surface.width * 4) != 0,
          "PNG output");
}
} // namespace
int main(int argc, char **argv) {
    try {
        check(argc == 3, "Supply game assets and scratch directory");
        const auto out = std::filesystem::u8path(argv[2]);
        std::filesystem::create_directories(out);
        check(SDL_Init(SDL_INIT_VIDEO), "SDL video");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        auto *window = SDL_CreateWindow("Five a Day integration", 480, 640,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        check(window, "Window");
        auto context = SDL_GL_CreateContext(window);
        check(context, "Context");
        check(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)), "GL loader");
        mino::Display display;
        check(display.init(false), "Display");
        mino::DisplaySettings settings;
        settings.internalScale = 1;
        display.configure(480, 640, {320, 480, .75f, .75f}, settings);
        Host host;
        host.assets = std::filesystem::u8path(argv[1]);
        host.display = &display;
        host.saves = out / "saves";
        std::filesystem::create_directories(host.saves);
        // The later selector fixture unlocks day two. Reset only this harness's
        // named records file so rerunning in the same scratch directory is stable.
        fiveaday::Records initialRecords;
        initialRecords.days[0].score = 12345;
        initialRecords.fruit = 10001;
        initialRecords.minotaurs = 1001;
        initialRecords.dirty = true;
        check(initialRecords.save((host.saves / "fiveaday-records.txt").u8string()),
              "Initial frontend fixture records");
        mino::HighScores initialScores;
        initialScores.init(1, {"STINKYOX"}, 1000, 100);
        check(initialScores.save((host.saves / "fiveaday-scores.txt").u8string()),
              "Initial local-score fixture");
        for (const char *missing : {"NeonResources.xml", "testGrid.ox", "grunt2.wav"}) {
            host.blocked = missing;
            std::unique_ptr<mino::Core> failure(mino::kFiveADayCore.create());
            check(!failure->init(host, display.surface()), "Missing asset fails cleanly");
            failure->shutdown();
            failure->shutdown();
        }
        host.blocked.clear();
        std::unique_ptr<mino::Core> core(mino::kFiveADayCore.create());
        check(core->init(host, display.surface()), "Core initialization");
        finishStartup(*core, display);
        display.beginFrame();
        core->frame(1. / 60);
        capture(out / "title.png", display.surface());
        check(host.awards.entries().empty(), "Eligible saved totals do not award in attract mode");
        auto press = [&](uint32_t key) {
            mino::PadState p;
            p.connected = true;
            core->pads(&p, 1);
            p.buttons = p.pressed = key;
            core->pads(&p, 1);
            p.pressed = 0;
            core->pads(&p, 1);
        };
        press(mino::PAD_A);
        std::vector<float> audio(1470);
        bool audible = false;
        for (int tick = 0; tick < 8000; ++tick) {
            mino::PadState pad;
            pad.connected = true;
            pad.lx = float(std::sin(double(tick) * .01));
            pad.ly = float(-std::cos(double(tick) * .01));
            core->pads(&pad, 1);
            display.beginFrame();
            core->frame(1. / 60);
            core->mixAudio(audio.data(), 735);
            for (float sample : audio) {
                check(std::isfinite(sample) && std::abs(sample) <= 1.f, "Audio bounds");
                audible |= sample != 0;
            }
            if (tick == 1)
                capture(out / "entering.png", display.surface());
            if (tick == 650)
                capture(out / "attackers.png", display.surface());
            if (tick == 1800)
                capture(out / "fruit-carriers.png", display.surface());
            if (tick == 5000)
                capture(out / "stage-bonus.png", display.surface());
            if (tick == 7000)
                capture(out / "next-stage.png", display.surface());
            check(!host.error, "Core checkpoint failure");
        }
        check(audible, "Imported source samples");
        // The moving 8000-frame render route can survive; continue without player
        // steering until an actual run ends, rather than assuming it already did.
        int bankingFrames = 0;
        for (; bankingFrames < 60000 && !host.awards.contains("fiveaday", "original-8");
             ++bankingFrames) {
            mino::PadState idle;
            idle.connected = true;
            core->pads(&idle, 1);
            display.beginFrame();
            core->frame(1. / 60);
            check(!host.error, "Lifetime banking route");
        }
        std::cout << "Lifetime banking route: " << bankingFrames << " additional frames\n";
        check(host.awards.contains("fiveaday", "original-7") &&
                  host.awards.contains("fiveaday", "original-8"),
              "Completed run reports both lifetime awards through Host");
        const auto awardPath = (out / "achievements.txt").u8string();
        check(host.awards.save(awardPath), "Shared achievement store saves repaired awards");
        mino::Achievements reopened;
        check(reopened.load(awardPath) && reopened.contains("fiveaday", "original-7") &&
                  reopened.contains("fiveaday", "original-8"),
              "Both repaired awards survive collection reload");
        // Continue the same completed run through name entry, automatic Done at
        // the eight-character limit, persistence and the scores/title return route.
        for (int n = 0; n < 110; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_A);
        for (int n = 0; n < 8; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "name-entry.png", display.surface());
        for (int n = 0; n < 8; ++n)
            press(mino::PAD_A);
        press(mino::PAD_A);
        for (int n = 0; n < 8; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "submitted-score.png", display.surface());
        mino::HighScores named;
        named.init(1, {"TEST"}, 1000, 100);
        check(named.load((host.saves / "fiveaday-scores.txt").u8string()) &&
                  named.lastName == "AAAAAAAA",
              "Eight-character entry automatically selects Done and persists");
        press(mino::PAD_B);
        host.quit = false;
        press(mino::PAD_B);
        check(host.quit, "Scores return through title to launcher");
        core->shutdown();
        check(core->init(host, display.surface()), "Frontend restart");
        finishStartup(*core, display);
        press(mino::PAD_A);
        for (int n = 0; n < 360; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        const auto surface = display.surface();
        core->touch({mino::TouchPhase::Down, 99, surface.width * .5f, surface.height * .15f});
        display.beginFrame();
        core->frame(1. / 60);
        capture(out / "touch-faded-pause.png", display.surface());
        core->touch({mino::TouchPhase::Up, 99, surface.width * .5f, surface.height * .15f});
        mino::PadState pad;
        pad.connected = true;
        pad.buttons = pad.pressed = mino::PAD_B;
        core->pads(&pad, 1); // resume the invisible touch hotspot's pause
        pad = {};
        pad.connected = true;
        core->pads(&pad, 1);
        pad.buttons = pad.pressed = mino::PAD_START;
        core->pads(&pad, 1);
        for (int n = 0; n < 24; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "pause.png", display.surface());
        pad.pressed = 0;
        for (int n = 0; n < 6; ++n) {
            core->pads(&pad, 1);
            core->frame(1. / 60);
        } // held Start must not close it
        pad = {};
        pad.connected = true;
        core->pads(&pad, 1);
        pad.ly = 1;
        core->pads(&pad, 1);
        pad.ly = 0;
        core->pads(&pad, 1);
        pad.ly = 1;
        core->pads(&pad, 1);
        pad.ly = 0;
        pad.buttons = pad.pressed = mino::PAD_A;
        core->pads(&pad, 1);
        display.beginFrame();
        core->frame(1. / 60);
        capture(out / "returned-title.png", display.surface());
        press(mino::PAD_B);
        check(host.quit, "Analog-only title selection then B exits to launcher");
        // Exit is a separate action from Title, by controller and by touch.
        host.quit = false;
        press(mino::PAD_A);
        press(mino::PAD_A);
        for (int n = 0; n < 12; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_START);
        for (int n = 0; n < 3; ++n) {
            mino::PadState move;
            move.connected = true;
            core->pads(&move, 1);
            move.ly = 1;
            core->pads(&move, 1);
        }
        press(mino::PAD_A);
        check(host.quit, "Controller Exit returns directly to iOX");
        host.quit = false;
        core->touch({mino::TouchPhase::Down, 88, surface.width * .5f, surface.height * .70f});
        core->touch({mino::TouchPhase::Up, 88, 0, 0});
        check(host.quit, "Exit sprite touch target returns directly to iOX");
        core->shutdown();
        core->shutdown();
        check(core->init(host, display.surface()), "Reopen after resource release");
        finishStartup(*core, display);
        display.beginFrame();
        core->frame(1. / 60);
        check(!host.error, "Reopened game");
        for (int n = 1; n <= 2910; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
            if (n == 600 || n == 1450 || n == 2100 || n == 2450 || n == 2910)
                capture(out / ("attract-" + std::to_string(n) + ".png"), display.surface());
            check(!host.error, "Complete attract/tutorial/credits loop");
        }
        core->shutdown();
        core.reset();
        // Controlled family render fixture: shorten the source missile countdown to
        // exercise its live birth/render path without claiming a reference timing.
        fiveaday::Backend backend;
        check(backend.init(host, display.surface()), "Boss family renderer");
        const auto *scoreButton = backend.resources().sprite(111);
        const auto *titleButton = backend.resources().sprite(112);
        check(titleButton && titleButton->page == 2, "Title sprite resource exists");
        const auto *scorePage = backend.resources().page(2);
        check(scoreButton && scoreButton->page == 2 && scorePage && scorePage->w == 512 &&
                  scorePage->h == 512,
              "Score button has a valid sprite and UV page");
        fiveaday::World game;
        game.spriteSize = [&](int frame) {
            const auto *sprite = backend.resources().sprite(frame + 2);
            check(sprite, "Source boss frame");
            return fiveaday::Vec2{double(sprite->w), double(sprite->h)};
        };
        game.start();
        game.scriptCursor = game.script.size();
        game.ship.state = fiveaday::Ship::State::Flying;
        game.ship.position = {};
        game.ship.fireTicks = game.ship.fireInterval = 100000;
        game.currentLaunch = game.launch();
        auto *boss = new fiveaday::Boss(game, game.currentLaunch, {false});
        boss->position = boss->retained = {.3, -.25};
        boss->missileTicks = 0;
        boss->missileChance = 512;
        game.add(game.enemies, boss);
        game.bossLaunched = true;
        std::vector<fiveaday::ActorDraw> actors;
        backend.background = [](auto &, double, double) {};
        backend.foreground = [&](auto &target, double width, double height) {
            game.prepareForeground(actors);
            actors.insert(actors.begin(), {{0, 0}, {1, 1}, .5f, 0, 0, fiveaday::unpack(game.sky)});
            backend.drawActors(target, width, height, actors);
        };
        backend.text = [&](auto &grid) {
            grid.setSize(29, 32);
            grid.cls();
            grid.clattr();
            for (const auto &m : game.messages)
                if (!m.text.empty()) {
                    const auto byte = [](double v) {
                        return uint32_t(std::clamp(v, 0., 1.) * 255.);
                    };
                    grid.cPrintAt(m.row, m.text.c_str(),
                                  (byte(m.colour.r) << 24) | (byte(m.colour.g) << 16) |
                                      (byte(m.colour.b) << 8) | byte(m.colour.a));
                }
        };
        bool missileSeen = false;
        for (int tick = 0; tick < 120; ++tick) {
            game.tick({});
            for (auto *e = game.enemies; e; e = e->next)
                missileSeen |= dynamic_cast<fiveaday::HomingMissile *>(e) != nullptr;
            display.beginFrame();
            backend.execute(game.random);
            backend.present();
            if (tick == 40)
                capture(out / "boss-missiles.png", display.surface());
        }
        check(missileSeen, "Live boss missile birth");
        game.start();
        game.scriptCursor = game.script.size();
        game.ship.state = fiveaday::Ship::State::Bonus;
        game.ship.collected = 6;
        game.bossLaunched = game.bossDefeated = true;
        fiveaday::Attacker carrier(game);
        carrier.carrier = true;
        for (int n = 0; n < 5; ++n) {
            auto *f = new fiveaday::Fruit(game, carrier);
            f->state = fiveaday::Fruit::State::Collected;
            game.add(game.fruits, f);
        }
        game.beginBonus();
        for (int tick = 0; tick < 1000; ++tick) {
            game.tick({});
            display.beginFrame();
            backend.execute(game.random);
            backend.present();
            if (tick == 250)
                capture(out / "fruit-tally.png", display.surface());
            if (tick == 800)
                capture(out / "sun-passage.png", display.surface());
        }
        game.start();
        game.newLevel();
        game.scriptCursor = game.script.size();
        game.ship.state = fiveaday::Ship::State::Flying;
        game.ship.position = {};
        auto p = game.launch();
        p.position = {-.2, -.2};
        p.heading = 0;
        game.add(game.enemies, new fiveaday::Bomber(game, p, {0, 0, 0, 0}));
        p.position = {.2, -.2};
        game.add(game.enemies, new fiveaday::MinotaurKiller(game, p));
        for (int tick = 0; tick < 120; ++tick) {
            game.tick({});
            display.beginFrame();
            backend.execute(game.random);
            backend.present();
            if (tick == 40)
                capture(out / "bomber-minotaur.png", display.surface());
        }
        for (int era = 0; era < 6; ++era) {
            game.start(era);
            game.scriptCursor = game.script.size();
            game.ship.state = fiveaday::Ship::State::Flying;
            game.month = float(era);
            game.week = .6f;
            auto launch = game.launch();
            launch.position = {.25, -.2};
            auto *miner = new fiveaday::Miner(game, launch);
            game.add(game.enemies, miner);
            launch.position = {-.25, -.2};
            game.add(game.enemies, new fiveaday::Jellyfish(game, launch));
            launch.position = {-.25, .2};
            game.add(game.enemies, new fiveaday::PinkPlane(game, launch));
            launch.position = {.25, .2};
            game.add(game.enemies, new fiveaday::DecoyCloud(game, launch));
            auto *eraBoss = new fiveaday::Boss(game, launch, {false});
            eraBoss->position = eraBoss->retained = {0, -.25};
            game.add(game.enemies, eraBoss);
            game.bossLaunched = true;
            for (int n = 0; n < 90; ++n) {
                game.ship.integrity = 1;
                game.tick({});
                display.beginFrame();
                backend.execute(game.random);
                backend.present();
            }
            capture(out / ("era-" + std::to_string(era) + ".png"), display.surface());
        }
        game.goGameOver();
        for (int n = 0; n < 1000; ++n) {
            game.tick({});
            display.beginFrame();
            backend.execute(game.random);
            backend.present();
        }
        capture(out / "game-over.png", display.surface());
        // Reuse the established proportional-line path with Five's extracted tables.
        backend.text = [&](auto &grid) {
            if (grid.cols != 29 || grid.rowsN != 32)
                grid.setSize(29, 32);
            grid.cls();
            grid.clattr();
            if (!grid.lines[11]->text)
                grid.QuickMsg("FRESH!", {0, 0}, {1.2, 2.4}, 4, 11, 3, 1, .5);
        };
        for (int n = 0; n < 80; ++n) {
            display.beginFrame();
            backend.execute(game.random);
            backend.present();
        }
        capture(out / "achievement-text.png", display.surface());
        // Compare rendered brightness, not just stored alpha: Five selects standard
        // alpha for the grid, unlike the inherited default premultiplied path.
        uint32_t textColour = 0xffffffffu;
        backend.foreground = [](auto &, double, double) {};
        backend.text = [&](auto &grid) {
            check(grid.fontY == 54 && grid.straightAlphaText,
                  "Five original font row and blend overrides");
            grid.clm();
            grid.cls();
            grid.clattr();
            grid.cPrintAt(10, "FIVE A DAY", textColour);
        };
        auto textBrightness = [&]() {
            for (int n = 0; n < 3; ++n) {
                display.beginFrame();
                backend.execute(game.random);
                backend.present();
            }
            const auto sf = display.surface();
            std::vector<uint8_t> pixels(size_t(sf.width) * sf.height * 4);
            glBindFramebuffer(GL_FRAMEBUFFER, sf.framebuffer);
            glReadPixels(0, 0, sf.width, sf.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            uint64_t value = 0;
            for (size_t i = 0; i < pixels.size(); i += 4)
                value += pixels[i] + pixels[i + 1] + pixels[i + 2];
            return value;
        };
        const auto opaque = textBrightness();
        capture(out / "original-font.png", display.surface(), false);
        textColour = 0xffffff33u;
        const auto faded = textBrightness();
        textColour = 0xffffff00u;
        const auto invisible = textBrightness();
        std::cout << "Text brightness: opaque=" << opaque << " faded=" << faded
                  << " invisible=" << invisible << "\n";
        check(opaque > faded * 2 && faded > invisible,
              "Text alpha reduces actual rendered brightness");
        backend.release();
        // Seed a legitimate unlocked day and test the real selector, including a
        // held confirm across entry and an analog direction edge.
        fiveaday::Records dayRecords;
        dayRecords.checkpoint(1, 4, 12345);
        check(dayRecords.save((host.saves / "fiveaday-records.txt").u8string()),
              "Day selector fixture record");
        core.reset(mino::kFiveADayCore.create());
        // A single held gesture must remain on day selection. Two B presses should
        // then return to title and iOX; if it started gameplay they pause/resume.
        for (bool touch : {false, true}) {
            host.quit = false;
            check(core->init(host, display.surface()), "Held-input selector regression");
            finishStartup(*core, display);
            const auto sf = display.surface();
            if (touch)
                core->touch({mino::TouchPhase::Down, 71, sf.width * .5f, sf.height * .72f});
            for (int n = 0; n < 120; ++n) {
                if (touch) {
                    core->touch({mino::TouchPhase::Down, 71, sf.width * .5f, sf.height * .72f});
                    core->touch({mino::TouchPhase::Move, 71, sf.width * .5f, sf.height * .72f});
                    if (n == 1)
                        core->touch({mino::TouchPhase::Down, 72, sf.width * .5f, sf.height * .72f});
                } else {
                    mino::PadState held;
                    held.connected = true;
                    held.buttons = held.pressed = mino::PAD_A | mino::PAD_START;
                    core->pads(&held, 1);
                }
                display.beginFrame();
                core->frame(1. / 60);
            }
            capture(out / (touch ? "held-touch-selector.png" : "held-pad-selector.png"), sf);
            if (touch) {
                core->touch({mino::TouchPhase::Up, 71, 0, 0});
                core->touch({mino::TouchPhase::Up, 72, 0, 0});
            }
            press(mino::PAD_B);
            check(!host.quit, "Selector Back returns to title first");
            press(mino::PAD_B);
            check(host.quit, "Held start did not skip selector");
            core->shutdown();
        }
        host.quit = false;
        check(core->init(host, display.surface()), "Saved-day frontend");
        finishStartup(*core, display);
        press(mino::PAD_A);
        for (int n = 0; n < 10; ++n) {
            mino::PadState held;
            held.connected = true;
            held.buttons = mino::PAD_A;
            core->pads(&held, 1);
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "day-selector.png", display.surface());
        mino::PadState stick;
        stick.connected = true;
        stick.lx = 1;
        core->pads(&stick, 1);
        display.beginFrame();
        core->frame(1. / 60);
        capture(out / "day-two.png", display.surface());
        press(mino::PAD_A);
        for (int n = 0; n < 350; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "saved-day-play.png", display.surface());
        press(mino::PAD_START);
        stick = {};
        stick.connected = true;
        core->pads(&stick, 1);
        stick.ly = 1;
        core->pads(&stick, 1);
        press(mino::PAD_A);
        display.beginFrame();
        core->frame(1. / 60);
        capture(out / "local-scores.png", display.surface());
        press(mino::PAD_B);
        // Resume, Title and Exit have distinct routes and matching sprite targets.
        core->touch({mino::TouchPhase::Down, 81, surface.width * .5f, surface.height * .15f});
        core->touch({mino::TouchPhase::Up, 81, 0, 0});
        press(mino::PAD_START);
        for (int n = 0; n < 24; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        core->touch({mino::TouchPhase::Down, 82, surface.width * .5f, surface.height * .535f});
        core->touch({mino::TouchPhase::Up, 82, 0, 0});
        press(mino::PAD_B);
        check(host.quit, "Title sprite returns to title before B exits to launcher");
        core->shutdown();
        core.reset();
        // Exercise the optional sensor path through the real Core/Host boundary.
        // This is simulated capability/lifecycle coverage, not physical calibration.
        host.saves = out / "motion-saves";
        std::filesystem::create_directories(host.saves);
        fiveaday::Records touchDefault;
        touchDefault.dirty = true;
        check(touchDefault.save((host.saves / "fiveaday-records.txt").u8string()),
              "Touch-default sensor fixture");
        host.motionAvailable = true;
        host.motionReads = 0;
        core.reset(mino::kFiveADayCore.create());
        check(core->init(host, display.surface()), "Sensor-capable frontend");
        finishStartup(*core, display);
        check(!host.motionEnabled, "Sensor capability does not select tilt by default");
        press(mino::PAD_A);
        for (int n = 0; n < 12; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        press(mino::PAD_START);
        for (int n = 0; n < 3; ++n) {
            mino::PadState move;
            move.connected = true;
            core->pads(&move, 1);
            move.ly = 1;
            core->pads(&move, 1);
        }
        press(mino::PAD_A);
        check(host.motionEnabled, "Controller selects original tilt control button");
        for (int n = 0; n < 30; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        capture(out / "motion-pause.png", display.surface());
        check(host.motionReads > 0 && !host.error,
              "Core consumes simulated gravity without errors");
        press(mino::PAD_B);
        const int readsBeforePlay = host.motionReads;
        for (int n = 0; n < 60; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        check(host.motionReads > readsBeforePlay && !host.error,
              "Playing samples simulated gravity");
        press(mino::PAD_START);
        core->touch({mino::TouchPhase::Down, 91, surface.width * .5f, surface.height * .70f});
        core->touch({mino::TouchPhase::Up, 91, 0, 0});
        check(!host.motionEnabled, "Original control artwork touch target selects touch");
        press(mino::PAD_A);
        check(host.motionEnabled, "Controller can restore tilt after touch selection");
        // With a sensor, the next visible option after Touch/Tilt is still Exit.
        host.quit = false;
        press(mino::PAD_DOWN);
        press(mino::PAD_A);
        check(host.quit, "Exit is last in sensor-enabled controller navigation");
        host.quit = false;
        core->touch({mino::TouchPhase::Down, 92, surface.width * .5f, surface.height * .865f});
        core->touch({mino::TouchPhase::Up, 92, 0, 0});
        check(host.quit, "Sensor-enabled bottom Exit touch target");
        core->pause(true);
        check(!host.motionEnabled, "Suspend disables sensor");
        core->pause(false);
        check(host.motionEnabled, "Resume restores selected sensor mode");
        core->shutdown();
        check(!host.motionEnabled, "Shutdown disables sensor");
        fiveaday::Records tiltSaved;
        check(tiltSaved.load((host.saves / "fiveaday-records.txt").u8string()) && tiltSaved.tilt,
              "Tilt selection persisted");
        host.motionAvailable = false;
        host.motionReads = 0;
        check(core->init(host, display.surface()), "Reopen tilt preference without sensor");
        finishStartup(*core, display);
        press(mino::PAD_A);
        for (int n = 0; n < 30; ++n) {
            display.beginFrame();
            core->frame(1. / 60);
        }
        check(!host.motionEnabled && host.motionReads == 0,
              "Unavailable sensor falls back to touch/direct controls");
        core->shutdown();
        core.reset();
        check(glGetError() == GL_NO_ERROR, "Native GL error");
        display.shutdown();
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "Five a Day native core: 8000 steps plus bonus/enemy fixtures, source "
                     "assets/audio, controller pause/exit, resource reopen passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
