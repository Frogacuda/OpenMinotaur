// Independent update bodies with only callable/member qualification adapted.
struct fiveExplosion : public fiveaday::Explosion {
    using fiveaday::Explosion::Explosion;
    void oldRun() {
        position += velocity;
        position += world.camera;
        progress += progressStep;
        if (progress > 1) {
            dead = true;
            progress = 1;
        }
        if (frame == 51)
            colour = world.palette(4);
        if (frame == 29)
            colour = unpack(interpolatePacked(0xffff00ff, 0x8000ffff, progress));
        colour.a = double(float(1.f - progress));
        radius += growth;
    }
};
struct gridXpart : public gr::reconstruction::ios::Entity {
    const std::array<gr::reconstruction::ios::Rgba, 33> &palette;
    float fade = 0, fadeStep = 0, growth = 1;
    int32_t colour = 0;
    explicit gridXpart(const std::array<gr::reconstruction::ios::Rgba, 33> &p) : palette(p) {}
    void oldRun() {
        scale.x *= double(growth);
        scale.y *= double(growth);
        position.x += velocity.x;
        position.y += velocity.y;
        fade += fadeStep;
        if (colour > 0) {
            if (colour >= int32_t(palette.size()))
                throw std::logic_error("Original xpart colour outside bound palette");
            tint = palette[size_t(colour)];
        }
        tint.a = double(1.f - fade);
        if (fade >= 1)
            dead = true;
    }
};
struct minosExplo : public minos::Explo {
    void oldRun() { // 0x541b8
        if (g->paused)
            return;
        t = t + rate;
        const float span = to - from;
        pos = {pos.x - g->scrollDelta.x, pos.y - g->scrollDelta.y};
        float tt = t;
        if (1.0f < tt) {
            tt = 1.0f;
            t = 1.0f;
            dead = true;
        }
        const float s = from + span * tt;
        scale = {double(s) * baseScale.x, double(s) * baseScale.y};
        col.a = double(1.0f - tt);
    }
};
