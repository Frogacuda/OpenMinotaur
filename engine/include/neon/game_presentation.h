#pragma once

namespace neon {

struct PresenterRules {
    bool purgeBefore = true;
    int blendCalls = 2, projectionFlags = 0, sampleFrame = 0x8b;
};

struct FeedbackState {
    bool inputFlipped = false;
    float rotation = 0, x = 0, y = 0, scaleX = 1, scaleY = 1;
    float colour[4] = {};
};

// Borrowed by an effect for its entire lifetime. The module owns the game.
// DrawTarget/TextGrid bind renderer types without coupling to a game class.
// prepare/release preserve the original showJoy resource lifecycle. They must
// be idempotent: release may be followed by the presenter's destructor.
// Simulation timing and input remain with the module; this interface never ticks.
template <class DrawTarget, class TextGrid> class GamePresentationHost {
  public:
    virtual ~GamePresentationHost() = default;
    virtual void prepareGame() = 0;
    virtual void releaseGame() = 0;
    virtual bool hasGame() const = 0;
    virtual bool isRunning() const = 0;
    virtual void attachTextGrid(TextGrid &grid) = 0;
    virtual bool hasBackground() const = 0;
    virtual void drawForeground(DrawTarget &target, double width, double height) = 0;
    virtual void drawBackground(DrawTarget &target, double width, double height) = 0;
    virtual FeedbackState feedback() const = 0;
    virtual PresenterRules presenterRules() const {
        return {};
    }
    virtual void finishForeground(DrawTarget &, double, double) {}
    virtual float flash() const = 0;
    virtual void decayFlash() = 0;
};
} // namespace neon

namespace neon {
// Original unsubmitted flash preparation, used by Minotron/Gridrunner. The
// target remains queued only through its existing vertices; no flash is emitted.
template <class Host, class Sprite>
void finishUnsubmittedFlash(Host &host, Sprite &target, double w, double h) {
    const float value = host.flash();
    if (.01 < double(value)) {
        target.width = w * .1;
        target.pos = {0, 0};
        target.height = h * .12;
        target.col.white(value > 1.f ? 1. : double(value));
        target.frame = 2;
        host.decayFlash();
    }
}
} // namespace neon
