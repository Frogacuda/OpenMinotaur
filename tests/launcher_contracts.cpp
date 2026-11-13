// Exercises the production launcher through real input and a hidden GL surface.
// Scratch settings/assets stay under the caller-supplied directory.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "../third_party/stb/stb_image_write.h"
#include "launcher.h"
#include "display.h"
#include "game_catalog.h"
#include "gridrunner_choices.h"
#include "dock_style_previews.h"
#include "gear_icon_preview.h"
#include "launcher_transition.h"
#include "startup_screen.h"
#include "bundled_assets.h"
#include "achievement_catalog.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace mino {
struct LauncherChecks {
    using Screen = iox::Screen;
    static void settle(Launcher &l) {
        for (int i = 0; l.appLaunch_.active() && i < 20; ++i)
            l.frame(1.0 / 60);
        REQUIRE(!l.appLaunch_.active());
    }
    static std::vector<uint8_t> readFrame(unsigned framebuffer, int width, int height) {
        std::vector<uint8_t> pixels(size_t(width) * height * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }
    static void sameRGB(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
        REQUIRE(a.size() == b.size());
        int largest = 0;
        size_t changed = 0;
        for (size_t i = 0; i < a.size(); ++i)
            if (i % 4 != 3) {
                const int delta = std::abs(int(a[i]) - int(b[i]));
                largest = std::max(largest, delta);
                changed += delta > 1;
            }
        if (changed)
            std::cerr << "Handoff differs in " << changed << " channels, max " << largest << "\n";
        REQUIRE(changed == 0); // Alpha in a render target is not window opacity.
    }
    static void focusVisibility(Launcher &l, bool &keyboard) {
        l.returnHome();
        l.homeEntrance_.finish();
        l.achievementRecords_ = nullptr;
        const std::array<Screen, 9> pages = {
            Screen::Home,  Screen::Options,    Screen::Wallpapers,   Screen::Players,
            Screen::About, Screen::Gridrunner, Screen::Achievements, Screen::AchievementIndex,
            Screen::Quit};
        for (const auto page : pages) {
            const int initial =
                (page == Screen::Options || page == Screen::Wallpapers || page == Screen::Players)
                    ? iox::options::firstRow
                : page == Screen::Achievements     ? 4
                : page == Screen::AchievementIndex ? 9
                                                   : 0;
            if (page == Screen::Gridrunner)
                l.gameScreen_.game = 5;
            l.open(page, initial);
            l.homeEntrance_.finish();
            keyboard = false;
            l.pads(nullptr, 0);
            REQUIRE(!l.menuFocusVisible());
            l.frame(0);
            const auto plain =
                readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height);
            PadState pad{};
            pad.connected = true;
            l.pads(&pad, 1);
            REQUIRE(l.menuFocusVisible());
            l.frame(0);
            REQUIRE(plain !=
                    readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height));
            l.pads(nullptr, 0);
            REQUIRE(!l.menuFocusVisible());
            l.frame(0);
            sameRGB(plain, readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height));
            keyboard = true;
            REQUIRE(l.menuFocusVisible());
            REQUIRE(!l.controllerPresent_);
            l.frame(0);
            REQUIRE(plain !=
                    readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height));
            keyboard = false;
        }
        l.returnHome();
        l.homeEntrance_.finish();
    }
    static void notes(Launcher &l, const std::filesystem::path &scratch, bool portrait) {
        const std::string prefix = portrait ? "portrait-" : "landscape-";
        Achievements records;
        records.unlock("minorescue", "original-0");
        records.unlock("gridrunner", "original-0");
        l.setAchievements(records);
        l.returnHome();
        l.homeEntrance_.finish();
        click(l, iox::home::achievements);
        REQUIRE(l.page() == Screen::Achievements);
        l.noteGame_ = 0;
        l.noteScroll_ = 0;
        frame(l, scratch, (prefix + "achievements-unlocked.png").c_str());
        REQUIRE(l.noteLines_.size() > 10 && l.noteHeight_ > l.achievementBody().h);
        auto &surface = l.surface_;
        const auto before = readFrame(surface.framebuffer, surface.width, surface.height);
        Achievements empty;
        l.setAchievements(empty);
        l.frame(0);
        const auto locked = readFrame(surface.framebuffer, surface.width, surface.height);
        REQUIRE(before != locked);
        l.setAchievements(records);
        PadState analog{};
        analog.connected = true;
        analog.ly = .5f;
        l.pads(&analog, 1);
        l.frame(1. / 60);
        REQUIRE(l.noteScroll_ > 0 && l.noteScroll_ < 4);
        analog.ly = 0;
        l.pads(&analog, 1);
        l.noteScroll_ = l.noteHeight_ - l.achievementBody().h;
        l.focus() = 0;
        analog.ly = .5f;
        l.pads(&analog, 1);
        l.frame(1. / 60);
        REQUIRE(l.focus() == 2);
        analog.ly = 0;
        l.pads(&analog, 1);
        l.noteScroll_ = 0;
        l.focus() = 0;
        const iox::CanvasTransform canvas(l.layout_, surface.width, surface.height);
        const auto r = l.achievementBody();
        const float x = r.cx() * canvas.scaleX, y = (r.y + 50) * canvas.scaleY;
        l.touch({TouchPhase::Down, 99, x, y});
        l.touch({TouchPhase::Move, 99, x, y - 80 * canvas.scaleY});
        l.touch({TouchPhase::Up, 99, x, y - 80 * canvas.scaleY});
        REQUIRE(l.noteScroll_ > 70 && l.page() == Screen::Achievements);
        frame(l, scratch, (prefix + "achievements-scrolled.png").c_str());
        for (int i = 0; i < 9; ++i) {
            l.noteGame_ = i;
            l.noteScroll_ = 0;
            l.prepareAchievements();
            if (i == 8)
                REQUIRE(l.noteLines_.empty());
            else
                REQUIRE(!l.noteLines_.empty());
            frame(l, scratch,
                  (prefix + "achievements-" + kGameCatalog[size_t(i)].id + ".png").c_str());
        }
        for (int slot = 1; slot <= 4; ++slot) {
            l.focus() = slot;
            frame(l, scratch, (prefix + "notes-focus-" + std::to_string(slot) + ".png").c_str());
        }
        click(l, 3);
        REQUIRE(l.noteGame_ == 0);
        click(l, 1);
        REQUIRE(l.noteGame_ == 8);
        click(l, 2);
        REQUIRE(l.page() == Screen::AchievementIndex);
        frame(l, scratch, (prefix + "achievement-index.png").c_str());
        click(l, 5);
        REQUIRE(l.page() == Screen::Achievements &&
                std::string(kGameCatalog[size_t(l.noteGame_)].id) == "gridrunner" &&
                l.focus() == 0);
        frame(l, scratch, (prefix + "achievements-classic-unlocked.png").c_str());
        l.back();
        REQUIRE(l.page() == Screen::Home && l.focus() == iox::home::achievements);
        size_t count = 0;
        for (const auto &a : iox::achievementNotes) {
            REQUIRE(a.id >= 0 && a.id < 10);
            ++count;
        }
        REQUIRE(count == 79);
        l.achievementRecords_ = nullptr;
    }
    static void apps(Launcher &l, const std::filesystem::path &scratch, bool portrait) {
        const std::string prefix = portrait ? "portrait-" : "landscape-";
        for (int index : {iox::home::options, iox::home::achievements, iox::home::about}) {
            l.returnHome();
            l.frame(1);
            l.navigation_.homeFocus() = index;
            l.key(0, 0, true, false);
            REQUIRE(l.page() == Screen::Home && l.appLaunch_.active());
            REQUIRE(l.appLaunch_.progress() == 0 && iox::AppLaunch::imageScale(0) == .25f);
            const auto reference =
                readFrame(l.appFramebuffer_, l.surface_.width, l.surface_.height);
            l.key(0, 0, true, false);
            l.back();
            REQUIRE(l.page() == Screen::Home && l.appLaunch_.progress() == 0);
            REQUIRE(l.takeRequest().action == Launcher::Action::None);
            const std::string name =
                prefix + (index == iox::home::options        ? "launch-settings-"
                          : index == iox::home::achievements ? "launch-achievements-"
                                                             : "launch-about-");
            for (int i = 0; i <= 15; ++i) {
                char file[96];
                std::snprintf(file, sizeof file, "%s%02d.png", name.c_str(), i);
                frame(l, scratch, file, i == 15 ? 0 : 1.0 / 60);
            }
            REQUIRE(!l.appLaunch_.active());
            REQUIRE(l.page() == (index == iox::home::options        ? Screen::Options
                                 : index == iox::home::achievements ? Screen::Achievements
                                                                    : Screen::About));
            sameRGB(reference,
                    readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height));
            l.frame(0);
            sameRGB(reference,
                    readFrame(l.surface_.framebuffer, l.surface_.width, l.surface_.height));
            l.back();
            REQUIRE(l.page() == Screen::Home && l.focus() == index);
        }
        l.returnHome();
    }
    static void archiveLaunches(Launcher &l, Host &host, Display &display, SDL_Window *window,
                                const std::function<void(const std::string &)> &game) {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        const auto original = l.surface_;
        DisplaySettings shell;
        shell.internalScale = 0;
        shell.orientation = Orientation::Landscape;
        if (l.layout_.portrait)
            shell.orientation = Orientation::Portrait;
        Display target;
        REQUIRE(target.init(false));
        int checked = 0;
        const auto run = [&](const std::string &id, DisplaySettings settings) {
            std::cout << "Launch handoff: " << id << std::endl;
            const auto *entry = findCore(id);
            REQUIRE(entry);
            std::unique_ptr<Core> core(entry->create());
            const auto native = core->nativeFormat();
            game(entry->info->assetGameId ? entry->info->assetGameId : id);
            l.resize(original);
            l.returnHome();
            l.homeEntrance_.finish();
            target.configure(width, height, native, settings);
            LauncherTransition transition;
            REQUIRE(transition.prepare(l, width, height, shell, false));
            bool completed = false, quit = false;
            const auto observe = [&](float p) {
                if (p >= 1) {
                    const auto result = readFrame(0, width, height);
                    const auto destination = readFrame(transition.framebuffer_, width, height);
                    stbi_flip_vertically_on_write(1);
                    sameRGB(result, destination);
                    completed = true;
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            };
            if (id == "gridrunner-classic") {
                display.beginFrame();
                REQUIRE(core->init(host, target.surface()));
                target.beginFrame();
                core->frame(0);
                REQUIRE(transition.playRendered(target, window, quit, observe));
                core->shutdown();
            } else {
                REQUIRE(transition.playArtwork(host, target, window, quit, observe));
                const auto &surface = target.surface();
                const auto expected = readFrame(surface.framebuffer, surface.width, surface.height);
                target.beginFrame();
                StartupScreen actual;
                REQUIRE(actual.begin(host, surface));
                sameRGB(expected, readFrame(surface.framebuffer, surface.width, surface.height));
                actual.release();
            }
            REQUIRE(completed && !quit && !transition.pending());
            REQUIRE(glGetError() == GL_NO_ERROR);
            ++checked;
        };
        DisplaySettings normal;
        normal.orientation = Orientation::BestFit;
        for (const auto &entry : kGameCatalog)
            run(entry.id, normal);
        run("gridrunner-classic", normal);
        DisplaySettings rotated = normal;
        rotated.orientation = Orientation::PortraitRight;
        rotated.aspect = AspectMode::Original;
        rotated.screenMode = ScreenMode::Sharp;
        run("minos", rotated);
        DisplaySettings crt = normal;
        crt.screenMode = ScreenMode::CRT;
        crt.backdrop = Backdrop::Neon;
        run("deflex", crt);
        DisplaySettings fill = normal;
        fill.aspect = AspectMode::Fill;
        run("goatup2", fill);
        target.shutdown();
        l.resize(original);
        display.beginFrame();
        l.returnHome();
        std::cout << checked
                  << " archive/title launch handoffs matched their final composed frame\n";
    }
    static void appIcon(Launcher &l, const std::filesystem::path &scratch) {
        // Export rounded legacy and unmasked adaptive artwork using iOX glass.
        REQUIRE(l.systemIcons_[2]);
        l.homeEntrance_.finish();
        l.frame(0);
        constexpr int size = 1024;
        unsigned target = 0, texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &target);
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        const auto surface = l.surface_;
        l.surface_.width = l.surface_.height = size;
        glViewport(0, 0, size, size);
        glUseProgram(l.program_);
        glUniform2f(l.uSize_, size, size);
        glBindBuffer(GL_ARRAY_BUFFER, l.vbo_);
        for (unsigned i = 0; i < 3; ++i)
            glEnableVertexAttribArray(i);
        const MenuRect art{0, 0, float(size), float(size)};
        for (int variant = 0; variant < 3; ++variant) {
            const bool black = variant == 1, rounded = variant != 2;
            glClearColor(0, 0, 0, black ? 1.f : 0.f);
            glClear(GL_COLOR_BUFFER_BIT);
            if (black)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
            l.quad(art, 0, 0, 1, 1, {1, 1, 1}, {1, 1, 1});
            l.roundEdge(art);
            l.draw(l.systemIcons_[2], rounded, 1.6f);
            const auto pixels = readFrame(target, size, size);
            stbi_flip_vertically_on_write(1);
            const char *name = variant == 0   ? "app-icon.png"
                               : variant == 1 ? "app-icon-on-black.png"
                                              : "app-icon-adaptive.png";
            REQUIRE(stbi_write_png((scratch / name).string().c_str(), size, size, 4, pixels.data(),
                                   size * 4));
        }
        REQUIRE(glGetError() == GL_NO_ERROR);
        for (unsigned i = 0; i < 3; ++i)
            glDisableVertexAttribArray(i);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnable(GL_BLEND);
        l.surface_ = surface;
        glBindFramebuffer(GL_FRAMEBUFFER, surface.framebuffer);
        glDeleteFramebuffers(1, &target);
        glDeleteTextures(1, &texture);
    }
    static void gearPreview(Launcher &l, const std::filesystem::path &scratch) {
        for (int variant = 0; variant < 3; ++variant) {
            const auto pixels = GearIconPreview::pixels(variant);
            const unsigned texture =
                l.makeTexture(GearIconPreview::size, GearIconPreview::size, pixels.data());
            l.returnHome();
            l.homeEntrance_.finish();
            l.frame(0);
            glBindBuffer(GL_ARRAY_BUFFER, l.vbo_);
            for (unsigned i = 0; i < 3; ++i)
                glEnableVertexAttribArray(i);
            l.rect({0, 0, l.layout_.width, l.layout_.height}, {.17f, .18f, .20f},
                   {.17f, .18f, .20f});
            const MenuRect art{l.layout_.width / 2 - 96, l.layout_.height / 2 - 96, 192, 192};
            l.icon(art, texture, "", false, {1, 1, 1}, {1, 1, 1});
            const iox::CanvasTransform canvas(l.layout_, l.surface_.width, l.surface_.height);
            const int width = int(224 * canvas.scaleX), height = int(224 * canvas.scaleY);
            std::vector<uint8_t> capture(size_t(width) * height * 4);
            glReadPixels(int((art.x - 16) * canvas.scaleX),
                         int((l.layout_.height - art.y - 208) * canvas.scaleY), width, height,
                         GL_RGBA, GL_UNSIGNED_BYTE, capture.data());
            stbi_flip_vertically_on_write(1);
            const char *filename = variant == 0   ? "options-gear-preview.png"
                                   : variant == 1 ? "options-gear-b.png"
                                                  : "options-gear-c.png";
            REQUIRE(stbi_write_png((scratch / filename).string().c_str(), width, height, 4,
                                   capture.data(), width * 4));
            for (unsigned i = 0; i < 3; ++i)
                glDisableVertexAttribArray(i);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDeleteTextures(1, &texture);
            REQUIRE(glGetError() == GL_NO_ERROR);
        }
    }
    static void dockVariants(Launcher &l, const std::filesystem::path &scratch, bool portrait) {
        const unsigned original = l.dockTexture_;
        l.returnHome();
        l.homeEntrance_.finish();
        l.navigation_.homeFocus() = 0;
        for (int style = 0; style < 2; ++style) {
            const auto pixels = dockStylePreview(style);
            l.dockTexture_ =
                l.makeTexture(iox::DockGraphic::width, iox::DockGraphic::height, pixels.data());
            const std::string name = std::string(style == 0 ? "a-carbon-" : "b-mesh-") +
                                     (portrait ? "portrait" : "landscape");
            frame(l, scratch, (name + ".png").c_str(), 0);
            const auto &r = l.layout_.dock;
            const iox::CanvasTransform canvas(l.layout_, l.surface_.width, l.surface_.height);
            const int width = int(r.w * canvas.scaleX), height = int(r.h * canvas.scaleY);
            std::vector<uint8_t> crop(size_t(width) * height * 4);
            glReadPixels(int(r.x * canvas.scaleX),
                         int((l.layout_.height - r.y - r.h) * canvas.scaleY), width, height,
                         GL_RGBA, GL_UNSIGNED_BYTE, crop.data());
            REQUIRE(stbi_write_png((scratch / (name + "-dock.png")).string().c_str(), width, height,
                                   4, crop.data(), width * 4));
            glDeleteTextures(1, &l.dockTexture_);
        }
        l.dockTexture_ = original;
    }
    static void select(Launcher &launcher) {
        launcher.key(0, 0, true, false);
        settle(launcher);
    }
    static void click(Launcher &launcher, int slot, bool cancel = false) {
        const auto rect = launcher.page() == Screen::Home ? launcher.layout_.items[size_t(slot)]
                                                          : launcher.pageButton(slot);
        const iox::CanvasTransform canvas(launcher.layout_, launcher.surface_.width,
                                          launcher.surface_.height);
        const float x = rect.cx() * canvas.scaleX, y = rect.cy() * canvas.scaleY;
        launcher.touch({TouchPhase::Down, 1, x, y});
        if (cancel)
            launcher.touch({TouchPhase::Move, 1, -100, -100});
        launcher.touch({TouchPhase::Up, 1, x, y});
        settle(launcher);
    }
    static void frame(Launcher &launcher, const std::filesystem::path &scratch, const char *name,
                      double dt = 1.0 / 60) {
        for (int slot = 0; slot < launcher.buttonCount(); ++slot) {
            if (!launcher.visibleControl(slot))
                continue;
            const auto rect = launcher.page() == Screen::Home ? launcher.layout_.items[size_t(slot)]
                                                              : launcher.pageButton(slot);
            REQUIRE(launcher.hit(rect.cx(), rect.cy()) == slot);
        }
        launcher.frame(dt);
        REQUIRE(glGetError() == GL_NO_ERROR);
        const auto &s = launcher.surface_;
        std::vector<unsigned char> pixels(size_t(s.width) * s.height * 4);
        glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        bool lit = false;
        for (size_t i = 0; i < pixels.size(); i += 4)
            lit |= pixels[i] > 32 || pixels[i + 1] > 32 || pixels[i + 2] > 32;
        REQUIRE(lit);
        stbi_flip_vertically_on_write(1);
        REQUIRE(stbi_write_png((scratch / name).string().c_str(), s.width, s.height, 4,
                               pixels.data(), s.width * 4));
    }
    static void entrance(Launcher &l, const std::filesystem::path &scratch, bool portrait) {
        l.returnHome();
        REQUIRE(l.enteringHome());
        const std::string prefix = portrait ? "portrait-entrance-" : "landscape-entrance-";
        l.returnHome();
        const int selected = l.focus();
        l.key(1, 0, true, false);
        l.back();
        click(l, iox::home::options);
        PadState pad{};
        pad.connected = true;
        pad.pressed = PAD_SOUTH;
        pad.buttons = PAD_SOUTH;
        l.pads(&pad, 1);
        REQUIRE(l.page() == Screen::Home && l.focus() == selected);
        REQUIRE(l.takeRequest().action == Launcher::Action::None);
        for (int i = 0; i < 9; ++i) {
            const auto pose = l.homeEntrance_.icon(l.layout_, i);
            const auto bounds = pose.apply(l.layout_.items[size_t(i)]);
            REQUIRE(pose.scale == 1.5f);
            REQUIRE(portrait ? bounds.y + bounds.h <= 0 : bounds.x + bounds.w <= 0);
        }
        for (int i = 9; i < iox::home::count; ++i)
            REQUIRE(l.homeEntrance_.icon(l.layout_, i).scale == 1);
        // Frame-driven sequence. Capture all frames for visual/motion review;
        // elapsed time is fed through the production Core::frame entry point.
        for (int i = 0; i <= 60; ++i) {
            char name[96];
            std::snprintf(name, sizeof name, "%s%03d.png", prefix.c_str(), i);
            frame(l, scratch, name, i == 60 ? 0 : 1.0 / 60);
            if (i == 0) {
                const auto &s = l.surface_;
                std::vector<unsigned char> first(size_t(s.width) * s.height * 4),
                    wallpaper(first.size());
                glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, first.data());
                glBindBuffer(GL_ARRAY_BUFFER, l.vbo_);
                for (unsigned attribute = 0; attribute < 3; ++attribute)
                    glEnableVertexAttribArray(attribute);
                l.drawWallpaper();
                l.drawBarBacking();
                for (unsigned attribute = 0; attribute < 3; ++attribute)
                    glDisableVertexAttribArray(attribute);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, wallpaper.data());
                REQUIRE(first ==
                        wallpaper); // No icon, shadow, dock, title or tip leaks into frame 1.
            }
        }
        REQUIRE(!l.enteringHome());
        REQUIRE(l.homeEntrance_.iconsProgress() == 1 && l.homeEntrance_.barsProgress() == 1);
        // Returning from an ordinary app should not restart the entrance.
        click(l, iox::home::options);
        REQUIRE(l.page() == Screen::Options);
        l.back();
        REQUIRE(l.page() == Screen::Home && !l.enteringHome());
        // A fresh return resets it, preserving the last selected home icon.
        l.returnHome();
        REQUIRE(l.enteringHome() && l.focus() == iox::home::options);
        const auto r = l.layout_.items[iox::home::options];
        const iox::CanvasTransform canvas(l.layout_, l.surface_.width, l.surface_.height);
        l.touch({TouchPhase::Down, 1, r.cx() * canvas.scaleX, r.cy() * canvas.scaleY});
        l.frame(.5);
        REQUIRE(l.homeEntrance_.barsProgress() == 0 && l.enteringHome());
        l.frame(.25);
        REQUIRE(l.homeEntrance_.barsProgress() == .5f && l.enteringHome());
        l.frame(.25);
        REQUIRE(!l.enteringHome());
        // A touch begun during the entrance cannot activate on release afterwards.
        l.touch({TouchPhase::Up, 1, r.cx() * canvas.scaleX, r.cy() * canvas.scaleY});
        REQUIRE(l.page() == Screen::Home);
    }
    static void routes(Launcher &l, Settings &settings, const std::filesystem::path &scratch,
                       bool portrait) {
        const auto originalIcon = l.icons_[0];
        const std::string prefix = portrait ? "portrait-" : "landscape-";
        auto capture = [&](const char *name) {
            frame(l, scratch, (prefix + name + ".png").c_str());
        };
        l.returnHome();
        l.frame(1);
        l.setControllerPrompts(false, "", "");
        REQUIRE(l.footerStatus() == "Exit");
        capture("footer-touch-exit");
        const iox::CanvasTransform footerCanvas(l.layout_, l.surface_.width, l.surface_.height);
        const float footerX = l.layout_.width / 2 * footerCanvas.scaleX;
        const float footerY = (l.layout_.height - 14) * footerCanvas.scaleY;
        l.touch({TouchPhase::Down, 91, footerX, footerY});
        l.touch({TouchPhase::Move, 91, -100, -100});
        l.touch({TouchPhase::Up, 91, footerX, footerY});
        REQUIRE(l.page() == Screen::Home);
        l.touch({TouchPhase::Down, 92, footerX, footerY});
        l.touch({TouchPhase::Up, 92, footerX, footerY});
        REQUIRE(l.page() == Screen::Quit);
        l.key(0, 1, true, false);
        REQUIRE(l.takeRequest().action == Launcher::Action::Quit);
        l.returnHome();
        l.frame(1);
        l.setControllerPrompts(true, "Cross", "Circle");
        REQUIRE(l.footerStatus().find("Circle: EXIT") != std::string::npos);
        capture("footer-playstation");
        l.setControllerPrompts(true, "B", "A");
        REQUIRE(l.footerStatus().find("A: EXIT") != std::string::npos);
        capture("footer-nintendo");
        REQUIRE(l.hit(l.layout_.width / 2, l.layout_.height - 14) != -2);
        l.setControllerPrompts(false, "", "");
        REQUIRE(l.footerStatus() == "Exit");
        l.open(Screen::About);
        REQUIRE(l.footerStatus().empty());
        REQUIRE(l.hit(l.layout_.width / 2, l.layout_.height - 14) != -2);
        l.back();
        l.returnHome();
        l.frame(1);
        l.navigation_.homeFocus() = iox::home::options;
        l.selectionTime_ = .45;
        capture("home");
        for (int pulse = 0; pulse < 36; ++pulse) {
            char name[96];
            std::snprintf(name, sizeof name, "%sselection-pulse-%02d.png", prefix.c_str(), pulse);
            frame(l, scratch, name, .05);
        }
        // A playable icon emits a direct launch request, with no intermediate screen.
        l.icons_[0].playable = true;
        l.icons_[0].installed = true;
        l.selectGame(kGameCatalog[0].id);
        select(l);
        const auto launch = l.takeRequest();
        REQUIRE(launch.action == Launcher::Action::Launch);
        REQUIRE(launch.gameId == kGameCatalog[0].id && l.page() == Screen::Home);

        // Touch cancellation and controller acceptance use the same home destination.
        click(l, iox::home::options, true);
        REQUIRE(l.page() == Screen::Home);
        l.navigation_.homeFocus() = iox::home::options;
        PadState pad{};
        pad.connected = true;
        pad.pressed = PAD_SOUTH;
        pad.buttons = PAD_SOUTH;
        l.pads(&pad, 1);
        settle(l);
        REQUIRE(l.page() == Screen::Options);
        pad.pressed = 0;
        l.pads(&pad, 1);
        settle(l);
        REQUIRE(l.page() == Screen::Options);
        click(l, iox::options::screenTab);
        capture("options");
        {
            const auto saved = settings.get("global", "screen_mode", "hires");
            const int focus = l.focus();
            auto filterRow = [&] {
                auto rows = l.optionRows();
                return *std::find_if(rows.begin(), rows.end(),
                                     [](const auto &row) { return row.key == "screen_mode"; });
            };
            settings.set("global", "screen_mode", "crt");
            auto row = filterRow();
            REQUIRE(row.value == "CRT 1" &&
                    row.tip.find("May impact performance on low-end devices.") !=
                        std::string::npos);
            auto rows = l.optionRows();
            int index = 0;
            while (rows[size_t(index)].key != "screen_mode")
                ++index;
            l.focus() = iox::options::firstRow + index;
            capture("crt1-options");
            l.key(0, 0, true, false);
            REQUIRE(settings.get("global", "screen_mode", "") == "crt2" &&
                    filterRow().value == "CRT 2");
            l.takeRequest();
            capture("crt2-options");
            l.key(0, 0, true, false);
            REQUIRE(settings.get("global", "screen_mode", "") == "hires");
            l.takeRequest();
            settings.set("global", "screen_mode", saved);
            l.focus() = focus;
        }
        l.key(0, -1, false, false);
        l.key(1, 0, false, false);
        capture("tabs-focus-controls");
        l.key(-1, 0, false, false);
        l.key(0, 1, false, false);
        const auto savedMode = settings.get("global", "window_mode", "windowed");
        settings.set("global", "window_mode", "borderless");
        for (const auto &row : l.optionRows()) {
            if (row.key == "fullscreen_resolution")
                REQUIRE(!row.enabled && row.label == "Resolution");
            if (row.key == "backdrop")
                REQUIRE(std::find(row.values.begin(), row.values.end(), "image") ==
                        row.values.end());
        }
        capture("options-borderless");
        l.configureOptions(settings, false, true, 1, {});
        for (const auto &row : l.optionRows())
            REQUIRE(row.key != "window_mode" && row.key != "fullscreen_resolution");
        capture("options-mobile");
        l.configureOptions(settings, true, true, 1, {"1280 x 720", "1920 x 1080"});
        settings.set("global", "window_mode", savedMode);
        const auto rows = l.optionRows();
        int wallpaper = -1;
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].key == "wallpaper")
                wallpaper = int(i) + iox::options::firstRow;
        REQUIRE(wallpaper >= iox::options::firstRow);
        click(l, wallpaper);
        REQUIRE(l.page() == Screen::Wallpapers);
        capture("wallpapers");
        // Subscreens have no invisible tab controls and restore their originating row.
        REQUIRE(!l.visibleControl(iox::options::screenTab));
        REQUIRE(l.hit(l.optionButton(0).cx(), l.optionButton(0).cy()) == -1);
        click(l, iox::options::firstRow + 1);
        REQUIRE(l.page() == Screen::Options);
        REQUIRE(l.focus() == wallpaper && settings.get("global", "wallpaper", "") == "1");
        l.showMessage("First", "Message");
        l.showMessage("Replacement", "Message");
        REQUIRE(l.navigation_.depth() == 2);
        capture("message");
        l.back();
        REQUIRE(l.page() == Screen::Options && l.focus() == wallpaper);

        click(l, iox::options::controlsTab);
        capture("controls");
        // Capture visual states without changing the existing workflow or saves.
        const auto originalRotate = settings.get("global", "rotate_controls", "off");
        const int controlsFocus = l.focus();
        settings.set("global", "rotate_controls", "on");
        l.focus() = l.buttonCount() - 1;
        capture("controls-on");
        settings.set("global", "rotate_controls", originalRotate);
        l.focus() = controlsFocus;
        l.configureOptions(settings, true, true, 0, {"1280x720", "1920x1080"});
        capture("controls-disabled");
        l.configureOptions(settings, true, true, 1, {"1280x720", "1920x1080"});
        l.beginTiltCalibration();
        REQUIRE(l.page() == Screen::Message);
        PadState calibrationPad;
        calibrationPad.connected = true;
        calibrationPad.buttons = PAD_SOUTH;
        l.pads(&calibrationPad, 1);
        REQUIRE(l.takeRequest().action == Launcher::Action::None);
        calibrationPad.buttons = 0;
        l.pads(&calibrationPad, 1);
        calibrationPad.buttons = PAD_EAST;
        l.pads(&calibrationPad, 1);
        REQUIRE(l.takeRequest().action == Launcher::Action::ConfirmTilt);
        l.back();
        l.beginTiltCalibration();
        l.touch({TouchPhase::Up, 99, 0, 0});
        REQUIRE(l.takeRequest().action == Launcher::Action::None);
        l.touch({TouchPhase::Down, 100, 0, 0});
        REQUIRE(l.takeRequest().action == Launcher::Action::ConfirmTilt);
        l.back();
        l.beginTiltCalibration();
        l.back();
        REQUIRE(l.takeRequest().action == Launcher::Action::CancelTilt);
        auto controls = l.optionRows();
        int remap = -1;
        for (size_t i = 0; i < controls.size(); ++i)
            if (controls[i].key == "remap")
                remap = int(i) + iox::options::firstRow;
        REQUIRE(remap >= iox::options::firstRow);
        click(l, remap);
        REQUIRE(l.page() == Screen::Players);
        capture("players");
        click(l, iox::options::firstRow + 2);
        REQUIRE(l.page() == Screen::Mapping);
        auto request = l.takeRequest();
        REQUIRE(request.action == Launcher::Action::Remap && request.gameId == "2");
        l.mappingProgress(6, false);
        capture("mapping");
        l.back();
        REQUIRE(l.takeRequest().action == Launcher::Action::CancelRemap);
        l.mappingFinished("Cancelled");
        REQUIRE(l.page() == Screen::Message);
        l.back();
        REQUIRE(l.page() == Screen::Options && l.focus() == remap);
        l.back();
        REQUIRE(l.page() == Screen::Home && l.focus() == iox::home::options);

        click(l, iox::home::about);
        REQUIRE(l.page() == Screen::About);
        capture("about");
        PadState analog{};
        analog.connected = true;
        analog.ly = .5f;
        l.pads(&analog, 1);
        l.frame(1. / 60);
        REQUIRE(l.aboutScreen_.scroll > 0 && l.aboutScreen_.scroll < 4);
        for (int i = 0; i < 1800 && l.focus() == iox::about::body; ++i)
            l.frame(1. / 60);
        REQUIRE(l.focus() == iox::about::donate);
        analog.ly = -.5f;
        l.pads(&analog, 1);
        l.frame(1. / 60);
        l.frame(1. / 60);
        REQUIRE(l.focus() == iox::about::body);
        analog.ly = 0;
        l.pads(&analog, 1);
        capture("about-scrolled");
        l.key(0, 1, false, false);
        REQUIRE(l.aboutScreen_.scroll > 0);
        l.key(1, 0, false, false);
        REQUIRE(l.focus() == iox::about::donate);
        l.showMessage("Link", "Unavailable");
        l.back();
        REQUIRE(l.page() == Screen::About && l.focus() == iox::about::donate);
        l.back();
        REQUIRE(l.page() == Screen::Home && l.focus() == iox::home::about);

        l.selectGame("gridrunner");
        select(l);
        REQUIRE(l.page() == Screen::Gridrunner);
        capture("gridrunner");
        // A modal result retains the selected variant.
        l.focus() = 1;
        l.showMessage("Unavailable", "Test");
        l.back();
        REQUIRE(l.page() == Screen::Gridrunner && l.focus() == 1);
        l.back();
        l.icons_[0].playable = false;
        l.icons_[0].installed = false;
        l.selectGame(kGameCatalog[0].id);
        select(l);
        REQUIRE(l.page() == Screen::Game);
        capture("unavailable");
        select(l);
        REQUIRE(l.page() == Screen::Home && l.takeRequest().action == Launcher::Action::None);
        l.selectGame(kGameCatalog[0].id);
        select(l);
        l.showMessage("Resource status", "Test");
        l.selectGame(kGameCatalog[1].id);
        l.back();
        REQUIRE(l.page() == Screen::Game);
        l.back();
        REQUIRE(l.focus() == 1);
        l.back();
        REQUIRE(l.page() == Screen::Quit && l.focus() == 0);
        capture("quit");
        select(l);
        REQUIRE(l.page() == Screen::Home && l.takeRequest().action == Launcher::Action::None);
        l.back();
        l.key(0, 1, true, false);
        REQUIRE(l.takeRequest().action == Launcher::Action::Quit);
        l.returnHome();
        REQUIRE(l.navigation_.depth() == 0);
        l.icons_[0] = originalIcon;
    }
};
} // namespace mino
namespace {
struct TestHost final : mino::Host {
    mino::Display *display;
    std::string gameId, savePath;
    bool keyboard = false;
    bool keyboardNavigationAvailable() const override {
        return keyboard;
    }
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        return mino::readBundledAsset(gameId, name, out);
    }
    std::string saveDir() const override {
        return savePath;
    }
    void log(const char *fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
    }
    std::string adaptShader(const std::string &source, int stage) const override {
        return display->adaptShader(source, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {}
};
} // namespace
int main(int argc, char **argv) {
    try {
        REQUIRE(argc == 2 || (argc == 3 && (std::string(argv[2]) == "--app-icon" ||
                                            std::string(argv[2]) == "--dock-previews" ||
                                            std::string(argv[2]) == "--icon-preview" ||
                                            (std::string(argv[2]) == "--launch-checks" ||
                                             std::string(argv[2]) == "--notes-checks"))));
        const bool appIcon = argc == 3 && std::string(argv[2]) == "--app-icon";
        const bool notesChecks = argc == 3 && std::string(argv[2]) == "--notes-checks";
        const bool dockPreviews = argc == 3 && std::string(argv[2]) == "--dock-previews";
        const bool iconPreview = argc == 3 && std::string(argv[2]) == "--icon-preview";
        const bool launchChecks = argc == 3 && std::string(argv[2]) == "--launch-checks";
        const auto scratch = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(scratch);
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        auto *window =
            SDL_CreateWindow("iOX checks", 640, 640, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        REQUIRE(window);
        auto context = SDL_GL_CreateContext(window);
        REQUIRE(context);
        REQUIRE(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)));
        mino::Display display;
        REQUIRE(display.init(false));
        TestHost host;
        host.display = &display;
        std::filesystem::create_directories(scratch / "saves");
        host.savePath = (scratch / "saves").generic_string() + "/";
        mino::Settings settings;
        settings.load((scratch / "settings.ini").string());
        mino::Launcher launcher;
        launcher.configureOptions(settings, true, true, 1, {"1280x720", "1920x1080"});
        launcher.setPaths(scratch.generic_string() + "/");
        mino::DisplaySettings output;
        for (bool portrait : {false, true}) {
            REQUIRE(SDL_SetWindowSize(window, portrait ? 480 : 640, portrait ? 640 : 480));
            SDL_PumpEvents();
            launcher.setPortrait(portrait);
            const auto &info = launcher.info();
            REQUIRE(display.configure(
                portrait ? 480 : 640, portrait ? 640 : 480,
                {info.baseWidth, info.baseHeight, info.minAspect, info.maxAspect}, output));
            if (!portrait)
                REQUIRE(launcher.init(host, display.surface()));
            else
                launcher.resize(display.surface());
            if (!appIcon && !iconPreview && !dockPreviews)
                mino::LauncherChecks::focusVisibility(launcher, host.keyboard);
            if (notesChecks) {
                mino::LauncherChecks::notes(launcher, scratch, portrait);
                continue;
            }
            if (appIcon) {
                mino::LauncherChecks::appIcon(launcher, scratch);
                break;
            }
            if (iconPreview) {
                mino::LauncherChecks::gearPreview(launcher, scratch);
                break;
            } else if (dockPreviews)
                mino::LauncherChecks::dockVariants(launcher, scratch, portrait);
            else {
                if (!launchChecks) {
                    mino::LauncherChecks::entrance(launcher, scratch, portrait);
                    mino::LauncherChecks::routes(launcher, settings, scratch, portrait);
                    mino::LauncherChecks::notes(launcher, scratch, portrait);
                }
                mino::LauncherChecks::apps(launcher, scratch, portrait);
                mino::LauncherChecks::archiveLaunches(
                    launcher, host, display, window,
                    [&](const std::string &id) { host.gameId = id; });
            }
        }
        // Nonuniform surface scaling preserves canvas hit coordinates, including misses outside it.
        mino::LauncherLayout layout(true);
        mino::iox::CanvasTransform transform(layout, 400, 600);
        REQUIRE(transform.canvasX(200) == 240 && transform.canvasY(300) == 320 &&
                transform.canvasX(-1) < 0);
        mino::iox::HomeEntrance timing;
        timing.start();
        timing.advance(.5);
        REQUIRE(timing.iconsProgress() == .875f && timing.barsProgress() == 0);
        timing.advance(.25);
        REQUIRE(timing.barsProgress() == .5f && timing.active());
        timing.advance(.25);
        REQUIRE(!timing.active());
        mino::iox::AppLaunch launch;
        launch.start();
        launch.advance(.125);
        REQUIRE(launch.progress() == .5f && launch.imageProgress(launch.progress()) == .25f);
        REQUIRE(launch.imageScale(launch.progress()) == .4375f);
        REQUIRE(launch.homePose(.5f).iconsProgress() == .875f &&
                launch.homePose(.5f).barsProgress() == 0);
        launch.advance(.125);
        REQUIRE(launch.progress() == 1);
        launch.cancel();
        REQUIRE(!launch.active());
        launcher.shutdown();
        launcher.shutdown();
        REQUIRE(glGetError() == GL_NO_ERROR);
        display.shutdown();
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        if (appIcon)
            std::cout
                << "About artwork exported for Android with launcher glass and rounded silhouette\n";
        else if (iconPreview)
            std::cout
                << "Gear icon preview captured with 20% stronger glass; production icons unchanged\n";
        else if (dockPreviews)
            std::cout
                << "Two dock alternatives captured in both orientations; production default unchanged\n";
        else if (notesChecks)
            std::cout
                << "Notes: 79 entries, unlock display, touch/analog scroll, game cycling/index and both orientations passed\n";
        else
            std::cout
                << "iOX: one-second entrance in both orientations, wallpaper/black-strip first frame, input gating, scale/easing/timing and existing screen/navigation contracts passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
