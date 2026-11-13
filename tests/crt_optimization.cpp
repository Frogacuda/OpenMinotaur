#include "royale_pipeline.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
static PFNGLDRAWARRAYSPROC realDraw = nullptr;
static int draws = 0;
static void GLAD_API_PTR countDraw(GLenum mode, GLint first, GLsizei count) {
    ++draws;
    realDraw(mode, first, count);
}
namespace mino {
struct RoyaleChecks {
    static void reference(Display &d) {
        d.directCRT_ = false;
        reference(d.royale_);
    }
    static bool direct(const Display &d) {
        return d.directCRT_;
    }
    static void reference(RoyalePipeline &p) {
        p.optimize_ = false;
    }
    static void uniformConsumer(RoyalePipeline &p, bool es) {
        const char *files[] = {"linear.glsl", "ntsc_module.glsl", "crt-consumer-1w.glsl"};
        const std::pair<const char *, float> parameters[] = {
            {"u_gamma", 1.1f},   {"u_svideo", 1.f}, {"u_system", 0.f},    {"u_comb", .6f},
            {"u_chroma", 1.5f},  {"LPC", .32f},     {"LPY", 1.6f},        {"u_res", 3.f},
            {"u_sharp", .25f},   {"u_warp", 0.f},   {"u_overscanx", .3f}, {"u_overscany", .3f},
            {"u_vignette", 0.f}, {"SCAN_LOW", .8f}, {"SCAN_HIGH", .3f},   {"MSK_SIZE", 1.f},
            {"MASK_LOW", .5f},   {"MASK_HIGH", .2f}};
        for (int i = 0; i < 3; ++i) {
            std::ifstream file(std::string("assets/shaders/consumer/") + files[i]);
            std::ostringstream text;
            text << file.rdbuf();
            if (!file)
                throw std::runtime_error("upstream fixture missing");
            auto &pass = p.passes_[i];
            glDeleteProgram(pass.program);
            pass.program = p.program("#define PARAMETER_UNIFORM\n" + text.str(), es, files[i]);
            if (!pass.program)
                throw std::runtime_error(p.error());
            auto loc = [&](const char *name) { return glGetUniformLocation(pass.program, name); };
            pass.matrix = loc("MVPMatrix");
            pass.output = loc("OutputSize");
            pass.input = loc("InputSize");
            pass.texture = loc("TextureSize");
            pass.frame = loc("FrameCount");
            pass.direction = loc("FrameDirection");
            pass.samplers = {{loc("Texture"), -1}};
            glUseProgram(pass.program);
            for (auto &value : parameters)
                glUniform1f(loc(value.first), value.second);
        }
    }
    static unsigned copyProgram(RoyalePipeline &p, bool es) {
        return p.program(
            "#ifdef VERTEX\nattribute vec2 VertexCoord;attribute vec2 TexCoord;varying vec2 "
            "uv;uniform bool Turn;void "
            "main(){gl_Position=vec4(VertexCoord,0,1);uv=Turn?vec2(1.-TexCoord.y,TexCoord.x):"
            "TexCoord;}\n#endif\n#ifdef FRAGMENT\nvarying vec2 uv;uniform sampler2D Texture;void "
            "main(){gl_FragColor=texture2D(Texture,uv);}\n#endif\n",
            es, "reference presentation");
    }
    static void copy(RoyalePipeline &p, unsigned program, unsigned texture, unsigned vbo,
                     unsigned fbo, int w, int h, bool turn) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(3, 5, w, h);
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "Turn"), turn);
        glUniform1i(glGetUniformLocation(program, "Texture"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        p.draw(vbo);
    }
    static void constants(RoyalePipeline &p) {
        for (int i = 0; i < p.passCount_; ++i)
            if (p.passes_[i].constant !=
                (p.profile_ == RoyalePipeline::Profile::Royale && (i == 7 || i == 8)))
                throw std::runtime_error("Unexpected static-pass dependencies");
    }
};
} // namespace mino
void require(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
struct Surface {
    unsigned tex = 0, fbo = 0;
    Surface(int w, int h) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "test surface");
    }
    ~Surface() {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
    }
};
int main(int argc, char **argv) {
    try {
        bool es = argc > 1 && std::string(argv[1]) == "es";
        bool consumer = argc > 2 && std::string(argv[2]) == "consumer";
        const auto profile = consumer ? mino::RoyalePipeline::Profile::Consumer
                                      : mino::RoyalePipeline::Profile::Royale;
        require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
        auto *window =
            SDL_CreateWindow("CRT comparison", 960, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(window, "window");
        auto context = SDL_GL_CreateContext(window);
        require(context, "context");
        require(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)), "GL load");
        {
            unsigned vao, vbo;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1};
            glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
            mino::RoyalePipeline reference, cached, fused, uniform;
            bool uniformReady = false;
            int uniformMax = 0;
            size_t uniformChanged = 0;
            mino::RoyaleChecks::reference(reference);
            unsigned copy = mino::RoyaleChecks::copyProgram(reference, es);
            require(copy != 0, "copy program");
            Surface source(480, 320);
            std::vector<unsigned char> input(480 * 320 * 4);
            unsigned rng = 17;
            int cases = 0;
            size_t changed = 0;
            int maximum = 0;
            for (auto size : {std::pair<int, int>{320, 240}, {960, 720}, {720, 960}, {1001, 703}}) {
                int w = size.first, h = size.second;
                Surface a(w + 8, h + 12), b(w + 8, h + 12);
                std::vector<unsigned char> pa(size_t(w) * h * 4), pb(pa.size());
                for (int turns = -2; turns < 2; ++turns) {
                    auto layout = mino::tubeLayout(w, h, turns);
                    require(reference.configure(es, layout, profile) &&
                                cached.configure(es, layout, profile) &&
                                fused.configure(es, layout, profile),
                            "configure");
                    mino::RoyaleChecks::constants(cached);
                    if (consumer) {
                        require(uniform.configure(es, layout, profile), "uniform configure");
                        if (!uniformReady) {
                            mino::RoyaleChecks::uniformConsumer(uniform, es);
                            uniformReady = true;
                        }
                    }
                    for (int pattern = 0; pattern < 7; ++pattern) {
                        for (int y = 0; y < 320; ++y)
                            for (int x = 0; x < 480; ++x) {
                                auto i = (y * 480 + x) * 4;
                                for (int c = 0; c < 3; ++c) {
                                    rng = rng * 1664525 + 1013904223;
                                    input[i + c] =
                                        uint8_t(pattern == 0   ? 0
                                                : pattern == 1 ? 255
                                                : pattern == 2 ? (x * 255 / 479)
                                                : pattern == 3 ? ((x + y) % 2) * 255
                                                : pattern == 4 ? (x % 17 == 0 || y % 19 == 0) * 255
                                                : pattern == 5 ? ((x / 60 + c) % 3 == 0) * 255
                                                               : rng >> 24);
                                }
                                input[i + 3] = 255;
                            }
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, source.tex);
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 480, 320, GL_RGBA, GL_UNSIGNED_BYTE,
                                        input.data());
                        unsigned original = reference.run(source.tex, vbo);
                        mino::RoyaleChecks::copy(reference, copy, original, vbo, a.fbo, w, h,
                                                 layout.clockwise);
                        glReadPixels(3, 5, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pa.data());
                        if (consumer) {
                            auto texture = uniform.run(source.tex, vbo);
                            mino::RoyaleChecks::copy(uniform, copy, texture, vbo, b.fbo, w, h,
                                                     layout.clockwise);
                            glReadPixels(3, 5, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
                            for (size_t i = 0; i < pa.size(); ++i) {
                                int d = std::abs(int(pa[i]) - int(pb[i]));
                                uniformChanged += d != 0;
                                uniformMax = std::max(uniformMax, d);
                            }
                        }
                        unsigned optimized = cached.run(source.tex, vbo);
                        mino::RoyaleChecks::copy(cached, copy, optimized, vbo, b.fbo, w, h,
                                                 layout.clockwise);
                        glReadPixels(3, 5, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
                        require(pa == pb, "Static cache differs from full pipeline");
                        mino::RoyalePipeline::Output output{b.fbo, 3, 5, w, h, layout.clockwise};
                        if (consumer) {
                            auto texture = fused.run(source.tex, vbo, nullptr, true);
                            mino::RoyaleChecks::copy(fused, copy, texture, vbo, b.fbo, w, h,
                                                     layout.clockwise);
                        } else
                            fused.run(source.tex, vbo, &output);
                        glReadPixels(3, 5, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
                        for (size_t i = 0; i < pa.size(); ++i) {
                            int d = std::abs(int(pa[i]) - int(pb[i]));
                            changed += d != 0;
                            maximum = std::max(maximum, d);
                        }
                        require(glGetError() == GL_NO_ERROR, "GL error");
                        ++cases;
                    }
                }
            }
            std::cout << (es ? "ES" : "GL") << ": " << cases
                      << " frames; static cache byte-identical; fused changed channels " << changed
                      << " max " << maximum << "\n";
            require(maximum == 0, "Direct presentation differs from full pipeline");
            // Constant folding and dynamic uniforms may straddle 8-bit rounding edges;
            // bound that precision difference separately from the exact copy bypass.
            if (consumer) {
                std::cout << "Locked constants versus original uniform parameters: changed "
                          << uniformChanged << " max " << uniformMax << "\n";
                require(uniformMax <= 2, "Locked defaults differ from original uniform preset");
            }
            mino::Display originalDisplay, reducedDisplay;
            require(originalDisplay.init(es) && reducedDisplay.init(es), "display init");
            mino::RoyaleChecks::reference(originalDisplay);
            int presented = 0;
            for (auto size : {std::pair<int, int>{960, 720}, {720, 960}})
                for (auto orientation :
                     {mino::Orientation::Landscape, mino::Orientation::Portrait,
                      mino::Orientation::PortraitRight, mino::Orientation::BestFit})
                    for (auto native : {mino::NativeFormat{480, 320, 1.5f, 1.5f},
                                        mino::NativeFormat{320, 480, 2.f / 3, 2.f / 3}}) {
                        mino::DisplaySettings settings;
                        settings.screenMode =
                            consumer ? mino::ScreenMode::CRT2 : mino::ScreenMode::CRT;
                        settings.aspect = mino::AspectMode::IPad;
                        settings.orientation = orientation;
                        originalDisplay.configure(size.first, size.second, native, settings);
                        reducedDisplay.configure(size.first, size.second, native, settings);
                        std::vector<unsigned char> a(size_t(size.first) * size.second * 4),
                            b(a.size());
                        for (int frame = 0; frame < 3; ++frame) {
                            auto render = [&](mino::Display &d,
                                              std::vector<unsigned char> &pixels) {
                                d.beginFrame();
                                glClearColor(.2f * frame, .6f, .3f, 1);
                                glClear(GL_COLOR_BUFFER_BIT);
                                glEnable(GL_SCISSOR_TEST);
                                glScissor(0, 0, d.surface().width / 3, d.surface().height / 2);
                                glClearColor(1, 0, 0, 1);
                                glClear(GL_COLOR_BUFFER_BIT);
                                glDisable(GL_SCISSOR_TEST);
                                d.present();
                                glReadPixels(0, 0, size.first, size.second, GL_RGBA,
                                             GL_UNSIGNED_BYTE, pixels.data());
                            };
                            render(originalDisplay, a);
                            render(reducedDisplay, b);
                            require(a == b, "Window presentation differs");
                            ++presented;
                        }
                    }
            std::cout << presented
                      << " complete window frames byte-identical; window supports direct RGB8="
                      << mino::RoyaleChecks::direct(reducedDisplay) << "\n";
            originalDisplay.shutdown();
            reducedDisplay.shutdown();
            glBindVertexArray(vao);
            auto layout = mino::tubeLayout(1280, 720, 0);
            require(reference.configure(es, layout, profile) &&
                        fused.configure(es, layout, profile),
                    "benchmark configure");
            Surface bench(1288, 732);
            mino::RoyalePipeline::Output destination{bench.fbo, 3, 5, 1280, 720, false};
            auto timing = [&](bool optimized) {
                auto frame = [&] {
                    if (optimized && !consumer)
                        fused.run(source.tex, vbo, &destination);
                    else {
                        auto &pipeline = optimized ? fused : reference;
                        auto texture =
                            pipeline.run(source.tex, vbo, nullptr, optimized && consumer);
                        mino::RoyaleChecks::copy(pipeline, copy, texture, vbo, bench.fbo, 1280, 720,
                                                 false);
                    }
                };
                for (int i = 0; i < 10; ++i)
                    frame();
                glFinish();
                auto start = SDL_GetPerformanceCounter();
                for (int i = 0; i < 100; ++i)
                    frame();
                glFinish();
                return double(SDL_GetPerformanceCounter() - start) * 1000 /
                       SDL_GetPerformanceFrequency() / 100;
            };
            realDraw = glad_glDrawArrays;
            glad_glDrawArrays = countDraw;
            fused.run(source.tex, vbo, &destination);
            draws = 0;
            if (consumer) {
                auto texture = fused.run(source.tex, vbo, nullptr, true);
                mino::RoyaleChecks::copy(fused, copy, texture, vbo, bench.fbo, 1280, 720, false);
            } else
                fused.run(source.tex, vbo, &destination);
            require(draws == (consumer ? 4 : 13), "Reduced draw count");
            draws = 0;
            auto original = reference.run(source.tex, vbo);
            mino::RoyaleChecks::copy(reference, copy, original, vbo, bench.fbo, 1280, 720, false);
            require(draws == (consumer ? 5 : 16), "Reference draw count");
            glad_glDrawArrays = realDraw;
            for (int trial = 0; trial < 3; ++trial) {
                double old = timing(false), now = timing(true);
                std::cout << "1280x720 completed GPU+submission ms/frame full=" << old
                          << " reduced=" << now << "\n";
            }
            reference.shutdown();
            cached.shutdown();
            fused.shutdown();
            uniform.shutdown();
            glDeleteProgram(copy);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
        }
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
