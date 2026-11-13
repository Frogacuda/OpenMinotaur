#pragma once
#include "widget_runtime.h"

namespace neon::widgets {

// Owns title semantics only. The application owns input, composition and iOX.
class TitleSequence {
  public:
    explicit TitleSequence(Value document);

    // Seed held controls so launching the application cannot also dismiss it.
    void begin(bool buttonsHeld = false, bool pointerHeld = false);
    void input(bool buttonsHeld, bool pointerHeld);
    void update(double elapsedSeconds, const Viewport &viewport);

    const Value &document() const {
        return runtime_.document();
    }
    const Frame &frame() const {
        return runtime_.frame();
    }
    Rect presentationViewport(int width, int height) const;
    double maxFrameRate() const;
    double opacity() const {
        return opacity_;
    }
    bool finished() const {
        return finished_;
    }

    // Consumed once, after the authored exit completes. The host clears touch
    // ownership and gates held controls before allowing iOX to receive input.
    bool takeLaunchRequest();

  private:
    Runtime runtime_;
    bool buttons_ = false, pointer_ = false, armed_ = true;
    bool leaving_ = false, finished_ = false, launchPending_ = false;
    double opacity_ = 0, exitBase_ = 1;
};
} // namespace neon::widgets
