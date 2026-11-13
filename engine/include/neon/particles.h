#pragma once
#include "neon/math.h"
#include "neon/foundation_rules.h"

namespace neon::particles {
// Source-selected remaining-life endpoint. Stage particles compare their
// binary32 opacity promoted to double against the literal .2 (not .2f).
// Retiring clamps to zero; an already-dead particle is never resurrected.
enum class LifeEnd { Below, AtOrBelow };
inline void retireLife(float &life, bool &dead, LifeEnd rule = LifeEnd::Below,
                       double cutoff = 0.0) {
    const bool ended = rule == LifeEnd::Below ? double(life) < cutoff : double(life) <= cutoff;
    if (ended) {
        life = 0.f;
        dead = true;
    }
}
// Exactly zero (and -0) remains alive under the early source rule.
inline void fadeLife(float &life, float amount, bool &dead) {
    life -= amount;
    retireLife(life, dead);
}

// Growing phases use different original terminal predicates. Keep the increment
// at its source call site: intervening camera, physics and colour work varies.
enum class ProgressEnd { AboveAndClamp, Above, AtOrAbove };
inline void retireProgress(float &phase, bool &dead, ProgressEnd rule) {
    const bool ended = rule == ProgressEnd::AtOrAbove ? phase >= 1.f : phase > 1.f;
    if (ended) {
        if (rule == ProgressEnd::AboveAndClamp)
            phase = 1.f;
        dead = true;
    }
}

// Matched early entity::blowUp payload. Construction must happen first (its
// seed draw is observable even though overwritten here). The module still
// assigns the owner, adopts the particle, then retires the source.
template <class Explosion, class Entity>
void initializeColourExplosion(Explosion &effect, const Entity &source, unsigned seed, int count) {
    effect.count = count;
    effect.from = source.col;
    effect.to = source.col;
    effect.from.a = 1.0;
    effect.to.a = 0.5;
    effect.rot = source.rot;
    effect.seed = seed;
    effect.pos = source.pos;
    effect.vel = source.vel * 0.1;
    effect.size = source.size;
}

// The owner performs its source integration first. Keep from/to callbacks in
// order, including the terminal tick, and pass the post-integration age. Palette
// rules belong to the module-supplied callback; there is no game-ID selection.
template <class Particle, class DynamicColour>
void updateAfterIntegration(Particle &particle, DynamicColour dynamicColour) {
    if (particle.fromDynamic >= 0)
        particle.from = dynamicColour(particle.fromDynamic, 1.0f, particle.age);
    if (particle.toDynamic >= 0)
        particle.to = dynamicColour(particle.toDynamic, 0.0f, particle.age);
    fadeLife(particle.life, particle.fade, particle.dead);
}

struct ScaleFrame {
    float progress, scale;
};
inline ScaleFrame scaleFrame(float life, float start, float end) {
    const float progress = 1.0f - life;
    return {progress, neon::interpolateFloat(start, end, progress)};
}

// Preserve the original different multiplication orders for X and Y. Colour,
// rotation, literal base, depth and sprite submission remain with the binding.
template <class View>
vec2d dimensions(float radius, float factor, const View &view, double scaleX, double scaleY) {
    return {double(radius) * view.x * double(factor) * scaleX,
            double(factor) * double(radius) * view.y * scaleY};
}
// Early seeded explosions deliberately consume a global draw, reseed for their
// stable pattern, then seed the global stream from that saved VALUE (not a saved
// state). Preserve this unusual source protocol, including zero/negative counts.
// Minotron 41c28 / Rescue 3f584 divide the radius in f32. Both multiply size,
// interpolation, then view-X; view-Y multiplies the completed extent.
template <class Explosion, class Sprite, class View, class Next, class Seed, class Range>
int drawSeededBurst(const Explosion &effect, Sprite &target, const View &view, Next next, Seed seed,
                    Range range) {
    using Point = decltype(target.pos);
    const unsigned saved = unsigned(next());
    seed(effect.seed);
    const float progress = 1.f - effect.life;
    target.col = effect.from.lerp(effect.to, progress);
    const float factor = neon::interpolateFloat(effect.sizeStart, effect.sizeEnd, progress);
    for (int i = 0; i < effect.count; ++i) {
        const double radius = range(double(effect.radius / 3.f), double(effect.radius));
        Point offset{radius * double(1.f - effect.life) * double(effect.size), 0.};
        offset.rotateBy(range(0., 6.2831853072));
        const double extent = (1.2 - double(effect.life)) * double(effect.size) * double(factor);
        target.width = extent * view.x;
        target.height = view.y * extent;
        target.pos = (effect.pos + offset) * view;
        target.setupChecking();
    }
    seed(saved);
    return effect.count * 6;
}

} // namespace neon::particles
