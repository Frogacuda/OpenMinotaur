#include "particles.h"
#include "neon/foundation_rules.h"
#include "neon/integer.h"
#include "neon/particles.h"
#include <stdexcept>
namespace deflex {
namespace {
neon::colour dynamicColour(int type, float alpha, int32_t age) {
    // Deflex getDynamicColour f478 matches these established palette rules:
    // triangular .9+.1 wash, magenta/white mask 12, red/yellow mask 4.
    constexpr neon::PaletteRules rules{neon::WashRule::TriangularRainbow, 12, 4};
    const auto c = neon::paletteColour(type, alpha, uint32_t(age), rules);
    return {c.r, c.g, c.b, c.a};
}
// rcols[1..11], plus the following original word. Rounding a near-2pi angle
// to float can make spark's angle-to-colour index 11; that word is red too.
constexpr uint32_t rainbow[] = {0xff0000ff, 0xff8000ff, 0xffff00ff, 0x80ff00ff,
                                0x00ff00ff, 0x00ff80ff, 0x00ffffff, 0x0080ffff,
                                0x0000ffff, 0x000080ff, 0x008040ff, 0xff0000ff};
} // namespace
void Particle::run(const CellGrid &grid) {
    if (fragment) {
        if (freezeMovement != 1)
            pos += vel;
        neon::particles::fadeLife(fragmentLife, fragmentFade, dead);
        return;
    }
    if (gridDisplacement) {
        neon::vec3d p{pos.x * 512, pos.y * 512, 0};
        grid.doDisplace(p);
        depth = float(p.z);
    }
    integrate();
    neon::particles::updateAfterIntegration(*this, dynamicColour);
}
void Particle::draw(ActorRenderer &renderer, neon::vec2d view, bool dimFragments) const {
    if (fragment) {
        ActorSprite s;
        s.colour = colour;
        s.frame = frame + 2;
        s.angle = double(angle);
        s.position = pos * view;
        const float diameter = size + size;
        s.size = {double(diameter) * view.x * scale.x, double(diameter) * view.y * scale.y};
        // shatterBit::draw uses owner pause/demo state, independently of life.
        if (dimFragments)
            s.colour.a = .3;
        s.region = true;
        s.regionOrigin = regionOrigin;
        s.regionSize = regionSize;
        s.orFlags = 0x400;
        renderer.emit(s);
        return;
    }
    const auto animation = neon::particles::scaleFrame(life, startScale, endScale);
    ActorSprite sprite;
    sprite.frame = frame + 2;
    sprite.angle = double(angle);
    sprite.depth = depth;
    sprite.size = neon::particles::dimensions(size, animation.scale, view, scale.x, scale.y);
    sprite.colour = from.lerp(to, animation.progress);
    sprite.position = pos * view;
    renderer.emit(sprite);
}
void Plinger::run(bool noteGate, ActorServices &services) {
    if (noteGate) {
        count = neon::integers::add32(count, 1);
        if (mode == 0) {
            services.note(neon::integers::add32(count, baseX));
            services.note(neon::integers::add32(count, baseY));
        } else {
            services.note(count);
            if (count >= 3)
                services.note(count - 3);
            if (count >= 6)
                services.note(count - 6);
        }
    }
    if (count > frames)
        dead = true;
}
Particle &Particles::add(ParticleLayer layer) {
    auto &particles = list(layer);
    particles.emplace_front();
    return particles.front();
}
void Particles::spark(int count, const Entity &e, neon::DarwinRandom &random) {
    // One common speed draw even for a zero/negative requested count.
    const double speed = random.range(0.002, 0.01);
    for (int i = 0; i < count; ++i) {
        auto &p = add(ParticleLayer::World);
        p.vel = {speed, 0};
        p.noWrap = true;
        const float angle = float(random.range(0, 6.2831853072));
        p.vel.rotateBy(double(angle));
        const int colour = int((double(angle) / 6.2831853072) * 11.0);
        p.from.unpackRGBA(rainbow[colour]);
        p.to = p.from;
        p.to.a = 0;
        p.startScale = 1;
        p.endScale = 2;
        p.fade = 0.01f;
        p.pos = e.pos;
        p.frame = 28;
    }
}
void Particles::trail(int count, const Entity &e, uint32_t gameFlags, neon::DarwinRandom &random) {
    const double radius = random.range(0.002, 0.01);
    const float fraction = float(uint32_t(e.age) & 127) * 0.0078125f;
    const int colour = int(fraction * 11.0f);
    for (int i = 0; i < count; ++i) {
        auto &p = add(ParticleLayer::Overlay);
        p.gridDisplacement = true;
        const float angle = float(random.range(0, 6.2831853072));
        p.pos = {radius, 0};
        p.pos.rotateBy(double(angle));
        p.from.unpackRGBA(rainbow[colour]);
        p.to = p.from;
        p.to.a = 0;
        p.startScale = p.endScale = float(random.range(1, 2));
        p.fade = float(random.range(0.005, 0.015));
        p.pos += e.pos;
        p.frame = 28;
        if (gameFlags & 0x20)
            p.startScale = p.endScale = 4;
    }
}
void Particles::explosion(neon::vec2d position) {
    auto &p = add(ParticleLayer::World);
    p.pos = position;
    p.frame = 38;
    p.fade = 0.01f;
    p.size = 0.04f;
    p.startScale = 1;
    p.endScale = 10;
    p.from.unpackRGBA(0xffc00050);
    p.to.unpackRGBA(0x80200008);
}
void Particles::bonusGlow(neon::vec2d position) {
    auto &p = add(ParticleLayer::World);
    p.pos = position;
    p.frame = 47;
    p.fade = 0.005f;
    p.size = 0.06f;
    p.startScale = 1;
    p.endScale = 4;
    p.scale.y = 2;
    p.from.unpackRGBA(0x5f0fff30);
    p.to.unpackRGBA(0x4f0f8f08);
}
void Particles::hazardParticle(const HazardParticleBirth &birth) {
    auto &p = add(ParticleLayer::Overlay);
    p.pos = birth.position;
    p.vel = birth.velocity;
    p.from = birth.from;
    p.to = birth.to;
    p.fade = birth.lifeRate;
    p.size = birth.size;
    p.endScale = float(birth.endScale);
    p.frame = birth.frame;
}
void Particles::plinger(int frames) {
    auto &p = add(ParticleLayer::Overlay);
    p.tone = Plinger{};
    p.tone->frames = frames;
}
void Particles::run(ParticleLayer layer, const CellGrid &grid, bool noteGate,
                    ActorServices &services, DeflectorInput *input, DeflectorServices *controls) {
    auto &particles = list(layer);
    for (auto it = particles.begin(); it != particles.end();) {
        if (it->button) {
            if (!input || !controls)
                throw std::logic_error("unbound Deflex controls");
            it->button->run(*input, *controls);
            it->dead = it->button->dead;
        } else if (it->tone) {
            it->tone->run(noteGate, services);
            it->dead = it->tone->dead;
        } else
            it->run(grid);
        if (it->dead)
            it = particles.erase(it);
        else
            ++it;
    }
}
void Particles::draw(ParticleLayer layer, ActorRenderer &renderer, neon::vec2d view,
                     bool dimFragments) const {
    for (const auto &p : list(layer))
        if (p.enabled && !p.tone) {
            if (p.button) {
                ActorSprite s;
                s.frame = 12;
                s.colour = p.button->colour;
                s.position = p.button->position * view;
                const float diameter = p.button->size + p.button->size;
                s.size = {double(diameter) * view.x * p.button->spriteScale.x,
                          double(diameter) * view.y};
                renderer.emit(s);
            } else
                p.draw(renderer, view, dimFragments);
        }
}
} // namespace deflex
