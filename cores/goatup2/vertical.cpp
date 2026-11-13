#include "vertical.h"
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace goatup2 {
namespace {
constexpr uint32_t breakableIds[] = {32, 33, 34, 39, 40, 41, 42};
constexpr uint32_t fragmentSprites[] = {145, 146, 0, 147, 148, 149, 150};

std::optional<uint32_t> fragmentFor(uint32_t tile) {
    for (unsigned i = 0; i < 7; ++i)
        if ((tile & 255U) == breakableIds[i]) {
            const uint32_t encoded = fragmentSprites[i] | (((tile >> 8) & 255U) << 16);
            if (encoded != 0xffffffffU)
                return encoded;
        }
    return std::nullopt;
}
bool isBreakable(uint32_t tile) {
    for (auto id : breakableIds)
        if ((tile & 255U) == id)
            return true;
    return false;
}
uint32_t query(const VerticalTerrainView &w, float x, float y) {
    return queryTerrain({w.working, w.backup, w.glyphLookup}, w.layout, w.bounds, x, y, w.clock,
                        w.cursor);
}
uint32_t masked(uint32_t tile, uint32_t mask) {
    return (tile & mask) == 0x400000U ? 0U : tile;
}
// Original positive indices can cross the physical 100-column row boundary.
// Preserve adjacent backup storage, rather than clamping to visible dimensions.
uint32_t &physicalTile(const VerticalTerrainView &w, int32_t column, int32_t row) {
    if (column < 0 || row < 0)
        throw std::out_of_range("negative tile mutation");
    return physicalTerrainWord({w.working, w.backup}, column, row);
}
uint32_t rawVisibleTile(const VerticalTerrainView &w, int32_t column, int32_t row) {
    if (column < 0 || row < 0 || column >= w.layout.columns || row >= w.layout.rows)
        return 0;
    return physicalTile(w, column, row);
}
void clearContactTiles(const VerticalTerrainView &w) {
    physicalTile(w, w.cursor.column, w.cursor.row) = 0;
    const auto column = w.cursor.column, row = w.cursor.row;
    for (int32_t neighbour : {column - 1, column + 1}) {
        // The original neighbour reader checks only nonnegative coordinates.
        if (neighbour >= 0 && row >= 0 && isBreakable(physicalTile(w, neighbour, row)))
            physicalTile(w, neighbour, row) = 0;
    }
}
int32_t truncatedIndex(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("nonfinite contact coordinate");
    if (value >= double(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    if (value <= double(std::numeric_limits<int32_t>::min()))
        return std::numeric_limits<int32_t>::min();
    return int32_t(value);
}
void checkBinding(const VerticalActorView &a) {
    if (&a.stagePosition != &a.medium.stagePosition)
        throw std::invalid_argument("vertical and medium views must bind the same position");
}
} // namespace

void moveVertical(VerticalActorView a, const VerticalTerrainView &w, MediumSharedState &shared,
                  neon::DarwinRandom &random, MediumOutput &mediumOutput,
                  ContactParticleOutput &particles) {
    checkBinding(a);
    auto &velocity = a.medium.velocity;
    // Snapshot these geometry inputs before medium's synchronous emissions.
    const double scaleY = w.layout.scale.y, reciprocalY = w.layout.displayReciprocal.y;
    auto tile = query(w, float(a.stagePosition.x), float(a.stagePosition.y));
    updateMedium(a.medium, shared, tile, random, mediumOutput);
    a.effectiveGravity = a.medium.gravity;
    if (a.medium.flags & 1U)
        a.effectiveGravity = a.medium.gravity * a.buoyancy;
    const double cellHeight = double(float(reciprocalY * scaleY));
    const double radiusHeight = double(a.radius) * std::fabs(a.drawScale.y);
    float offset = float((cellHeight + radiusHeight) * double(.65f));
    if (velocity.y < 0)
        offset = -offset; // IT MI at 0x574f0
    const double supportOffset = double(offset);
    tile = masked(query(w, float(a.stagePosition.x), float(supportOffset + a.stagePosition.y)),
                  a.tileInteractionMask);
    if (a.restingContact && a.policy.solidCollisions) {
        if (tile & 0x80000U)
            a.medium.hazard = true;
        if (tile & 0x10000U) {
            velocity.y = 0;
            a.supportColumn = w.cursor.column;
            a.supportRow = w.cursor.row;
            return;
        }
        a.restingContact = false;
    }
    if ((a.motionFlags & 8U) && (tile & 0x30000U)) {
        const double old = velocity.y;
        velocity.y = -old;
        a.stagePosition.y = a.stagePosition.y - old;
        a.verticalContact = true;
        return;
    }
    a.stagePosition.y = a.stagePosition.y + velocity.y;
    a.travelled = std::fabs(velocity.y) + a.travelled;
    tile = masked(query(w, float(a.stagePosition.x), float(supportOffset + a.stagePosition.y)),
                  a.tileInteractionMask);
    if (a.medium.player &&
        ((velocity.x > 0 && (tile & 255U) == 90) || (velocity.x < 0 && (tile & 255U) == 91)))
        tile = 0;
    if (tile & 0x80000U)
        a.medium.hazard = true;
    if (!a.policy.automaticVerticalHandling) {
        if (!(tile & 0x30000U))
            return;
        a.verticalContact = true;
        if (a.policy.airborneResponse == 2) {
            a.stagePosition.y = a.stagePosition.y - velocity.y;
            velocity.y = 0;
        } else if (a.policy.airborneResponse == 1) {
            a.stagePosition.y = a.stagePosition.y - velocity.y;
            velocity.y = -(velocity.y * double(a.elasticity));
        }
        return;
    }
    if (!a.policy.solidCollisions || a.medium.gravity == 0)
        return;
    const double oldVelocity = velocity.y;
    const float narrowVelocity = float(oldVelocity);
    auto particleSource = [&]() {
        return ParticleSourceView{a.stagePosition, a.drawScale, a.radius, a.medium.gravity};
    };
    const ParticleSoundSettings sound{shared.sampleVolume, shared.splashSoundSuppressed};
    auto breakLanding = [&](uint32_t raw) {
        if (!a.medium.player ||
            (a.forcedJumpTicks == 0 && !(double(std::fabs(narrowVelocity)) > .009)))
            return false;
        const auto sprite = fragmentFor(raw);
        if (!sprite)
            return false;
        emitTileFragments(particleSource(), sound, w.palette, 10, *sprite, random, particles);
        a.forcedJumpTicks = a.forcedJumpTicks / 4; // source signed truncation toward zero
        clearContactTiles(w);
        return true;
    };
    auto snapLanding = [&](bool positiveGravity) {
        const double cellX = w.layout.scale.x * w.layout.displayReciprocal.x;
        const double cellY = w.layout.scale.y * w.layout.displayReciprocal.y;
        const float shiftedX = float(a.stagePosition.x) + w.bounds.halfWidth;
        const float shiftedY = float(supportOffset + a.stagePosition.y) + w.bounds.halfHeight;
        w.cursor.column = truncatedIndex(double(shiftedX) / cellX);
        w.cursor.row = truncatedIndex(double(shiftedY) / cellY);
        const double row = double(float(w.cursor.row));
        const double centre = cellY * .5 + (cellY * row - double(w.bounds.halfHeight));
        const double padding = double(a.radius) * std::fabs(a.drawScale.y) * double(.65f);
        a.stagePosition.y =
            positiveGravity ? (centre - cellHeight) - padding : (cellHeight + centre) + padding;
        velocity.y = 0;
    };
    if (a.medium.gravity > 0) {
        if ((tile & 0x10000U) && oldVelocity >= 0) {
            a.supportColumn = w.cursor.column;
            a.supportRow = w.cursor.row;
            const auto raw = rawVisibleTile(w, w.cursor.column, w.cursor.row);
            snapLanding(true);
            if (breakLanding(raw))
                return;
            const auto above = rawVisibleTile(w, a.supportColumn, a.supportRow - 1);
            if (above & 0x30000U)
                return; // original suppresses normal landing flags here
            a.restingContact = true;
            a.landed = true;
            a.verticalContact = true;
            return;
        }
        if (!(tile & 0x20000U) || oldVelocity >= 0)
            return;
    } else {
        if ((tile & 0x10000U) && oldVelocity <= 0) {
            a.supportColumn = w.cursor.column;
            a.supportRow = w.cursor.row;
            const auto raw = rawVisibleTile(w, w.cursor.column, w.cursor.row);
            if (breakLanding(raw))
                return; // negative gravity breaks BEFORE snapping
            snapLanding(false);
            a.restingContact = true;
            a.landed = true;
            a.verticalContact = true;
            return;
        }
        if (!(tile & 0x20000U) || oldVelocity <= 0)
            return;
    }
    if (!(a.medium.flags & 1U))
        velocity.y = 0;
    else if (double(std::fabs(narrowVelocity)) >= .001)
        velocity.y = double(narrowVelocity) * (-.2);
    else
        velocity.y = (narrowVelocity < 0 ? -1. : 1.) * (-.001);
    if (a.medium.player) {
        if (!(a.medium.flags & 1U))
            emitImpactParticles(particleSource(), sound, 10, random, particles);
        const auto sprite = fragmentFor(tile);
        if (sprite) {
            emitTileFragments(particleSource(), sound, w.palette, 20, *sprite, random, particles);
            clearContactTiles(w);
        } else
            a.forcedJumpTicks = 0;
    }
    a.oppositeContact259 = true;
    a.oppositeContact25a = true;
}

void applyFalling(VerticalActorView a, const VerticalTerrainView &w, MediumSharedState &shared,
                  neon::DarwinRandom &random, MediumOutput &mediumOutput,
                  ContactParticleOutput &particles) {
    checkBinding(a);
    const auto tile = query(w, float(a.stagePosition.x), float(a.stagePosition.y));
    updateMedium(a.medium, shared, tile, random, mediumOutput);
    const bool wet = (a.medium.flags & 1U) != 0;
    // Do not load the unused original buoyancy field on the dry branch.
    const float buoyancy = wet ? a.buoyancy : 1.f;
    const auto result = calculateFallingVelocity(a.medium.velocity.y, a.medium.verticalImpulse,
                                                 a.medium.gravity, wet, buoyancy, w.layout);
    a.medium.velocity.y = result.verticalVelocity;
    a.medium.verticalImpulse = result.residualImpulse;
    a.effectiveGravity = result.effectiveGravity;
    moveVertical(a, w, shared, random, mediumOutput, particles);
}
} // namespace goatup2
