#pragma once
#include "terrain.h"

namespace goatup2 {
// Original stage-entity contact policy at construction (0x5659c). Callers may
// override individual fields; the integer modes have distinct per-site meaning.
struct StageContactPolicy {
    int32_t ledgeResponse = 1;
    int32_t groundedWallResponse = 1;
    int32_t airborneResponse = 2;
    bool automaticVerticalHandling = true;
    bool solidCollisions = true;
};

// A borrowed original-field view, not a second persistent actor representation.
struct HorizontalActorView {
    neon::vec2d &stagePosition;
    neon::vec2d &velocity;
    double &residualImpulse;
    double &travelled;
    const int32_t &freezeTicks;
    int32_t &hopCounter;
    const int32_t &hopPeriod;
    const double &hopSpeedX;
    const double &hopSpeedY;
    const uint32_t &mediumFlags;
    const float &gravity;
    const float &effectiveGravity;
    const float &radius;
    const neon::vec2d &drawScale;
    const StageContactPolicy &policy;
    bool player;
    uint32_t tileInteractionMask;
    bool &restingContact;
    bool &hazard;
    bool &turnSignal;
    bool &horizontalContact;
    int32_t &ledgeCounter;
    int32_t &supportColumn;
    int32_t &supportRow;
};

struct HorizontalTerrainView {
    TerrainGridView grid;
    const TerrainLayout &layout;
    const TerrainBounds &bounds;
    const uint32_t &clock;
    TerrainCursor &cursor;
};

// Complete normal finite-state 0x58680 operation. No random/audio/render calls.
void moveHorizontal(HorizontalActorView actor, const HorizontalTerrainView &world);
} // namespace goatup2
