// Independently source-derived literals for the falling arithmetic block.
// Explicit scalar setup; not a whole falling/medium/contact execution witness.
#include "falling.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
int main() {
    try {
        using namespace goatup2;
        // Height 5/32 gives an exactly representable threshold of 1/8.
        auto terrain = calculateTerrainLayout(100, 100, 32, 32, {1, 5}, nullptr);
        auto step = [&](double velocity, double impulse, float gravity, bool wet = false,
                        float buoyancy = -.5f) {
            return calculateFallingVelocity(velocity, impulse, gravity, wet, buoyancy, terrain);
        };
        auto r = step(0, 0, .03125f, false, 0);
        CHECK(r.verticalVelocity == .03125 && r.effectiveGravity == .03125f &&
              r.residualImpulse == 0);
        r = step(0, 0, .03125f, false, std::numeric_limits<float>::quiet_NaN());
        CHECK(r.verticalVelocity == .03125); // original dry branch does not load buoyancy
        r = step(.09375, 0, .03125f);
        CHECK(r.verticalVelocity == .09375); // candidate equals limit: keep old velocity
        r = step(.1, 0, .03125f);
        CHECK(r.verticalVelocity == .1); // no clamp, no gratuitous narrowing of retained double
        r = step(-.09375, 0, -.03125f);
        CHECK(r.verticalVelocity == -.09375);
        r = step(-.5, 0, .03125f);
        CHECK(r.verticalVelocity == -.46875); // opposite gravity bypasses speed rejection
        r = step(.5, 0, -.03125f);
        CHECK(r.verticalVelocity == .46875);
        r = step(-.5, -.25, 0);
        CHECK(r.verticalVelocity == -.75 && r.residualImpulse == -.225);
        r = step(.0625, 0, .03125f, true);
        CHECK(r.effectiveGravity == -.015625f && r.verticalVelocity == .046875);
        r = step(.1, .0625, .03125f);
        CHECK(r.verticalVelocity == .1 &&
              r.residualImpulse == .05625); // rejected step still damps impulse
        r = step(.124999999, 0, 0);
        CHECK(r.verticalVelocity ==
              .124999999); // candidate rounds UP to the rejected float threshold
        r = step(-1e16, 1e16, .03125f);
        CHECK(r.verticalVelocity == 0 && r.residualImpulse == 9e15); // retain source sum grouping

        // A negative-zero gravity must not turn sign(0) into the negative sign.
        r = step(-.25, -.25, -0.f);
        CHECK(r.verticalVelocity == -.5 && std::signbit(r.effectiveGravity));

        // Source multiplies the widened float cell height by DOUBLE .8. A
        // float .8f instead raises the limit by one ulp and accepts this step.
        terrain = calculateTerrainLayout(100, 100, 32, 32, {1, 0x1.000002p+5}, nullptr);
        r = step(0x1.59999cp-1, 0, .125f);
        CHECK(r.verticalVelocity == 0x1.59999cp-1);

        bool rejected = false;
        try {
            step(0, 0, .03125f, true, std::numeric_limits<float>::quiet_NaN());
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        CHECK(rejected);
        std::cout << "Falling arithmetic witnesses passed: conditional buoyancy, strict rejection, "
                     "sign/zero rules, "
                     "impulse decay and source rounding/association. Medium/contact execution "
                     "remains outside scope.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
