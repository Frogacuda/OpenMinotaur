#pragma once
#include "core_api.h"
#include <memory>

namespace mino {
// Optional landscape prototype. Owns presentation resources, never the game.
class GridrunnerWidgets {
  public:
    GridrunnerWidgets();
    ~GridrunnerWidgets();
    void prepare(int width, int height, int x, int y, int gameplayWidth, int gameplayHeight,
                 int turns);
    bool draw(const Core &, bool refresh, unsigned framebuffer, int width, int height,
              int x, int y, int gameplayWidth, int gameplayHeight, int turns);
    void release();
    void abandonGraphics();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mino
