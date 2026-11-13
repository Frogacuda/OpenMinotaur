struct LegacyMinosScore : minos::FloatingScore {
    void oldRun() { // 0x54278
        if (g->paused)
            return;
        pos = {pos.x + vel.x, pos.y + vel.y};
        col = g->colours[3]; // theGame+0x5c8
        col.a = double(alpha);
        alpha = alpha + fade;
        if (alpha < 0.0f)
            dead = true;
    }
    int oldDraw(EntityRenderer &r, Vec2 screen) { // 0x55478
        auto *sr = dynamic_cast<SpriteRenderer *>(&r);
        if (!sr)
            return 0;
        // Unlike entity::draw, glyph dimensions are size*scale (not twice size).
        // 0x55618 holds binary64 1.2. Advance both axes before the half-height
        // origin adjustment; preserve the source's operation order for centring.
        const Vec2 dimensions{scale.x * double(size), scale.y * double(size)};
        const Vec2 advance{dimensions.x * 1.2, dimensions.y * 1.2};
        const int n = 15 - first;
        double x = pos.x + advance.x * double(float(n)) * -0.5 + advance.x * 0.5;
        const double y = pos.y + advance.y * -0.5;
        SpriteFields f;
        f.orFlags = 3;
        f.col = col;
        f.angle = double(rot);
        f.size = {dimensions.x * screen.x, dimensions.y * screen.y};
        for (int i = first + 1; i <= 15; i++) {
            f.frame = spriteBase + digits[i] + 2;
            f.pos = {x * screen.x, y * screen.y};
            sr->drawSprite(f);
            x += advance.x;
        }
        return n * 6;
    }
};
