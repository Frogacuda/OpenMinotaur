#pragma once

#include "widget_renderer.h"

namespace mino {
class InGameMenu {
  public:
    enum class Action { None, Display, Control, Exit, Resume };

    void open();
    void move(int dy);
    Action selected() const;
    Action touch(double x, double y, int width, int height);
    bool draw(int width, int height);
    void release();
    void abandonGraphics();

  private:
    neon::widgets::Runtime runtime_;
    neon::widgets::Renderer renderer_{{1.35f, .35f, 1.25f, .22f, 1.30f}};
    bool ready_ = false;
    int focus_ = 0;
    uint64_t sequence_ = 0;
};
} // namespace mino
