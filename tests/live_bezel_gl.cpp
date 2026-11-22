#include "effect_runtime.h"
#include "live_bezel.h"
#include "neon_performance.h"
#include "presentation_assets.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <fstream>
#include <glad/gl.h>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    try {
        require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        window =
            SDL_CreateWindow("Live bezel checks", 960, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(window != nullptr, SDL_GetError());
        context = SDL_GL_CreateContext(window);
        require(context != nullptr, SDL_GetError());
        require(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) != 0,
                "Missing GL entry points");
        require(neon::effects::Runtime::loadGlesEntryPoints([](const char *name) -> void * {
                    return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
                }),
                "Missing GLES entry points");
        if (argc == 2 && std::string(argv[1]) == "--deflex-check") {
            for (bool lowQuality : {true, false}) {
                mino::LiveBezel bezel;
                mino::PresentationState state;
                state.gameplay = true;
                state.session = 1;
                state.collections["ball.turns"] = {};
                for (int frame = 0; frame < 420; ++frame) {
                    state.values = {{"ball.shell", 1. - (frame % 120) / 119.},
                                    {"level.number", int64_t(1 + frame / 60)}};
                    if (frame % 45 == 15)
                        state.collections["ball.turns"].push_back(
                            {uint64_t(frame), frame % 90 == 15 ? 1 : -1});
                    bezel.update("deflex", state, 1. / 60, lowQuality);
                    require(bezel.available(), "Deflex live profile initialization failed");
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glClearColor(.1f, .2f, .3f, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    require(bezel.draw(0, 960, 720, 240, 200, 480, 320, (frame / 105) % 4),
                            "Deflex live draw failed");
                    require(glGetError() == GL_NO_ERROR, "Deflex live GLES error");
                }
                std::vector<unsigned char> pixels(960 * 720 * 4);
                glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                unsigned lit = 0;
                for (size_t n = 0; n < pixels.size(); n += 4)
                    lit += pixels[n] > 40 || pixels[n + 1] > 60 || pixels[n + 2] > 90;
                require(lit > 1000, "Deflex bezel is empty");
            }
            std::cout
                << "PASS: Deflex compiled scene, ordered turns, shell mapping, all shader choices, all rotations and HQ/LQ playback.\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--goatup2-check") {
            for (bool lowQuality : {true, false}) {
                mino::LiveBezel bezel;
                mino::PresentationState state;
                state.gameplay = true;
                state.session = 1;
                state.collections["inventory.pickups"] = {};
                for (int frame = 0; frame < 240; ++frame) {
                    state.values = {{"player.hunger", 1. - frame / 239.},
                                    {"power.invincible", frame >= 30 && frame < 150}};
                    if (frame == 20)
                        state.collections["inventory.pickups"].push_back({1, 71, 1});
                    if (frame == 70)
                        state.collections["inventory.pickups"].push_back({2, 120});
                    state.colours = {
                        {1, {float(frame % 60) / 59, 1 - float(frame % 60) / 59, .5f}}};
                    bezel.update("goatup2", state, 1. / 60, lowQuality);
                    require(bezel.available(), "GoatUp2 live profile initialization failed");
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glClearColor(.1f, .2f, .3f, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    require(bezel.draw(0, 960, 720, 240, 200, 480, 320, frame % 4),
                            "GoatUp2 live draw failed");
                    require(glGetError() == GL_NO_ERROR, "GoatUp2 live GLES error");
                }
                std::vector<unsigned char> pixels(960 * 720 * 4);
                glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                unsigned lit = 0;
                for (size_t n = 0; n < pixels.size(); n += 4)
                    lit += pixels[n] > 40 || pixels[n + 1] > 60 || pixels[n + 2] > 90;
                require(lit > 1000, "GoatUp2 bezel is empty");
            }
            std::cout
                << "PASS: GoatUp2 compiled scene, eight-item insertion runs, hunger, invincibility, all rotations and HQ/LQ playback.\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--minos-check") {
            for (bool lowQuality : {true, false}) {
                mino::LiveBezel bezel;
                mino::PresentationState state;
                state.gameplay = true;
                state.session = 1;
                for (int level : {1, 2, 3, 4, 5, 6, 7, 13, 21, 1}) {
                    state.values = {{"level.number", int64_t(level)},
                                    {"screen.scroll", .25},
                                    {"minotaurs.rescued", int64_t(0)},
                                    {"game.phase", int64_t(4)},
                                    {"gravity.influence", 0.}};
                    for (int frame = 0; frame < 180; ++frame) {
                        state.values["screen.scroll"] = frame * .25;
                        state.values["minotaurs.rescued"] = int64_t(frame >= 10);
                        state.values["game.phase"] = int64_t(frame >= 20 ? 5 : 4);
                        state.values["gravity.influence"] = .3 * std::sin(frame * .05);
                        bezel.update("minos", state, 1. / 60., lowQuality);
                        require(bezel.available(), "Minos prepared profile initialization failed");
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        glClearColor(.1f, .2f, .3f, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                        require(bezel.draw(0, 960, 720, 240, 200, 480, 320, level % 4),
                                "Minos live profile draw failed");
                        require(glGetError() == GL_NO_ERROR, "Minos live profile GLES error");
                    }
                    std::vector<unsigned char> pixels(960 * 720 * 4);
                    glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    unsigned lit = 0;
                    for (int y = 0; y < 720; ++y)
                        for (int x = 0; x < 960; ++x) {
                            const auto *p = &pixels[(y * 960 + x) * 4];
                            if (x >= 240 && x < 720 && y >= 200 && y < 520)
                                require((p[0] == 25 || p[0] == 26) && (p[1] == 50 || p[1] == 51) &&
                                            (p[2] == 76 || p[2] == 77),
                                        "Minos changed the excluded gameplay region");
                            else
                                lit += unsigned(p[0]) + p[1] + p[2] > 10;
                        }
                    std::cout << "Minos level=" << level << " lq=" << lowQuality << " lit=" << lit
                              << "\n";
                    require(lit > 100, "Minos produced an empty bezel");
                }
            }
            SDL_GL_DestroyContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            std::cout
                << "PASS: Minos embedded six-texture closure, level jumps, HQ/LQ, scroll, rotation and exclusion.\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--handoff-check") {
            auto reader = [](const std::filesystem::path &name, std::vector<uint8_t> &bytes) {
                return mino::readPresentationAsset("neon/" + name.generic_string(), bytes);
            };
            for (const char *name : {"FiveADay", "GoatUp", "Gridrunner", "GridrunnerClassic",
                                     "Minotron", "MinotaurRescue", "SuperOxWars", "Minos"}) {
                std::vector<uint8_t> bytes;
                require(
                    mino::readPresentationAsset(std::string("bezels/") + name + ".neonb", bytes),
                    "Prepared scene missing");
                std::string document(bytes.begin(), bytes.end());
                std::istringstream a(document), b(document);
                auto preserved = neon::effects::Runtime::loadBaked(a, {}, reader);
                auto owned = neon::effects::Runtime::loadBaked(b, {}, reader);
                neon::effects::Destination target{0, 960, 720, {0, 0, 960, 720}, {}};
                auto exclusive = target;
                exclusive.preserveHostState = false;
                exclusive.clearDestination = true;
                std::vector<unsigned char> reference(960 * 720 * 4), actual(reference.size());
                for (int frame = 0; frame < 12; ++frame) {
                    target.clockwiseTurns = exclusive.clockwiseTurns = frame >= 6 ? 1 : 0;
                    target.exclude = exclusive.exclude =
                        frame >= 4 ? neon::effects::Rect{320, 220, 320, 280}
                                   : neon::effects::Rect{};
                    auto clear = [&]() {
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        glDisable(GL_SCISSOR_TEST);
                        glColorMask(1, 1, 1, 1);
                        glClearColor(0, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);
                        if (frame >= 4) {
                            glEnable(GL_SCISSOR_TEST);
                            glScissor(320, 220, 320, 280);
                            glClearColor(1, 0, 1, 1);
                            glClear(GL_COLOR_BUFFER_BIT);
                            glDisable(GL_SCISSOR_TEST);
                        }
                    };
                    clear();
                    glViewport(7, 9, 31, 29);
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(3, 4, 20, 21);
                    glColorMask(1, 0, 1, 0);
                    glActiveTexture(GL_TEXTURE2);
                    if (frame % 2)
                        require(preserved->presentCached(target), "Preserved cache unavailable");
                    else
                        preserved->frameSampled(1. / 30, target);
                    GLint viewport[4], active = 0;
                    GLboolean mask[4];
                    glGetIntegerv(GL_VIEWPORT, viewport);
                    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
                    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
                    require(viewport[0] == 7 && viewport[1] == 9 && viewport[2] == 31 &&
                                viewport[3] == 29 && active == GL_TEXTURE2 &&
                                glIsEnabled(GL_SCISSOR_TEST) && mask[0] && !mask[1] && mask[2] &&
                                !mask[3],
                            "Default embedding changed caller state");
                    glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
                    clear();
                    if (frame % 2)
                        require(owned->presentCached(exclusive), "Owned cache unavailable");
                    else
                        owned->frameSampled(1. / 30, exclusive);
                    glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
                    for (size_t i = 0; i < actual.size(); ++i) {
                        const size_t x = (i / 4) % 960, y = (i / 4) / 960;
                        const bool hole = frame >= 4 && x >= 320 && x < 640 && y >= 220 && y < 500;
                        if (hole) {
                            require(reference[i] == (i % 4 == 1 ? 0 : 255),
                                    "Default exclusion changed caller pixels");
                            require(actual[i] == (i % 4 == 3 ? 255 : 0),
                                    "Explicit whole-destination clear missed exclusion");
                        } else
                            require(std::abs(int(actual[i]) - int(reference[i])) <= 1,
                                    "Exclusive pass pixel mismatch");
                    }
                    require(glGetError() == GL_NO_ERROR,
                            (std::string("Exclusive pass GLES error: ") + name + " frame " +
                             std::to_string(frame))
                                .c_str());
                    // Exclusive hosts must release bindings to the pass's owned
                    // objects before another pass or resource destruction.
                    glUseProgram(0);
                    glBindVertexArray(0);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    for (int unit = 0; unit < 3; ++unit) {
                        glActiveTexture(GL_TEXTURE0 + unit);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glBindSampler(unit, 0);
                    }
                    glActiveTexture(GL_TEXTURE0);
                }
                std::cout
                    << "PASS: " << name
                    << " preserved/exclusive pixels, cached output, rotation and default state contract.\n";
            }
            SDL_GL_DestroyContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--cadence-check") {
            mino::LiveBezel bezel;
            mino::PresentationState state;
            bezel.update("minotron", state, 1. / 60, true);
            require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0), "Initial cadence draw");
            bezel.beginCadenceAssessment();
            for (int n = 0; n < 40; ++n)
                bezel.observeFrame(20, 33.3);
            require(bezel.cadenceDivisor() == 1, "Cadence changed during loading assessment");
            bezel.finishCadenceAssessment();
            require(bezel.cadenceDivisor() == 2, "Sustained pressure did not select 30 Hz");
            for (int n = 0; n < 12; ++n) {
                bezel.update("minotron", state, 1. / 60, true);
                require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0), "Cached cadence draw");
                require(bezel.refreshedFrame() == (n % 2 == 1), "Cached draw reported a fresh synth frame");
            }
            bezel.discardPendingFrame();
            require(!bezel.refreshedFrame(), "Discard retained a refresh notification");
            require(bezel.cadenceDivisor() == 2, "Hidden output changed locked cadence");
            mino::NeonFrameTimer timer;
            unsigned samples = 0;
            for (int n = 0; n < 30; ++n) {
                timer.begin(1. / 60);
                glClear(GL_COLOR_BUFFER_BIT);
                if (timer.end())
                    ++samples;
                glFlush();
                SDL_Delay(1);
            }
            require(samples > 0, "CPU frame timing produced no samples");
            timer.reset();
            bezel.reset();
            require(glGetError() == GL_NO_ERROR, "Cadence/timer GLES error");
            SDL_GL_DestroyContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            std::cout << "PASS: locked half-rate cadence and CPU frame "
                         "timing.\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--program-cache-check") {
            auto folder =
                std::filesystem::path(argv[2]) /
                ("run-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::vector<uint8_t> document;
            require(mino::readPresentationAsset("bezels/Gridrunner.neonb", document),
                    "Missing cache scene");
            std::vector<unsigned char> baseline;
            GLint formats = 0;
            glGetIntegerv(0x87fe, &formats);
            for (int pass = 0; pass < 4; ++pass) {
                if (pass == 2 && formats > 0) {
                    bool corrupted = false;
                    for (const auto &entry : std::filesystem::directory_iterator(folder))
                        if (entry.path().extension() == ".nglp") {
                            std::ofstream bad(entry.path(), std::ios::binary | std::ios::trunc);
                            bad << "invalid cache";
                            corrupted = true;
                            break;
                        }
                    require(corrupted, "No shader binary cache was written");
                }
                auto reader = [&](const auto &name, auto &bytes) {
                    bool ok = mino::readPresentationAsset("neon/" + name.generic_string(), bytes);
                    if (ok && pass == 3 && name.extension() == ".frag") {
                        std::string note = "\n// source-key invalidation check\n";
                        bytes.insert(bytes.end(), note.begin(), note.end());
                    }
                    return ok;
                };
                std::istringstream input(std::string(document.begin(), document.end()));
                auto effect = neon::effects::Runtime::loadBaked(input, {}, reader);
                effect->setShaderCacheDirectory(folder);
                effect->parameters().renderScale = .5f;
                effect->components().at(3).resolution = 512;
                for (int tick = 0; tick < 4; ++tick)
                    effect->frame(1. / 60, {0, 960, 720, {0, 0, 960, 720}, {}});
                std::vector<unsigned char> pixels(960 * 720 * 4);
                glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                if (pass == 0)
                    baseline = pixels;
                else
                    for (size_t n = 0; n < pixels.size(); ++n)
                        if (n % 4 != 3)
                            require(pixels[n] == baseline[n], "Cached program changes output");
                auto stat = effect->statistics();
                if (formats > 0) {
                    if (pass == 0)
                        require(stat.shaderCompilations > 0, "Cold cache did not compile");
                    if (pass == 1)
                        require(stat.shaderCacheHits > 0 && stat.shaderCompilations == 0,
                                "Warm cache did not reuse binaries");
                    if (pass >= 2)
                        require(stat.shaderCompilations > 0,
                                "Invalid or changed shader did not recompile");
                }
                require(glGetError() == GL_NO_ERROR, "Program cache GLES error");
                std::cout << "Cache pass " << pass << " compiles=" << stat.shaderCompilations
                          << " binary_hits=" << stat.shaderCacheHits << "\n";
                effect->releaseGraphics();
            }
            SDL_GL_DestroyContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--startup-profile") {
            using Clock = std::chrono::steady_clock;
            for (const char *stem : {"Gridrunner", "GoatUp"}) {
                size_t reads = 0, bytesRead = 0, presetReads = 0;
                std::set<std::string> uniquePresets;
                auto reader = [&](const auto &name, auto &bytes) {
                    bool ok = mino::readPresentationAsset("neon/" + name.generic_string(), bytes);
                    if (ok) {
                        ++reads;
                        bytesRead += bytes.size();
                        if (name.extension() == ".grpreset") {
                            ++presetReads;
                            uniquePresets.insert(name.generic_string());
                        }
                    }
                    return ok;
                };
                auto start = Clock::now();
                std::vector<uint8_t> bytes;
                require(
                    mino::readPresentationAsset(std::string("bezels/") + stem + ".neonb", bytes),
                    "Missing prepared scene");
                std::istringstream document(std::string(bytes.begin(), bytes.end()));
                auto prepared = neon::effects::Runtime::loadBaked(document, {}, reader);
                auto &effect = *prepared;
                auto constructed = Clock::now();
                auto parsed = constructed;
                auto targets = effect.responseParameters();
                auto described = Clock::now();
                effect.parameters().renderScale = .5f;
                if (std::string(stem) == "Gridrunner")
                    effect.components().at(3).resolution = 512;
                effect.frame(1. / 60, {0, 960, 720, {0, 0, 960, 720}, {}});
                glFinish();
                auto first = Clock::now();
                effect.frame(1. / 60, {0, 960, 720, {0, 0, 960, 720}, {}});
                glFinish();
                auto second = Clock::now();
                auto ms = [](auto a, auto b) {
                    return std::chrono::duration<double, std::milli>(b - a).count();
                };
                std::cout << stem << " construct_ms=" << ms(start, constructed)
                          << " parse_validate_ms=" << ms(constructed, parsed)
                          << " descriptors_ms=" << ms(parsed, described)
                          << " first_frame_ms=" << ms(described, first)
                          << " second_frame_ms=" << ms(first, second) << " asset_reads=" << reads
                          << " bytes=" << bytesRead << " preset_reads=" << presetReads
                          << " unique_presets=" << uniquePresets.size()
                          << " targets=" << targets.size() << "\n";
                effect.releaseGraphics();
            }
            SDL_GL_DestroyContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 0;
        }
        {
            mino::LiveBezel bezel;
            mino::PresentationState state;
            state.gameplay = true;
            state.session = 1;
            state.values = {{"player.orientation", 0.},
                            {"player.fireBoost", 0.},
                            {"level", int64_t(0)},
                            {"minotaurs.rescued", uint64_t(0)},
                            {"center.contacts", uint64_t(0)},
                            {"player1.hits", uint64_t(0)},
                            {"player2.hits", uint64_t(0)}};
            for (int frame = 0; frame < 180; ++frame) {
                if (frame == 60) {
                    state.values["player.orientation"] = 2.;
                    state.values["player.fireBoost"] = 1.;
                    state.values["level"] = int64_t(1);
                    state.values["minotaurs.rescued"] = uint64_t(1);
                    state.values["center.contacts"] = uint64_t(1);
                    state.values["player2.hits"] = uint64_t(1);
                }
                bezel.update("minorescue", state, 1. / 60., true);
                require(bezel.available(), "Rescue live profile initialization failed");
                glClearColor(.1f, .2f, .3f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0),
                        "Rescue live profile draw failed");
                require(glGetError() == GL_NO_ERROR, "Rescue live profile GLES error");
            }
            std::vector<unsigned char> pixels(960 * 720 * 4);
            glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            require(glGetError() == GL_NO_ERROR, "GLES readback failed");
            unsigned bright = 0, dark = 0;
            for (int y = 0; y < 720; ++y)
                for (int x = 0; x < 960; ++x) {
                    const auto *p = &pixels[(y * 960 + x) * 4];
                    if (x >= 240 && x < 720 && y >= 200 && y < 520)
                        require((p[0] == 25 || p[0] == 26) && (p[1] == 50 || p[1] == 51) &&
                                    (p[2] == 76 || p[2] == 77),
                                "Bezel changed the excluded gameplay region");
                    else {
                        bright += unsigned(p[0]) + p[1] + p[2] > 90;
                        dark += unsigned(p[0]) + p[1] + p[2] < 30;
                    }
                }
            require(bright > 1000 && dark > 1000, "Rescue effect produced no visible field");
            if (argc == 2) {
                std::ofstream out(argv[1], std::ios::binary);
                out << "P6\n960 720\n255\n";
                for (int y = 719; y >= 0; --y)
                    for (int x = 0; x < 960; ++x)
                        out.write(reinterpret_cast<const char *>(pixels.data() + (y * 960 + x) * 4),
                                  3);
            }
            for (int turns = 0; turns < 4; ++turns) {
                bezel.update("minorescue", state, 1. / 60., false);
                require(bezel.draw(0, 720, 960, 200, 240, 320, 480, turns),
                        "Rotated Rescue bezel draw failed");
                require(glGetError() == GL_NO_ERROR, "Rotated Rescue bezel GLES error");
            }
            bezel.reset();
            mino::PresentationState menu;
            menu.paused = true;
            auto startupPixels = [&]() {
                std::vector<unsigned char> p(960 * 720 * 4);
                glReadPixels(0, 0, 960, 720, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
                return p;
            };
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            bezel.update("minotron", menu, 0, true);
            require(bezel.available(), "Startup bezel initialization failed");
            require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0), "Startup bezel draw failed");
            auto firstFrame = startupPixels();
            unsigned startupBright = 0;
            for (size_t i = 0; i < firstFrame.size(); i += 4)
                startupBright += firstFrame[i] + firstFrame[i + 1] + firstFrame[i + 2] > 30;
            require(startupBright > 1000, "Zero-time startup bezel is blank");
            for (int frame = 0; frame < 45; ++frame) {
                bezel.update("minotron", menu, 1. / 60., true);
                bezel.draw(0, 960, 720, 240, 200, 480, 320, 0);
            }
            require(firstFrame != startupPixels(),
                    "Menu bezel waits for gameplay before animating");
            bezel.reset();
            state.session = 2;
            state.values = {{"level", int64_t(0)},
                            {"levels.completed", uint64_t(0)},
                            {"audio.level", 0.},
                            {"player.shots", int64_t(1)},
                            {"player.invincible", false}};
            for (int frame = 0; frame < 200; ++frame) {
                if (frame == 20) {
                    state.values["level"] = int64_t(1);
                    state.values["levels.completed"] = uint64_t(1);
                }
                if (frame == 50)
                    state.values["player.shots"] = int64_t(3);
                if (frame == 80)
                    state.values["player.invincible"] = true;
                if (frame == 110) {
                    state.values["player.invincible"] = false;
                    state.values["audio.level"] = 1.;
                }
                if (frame == 140) {
                    state.values["player.shots"] = int64_t(1);
                    state.values["bombs.used"] = uint64_t(1);
                }
                if (frame == 160)
                    state.values["extraLives.gained"] = uint64_t(1);
                bezel.update("minotron", state, 1. / 60., true);
                require(bezel.available(), "Minotron live profile initialization failed");
                require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0),
                        "Minotron live profile draw failed");
                require(glGetError() == GL_NO_ERROR, "Minotron live GLES error");
            }
            bezel.reset();
            state.values = {{"player.followers", int64_t(0)},
                            {"score.redShare", .5},
                            {"score.blueShare", .5},
                            {"world.tilt", 0.}};
            for (int frame = 0; frame < 120; ++frame) {
                if (frame == 60) {
                    state.values["player.followers"] = int64_t(5);
                    state.values["score.redShare"] = 1.;
                    state.values["score.blueShare"] = 0.;
                    state.values["world.tilt"] = -.8;
                }
                bezel.update("superoxwars", state, 1. / 60., frame < 60);
                require(bezel.available(), "Super Ox Wars live profile initialization failed");
                require(bezel.draw(0, 960, 720, 240, 200, 480, 320, 0), "Ox bezel draw failed");
                require(glGetError() == GL_NO_ERROR, "Ox GLES error");
            }
            for (bool mobile : {false, true}) {
                bezel.reset();
                mino::PresentationState colours;
                colours.gameplay = true;
                colours.session = 1;
                colours.values = {{"player.followers", int64_t(0)},
                                  {"score.redShare", 1.},
                                  {"score.blueShare", 0.},
                                  {"world.tilt", 0.}};
                for (int tick = 0; tick < 120; ++tick) {
                    bezel.update("superoxwars", colours, 1. / 60., mobile);
                    require(bezel.draw(0, 960, 720, 0, 0, 0, 0, 0), "Colour rig draw failed");
                }
                auto rigged = startupPixels();

                std::vector<uint8_t> sceneBytes;
                require(mino::readPresentationAsset("bezels/SuperOxWars.neonb", sceneBytes),
                        "Missing colour reference scene");
                std::istringstream saved(std::string(sceneBytes.begin(), sceneBytes.end()));
                auto reference =
                    neon::effects::Runtime::loadBaked(saved, {}, [](const auto &name, auto &bytes) {
                        return mino::readPresentationAsset("neon/" + name.generic_string(), bytes);
                    });
                reference->parameters().renderScale = mobile ? .5f : 1.f;
                for (auto &control : reference->authoring().controls(24)) {
                    if (control.key == "colour.7.red" || control.key == "colour.10.blue" ||
                        control.key == "node.20.f.2") {
                        control.enabled = true;
                        control.value = control.key == "colour.7.red"     ? 3.f
                                        : control.key == "colour.10.blue" ? 1.25f
                                                                          : 0.f;
                    }
                }
                for (int tick = 0; tick < 120; ++tick)
                    reference->frame(1. / 60., {0, 960, 720, {0, 0, 960, 720}, {}});
                auto authored = startupPixels();
                unsigned difference = 0;
                for (size_t n = 0; n < authored.size(); ++n)
                    if (n % 4 != 3)
                        difference += unsigned(std::abs(int(authored[n]) - int(rigged[n])));
                std::cout << "Ox live versus authored colour difference: " << difference << "\n";
                require(difference == 0, "Live colour controls differ from authored controls");
                reference->releaseGraphics();
            }
            for (bool mobile : {false, true}) {
                for (int position = 0; position < 4; ++position) {
                    bezel.reset();
                    mino::PresentationState state;
                    state.gameplay = position != 3;
                    state.session = 1;
                    if (position != 3)
                        state.values = {
                            {"player.x", .015},         {"player.y", -.228 + .328 * position},
                            {"power.slow", false},      {"power.multishot", false},
                            {"power.allAround", false}, {"lives", int64_t(3)}};
                    for (int tick = 0; tick < 12; ++tick) {
                        bezel.update("gridrunner", state, 1. / 60., mobile);
                        require(bezel.draw(0, 960, 720, 0, 0, 0, 0, 0), "Grid pitch draw failed");
                    }
                    auto rigged = startupPixels();

                    std::vector<uint8_t> bytes;
                    require(mino::readPresentationAsset("bezels/Gridrunner.neonb", bytes),
                            "Missing grid scene");
                    std::istringstream saved(std::string(bytes.begin(), bytes.end()));
                    auto reference = neon::effects::Runtime::loadBaked(
                        saved, {}, [](const auto &name, auto &bytes) {
                            return mino::readPresentationAsset("neon/" + name.generic_string(),
                                                               bytes);
                        });
                    reference->parameters().renderScale = mobile ? .5f : 1.f;
                    reference->components().at(3).resolution = mobile ? 512 : 1024;
                    reference->components().at(4).values[neon::effects::Pitch] =
                        position == 3 ? 0.f : .3f - .15f * position;
                    reference->components().at(4).values[neon::effects::Yaw] = 0.f;
                    for (int tick = 0; tick < 12; ++tick)
                        reference->frame(1. / 60., {0, 960, 720, {0, 0, 960, 720}, {}});
                    auto authored = startupPixels();
                    unsigned difference = 0;
                    for (size_t n = 0; n < authored.size(); ++n)
                        if (n % 4 != 3)
                            difference += unsigned(std::abs(int(authored[n]) - int(rigged[n])));
                    std::cout << "Grid pitch position " << position << " mobile " << mobile
                              << " difference " << difference << "\n";
                    require(difference == 0, "Grid plane pitch differs from authored angle");
                    reference->releaseGraphics();
                }
            }
            {
                GLuint gameTexture = 0;
                glGenTextures(1, &gameTexture);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gameTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                std::vector<unsigned char> source(32 * 48 * 4, 255);
                std::vector<unsigned char> first;
                for (int colour = 0; colour < 2; ++colour) {
                    for (size_t n = 0; n < source.size(); n += 4) {
                        source[n] = colour ? 0 : 220;
                        source[n + 1] = colour ? 220 : 0;
                        source[n + 2] = 40;
                    }
                    glBindTexture(GL_TEXTURE_2D, gameTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 48, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 source.data());
                    bezel.reset();
                    for (int tick = 0; tick < 90; ++tick) {
                        mino::PresentationState rotation;
                        rotation.gameplay = true;
                        rotation.session = 1;
                        rotation.values = {
                            {"player.rotation", (tick / 90.) * 6.283185307 - 3.141592654},
                            {"levels.completed", uint64_t(tick >= 15)},
                            {"minotaurs.rescued", uint64_t(tick >= 30)},
                            {"fruit.trail", int64_t(tick / 3)}};
                        bezel.update("fiveaday", rotation, 1. / 60., true);
                        bezel.setGameplayTexture(gameTexture, 32, 48);
                        require(bezel.available() && bezel.draw(0, 960, 720, 240, 200, 480, 320, 0),
                                "Five a Day gameplay texture failed");
                    }
                    auto pixels = startupPixels();
                    if (!colour)
                        first = pixels;
                    else
                        require(first != pixels, "Gameplay texture did not affect the feedback");
                    glBindTexture(GL_TEXTURE_2D, gameTexture);
                    GLint filter, wrap;
                    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
                    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
                    require(filter == GL_NEAREST && wrap == GL_CLAMP_TO_EDGE,
                            "Bezel changed gameplay texture sampling state");
                    require(glGetError() == GL_NO_ERROR, "Gameplay texture GLES error");
                }

                std::vector<uint8_t> sceneBytes;
                require(mino::readPresentationAsset("bezels/FiveADay.neonb", sceneBytes),
                        "Missing Five a Day scene");
                std::istringstream saved(std::string(sceneBytes.begin(), sceneBytes.end()));
                auto cost =
                    neon::effects::Runtime::loadBaked(saved, {}, [](const auto &name, auto &bytes) {
                        return mino::readPresentationAsset("neon/" + name.generic_string(), bytes);
                    });
                cost->parameters().renderScale = .5f;
                cost->frame(1. / 60., {0, 960, 720, {0, 0, 960, 720}, {}});
                auto staticCost = cost->statistics();
                cost->setGameplayTexture(gameTexture, 32, 48);
                cost->frame(1. / 60., {0, 960, 720, {0, 0, 960, 720}, {}});
                auto liveCost = cost->statistics();
                require(staticCost.drawCalls == liveCost.drawCalls &&
                            staticCost.targetBytes == liveCost.targetBytes,
                        "Gameplay input added a draw pass or render target");
                std::cout << "Five a Day half-res: " << liveCost.drawCalls << " draws, "
                          << liveCost.targets << " targets, " << liveCost.targetBytes
                          << " target bytes; gameplay input adds zero passes/target bytes.\n";
                cost->releaseGraphics();
                bezel.reset();
                require(glIsTexture(gameTexture), "Bezel deleted a borrowed gameplay texture");
                glDeleteTextures(1, &gameTexture);
                std::cout << "PASS: Five a Day borrowed gameplay input, visible feedback response "
                             "and texture ownership/state.\n";
            }
            bezel.reset();
            state.session = 4;
            state.values = {{"player.x", .015},         {"player.y", -.228},
                            {"power.slow", false},      {"power.multishot", false},
                            {"power.allAround", false}, {"lives", int64_t(3)}};
            for (int tick = 0; tick < 120; ++tick) {
                if (tick == 20) {
                    state.values["power.slow"] = true;
                    state.values["power.multishot"] = true;
                    state.values["player.x"] = .43;
                    state.values["player.y"] = .428;
                }
                if (tick == 40)
                    state.values["power.allAround"] = true;
                if (tick == 60)
                    state.values["lives"] = int64_t(4);
                bezel.update("gridrunner", state, 1. / 60., true);
                require(bezel.available() && bezel.draw(0, 960, 720, 240, 100, 480, 520, 0),
                        "Gridrunner live bezel failed");
                require(glGetError() == GL_NO_ERROR, "Gridrunner live GLES error");
            }
            std::cout << "PASS: embedded Gridrunner candidate, nested power-up inputs and "
                         "independent plane motion.\n";
            bezel.reset();
            mino::PresentationState classic;
            classic.gameplay = true;
            classic.session = 1;
            for (int tick = 0; tick < 30; ++tick) {
                classic.values = {{"player.x", 1. + 20.9 * tick / 29},
                                  {"player.y", 8. + 10.9 * tick / 29}};
                bezel.update("gridrunner-classic", classic, 1. / 60, true);
                require(bezel.available() && bezel.draw(0, 960, 720, 160, 120, 640, 480, 0),
                        "Classic live bezel failed");
                require(glGetError() == GL_NO_ERROR, "Classic live GLES error");
            }
            std::cout << "PASS: Classic embedded profile, mobile quality budget and position-only "
                         "playback.\n";
            bezel.reset();
            bezel.update("goatup", {}, 0, true);
            require(bezel.available(), "GoatUp profile regression");
            state.values = {{"zone", int64_t(0)},  {"mushroom.active", false},
                            {"kids", uint64_t(0)}, {"billyGoats.kissed", uint64_t(0)},
                            {"player.x", 0.},      {"screen.elevation", 0.}};
            for (int tick = 0; tick < 180; ++tick) {
                if (tick == 30)
                    state.values["zone"] = int64_t(1);
                if (tick == 60)
                    state.values["mushroom.active"] = true;
                if (tick == 150)
                    state.values["mushroom.active"] = false;
                bezel.update("goatup", state, 1. / 60., true);
                require(bezel.available() && bezel.draw(0, 960, 720, 240, 200, 480, 320, 0),
                        "GoatUp mushroom playback failed");
                require(glGetError() == GL_NO_ERROR, "GoatUp mushroom GLES error");
            }
        }
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "PASS: embedded profiles, live targets, GLES playback, exclusion, "
                     "half/full resolution and orientation.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (context)
            SDL_GL_DestroyContext(context);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
