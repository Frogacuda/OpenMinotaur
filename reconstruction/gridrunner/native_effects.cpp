#include "neon/particles.h"
#include "native_effects.h"
#include <cmath>
namespace gr::reconstruction::ios {
Rgba unpackRgba(uint32_t p) {
    return {double((p >> 24) & 255) / 255.0, double((p >> 16) & 255) / 255.0,
            double((p >> 8) & 255) / 255.0, double(p & 255) / 255.0};
}
void Xpart::run() {
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
    neon::particles::retireProgress(fade, dead, neon::particles::ProgressEnd::AtOrAbove);
}
void Effects::explode(const HeadExplosion &s) {
    for (int32_t n = 0; n < s.count; ++n) {
        auto effect = std::make_unique<Xpart>(palette);
        effect->velocity = {0, random.range(double(s.minSpeed), double(s.maxSpeed))};
        effect->velocity.rotateBy(random.range(-3.141592654, 3.141592654));
        effect->growth = 1;
        effect->colour = -1;
        if (s.colour < 33)
            effect->colour = int32_t(s.colour);
        else
            effect->tint = unpackRgba(s.colour);
        effect->position = s.position;
        effect->fade = 0;
        effect->fadeStep = .02f;
        effect->sprite = s.sprite;
        effect->radius = s.radius;
        lists.prepend(List::L3bc, std::move(effect));
    }
}
void Effects::bonus(const HeadBonus &s, const Ship &ship) {
    if (!textureSize)
        throw std::logic_error("Original bonus requires sprite dimensions");
    auto effect = std::make_unique<Xpart>(palette);
    const Vec2 dimensions = textureSize(s.sprite + 2);
    if (dimensions.x <= 0)
        throw std::logic_error("Invalid original sprite width");
    effect->growth = 1;
    effect->scale.y *= dimensions.y / dimensions.x;
    effect->scale.x = 1;
    effect->radius = s.radius;
    effect->colour = 0;
    if (s.colour < 10)
        effect->colour = int32_t(s.colour);
    else
        effect->tint = unpackRgba(s.colour);
    effect->position = s.position;
    effect->sprite = s.sprite;
    effect->fade = 0;
    effect->fadeStep = .01f;
    Vec2 direction{ship.position.x - s.position.x, ship.position.y - s.position.y};
    const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0) {
        direction.x /= length;
        direction.y /= length;
    } else
        direction = {};
    const double speed = random.range(.002, .007);
    effect->velocity = {direction.x * speed, direction.y * speed};
    lists.prepend(List::L3bc, std::move(effect));
}
void Effects::outline(const HeadOutline &s) {
    auto effect = std::make_unique<Xpart>(palette);
    effect->position = s.position;
    effect->velocity = s.velocity;
    effect->angle = s.angle;
    effect->radius = s.radius;
    effect->growth = s.growth;
    effect->sprite = s.sprite;
    effect->colour = s.colour;
    effect->fade = 0;
    effect->fadeStep = s.fade;
    lists.prepend(List::L3bc, std::move(effect));
}
} // namespace gr::reconstruction::ios
