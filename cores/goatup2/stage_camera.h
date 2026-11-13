#pragma once
#include "terrain.h"

namespace goatup2 {
// StageTemplate's camera, in grid units. Requests precede player movement;
// smoothing follows terrain submission. The current offset belongs to the
// stage runtime and is also used by actor/history coordinate conversion.
class StageCamera {
  public:
    void measure(const TerrainLayout &, TerrainGridView, int visibleColumns, int visibleRows);
    void request(neon::vec2d localPosition, const TerrainLayout &, TerrainBounds,
                 double facingScaleX, double verticalVelocity);
    void finishTerrain(neon::vec2d &currentOffset, neon::vec2d playerVelocity,
                       double horizontalImpulse, double verticalImpulse, bool editing,
                       bool attract);
    neon::vec2d requested{0, 0}, backgroundRequest{0, 0};
    neon::vec2d upper{0, 0}, lower{0, 0};
    int fastFrames = 0;
};
} // namespace goatup2
