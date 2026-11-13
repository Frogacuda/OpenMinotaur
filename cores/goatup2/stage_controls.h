#pragma once
#include "stage_runtime.h"
#include "neon/controller_session.h"
#include "neon/touch_slots.h"
#include "neon/device_motion.h"

namespace goatup2 {
enum class TouchHand { MovementLeft, MovementRight };
struct StageIntent {
    double movement = 0;
    int32_t jump = 0;
};
// Touch keeps source scaling/friction with stable region ownership and return
// strokes. Controllers enter directly as intent, independently of touches or tilt.
class StageControls {
  public:
    void pads(const neon::input::ControllerState *, int count);
    void touch(neon::input::TouchPhase, uint32_t id, neon::vec2d normalizedPoint);
    StageIntent tick(StageRuntime &, bool playerActive = true);
    void cancel();
    void motion(neon::input::MotionState value, bool enabled);
    void beginStage();
    bool anyReportInput() const;
    bool ownsTouch(uint32_t id) const;
    TouchHand hand = TouchHand::MovementLeft;
    bool fastTouchResponse = false; // source +3c9d4; factory starts false
    static float gentle(float axis);

  private:
    neon::input::ControllerFrame pads_;
    neon::input::TouchState<neon::vec2d> touches_;
    // Response anchors may slide; touchStartPos retains the gesture's region.
    neon::vec2d floatingOrigin_[4]{};
    struct GestureAxis {
        double peak = 0, retreat = 0;
        int direction = 0;
    };
    GestureAxis horizontalGesture_[4]{}, verticalGesture_[4]{};
    double signal_ = 0, digitalX_ = 0, digitalSignal_ = 0;
    int32_t touchJump_ = 0, padJump_ = 0;
    bool awaitNeutral_ = false, tilt_ = false;
    double previousMotionX_ = 0, tiltX_ = 0;
};
} // namespace goatup2
