#pragma once
namespace neon::input {
// Gravity-inclusive acceleration in g, pointing with gravity (CoreMotion sign).
// X points right, Y up, Z towards the player. Platform bindings rotate device
// axes into the game's displayed axes; controllers remain a separate modality.
struct MotionState {
    bool available = false;
    double x = 0, y = 0, z = 0;
};
inline MotionState rotateMotion(MotionState value, int quarterTurns) {
    const int count = (quarterTurns % 4 + 4) % 4;
    for (int n = 0; n < count; ++n) {
        const double x = value.x;
        value.x = -value.y;
        value.y = x;
    }
    return value;
}
} // namespace neon::input
