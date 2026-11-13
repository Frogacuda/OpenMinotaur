#ifdef NDEBUG
#undef NDEBUG
#endif
#include "neon/tilt_calibration.h"
#include "display_layout.h"
#include "cores/goatup/climber.h"
#include "cores/fiveaday/world.h"
#include <cassert>
#include <iostream>
using neon::input::MotionState;
bool near(double a, double b) {
    return std::fabs(a - b) < 1e-12;
}
int main() {
    for (MotionState neutral : {MotionState{true, 0, 0, -1},
                                {true, 0, .8, -.6},
                                {true, .6, 0, -.8},
                                {true, 1, 0, 0},
                                {true, 0, 0, 1}}) {
        neon::input::TiltCalibration calibration;
        assert(calibration.set(neutral));
        auto v = calibration.apply(neutral);
        assert(near(v.x, 0) && near(v.y, 0) && near(v.z, -1));
        neon::input::TiltCalibration loaded;
        loaded.decode(calibration.encode());
        auto restored = loaded.apply(neutral);
        assert(near(restored.x, 0) && near(restored.y, 0));
        for (int turn : {-1, 0, 1, 2}) {
            mino::DisplayLayout layout;
            layout.turns = turn;
            auto game = layout.nativeMotion(neon::input::rotateMotion(v, turn));
            assert(near(game.x, 0) && near(game.y, 0));
            const auto direction = fiveaday::Input::directionFromGravity({game.x, game.y});
            assert(near(direction.x, 0) && near(direction.y, 0));
            assert(near(goatup::ClimbInput::tiltAxisFromGravity(game.x), 0));
        }
        MotionState moved{true, .12, -.18, -.97};
        v = calibration.apply(moved);
        assert(near(v.x * v.x + v.y * v.y + v.z * v.z,
                    moved.x * moved.x + moved.y * moved.y + moved.z * moved.z));
    }
    neon::input::TiltCalibration c;
    assert(!c.set({true, 0, 0, 0}));
    assert(!c.set({false, 0, 0, -1}));
    c.decode("nan,0,-1");
    auto v = c.apply({true, .2, -.3, -.9});
    assert(v.x == .2 && v.y == -.3 && v.z == -.9);
    std::cout
        << "Tilt calibration: neutral rotation, persistence, all screen turns, steering/gravity consumers and invalid samples passed\n";
}
