#pragma once
#include "pc_cube.h"
#include "pc_render_state.h"
#include <filesystem>
namespace neon::pc {
struct TunnelProfiles {
    std::array<float, 14 * 195> positions{}, normals{};
    static TunnelProfiles load(const std::filesystem::path &, const AssetReader &reader = {});
};
struct TunnelFrame {
    CubeFace vertices;
    Matrix4 modelView;
    Point eye, target, up;
};
class TunnelPlayback {
  public:
    void reset();
    void tick(const ShapeSettings &, const TunnelProfiles &, bool advance = true);
    TunnelFrame frame(const ShapeSettings &, const Matrix4 &parent = identityMatrix()) const;
    uint32_t phase() const {
        return word(0x28218);
    }
    bool ready() const {
        return initialized_;
    }
    void readState(const std::filesystem::path &);

  private:
    std::array<uint32_t, 0x28254 / 4> data_{};
    bool initialized_ = false;
    uint32_t word(size_t o) const {
        return data_[o / 4];
    }
    float scalar(size_t) const;
    void word(size_t o, uint32_t v) {
        data_[o / 4] = v;
    }
    void scalar(size_t, float);
    Point point(size_t) const;
    void point(size_t, Point);
    void generate(const TunnelProfiles &);
    Point path(uint32_t) const;
    Point normal(uint32_t) const;
};
} // namespace neon::pc
