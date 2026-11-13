struct LegacyBurst : explosion {
    int oldDraw(sprite *s, vec2d sz) {
        s->frame = 0x8d;
        s->angle = double(rot);
        int saved = neonRand();
        neonSrand(seed);
        float t = 1.0f - life;
        s->col = from.lerp(to, t);
        float k = lerp(sizeStart, sizeEnd, t);
        double dk = double(k);
        for (int i = 0; i < count; i++) {
            double r = oxRNG::rand(double(radius) / 3.0, double(radius));
            vec2d v(r * double(1.0f - life) * double(size), 0.0);
            v.rotateBy(oxRNG::rand(6.2831853072));
            s->width = (1.2 - double(life)) * double(size) * dk * sz.x;
            s->height = sz.y * (1.2 - double(life)) * double(size) * dk;
            s->pos = (pos + v) * sz;
            s->setupChecking();
        }
        neonSrand(unsigned(saved));
        return count * 6;
    }
};
