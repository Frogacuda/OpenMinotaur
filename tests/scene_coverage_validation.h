#pragma once
#include "effect_runtime.h"
#include "layer_coverage.h"
#include <glad/gl.h>
#include <cmath>
#include <iostream>
#include <sstream>
namespace neon::effects {
inline void validateSceneCoverage(const std::filesystem::path &data) {
    auto need = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    GLuint image = 0, fbo = 0;
    glGenTextures(1, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 640, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    auto set = [&](Runtime &r, const std::string &key, float value) {
        for (auto &c : r.authoring().controls(r.selectedPreset()))
            if (c.key == key) {
                c.enabled = true;
                c.motion = 0;
                c.value = value;
                return;
            }
        throw std::runtime_error("Missing coverage control " + key);
    };
    auto render = [&](Runtime &r, int width, int height, int frames) {
        r.reset();
        for (int frame = 0; frame < frames; ++frame)
            r.frameSampled(1. / 60, {fbo, 640, 640, {0, 0, width, height}, {}});
        std::vector<unsigned char> pixels(size_t(width) * height * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 640 - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        need(glGetError() == GL_NO_ERROR, "Coverage GLES error");
        return pixels;
    };
    {
        Runtime sprites(data);
        sprites.selectPreset(16);
        set(sprites, "node.22.i.2", 26);
        set(sprites, "node.22.i.1", 255);
        set(sprites, "input.22.0", 35);
        set(sprites, "scene.output", 5);
        for (int k = 0; k < 10; ++k)
            set(sprites, "node.22.f." + std::to_string(k), 0);
        for (auto dims : {std::pair{320, 240}, std::pair{180, 320}, std::pair{240, 320}}) {
            set(sprites, "sampling.22.0.3", 0);
            sprites.parameters().layerPreview = -1;
            auto combined = render(sprites, dims.first, dims.second, 180);
            const auto bytes = sprites.statistics().targetBytes;
            sprites.parameters().layerPreview = 4;
            auto isolated = render(sprites, dims.first, dims.second, 180);
            unsigned long long error = 0;
            for (size_t i = 0; i < combined.size(); ++i)
                if (i % 4 != 3)
                    error += std::abs(int(combined[i]) - int(isolated[i]));
            std::cout << "Sprite surface " << dims.first << "x" << dims.second << " RGB difference "
                      << error << '\n';
            need(error == 0, "A scene composite tiled or mirrored its sprite producer");
            sprites.parameters().layerPreview = -1;
            set(sprites, "sampling.22.0.3", 1);
            auto tiled = render(sprites, dims.first, dims.second, 180);
            need(sprites.statistics().targetBytes == bytes,
                 "Coverage allocated additional render targets");
            if (dims.first < dims.second)
                need(tiled != combined,
                     "Sprite witness did not distinguish intentional material tiling");
        }
        sprites.releaseGraphics();
    }
    {
        Runtime pipe(data);
        pipe.loadSceneFile(data.parent_path() / "bezels/Minos.neongles");
        ImageAssets assets;
        std::vector<unsigned char> stripe(128 * 8 * 4, 255);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 128; ++x) {
                auto i = (y * 128 + x) * 4;
                auto value = static_cast<unsigned char>(
                    128 + 110 * std::sin(x * 6.283185307179586 / 128 * 3));
                stripe[i] = stripe[i + 1] = stripe[i + 2] = value;
            }
        const int source = assets.addPixels("Directional sampling witness", 128, 8, stripe);
        pipe.setImageAssets(assets);
        set(pipe, "node.18.i.2", 26);
        set(pipe, "node.18.i.5", float(source));
        set(pipe, "node.18.i.1", 255);
        set(pipe, "node.15.i.2", 26);
        set(pipe, "node.15.i.1", 255);
        for (int k = 0; k < 10; ++k)
            set(pipe, "node.18.f." + std::to_string(k), 0);
        set(pipe, "sampling.18.0.1", 1);
        set(pipe, "sampling.18.0.2", 1);
        auto shift = [&](const auto &before, const auto &after, int y0, int y1) {
            double best = 1e30;
            int result = 0;
            for (int dy = -18; dy <= 18; ++dy) {
                double error = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = 87; x < 93; ++x)
                        error += std::abs(int(after[(y * 180 + x) * 4]) -
                                          int(before[((y + dy) * 180 + x) * 4]));
                if (error < best) {
                    best = error;
                    result = dy;
                }
            }
            return result;
        };
        for (float scale : {1.f, .5f}) {
            pipe.parameters().renderScale = scale;
            set(pipe, "sampling.8.0.3", 3);
            set(pipe, "sampling.15.0.3", 3);
            set(pipe, "node.18.f.2", 0);
            auto before = render(pipe, 180, 320, 4);
            auto bytes = pipe.statistics().targetBytes;
            set(pipe, "node.18.f.2", .06f);
            auto after = render(pipe, 180, 320, 4);
            int top = shift(before, after, 20, 80), middle = shift(before, after, 125, 195),
                bottom = shift(before, after, 240, 300);
            std::cout << "Pipe scroll scale=" << scale << " shifts " << top << "," << middle << ","
                      << bottom << '\n';
            need(top != 0 && top * middle > 0 && top * bottom > 0,
                 "Scrolling reversed outside the original frame");
            need(pipe.statistics().targetBytes == bytes,
                 "Scroll coverage changed target allocations");
            set(pipe, "sampling.8.0.3", 1);
            set(pipe, "sampling.15.0.3", 1);
            set(pipe, "node.18.f.2", 0);
            before = render(pipe, 180, 320, 4);
            set(pipe, "node.18.f.2", .06f);
            after = render(pipe, 180, 320, 4);
            top = shift(before, after, 20, 80);
            middle = shift(before, after, 125, 195);
            bottom = shift(before, after, 240, 300);
            std::cout << "Intentional mirror witness shifts " << top << "," << middle << ","
                      << bottom << '\n';
            need(top * middle < 0 || middle * bottom < 0,
                 "Directional witness did not expose mirrored-scene reversal");
        }
        set(pipe, "sampling.8.0.3", 3);
        set(pipe, "sampling.15.0.3", 3);
        auto authored = render(pipe, 180, 320, 4);
        pipe.reset();
        std::ostringstream document(std::ios::binary);
        pipe.writeBaked(document);
        std::istringstream input(document.str(), std::ios::binary);
        auto prepared = Runtime::loadBaked(input, data);
        prepared->parameters().renderScale = pipe.parameters().renderScale;
        auto baked = render(*prepared, 180, 320, 4);
        for (size_t i = 0; i < authored.size(); ++i)
            if (i % 4 != 3)
                need(authored[i] == baked[i], "Prepared playback changed scene coverage");
        prepared->releaseGraphics();
        pipe.releaseGraphics();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &image);
    std::cout
        << "PASS: scene-surface sprite parity, independent material axes, uniform pipe scroll and unchanged render-target allocation.\n";
}
} // namespace neon::effects
