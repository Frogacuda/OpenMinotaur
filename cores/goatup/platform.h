#pragma once
#include "neon/math.h"
#include "neon/source_random.h"
#include <array>
#include <functional>
#include <vector>

namespace goatup {
using Vec2 = neon::vec2d;
constexpr int PlatformTiles = 40, PlatformSlots = 40;

// Native platform (0x3fc), represented by responsibilities rather than a memory
// overlay. Projected centres are also consumed by births and break/tread effects.
struct Platform {
    bool active = false, visible = false, smashable = false;
    int level = 0, theme = 0, offscreenTicks = 0, special = -1;
    float y = -12345.f, displacement = 0, thicknessScale = 1, thickness = .05f;
    std::array<float, PlatformTiles> belt{};
    std::array<int, PlatformTiles> tiles{};
    std::array<Vec2, PlatformTiles> centres{};
    std::array<bool, PlatformTiles / 2> spawnable{};
    void reset();
    void earlyLayout(bool returning);
    void layout(bool returning, neon::DarwinRandom *random,
                const std::function<void(int, float, int)> &endCap);
    int selectBirthSlot(const std::function<double(double, double)> &random) const;
};

struct SurfaceQuad {
    int frame = 0; // Resource-catalog index, already including the two literal pages.
    double alpha = 1;
    std::array<Vec2, 4> corners{};
};

// theGame::platformDraw's original pool and preparation boundary. Generation is
// deliberately synchronous in prepare(); redraw consumes the retained quads.
class TowerSurface {
  public:
    std::array<Platform, PlatformSlots> platforms;
    std::vector<SurfaceQuad> quads;
    float scroll = -.2f, angle = 0, tileStep = .05f, fade = 0;
    bool dim = false, smashableVisible = false;
    std::function<void(Platform &)> generate;
    std::function<void(Platform &)> firstOffscreen;
    TowerSurface();
    void reset(float initialY = -12345.f);
    void prepare();
    void line(Platform &);
    void column(int faces, Vec2 pos, Vec2 size);
    Platform *find(int level);

  private:
    double brightness() const;
};
} // namespace goatup
