#pragma once
#include "effect_runtime.h"
#include <glad/gl.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace neon::effects {
inline void validateLayerDomains(const std::filesystem::path &data) {
    auto need = [](bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    };
    GLuint texture = 0, fbo = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1280, 1280, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    need(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
         "Layer-domain target failed");
    Runtime r(data);
    r.selectPreset(34);
    ImageAssets images;
    std::vector<uint8_t> pattern(128 * 128 * 4);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            auto n = (y * 128 + x) * 4;
            pattern[n] = uint8_t(x * 2);
            pattern[n + 1] = uint8_t(y * 2);
            pattern[n + 2] = uint8_t((x + y) % 128 * 2);
            pattern[n + 3] = 255;
        }
    int source = images.addPixels("Layer coordinate witness", 128, 128, pattern);
    images.at(source).mipmaps = false;
    r.setImageAssets(images);
    auto set = [&](const std::string &key, float value) {
        for (auto &e : r.authoring().controls(34))
            if (e.key == key) {
                e.enabled = true;
                e.value = value;
                e.motion = 0;
                return;
            }
        throw std::runtime_error("Layer-domain control missing: " + key);
    };
    set("node.18.i.2", 26);
    set("node.18.i.5", float(source));
    set("node.18.i.1", 255);
    set("node.8.i.2", 26);
    set("node.8.i.5", 37);
    set("node.8.i.1", 255);
    set("node.15.i.2", 14);
    set("node.15.i.5", 32);
    set("node.15.i.1", 255);
    for (int node : {8, 15, 18})
        for (int slot = 0; slot < 10; ++slot)
            set("node." + std::to_string(node) + ".f." + std::to_string(slot), 0);
    set("node.15.f.2", .5f);
    set("node.15.f.4", -1.5707963267948966f);
    auto capture = [&](int w, int h) {
        r.reset();
        for (int frame = 0; frame < 2; ++frame)
            r.frame(1. / 60, {fbo, 1280, 1280, {0, 0, w, h}, {}});
        std::vector<unsigned char> pixels(size_t(w) * h * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 1280 - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        need(glGetError() == GL_NO_ERROR, "Layer-domain GLES error");
        return pixels;
    };
    auto difference = [](const auto &a, const auto &b, int w, int h) {
        double error = 0;
        int ox = (w - 576) / 2, oy = (h - 432) / 2;
        for (int y = 0; y < 432; ++y)
            for (int x = 0; x < 576; ++x)
                for (int c = 0; c < 3; ++c)
                    error += std::abs(int(a[(y * 576 + x) * 4 + c]) -
                                      int(b[((y + oy) * w + x + ox) * 4 + c]));
        return error / (576 * 432 * 3);
    };
    for (float scale : {1.f, .5f})
        for (int address : {1, 2, 3}) {
            r.parameters().renderScale = scale;
            set("sampling.15.0.1", float(address));
            set("sampling.15.0.2", float(address));
            set("sampling.15.0.3", 1);
            auto reference = capture(576, 432);
            for (auto dims : {std::pair<int, int>{768, 432},
                              {576, 1024},
                              {576, 768},
                              {1152, 432},
                              {576, 1152}}) {
                auto extended = capture(dims.first, dims.second);
                double error = difference(reference, extended, dims.first, dims.second);
                std::cout << "Layer domain scale=" << scale << " address=" << address
                          << " size=" << dims.first << "x" << dims.second
                          << " central RGB error=" << error << "\n";
                need(error < (scale == 1 ? .8 : 1.5),
                     "Aspect change moved the rotated layer's reference content");
            }
            if (scale == 1 && address == 2) {
                set("sampling.15.0.3", 2);
                auto old = capture(576, 1024);
                need(difference(reference, old, 576, 1024) > 5,
                     "Layer-domain witness did not expose the old portrait failure");
                set("sampling.15.0.3", 1);
            }
        }
    set("node.15.f.2", 0);
    set("node.15.f.4", 0);
    set("sampling.15.0.1", 2);
    set("sampling.15.0.2", 2);
    set("sampling.15.0.3", 1);
    set("node.16.f.4", .35f);
    set("node.16.f.6", .5f);
    set("node.16.f.7", .5f);
    set("node.16.f.9", 0);
    set("node.16.f.10", 0);
    for (int shader : {94, 95})
        for (float axis : {0.f, 1.f})
            for (float scale : {1.f, .5f}) {
                set("node.15.i.2", float(shader));
                set("node.16.f.5", axis);
                r.parameters().renderScale = scale;
                auto reference = capture(576, 432);
                for (auto dims : {std::pair<int, int>{768, 432}, {576, 1024}, {576, 768}}) {
                    auto extended = capture(dims.first, dims.second);
                    double error = difference(reference, extended, dims.first, dims.second);
                    std::cout << "Barrel reference shader=" << shader << " axis=" << axis
                              << " scale=" << scale << " size=" << dims.first << "x" << dims.second
                              << " RGB error=" << error << "\n";
                    need(error < (scale == 1 ? 1. : 2.),
                         "Aspect change stretched the barrel's reference geometry");
                }
            }
    r.releaseGraphics();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    std::cout
        << "PASS: rotated-layer repeat/mirror/clamp keep reference coordinates at full/half resolution in wide and portrait views.\n";
}
} // namespace neon::effects
