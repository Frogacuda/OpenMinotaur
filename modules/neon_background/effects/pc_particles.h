#pragma once
#include "pc_cube.h"
#include "pc_render_state.h"
#include <filesystem>
namespace neon::pc {
struct PobParticle {
    std::array<float, 20> data{};
};
CubeFace particleBillboards(const ShapeSettings &, const std::array<PobParticle, 1024> &);
class ParticlePlayback {
  public:
    void reset();
    void tick(const ShapeSettings &, const std::array<uint32_t, 256> &);
    void readState(const std::filesystem::path &);
    const std::array<PobParticle, 1024> &particles() const {
        return particles_;
    }
    int countdown() const {
        return countdown_;
    }
    bool ready() const {
        return ready_;
    }

  private:
    std::array<PobParticle, 1024> particles_{};
    int countdown_ = -1;
    uint32_t rng_ = 1;
    bool ready_ = false;
    float random(float);
    void spawn(PobParticle &, const ShapeSettings &, const std::array<uint32_t, 256> &);
    void update(const ShapeSettings &, const std::array<uint32_t, 256> &);
};
} // namespace neon::pc
