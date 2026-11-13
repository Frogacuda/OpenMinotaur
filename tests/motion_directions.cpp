#ifdef NDEBUG
#undef NDEBUG
#endif
#include "display_layout.h"
#include "cores/goatup/climber.h"
#include "cores/fiveaday/world.h"
#include <cassert>
#include <iostream>
#include <memory>

int main() {
    // Physical tilt survives the inverse presentation transform, then reaches
    // each game's original steering units without a second orientation policy.
    for (int turns : {-1, 0, 1, 2})
        for (double x : {-.4, 0., .4})
            for (double y : {-.4, 0., .4}) {
                mino::DisplayLayout layout;
                layout.turns = turns;
                mino::MotionState native{true, x, y, -.8};
                const auto screen = neon::input::rotateMotion(native, -turns);
                const auto restored = layout.nativeMotion(screen);
                assert(restored.x == x && restored.y == y && restored.z == -.8);
                const auto direction =
                    fiveaday::Input::directionFromGravity({restored.x, restored.y});
                auto world = std::make_unique<fiveaday::World>();
                fiveaday::Ship tilt(*world), pad(*world);
                tilt.steer({direction, true});
                pad.steer({{x, -y}, true});
                assert(tilt.targetHeading == pad.targetHeading && tilt.turnRate == pad.turnRate);
                goatup::TowerSurface tower;
                goatup::ClimbContext context(tower);
                context.contacts = [] {};
                goatup::Climber goatTilt, goatPad;
                goatTilt.step(context, {goatup::ClimbInput::Method::Accelerometer,
                                        goatup::ClimbInput::tiltAxisFromGravity(restored.x)});
                goatPad.step(context, {goatup::ClimbInput::Method::DirectAxis, x});
                assert(goatTilt.velocity.x == goatPad.velocity.x &&
                       goatTilt.heading == goatPad.heading);
                if (x)
                    assert(goatTilt.velocity.x * x > 0);
            }
    std::cout
        << "Gravity directions reach original GoatUp drive and Five a Day steering across four screen rotations\n";
}
