#pragma once
#include "falling.h"
#include "horizontal.h"
#include "medium.h"

namespace goatup2 {
// Borrowed original fields. The two position references must designate the
// same actor field; no construction defaults are supplied by this projection.
struct VerticalActorView {
    MediumActorView medium;
    neon::vec2d &stagePosition;
    const neon::vec2d &drawScale;
    const float &radius;
    const float &buoyancy;
    const float &elasticity;
    float &effectiveGravity;
    double &travelled;
    const StageContactPolicy &policy;
    const uint32_t &motionFlags;
    const uint32_t &tileInteractionMask;
    bool &restingContact;
    bool &landed;
    bool &verticalContact;
    bool &oppositeContact259;
    bool &oppositeContact25a;
    int32_t &supportColumn;
    int32_t &supportRow;
    int32_t &forcedJumpTicks;
};
struct VerticalTerrainView {
    std::vector<uint32_t> &working;
    std::vector<uint32_t> &backup;
    const TerrainLayout &layout;
    const TerrainBounds &bounds;
    const uint32_t &clock;
    TerrainCursor &cursor;
    const std::array<neon::colour, 32> &palette;
    const std::array<uint32_t, 98> *glyphLookup = nullptr;
};

// 0x57428, including its medium visit, contacts and ordered effect/tile writes.
void moveVertical(VerticalActorView actor, const VerticalTerrainView &world,
                  MediumSharedState &medium, neon::DarwinRandom &random, MediumOutput &mediumOutput,
                  ContactParticleOutput &particles);

// 0x55e80: first medium visit, falling arithmetic, then full vertical method
// (which performs the second medium visit). Does not run horizontal movement.
void applyFalling(VerticalActorView actor, const VerticalTerrainView &world,
                  MediumSharedState &medium, neon::DarwinRandom &random, MediumOutput &mediumOutput,
                  ContactParticleOutput &particles);
} // namespace goatup2
