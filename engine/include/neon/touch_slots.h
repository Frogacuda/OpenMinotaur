#pragma once
#include <cstddef>
#include <cstdint>
#include "neon/integer.h"
namespace neon::input {
// Bounded slot operations shared by the legacy field binding and later gesture
// records. Policies and pause gestures are supplied by the game module.
template <class Predicate> int firstSlot(int count, Predicate matches) {
    for (int i = 0; i < count; ++i)
        if (matches(i))
            return i;
    return -1;
}
template <class Active, class Better> int selectSlot(int count, Active active, Better better) {
    int best = -1;
    for (int i = 0; i < count; ++i)
        if (active(i) && (best < 0 || better(i, best)))
            best = i;
    return best;
}
enum class TouchTracking { Basic, MoveCounter, FrameStamped };
template <class Point, std::size_t Capacity = 4> struct TouchState {
    static constexpr int TouchCapacity = int(Capacity);
    using PointType = Point;
    Point joyOrigin[Capacity]{}, joyDelta[Capacity]{};
    bool joyActive[Capacity]{};
    unsigned joyTouchId[Capacity]{};
    Point touchPos[Capacity]{};
    unsigned touchId[Capacity]{};
    int touchIdleFrames[Capacity]{};
    bool touchActive[Capacity]{}, touchFlag[Capacity]{};
    Point touchLast[Capacity]{}, touchMove[Capacity]{};
    int touchStartFrame[Capacity]{};
    Point touchStartPos[Capacity]{}, touchPrevPos[Capacity]{};
};
template <class State, class Point>
int beginTouch(State &state, unsigned id, Point point, TouchTracking tracking, int frame = 0) {
    const int i = firstSlot(State::TouchCapacity, [&](int k) { return !state.touchActive[k]; });
    if (i >= 0) {
        state.touchActive[i] = true;
        state.touchPos[i] = point;
        state.touchId[i] = id;
        if (tracking != TouchTracking::Basic) {
            state.touchIdleFrames[i] = 0;
            state.touchStartPos[i] = point;
            state.touchStartFrame[i] = tracking == TouchTracking::FrameStamped ? frame : 0;
            if (tracking == TouchTracking::MoveCounter) {
                state.touchFlag[i] = false;
                state.touchPrevPos[i] = point;
            }
        }
    }
    return i;
}
template <class State, class Point>
bool claimTouchJoystick(State &state, int zone, unsigned id, Point point) {
    if (zone < 0 || zone >= State::TouchCapacity || state.joyActive[zone])
        return false;
    state.joyActive[zone] = true;
    state.joyTouchId[zone] = id;
    state.joyOrigin[zone] = point;
    state.joyDelta[zone] = {0, 0};
    return true;
}
template <class State> void endTouch(State &state, unsigned id) {
    for (int i = 0; i < State::TouchCapacity; ++i)
        if (state.touchActive[i] && state.touchId[i] == id)
            state.touchActive[i] = false;
    const int i = firstSlot(State::TouchCapacity, [&](int k) { return state.joyTouchId[k] == id; });
    if (i >= 0) {
        state.joyActive[i] = false;
        state.joyTouchId[i] = 0x3d0dead;
    }
}
template <class State, class Point>
void moveTouch(State &state, unsigned id, Point point, TouchTracking tracking) {
    for (int i = 0; i < State::TouchCapacity; ++i)
        if (state.touchActive[i] && state.touchId[i] == id) {
            if (tracking == TouchTracking::MoveCounter) {
                const auto last = state.touchLast[i];
                const int n = state.touchStartFrame[i] =
                    neon::integers::add32(state.touchStartFrame[i], 1);
                state.touchLast[i] = point;
                state.touchMove[i] = {last.x - point.x, last.y - point.y};
                if (n > 1)
                    state.touchFlag[i] = true;
            }
            state.touchPos[i] = point;
            if (tracking != TouchTracking::Basic)
                state.touchIdleFrames[i] = 0;
        }
    // Preserve first-ID-match semantics, including inactive/stale joystick IDs.
    const int i = firstSlot(State::TouchCapacity, [&](int k) { return state.joyTouchId[k] == id; });
    if (i >= 0)
        state.joyDelta[i] = point - state.joyOrigin[i];
}
template <class State> void purgeTouchState(State &state) {
    for (int i = 0; i < State::TouchCapacity; ++i) {
        state.joyActive[i] = false;
        state.touchActive[i] = false;
    }
}
template <class State> int oldestTouch(const State &state, int excluded, int frame) {
    int age = -1, best = -1;
    for (int i = 0; i < State::TouchCapacity; ++i)
        if (i != excluded && state.touchActive[i]) {
            const int value = neon::integers::subtract32(frame, state.touchStartFrame[i]);
            if (value > age) {
                age = value;
                best = i;
            }
        }
    return best;
}
template <class State, class Purge> void watchTouchIdle(State &state, Purge purge) {
    int active = 0, idle = 0;
    for (int i = 0; i < State::TouchCapacity; ++i)
        if (state.touchActive[i]) {
            ++active;
            state.touchIdleFrames[i] = neon::integers::add32(state.touchIdleFrames[i], 1);
            if (state.touchIdleFrames[i] > 240)
                ++idle;
        }
    if (idle != 0 && active == idle)
        purge();
}
} // namespace neon::input
