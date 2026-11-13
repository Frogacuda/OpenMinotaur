#include "falling.h"
#include <cmath>
#include <stdexcept>

namespace goatup2 {
FallingVelocity calculateFallingVelocity(double currentVelocity, double residualImpulse,
                                         float gravity, bool submerged, float buoyancyMultiplier,
                                         const TerrainLayout &terrain) {
    if (!std::isfinite(currentVelocity) || !std::isfinite(residualImpulse) ||
        !std::isfinite(gravity) || (submerged && !std::isfinite(buoyancyMultiplier)) ||
        !std::isfinite(terrain.displayReciprocal.y) || terrain.displayReciprocal.y <= 0 ||
        !std::isfinite(terrain.scale.y) || terrain.scale.y <= 0)
        throw std::invalid_argument("unsupported falling arithmetic input");

    // ITT NE guards BOTH the load and multiply; no multiplication occurs in air.
    float effectiveGravity = gravity;
    if (submerged)
        effectiveGravity = gravity * buoyancyMultiplier;
    const double velocityWithGravity = currentVelocity + double(effectiveGravity);
    const float candidate = float(residualImpulse + velocityWithGravity);
    const float cellHeight = float(terrain.displayReciprocal.y * terrain.scale.y);
    const float threshold = float(double(cellHeight) * .8);
    const double remainingImpulse = residualImpulse * .9;

    // Original sign helper treats either zero as positive. Equality with the
    // threshold rejects the candidate unless it points against gravity.
    const bool againstGravity = (effectiveGravity < 0.f) != (candidate < 0.f);
    const double velocity =
        (std::fabs(candidate) < threshold || againstGravity) ? double(candidate) : currentVelocity;
    return {velocity, remainingImpulse, effectiveGravity};
}
} // namespace goatup2
