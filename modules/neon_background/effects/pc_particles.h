#pragma once
#include "pc_cube.h"
#include "sprite_emission.h"
#include "pc_render_state.h"
#include <filesystem>
#include <map>
namespace neon::pc {
struct PobParticle {
    std::array<float, 20> data{};
};
CubeFace particleBillboards(const ShapeSettings &, const std::array<PobParticle, 1024> &,
                            const std::array<int, 1024> *frames = nullptr, int columns = 1,
                            int rows = 1, float paletteTint = 1,
                            const std::array<SpriteParticleStyle, 1024> *styles = nullptr,
                            const std::map<int, std::array<float, 3>> *colours = nullptr);
class ParticlePlayback {
  public:
    void configureSprites(SpriteCycle cycle) {
        sequence_.configure(cycle);
    }
    bool insertSprite(int frame, unsigned count, float speed = 1, int colourSource = -1) {
        return sequence_.insert(frame, count, speed, colourSource);
    }
    const std::array<int, 1024> &spriteFrames() const {
        return spriteFrames_;
    }

    const std::array<SpriteParticleStyle, 1024> &spriteStyles() const {
        return spriteStyles_;
    }
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
    SpriteEmissionSequence sequence_;
    std::array<SpriteParticleStyle, 1024> spriteStyles_{};
    std::array<int, 1024> spriteFrames_{};
    bool warming_ = false;

    std::array<PobParticle, 1024> particles_{};
    int countdown_ = -1;
    uint32_t rng_ = 1;
    bool ready_ = false;
    float random(float);
    void spawn(PobParticle &, const ShapeSettings &, const std::array<uint32_t, 256> &);
    void update(const ShapeSettings &, const std::array<uint32_t, 256> &);
};
} // namespace neon::pc
