#pragma once
#include "denizens.h"

namespace goatup {
struct KidState {
    bool tally = false;
    int releaseOrdinal = 0, tallyMultiplier = 0;
    int mostInRun = 0, statusKind = 0, statusTicks = 0;
};
// Source kid::init/run. Kids follow nine-tick history intervals, retaining the
// recorded platform offset as that platform scrolls; they are not spring bodies.
class Kids {
  public:
    Kids(DenizenContext &c, KidState &state) : context_(c), state_(state) {}
    ~Kids();
    Kids(const Kids &) = delete;
    Kids &operator=(const Kids &) = delete;
    std::function<void(Vec2, float, int, bool &)> touch;
    void create();
    void tick();
    void draw(std::vector<ActorDraw> &) const;
    void reset();
    int count() const;

  private:
    struct Kid;
    Kid *kids_ = nullptr;
    DenizenContext &context_;
    KidState &state_;
    const ClimbHistory &history(int ordinal) const;
};
void treadOnTile(Platform &, int tile, DenizenContext &, Kids &);
} // namespace goatup
