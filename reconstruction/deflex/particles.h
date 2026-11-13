// Actual consumers of Deflex's particle births. The game owner schedules the
// two original lists separately; these operations do not invent a frame order.
#pragma once
#include "actors.h"
#include <list>
namespace deflex {
struct Plinger {
    int baseX = 0, baseY = 0, count = 1, frames = 3, mode = 0;
    bool dead = false;
    void run(bool noteGate, ActorServices &services);
};
struct Particle : Entity {
    using Entity::run;
    using Entity::draw;
    neon::colour from, to;
    float life = 1, fade = 0.01f, startScale = 1, endScale = 4;
    int fromDynamic = -1, toDynamic = -1;
    bool gridDisplacement = false;
    std::optional<Plinger> tone; // original plingers share the +1c4 effect list
    std::optional<DeflectorButton> button;
    bool fragment = false;
    int freezeMovement = 0;
    float fragmentLife = 1, fragmentFade = .02f;
    neon::vec2d regionOrigin, regionSize;
    Particle() {
        size = 0.005f;
    }
    void run(const CellGrid &grid);
    void draw(ActorRenderer &renderer, neon::vec2d view, bool dimFragments = false) const;
};
enum class ParticleLayer { World, Overlay }; // source lists +1bc and +1c4
class Particles {
    std::list<Particle> world_, overlay_;
    std::list<Particle> &list(ParticleLayer layer) {
        return layer == ParticleLayer::World ? world_ : overlay_;
    }
    const std::list<Particle> &list(ParticleLayer layer) const {
        return layer == ParticleLayer::World ? world_ : overlay_;
    }

  public:
    Particle &add(ParticleLayer layer);
    void spark(int count, const Entity &entity, neon::DarwinRandom &random);
    void trail(int count, const Entity &entity, uint32_t gameFlags, neon::DarwinRandom &random);
    void explosion(neon::vec2d position);
    void bonusGlow(neon::vec2d position); // caller owns bonus-chain counter/goals
    void hazardParticle(const HazardParticleBirth &birth);
    void plinger(int frames);
    void run(ParticleLayer layer, const CellGrid &grid, bool noteGate, ActorServices &services,
             DeflectorInput *input = nullptr, DeflectorServices *controls = nullptr);
    void draw(ParticleLayer layer, ActorRenderer &renderer, neon::vec2d view,
              bool dimFragments = false) const;
    void clear() {
        world_.clear();
        overlay_.clear();
    }
    const std::list<Particle> &particles(ParticleLayer layer) const {
        return list(layer);
    }
};
} // namespace deflex
