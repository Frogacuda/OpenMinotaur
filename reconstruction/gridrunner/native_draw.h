#pragma once
#include "native_campaign.h"
#include "native_star.h"
namespace gr::reconstruction::ios {
struct EntityDrawSample {
    bool visible = false;
    Rgba tint;
};
// One original Entity::draw invocation, including its counter side effects.
// Called while preparing a logical frame, never by a host redraw.
EntityDrawSample sampleEntityDraw(Entity &, int32_t frame, Rgba tint);

// A sprite draw, not an enemy representation. Owns its sampled values and
// remains valid after the original object is changed or retired.
struct DrawSprite {
    Vec2 position, scale{1, 1};
    Rgba tint;
    int32_t frame = 0;
    float angle = 0, depth = 0, diameter = 0;
    // LaserBolt bypasses Entity::draw. Preserve its separate pixel-height
    // rounding and warning-width calculation in the host adapter.
    int32_t beamRow = -1;
    float beamRadius = 0, chargeFraction = 0;
    bool charging = false;
};
class DrawFrame {
  public:
    // Once after each native update; sprites() is a read-only snapshot.
    void prepare(Campaign &);
    void prepare(Starfield &);
    const std::vector<DrawSprite> &sprites() const {
        return sprites_;
    }
    void clear() {
        sprites_.clear();
    }

  private:
    std::vector<DrawSprite> sprites_;
    void entity(Entity &, Vec2 position, Vec2 scale, int32_t frame, Rgba tint);
    void tao(Tao &);
    void ship(Ship &, const Campaign &);
    void zakato(Zakato &, const Campaign &);
};
} // namespace gr::reconstruction::ios
