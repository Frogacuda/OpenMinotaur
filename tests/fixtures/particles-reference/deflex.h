// Saved reference native particle methods.
struct LegacyParticle final : Particle {
    void run(const CellGrid &grid);
    void draw(ActorRenderer &renderer, neon::vec2d view, bool dimFragments) const;
};
void LegacyParticle::run(const CellGrid &grid) {
    if (fragment) {
        if (freezeMovement != 1)
            pos += vel;
        fragmentLife -= fragmentFade;
        if (fragmentLife < 0) {
            fragmentLife = 0;
            dead = true;
        }
        return;
    }
    if (gridDisplacement) {
        neon::vec3d p{pos.x * 512, pos.y * 512, 0};
        grid.doDisplace(p);
        depth = float(p.z);
    }
    integrate();
    if (fromDynamic >= 0)
        from = dynamicColour(fromDynamic, 1, age);
    if (toDynamic >= 0)
        to = dynamicColour(toDynamic, 0, age);
    life -= fade;
    if (life < 0) {
        life = 0;
        dead = true;
    }
}

void LegacyParticle::draw(ActorRenderer &renderer, neon::vec2d view, bool dimFragments) const {
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
    const float progress = 1.0f - life;
    const float zoom = neon::interpolateFloat(startScale, endScale, progress);
    ActorSprite sprite;
    sprite.frame = frame + 2;
    sprite.angle = double(angle);
    sprite.depth = depth;
    sprite.size = {double(size) * view.x * double(zoom) * scale.x,
                   double(zoom) * double(size) * view.y * scale.y};
    sprite.colour = from.lerp(to, progress);
    sprite.position = pos * view;
    renderer.emit(sprite);
}
