#pragma once
#include "tube_layout.h"
#include <array>
#include <cstddef>
#include <string>
#include <vector>
namespace mino {
const unsigned char *royaleArchive(std::size_t &size);
const unsigned char *consumerArchive(std::size_t &size);
// Optional presentation-only pipeline. Owns programs, LUTs and per-viewport
// intermediate targets; original game/resource/effect execution stays separate.
class RoyalePipeline {
  public:
    enum class Profile { Royale, Consumer };
    bool configure(bool es, const TubeLayout &layout, Profile profile = Profile::Royale);
    // An optional linear RGB8 destination lets the final shader replace the
    // separate presentation copy. Dimensions are physical viewport pixels.
    struct Output {
        unsigned framebuffer = 0;
        int x = 0, y = 0, width = 0, height = 0;
        bool clockwise = false;
    };
    // A native-sized, upright RGB8 source already is the tube input; Consumer
    // can sample it directly instead of copying it through an identical target.
    unsigned run(unsigned source, unsigned quadVbo, const Output *output = nullptr,
                 bool canonicalRGB8 = false);
    void releaseTargets();
    void shutdown();
    void abandonGraphics() {
        const auto frame = frame_;
        const auto profile = profile_;
        *this = RoyalePipeline{};
        profile_ = profile;
        frame_ = frame;
    }
    const std::string &error() const {
        return error_;
    }

  private:
    friend struct RoyaleChecks;
    bool optimize_ = true;
    Profile profile_ = Profile::Royale;
    int passCount_ = 14;
    struct Target {
        unsigned texture = 0, fbo = 0;
        int w = 0, h = 0;
    };
    struct Sampler {
        int location = -1, index = -1;
    }; // -1 current input; -2 original; 0..13 pass; 14..19 LUT
    struct Size {
        int location = -1, index = -1;
    };
    struct Pass {
        unsigned program = 0;
        int output = -1, input = -1, texture = -1, frame = -1, direction = -1, matrix = -1;
        bool constant = false, valid = false;
        std::vector<Sampler> samplers;
        std::vector<Size> sizes;
    };
    bool initialize(bool es);
    bool target(Target &out, int w, int h, unsigned format);
    unsigned program(const std::string &source, bool es, const std::string &label);
    void draw(unsigned vbo);
    Target source_;
    std::array<Target, 14> targets_{};
    std::array<Pass, 14> passes_{};
    std::array<Target, 6> masks_{};
    unsigned blit_ = 0;
    int turn_ = -1, blitTex_ = -1;
    TubeLayout layout_;
    bool es_ = false, ready_ = false;
    unsigned frame_ = 0;
    std::string error_;
};
} // namespace mino
