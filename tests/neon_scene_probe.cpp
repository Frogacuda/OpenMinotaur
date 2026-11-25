// Capture a saved scene at fixed synth time for rendering diagnostics.
#include "effect_runtime.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void capture(const std::filesystem::path &path, int width, int height, int surfaceHeight) {
    std::vector<unsigned char> pixels(size_t(width) * height * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(0, surfaceHeight - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Scene readback failed");
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        for (int x = 0; x < width; ++x)
            out.write(reinterpret_cast<const char *>(pixels.data() + (y * width + x) * 4), 3);
    if (!out)
        throw std::runtime_error("Scene capture write failed");
}
} // namespace
int main(int argc, char **argv) {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    try {
        if (argc < 4 || argc > 6)
            throw std::runtime_error(
                "Usage: scene-probe data scene output [hue-turns] [render-scale]");
        if (!SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        window = SDL_CreateWindow("Scene probe", 1280, 1280, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        context = SDL_GL_CreateContext(window);
        if (!context || !gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
            throw std::runtime_error("Scene probe GL initialization failed");
        if (!neon::effects::Runtime::loadGlesEntryPoints([](const char *name) -> void * {
                return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            }))
            throw std::runtime_error("Scene probe GLES initialization failed");
        {
            neon::effects::Runtime effect(argv[1]);
            std::ifstream scene(argv[2]);
            effect.readScene(scene);
            if (argc >= 5)
                effect.parameters().hueShift = std::stof(argv[4]);
            if (argc >= 6)
                effect.parameters().renderScale = std::stof(argv[5]);
            const std::filesystem::path output(argv[3]);
            std::filesystem::create_directories(output);
            for (auto size : {std::pair<int, int>{960, 540}, {720, 540}, {540, 960}, {540, 720}}) {
                effect.reset();
                for (int tick = 0; tick < 120; ++tick) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glClearColor(0, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    effect.frame(1. / 60., {0, 1280, 1280, {0, 0, size.first, size.second}, {}});
                }
                capture(output / (std::to_string(size.first) + "x" + std::to_string(size.second) +
                                  ".ppm"),
                        size.first, size.second, 1280);
            }
        }
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        if (context)
            SDL_GL_DestroyContext(context);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
