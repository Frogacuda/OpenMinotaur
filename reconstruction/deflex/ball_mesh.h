// mainGameBall's 128-point shell. Its draw is intentionally stateful, as in
// 57794: detached particles advance/decay here, not in the movement simulation.
#pragma once
#include "actors.h"
namespace deflex {
struct BallMeshPoint {
    neon::vec3d direction;
    uint32_t colour = 0;
    float radius = 10, shrink = 0;
    int state = 0; // 0 entering, 2 attached, 3 detached
    float detachedRadius = 0, growth = 0, life = 0, fade = 0;
    neon::vec2d origin, rotation, drift;
    std::array<neon::vec3d, 16> history{};
    uint32_t historyCursor = 0;
};
class BallMesh {
  public:
    std::array<BallMeshPoint, 128> points{};
    int depleted = 0, startupFrames = 50;
    float lossPhase = 1, lossRate = 0;
    neon::vec2d rotation;
    void init(neon::DarwinRandom &random, uint32_t colour, float lossRate);
    void derez(const Entity &ball, int count, float velocityGain, float fade,
               neon::DarwinRandom &random);
    void restore(int count, neon::DarwinRandom &random);
    void draw(Entity &ball, ActorContext &context, ActorRenderer &renderer, neon::vec2d view,
              uint32_t colour, bool paused, bool demo);
};
} // namespace deflex
