#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace mino {

class ShellAudio {
  public:
    enum class Scene { Title, Launcher, Leaving };
    enum class Mode { Full, Cues, Off };
    enum class Cue { Move, Confirm, Tick, Kick, Back, Enter, Launch };

    ShellAudio();
    ~ShellAudio();

    void setScene(Scene scene) {
        scene_.store(scene, std::memory_order_relaxed);
    }
    void setMode(Mode mode) {
        mode_.store(mode, std::memory_order_relaxed);
    }
    void markTitleVisible() {
        titleVisible_.store(true, std::memory_order_relaxed);
    }
    void play(Cue cue);

    // Preload and reset run with the stream closed. The audio thread owns mix and suspend.
    void preload();
    void mix(float *out, int frames);
    void reset();
    void suspend();

  private:
    struct State;
    void startCue(Cue cue);

    std::atomic<Scene> scene_{Scene::Launcher};
    std::atomic<Mode> mode_{Mode::Full};
    std::atomic_bool titleVisible_{false};
    // The shell publishes accepted input. The callback consumes it in order.
    std::array<Cue, 32> queue_{};
    std::atomic<unsigned> read_{0}, write_{0};
    std::unique_ptr<State> state_;
};

} // namespace mino
