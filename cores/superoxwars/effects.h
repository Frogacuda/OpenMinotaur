#pragma once
#include "world.h"
namespace superoxwars {
struct Explosion : GameEntity {
    float opacity = 1, fade = .1f, growth = 1.06f;
    Explosion(World &, Vec2, float radius, float fade, float growth);
    void run() override;
};
// floatingScore 455f8/457a0/45834 uses the original numeral sprite strip.
struct FloatingScore : GameEntity {
    std::string digits;
    int paletteSlot = 0;
    float opacity = 1;
    FloatingScore(World &, int, Vec2, int, Vec2 velocity = {0, .003f}, float radius = .02f);
    void run() override;
    void prepare(std::vector<ActorDraw> &) override;
};
struct Fragment : PlayerBullet {
    float travel = 0, spin = 0, clock = 0, lifetimeRate = .002f, gravity = 0;
    explicit Fragment(World &w) : PlayerBullet(w) {}
    void run() override;
};
void breakup(World &, Vec2, int count, int mode);
void scatter(World &, Vec2, int count, int mode);
} // namespace superoxwars
