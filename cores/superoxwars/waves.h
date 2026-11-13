#pragma once
#include "world.h"
namespace superoxwars {
struct PodLaunch {
    Vec2 position{0, .7};
    float phase = 0, phaseStep = 0, secondPhase = 0, secondStep = 0, amplitude = .1f;
    int archetype = 0, path = 0, ordinal = 0, waveOrdinal = 0, group = 0;
    float fraction = 0;
};
struct LaunchPod : GameEntity {
    PodLaunch launch;
    int remaining = 0;
    LaunchPod(World &, const PodLaunch &);
    void run() override;
};
// shipLauncher::sequenceLaunch (49aa8) and tick (49de4). Beginning a wave
// advances on terrain row crossings; individual pods advance every game tick.
class WaveController {
  public:
    explicit WaveController(World &world) : world_(world) {}
    void init(int script, const std::array<uint32_t, 1024> &seeds);
    void rowCrossed();
    void tick();
    bool active = false, blocked = false;
    int cursor = 0, rowDelay = 4, remaining = 0, interval = 0, countdown = 0, archetype = 0,
        path = 0;
    int group = 0, groupCursor = 0, ordinal = 0, waveOrdinal = 0;
    std::array<int, 100> archetypeCount{};
    float fraction = 0, fractionStep = 0;

  private:
    World &world_;
    int script_ = 0;
    std::array<uint32_t, 1024> seeds_{};
    std::array<float, 2> phase_{}, phaseStep_{};
    float podPhase_ = 0, podPhaseStep_ = 0, amplitudePhase_ = 0, amplitudeStep_ = 0, sweep_ = 0,
          sweepStep_ = 0;
};
} // namespace superoxwars
