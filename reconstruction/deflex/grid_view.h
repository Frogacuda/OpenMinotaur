#pragma once
#include "actors.h"
#include <list>
namespace deflex {
struct GridVertex {
    float x, y, z, u, v;
    uint32_t colour;
};
struct GridRenderer {
    virtual ~GridRenderer() = default;
    virtual void quad(const std::array<GridVertex, 6> &vertices, bool additive = true,
                      int texture = 1) = 0;
};
struct ProportionalLine {
    std::string text;
    neon::vec2d pos, scale{1, 1.8}, to, from, away, inScale{.8, .8}, outScale{.8, .8}, currentScale;
    float inCurve = 1, outCurve = 1, progress = 0, inRate = .01f, outRate = .02f, spacing = 1,
          currentSpacing = 0;
    int hold = -1, state = 0, sequence = 0, align = 1;
    float phaseRate = 1, charStep = .5f, alpha = 0, phase = 0;
    uint32_t rgba = 0xffffffff;
    void update();
};
class GridView {
    CellGrid &grid_;
    struct Star : Entity {
        using Entity::run;

        float phase = 0, speed = 0;
        void init(neon::DarwinRandom &);
        void run();
    };
    std::list<Star> stars_;
    int starClock_ = 0;

  public:
    explicit GridView(CellGrid &grid) : grid_(grid) {}
    std::array<ProportionalLine, 129> lines;
    int fontY = 0, glyphW = 16, glyphH = 17;
    bool stars = true;
    int cursorX = -1, cursorY = -1;
    uint32_t cursorColour = 0xffffffff;
    int cursorClock = 30;
    bool cursorVisible = true;
    // The original cellGrid outlives reset/newLevel. Scene ownership must not
    // discard its star list, birth cadence or cursor blink with arena actors.
    void takeBackground(GridView &prior) {
        stars_.splice(stars_.end(), prior.stars_);
        starClock_ = prior.starClock_;
        cursorClock = prior.cursorClock;
        cursorVisible = prior.cursorVisible;
    }
    void quick(const std::string &, neon::vec2d pos, neon::vec2d scale, int hold, int row,
               int sequence, float phaseRate = 1, float charStep = .5f);
    void draw(CellGrid &, const neon::colour *palette, neon::DarwinRandom &, bool paused,
              GridRenderer &, ActorRenderer &, neon::vec2d view);
    void text(GridRenderer &, const std::string &, neon::vec2d position, neon::vec2d scale,
              uint32_t rgba, int align = 1) const;

  private:
    void line(GridRenderer &, const ProportionalLine &) const;
};
} // namespace deflex
