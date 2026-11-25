#pragma once
#include "title_sequence.h"
#include "widget_renderer.h"

namespace neon::widgets {
// Draws into a host-owned target. The background callback can render the light
// synth without introducing a dependency from widget semantics to that module.
class TitlePresenter {
  public:
    using Background = std::function<void(const RenderTarget &)>;
    void assets(const TitleSequence &title, ResourceReader reader);
    void render(const TitleSequence &, const RenderTarget &, const Background &,
                double exitBackgroundOpacity = -1);
    void release() {
        renderer_.release();
    }
    void abandonGraphics() {
        renderer_.abandonGraphics();
    }

  private:
    Renderer renderer_{{1.25f, .25f, 3.f, .10f, 1.f}};
};
} // namespace neon::widgets
