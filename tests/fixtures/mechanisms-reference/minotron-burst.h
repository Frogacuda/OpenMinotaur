struct LegacyBurst : explosion {
    int oldDraw(sprite *s, vec2d sz) {
        s->frame = 0x8e;
        s->angle = double(rot);
        int saved = neonRand();
        neonSrand(seed);
        float t = 1.0f - life;
        s->col = from.lerp(to, t);
        float k = lerp(sizeStart, sizeEnd, t);
        double dk = double(k);
        for (int i = 0; i < count; i++) {
            double r = oxRNG::rand(double(radius / 3.0f), double(radius));
            vec2d v(r * double(1.0f - life) * double(size), 0.0);
            v.rotateBy(oxRNG::rand(kTwoPi));
            s->width = (1.2 - double(life)) * double(size) * sz.x * dk;
            s->height = sz.y * (1.2 - double(life)) * double(size) * dk;
            s->pos = (pos + v) * sz;
            s->setupChecking();
        }
        neonSrand(unsigned(saved));
        return count * 6;
    }
};
