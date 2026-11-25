#pragma once
#include "core_api.h"
#include <memory>

namespace mino {
enum class WidgetGame {
    Gridrunner,
    GoatUp,
    SuperOxWars,
    FiveADay,
    Minos,
    GoatUp2,
    Minotron,
    Deflex,
    MinotaurRescue
};

// Owns the presentation resources for in-game letterbox widgets.
class GameplayWidgets {
  public:
    GameplayWidgets();
    ~GameplayWidgets();
    void prepare(WidgetGame, int width, int height, int x, int y, int gameplayWidth,
                 int gameplayHeight, int turns);
    bool draw(WidgetGame, const Core &, bool refresh, unsigned framebuffer, int width, int height,
              int x, int y, int gameplayWidth, int gameplayHeight, int turns);
    void release();
    void abandonGraphics();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mino
