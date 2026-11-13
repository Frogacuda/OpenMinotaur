#pragma once
#include "terrain.h"

namespace goatup2 {
// Results of the original falling arithmetic block after medium handling.
// No entity construction, position integration, collision or event execution.
struct FallingVelocity {
    double verticalVelocity;
    double residualImpulse;
    float effectiveGravity;
};

// 0x55ec2..0x55f70. Inputs are the state AFTER the first medium update.
// Uses finite scalar inputs and positive finite terrain scale/reciprocals.
// An unused dry-state buoyancy value is not evaluated or validated.
FallingVelocity calculateFallingVelocity(double currentVelocity, double residualImpulse,
                                         float gravity, bool submerged, float buoyancyMultiplier,
                                         const TerrainLayout &terrain);
} // namespace goatup2
