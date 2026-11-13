#pragma once
#include "core_api.h"

namespace minos {
// EAGLView::drawView: average consecutive raw X samples, round to float,
// then widen to the game's double field. Presentation axes come from Host.
class MotionInput {
  public:
    double sample(mino::Host &host, bool inputFlipped, bool playing) {
        if (!requested_ || (playing && !playing_ && !available_)) {
            host.enableMotion(true);
            requested_ = true;
        }
        playing_ = playing;
        const auto motion = host.motion();
        available_ = motion.available;
        if (!motion.available) {
            previousX_ = 0;
            return 0;
        }
        const float x = float((motion.x + previousX_) * 0.5);
        previousX_ = motion.x;
        return inputFlipped ? -double(x) : double(x);
    }
    void release(mino::Host &host) {
        if (requested_)
            host.enableMotion(false);
        requested_ = false;
        playing_ = available_ = false;
        previousX_ = 0;
    }

  private:
    bool requested_ = false;
    bool playing_ = false, available_ = false;
    double previousX_ = 0;
};
} // namespace minos
