#include "horizontal.h"
#include "neon/integer.h"
#include <cmath>

namespace goatup2 {
void moveHorizontal(HorizontalActorView actor, const HorizontalTerrainView &world) {
    actor.horizontalContact = false;
    if (actor.freezeTicks != 0)
        return;
    double displacement = actor.residualImpulse + actor.velocity.x;
    actor.velocity.x = displacement;
    actor.residualImpulse = actor.residualImpulse * .9;
    if (actor.hopCounter >= 0 && (actor.mediumFlags & 1U)) {
        actor.hopCounter = neon::integers::add32(actor.hopCounter, 1);
        if (actor.hopPeriod < actor.hopCounter) {
            actor.hopCounter = 0;
            displacement = actor.drawScale.x > 0 ? -actor.hopSpeedX : actor.hopSpeedX;
            actor.velocity.x = displacement;
            actor.velocity.y = actor.gravity > 0 ? -actor.hopSpeedY : actor.hopSpeedY;
        }
    }
    if (!actor.policy.solidCollisions) {
        actor.restingContact = false;
        actor.stagePosition.x = displacement + actor.stagePosition.x;
        actor.travelled = std::fabs(displacement) + actor.travelled;
        return;
    }

    auto query = [&](float x, float y) {
        return queryTerrain(world.grid, world.layout, world.bounds, x, y, world.clock,
                            world.cursor);
    };
    auto directional = [&](uint32_t tile) {
        if (actor.player && ((actor.velocity.x > 0 && (tile & 255U) == 90U) ||
                             (actor.velocity.x < 0 && (tile & 255U) == 91U)))
            return uint32_t(0);
        return tile;
    };
    auto exactMask = [&](uint32_t tile) {
        return (tile & actor.tileInteractionMask) == 0x400000U ? 0U : tile;
    };
    const float cellHeight = float(world.layout.displayReciprocal.y * world.layout.scale.y);
    const double paddedHeight =
        double(cellHeight) + double(actor.radius) * std::fabs(actor.drawScale.y);
    float supportOffset = float(paddedHeight * double(.65f));
    float bodyOffset = supportOffset - cellHeight;
    if (actor.effectiveGravity < 0) {
        supportOffset = float(paddedHeight * double(-.65f));
        bodyOffset = cellHeight + supportOffset;
    }
    // Probe the leading edge in the direction of travel (IT LE at 0x58740).
    float halfRadius = (displacement > 0 ? actor.radius : -actor.radius) * .5f;
    float candidate = float(actor.stagePosition.x + displacement);
    const float probeX = halfRadius + candidate;
    const float bodyY = float(double(bodyOffset) + actor.stagePosition.y);
    const float supportY = float(double(supportOffset) + actor.stagePosition.y);
    const uint32_t first = directional(query(probeX, bodyY));
    const uint32_t centre = directional(query(probeX, float(actor.stagePosition.y)));
    uint32_t support = exactMask(query(probeX, supportY));
    uint32_t combined = exactMask(centre | first);
    support = directional(support);
    if (combined & 0x80000U)
        actor.hazard = true;

    if (!(support & 0x10000U) && actor.restingContact) {
        const auto mode = actor.policy.ledgeResponse;
        if (mode == 3)
            actor.turnSignal = true;
        else if (mode == 2) {
            const double old = actor.velocity.x;
            actor.velocity.x = 0;
            candidate = float(actor.stagePosition.x - old);
        } else if (mode == 1) {
            const double old = actor.velocity.x;
            actor.velocity.x = -old;
            candidate = float(actor.stagePosition.x - old);
            combined = query(candidate - halfRadius, bodyY);
            halfRadius = -halfRadius;
            // This reverse-probe rule is deliberately stronger than exactMask.
            if (combined & actor.tileInteractionMask)
                combined = 0;
            if (combined & 0x80000U)
                actor.hazard = true;
        }
        actor.ledgeCounter = 10;
    }

    const bool blocked = actor.policy.automaticVerticalHandling ? (combined & 0x30000U) == 0x30000U
                                                                : (combined & 0x30000U) != 0;
    bool stopped = false;
    if (blocked) {
        const int32_t mode = actor.restingContact ? actor.policy.groundedWallResponse
                                                  : actor.policy.airborneResponse;
        actor.horizontalContact = true;
        if (mode == 3) {
            actor.velocity.x = -actor.velocity.x;
            actor.turnSignal = true;
        } else if (mode == 2) {
            const double old = actor.velocity.x;
            actor.velocity.x = 0;
            candidate = float(double(candidate) - old);
            stopped = true;
        } else if (mode == 1) {
            const double old = actor.velocity.x;
            actor.velocity.x = -old;
            candidate = float(actor.stagePosition.x - old);
            const auto reverse = query(candidate - halfRadius, bodyY);
            if (reverse & 0x80000U)
                actor.hazard = true;
        }
    }
    if (!stopped && actor.velocity.x != 0 && !std::isnan(actor.velocity.x))
        actor.travelled = std::fabs(actor.stagePosition.x - double(candidate)) + actor.travelled;
    actor.stagePosition.x = double(candidate);
    if (actor.restingContact) {
        if (query(candidate, supportY) & 0x80000U)
            actor.hazard = true;
        actor.supportColumn = world.cursor.column;
        actor.supportRow = world.cursor.row;
    }
}
} // namespace goatup2
