// Saved reference particle methods; independent of the new particle helpers.
struct LegacyParticle final : _particle {
    int draw(sprite *s, vec2d sz);
    void run();
};
int LegacyParticle::draw(sprite *s, vec2d sz) {
    s->angle = double(rot);
    s->frame = spriteIndex + kLitBase;
    float k = lerp(sizeStart, sizeEnd, 1.0f - life);
    s->width = double(size) * sz.x * double(k) * scaleX;
    s->height = double(k) * double(size) * sz.y * scaleY;
    s->col = clerp(from, to, 1.0f - life);
    s->pos = pos * sz;
    s->zOffset = zOffset;
    s->setupChecking();
    return 6;
}

void LegacyParticle::run() {
    entity::run();
    if (fromDynamic > -1)
        from = getDynamicColour(fromDynamic, 1.0f, age);
    if (toDynamic > -1)
        to = getDynamicColour(toDynamic, 0.0f, age);
    life -= fade;
    if (life < 0.0f) {
        life = 0.0f;
        dead = true;
    }
}
