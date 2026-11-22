#pragma once

#include <memory>

namespace mino {

// Small, overlapping phrases from the bundled game samples. Archive access
// happens on the application thread before the shell audio stream starts.
class ShellArcade {
  public:
    ShellArcade();
    ~ShellArcade();
    void preload();
    void reset();
    void mix(float &left, float &right, bool active);

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace mino
