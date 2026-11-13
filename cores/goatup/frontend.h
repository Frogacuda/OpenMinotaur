#pragma once
#include "backend.h"
#include "world.h"

namespace goatup {
// Native character-grid menus (charScreen 56d98), with canonical pad focus.
class Frontend {
  public:
    enum class Screen { Loading, Resetting, Boot, Title, Info, Playing } screen = Screen::Loading;
    enum class Action { None, Start, Exit, Changed };
    int bootTicks = 0, ticks = 0, paletteTicks = 0, page = 0, selection = 2, control = 0;
    int preferredControl = 0;
    int best = 0, lastScore = 0, highest = 0;
    bool infoFromPause = false, tiltAvailable = false, touchAvailable = false,
         controllerFocus = false;
    void capabilities(bool touch, bool tilt);
    Action command(uint32_t edges, bool paused);
    Action touch(double x, double y, bool paused);
    void title();
    void drawText(Neon_Internals::cellGrid &, const World &);
    void drawArt(std::vector<ActorDraw> &) const;

  private:
    neon::colour palette(int type) const;
};
} // namespace goatup
