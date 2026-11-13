// Saved reference private particle; birth/list adapter is in particles_native.cpp.
struct LegacyGoatParticle {
    LegacyGoatParticle *prev = nullptr;
    LegacyGoatParticle *next = nullptr;
    bool dead = false, wrapped = false;
    int age = 0;
    Vec2 position{}, velocity{};
    float energy = 1, decay = .03f;
    neon::colour from, to;
    void run() {
        // gpart adds gravity before _particle integrates and applies damping.
        velocity.y += .0006;
        neon::entities::integrate(position, velocity, Vec2{.98, .98}, age, true, wrapped,
                                  Vec2{1, 1});
        energy = float(energy - decay);
        if (energy < 0) {
            energy = 0;
            dead = true;
        }
    }
    ActorDraw draw() const {
        const float progress = float(1.f - energy);
        ActorDraw result;
        result.position = position;
        result.frame = 0;
        result.particle = true;
        result.radius = .005f;
        const float size = neon::interpolateFloat(1, 2, progress);
        result.scale = {double(size), double(size)};
        result.colour = neon::clerp(from, to, progress);
        return result;
    }
};
