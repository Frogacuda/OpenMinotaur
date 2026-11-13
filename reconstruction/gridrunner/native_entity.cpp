#include "neon/entity_motion.h"
#include "native_entity.h"
#include "neon/foundation_rules.h"
#include <cmath>
#include <cstring>
namespace gr::reconstruction::ios {
int32_t add32(int32_t value, int32_t delta) {
    return neon::entities::add32(value, delta);
}
void Vec2::rotateBy(double a) {
    neon::rotateClockwise(x, y, a, neon::ZeroAngleRule::PreserveCoordinates);
}
void Entity::run() {
    neon::entities::integrate(position, velocity, damping, age, noWrap, wrapped, wrapExtent);
}
void Tao::initTrail() {
    arrival = 0;
    arrivalStep = .005f;
    trailFrame = 0;
    trail.fill(position);
}
void Tao::run() {
    // iOS bottomLaser inlines this prefix at 0x4c168; original Tao::run does
    // not call Entity::run, integrate velocity or increment Entity::age.
    trail[trailFrame & 511] = position;
    ++trailFrame;
    if (arrivalStep > 0) {
        const float nextArrival = arrivalStep + arrival;
        arrival = nextArrival;
        arrivalStep = float(double(arrivalStep) * 1.01); // double literal at iOS 0x4a7a0
        if (nextArrival >= 1) {
            arrival = 1;
            arrivalStep = 0;
        }
    }
}
void Ship::rerez(bool alternateLayout) {
    // Original iOS 0x497d8 init and Ship::run respawn branch; Android rerez
    // 0x8b218 confirms the same order. Preserve fire timers across rerez.
    tint.unpack(0x20ff40ff);
    position = {0, alternateLayout ? .35 : .1};
    velocity = {};
    initTrail();
    state = State::Arriving;
}
void Ship::initialize(bool alternateLayout) {
    radius = .02f;
    scale = {1.33, 1.33};
    type = 8;
    sprite = 4;
    rerez(alternateLayout);
    fireTimer = 0;
    firePeriod = 20;
    fireBoost = 0;
}
} // namespace gr::reconstruction::ios
