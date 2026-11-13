// Host seam for launch artwork. Uses the archive's original Launch.png and the
// actual startup/presentation path; no screenshot assets or duplicate artwork.
#pragma once
#include "launcher.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <functional>

namespace mino {
class LauncherTransition {
  public:
    bool prepare(Launcher &launcher, int width, int height, const DisplaySettings &shellSettings,
                 bool es);
    using FrameObserver = std::function<void(float)>;
    bool playArtwork(Host &host, Display &target, SDL_Window *window, bool &quit,
                     const FrameObserver &observer = {});
    bool playRendered(Display &target, SDL_Window *window, bool &quit,
                      const FrameObserver &observer = {});
    void cancel();
    void abandonGraphics() {
        framebuffer_ = texture_ = program_ = vbo_ = vao_ = 0;
        source_.abandonGraphics();
        cancel();
    }
    bool pending() const {
        return launcher_ != nullptr;
    }

  private:
    friend struct LauncherChecks;
    bool play(Display &target, SDL_Window *window, bool &quit, const FrameObserver &observer);
    Display source_;
    Launcher *launcher_ = nullptr;
    unsigned texture_ = 0, framebuffer_ = 0, program_ = 0, vbo_ = 0, vao_ = 0;
    int width_ = 0, height_ = 0;
};
} // namespace mino
