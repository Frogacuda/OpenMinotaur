#ifdef NDEBUG
#undef NDEBUG
#endif
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <map>
namespace mino {
struct RoyaleChecks {
    static void separateCopy(Display &d) {
        d.directCRT_ = false;
    }
    static std::vector<unsigned char> crtPixels(Display &d) {
        const auto &target = d.royale_.targets_[size_t(d.royale_.passCount_ - 1)];
        std::vector<unsigned char> p(size_t(target.w) * target.h * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        glReadPixels(0, 0, target.w, target.h, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
        return p;
    }
};
} // namespace mino
namespace {
using namespace mino;
void check(bool ok, const char *text) {
    if (!ok)
        throw std::runtime_error(text);
}
struct Pixels {
    int w, h;
    std::vector<unsigned char> data;
};
Pixels render(Display &display, int ww, int wh, NativeFormat native, Orientation orientation,
              ScreenMode mode, bool white) {
    DisplaySettings s;
    s.aspect = AspectMode::Fill;
    s.orientation = orientation;
    s.screenMode = mode;
    display.configure(ww, wh, native, s);
    if (!display.error().empty())
        throw std::runtime_error(display.error());
    display.beginFrame();
    auto surface = display.surface();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!white) {
        glEnable(GL_SCISSOR_TEST);
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                glScissor(x * surface.width / 2, y * surface.height / 2, surface.width / 2,
                          surface.height / 2);
                glClearColor(x ? 0.f : 1.f, y ? 1.f : 0.f, x ? 1.f : 0.f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        glDisable(GL_SCISSOR_TEST);
    }
    // Compare the same phase of the original two-phase S-video signal.
    display.present();
    display.present();
    Pixels p{ww, wh, std::vector<unsigned char>(size_t(ww) * wh * 4)};
    glReadPixels(0, 0, ww, wh, GL_RGBA, GL_UNSIGNED_BYTE, p.data.data());
    check(glGetError() == GL_NO_ERROR, "GL error during CRT rendering");
    return p;
}
void save(const Pixels &p, const std::string &path) {
    stbi_flip_vertically_on_write(1);
    check(stbi_write_png(path.c_str(), p.w, p.h, 4, p.data.data(), p.w * 4) != 0,
          "PNG write failed");
}
} // namespace
int main(int argc, char **argv) {
    const bool es = argc > 1 && std::string(argv[1]) == "es";
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    mino::Display display;
    try {
        check(SDL_Init(SDL_INIT_VIDEO), "SDL video init");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
        window = SDL_CreateWindow("CRT pipeline checks", 960, 960,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        check(window, "hidden window");
        context = SDL_GL_CreateContext(window);
        check(context, "GL context");
        check(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) != 0, "GL load");
        check(display.init(es), "presentation init");
        // Inspect the separate presentation copy here. The optimization
        // fixture compares the fused final write against this same path.
        mino::RoyaleChecks::separateCopy(display);
        int cases = 0;
        for (auto mode :
             {mino::ScreenMode::Scanlines, mino::ScreenMode::CRT, mino::ScreenMode::CRT2}) {
            std::map<std::pair<int, int>, Pixels> bases;
            for (auto size : {std::pair<int, int>{960, 720}, {720, 960}})
                for (auto orientation :
                     {mino::Orientation::Landscape, mino::Orientation::Portrait,
                      mino::Orientation::PortraitRight, mino::Orientation::BestFit})
                    for (auto native : {mino::NativeFormat{480, 320, 1.5f, 1.5f},
                                        mino::NativeFormat{320, 480, 2.f / 3, 2.f / 3}}) {
                        auto p = render(display, size.first, size.second, native, orientation, mode,
                                        true);
                        DisplaySettings settings;
                        settings.aspect = AspectMode::Fill;
                        settings.orientation = orientation;
                        settings.screenMode = mode;
                        const auto layout = displayLayout(p.w, p.h, native, settings);
                        const auto tube =
                            tubeLayout(p.w, p.h, layout.turns, layout.width, layout.height);
                        const auto key = std::make_pair(tube.sourceWidth, tube.sourceHeight);
                        if (!bases.count(key))
                            bases[key] = p;
                        const auto &base = bases.at(key);
                        // The white-field raster is invariant under GAME rotation. For
                        // a portrait screen it is a quarter-turn of the same tube.
                        double difference = 0;
                        for (int y = 20; y < p.h - 20; y += 3)
                            for (int x = 20; x < p.w - 20; x += 3) {
                                const bool transpose = (p.w > p.h) != (base.w > base.h);
                                int bx = transpose ? base.w - 1 - y : x, by = transpose ? x : y;
                                for (int c = 0; c < 3; ++c)
                                    difference += std::abs(
                                        int(p.data[(size_t(y) * p.w + x) * 4 + c]) -
                                        int(base.data[(size_t(by) * base.w + bx) * 4 + c]));
                            }
                        const double samples =
                            double(((p.h - 40 + 2) / 3) * ((p.w - 40 + 2) / 3) * 3);
                        if (difference / samples >= 2.0) {
                            std::cerr << "raster case " << cases << " size " << p.w << "," << p.h
                                      << " native " << native.width << "," << native.height
                                      << " orientation " << int(orientation) << " mode "
                                      << int(mode) << " difference " << difference / samples
                                      << "\n";
                            if (argc > 2) {
                                save(base, std::string(argv[2]) + "/debug-base.png");
                                save(p, std::string(argv[2]) + "/debug-current.png");
                            }
                            throw std::runtime_error(
                                "Raster rotated with game or changed on transpose");
                        }
                        if (mino::isCRT(mode)) {
                            // The final presenter must only copy/rotate the complete
                            // CRT result; it must never multiply another row pattern.
                            const auto filtered = mino::RoyaleChecks::crtPixels(display);
                            int maximum = 0;
                            for (int y = 0; y < p.h; ++y)
                                for (int x = 0; x < p.w; ++x) {
                                    const int tx = tube.clockwise ? tube.width - 1 - y : x,
                                              ty = tube.clockwise ? x : y;
                                    for (int c = 0; c < 3; ++c)
                                        maximum = std::max(
                                            maximum,
                                            std::abs(
                                                int(p.data[(size_t(y) * p.w + x) * 4 + c]) -
                                                int(filtered[(size_t(ty) * tube.width + tx) * 4 +
                                                             c])));
                                }
                            if (maximum > 1 && argc > 2) {
                                save(p, std::string(argv[2]) + "/copy-screen.png");
                                save({tube.width, tube.height, filtered},
                                     std::string(argv[2]) + "/copy-filter.png");
                            }
                            if (maximum > 1)
                                std::cerr << "copy max " << maximum << " case " << cases
                                          << " native " << native.width << "," << native.height
                                          << " orientation " << int(orientation) << " mode "
                                          << int(mode) << " size " << p.w << "," << p.h << "\n";
                            check(maximum <= 1, "Presentation added an effect over the CRT output");
                            auto mean = [&](int x, int y, int w, int h) {
                                double sum = 0;
                                for (int j = y; j < y + h; ++j)
                                    for (int i = x; i < x + w; ++i)
                                        for (int c = 0; c < 3; ++c)
                                            sum += p.data[(size_t(j) * p.w + i) * 4 + c];
                                return sum / (w * h * 3);
                            };
                            const double interior = mean(p.w / 4, p.h / 4, p.w / 2, p.h / 2);
                            // Average over the phosphor/scanline pattern. All four edges
                            // and corners must retain the white field, without a bezel fade.
                            const double edges[] = {mean(0, p.h / 4, 8, p.h / 2),
                                                    mean(p.w - 8, p.h / 4, 8, p.h / 2),
                                                    mean(p.w / 4, 0, p.w / 2, 8),
                                                    mean(p.w / 4, p.h - 8, p.w / 2, 8),
                                                    mean(0, 0, 8, 8),
                                                    mean(p.w - 8, 0, 8, 8),
                                                    mean(0, p.h - 8, 8, 8),
                                                    mean(p.w - 8, p.h - 8, 8, 8)};
                            for (double edge : edges) {
                                if (edge < interior * .85) {
                                    std::cerr << "edge brightness " << edge << " interior "
                                              << interior << '\n';
                                    if (argc > 2)
                                        save(p, std::string(argv[2]) + "/edge-failure.png");
                                    throw std::runtime_error("CRT darkens the flat screen edges");
                                }
                            }
                        }
                        ++cases;
                    }
            if (argc > 2) {
                auto p = render(display, 960, 720, {480, 320, 1.5f, 1.5f},
                                mino::Orientation::BestFit, mode, true);
                save(p, std::string(argv[2]) + (mode == mino::ScreenMode::CRT ? "/royale-white.png"
                                                : mode == mino::ScreenMode::CRT2
                                                    ? "/consumer-white.png"
                                                    : "/scanline-white.png"));
            }
        }
        // Count actual dark gaps, rather than accepting a visually plausible
        // periodic pattern. At 2x/3x there must be one per native raster row.
        for (auto native :
             {NativeFormat{480, 320, 1.5f, 1.5f}, NativeFormat{320, 480, 2.f / 3, 2.f / 3}})
            for (auto orientation : {Orientation::Landscape, Orientation::Portrait,
                                     Orientation::PortraitRight, Orientation::BestFit}) {
                const auto p =
                    render(display, 960, 960, native, orientation, ScreenMode::Scanlines, true);
                DisplaySettings s;
                s.aspect = AspectMode::Fill;
                s.orientation = orientation;
                s.screenMode = ScreenMode::Scanlines;
                const auto l = displayLayout(p.w, p.h, native, s);
                const auto tube = tubeLayout(p.w, p.h, l.turns, l.width, l.height);
                int gaps = 0;
                bool dark = false;
                for (int y = 0; y < p.h; ++y) {
                    const bool next = p.data[(size_t(y) * p.w + p.w / 2) * 4] < 240;
                    if (next && !dark)
                        ++gaps;
                    dark = next;
                }
                check(gaps == tube.sourceHeight, "Scanline gaps do not match native raster rows");
                ++cases;
            }
        // Flat geometry: compare interior corner colour classification against
        // the unfiltered game, in every input/final quarter-turn combination.
        for (auto size : {std::pair<int, int>{960, 720}, {720, 960}})
            for (auto mode : {mino::ScreenMode::CRT, mino::ScreenMode::CRT2})
                for (auto orientation :
                     {mino::Orientation::Landscape, mino::Orientation::Portrait,
                      mino::Orientation::PortraitRight, mino::Orientation::BestFit})
                    for (auto native : {mino::NativeFormat{480, 320, 1.5f, 1.5f},
                                        mino::NativeFormat{320, 480, 2.f / 3, 2.f / 3}}) {
                        auto reference = render(display, size.first, size.second, native,
                                                orientation, mino::ScreenMode::Smooth, false);
                        auto p = render(display, size.first, size.second, native, orientation, mode,
                                        false);
                        for (float u : {.1f, .9f})
                            for (float v : {.1f, .9f}) {
                                int x = int(u * p.w), y = int(v * p.h);
                                size_t at = (size_t(y) * p.w + x) * 4;
                                double minOn = 255, maxOff = 0;
                                for (int c = 0; c < 3; ++c) {
                                    double value = p.data[at + c];
                                    // Consumer's carrier creates chroma cross-talk even within
                                    // flat fields. Orientation preserves the mean hue ordering.
                                    if (mode == mino::ScreenMode::CRT2) {
                                        value = 0;
                                        for (int dy = -15; dy <= 15; ++dy)
                                            for (int dx = -15; dx <= 15; ++dx)
                                                value +=
                                                    p.data[(size_t(y + dy) * p.w + x + dx) * 4 + c];
                                        value /= 31 * 31;
                                        if (reference.data[at + c] > 128)
                                            minOn = std::min(minOn, value);
                                        else
                                            maxOff = std::max(maxOff, value);
                                    } else
                                        check((reference.data[at + c] > 128) == (value > 40),
                                              "Flat CRT corner or orientation differs");
                                }
                                if (mode == mino::ScreenMode::CRT2)
                                    check(minOn > maxOff + 16,
                                          "Consumer mean hue rotated or flipped");
                            }
                        ++cases;
                    }
        // Upright portrait inside a landscape desktop must have a portrait tube.
        mino::DisplaySettings s;
        s.screenMode = mino::ScreenMode::CRT;
        s.orientation = mino::Orientation::Landscape;
        s.aspect = mino::AspectMode::IPad;
        auto layout = mino::displayLayout(1280, 720, {320, 480, 2.f / 3, 2.f / 3}, s);
        auto tube = mino::tubeLayout(layout.view.w, layout.view.h, layout.turns);
        check(tube.clockwise && tube.inputTurns == -1,
              "Letterboxed portrait raster used desktop axes");
        std::cout
            << (es ? "ES" : "Desktop") << ": " << cases
            << " GPU fixtures passed: Royale and Consumer, flat corners, monitor-axis scanlines/masks, game rotation and portrait transpose\n";
        display.shutdown();
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        display.shutdown();
        if (context)
            SDL_GL_DestroyContext(context);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
