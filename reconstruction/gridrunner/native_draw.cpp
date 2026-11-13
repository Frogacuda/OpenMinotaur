#include "native_draw.h"
namespace gr::reconstruction::ios {
EntityDrawSample sampleEntityDraw(Entity &e, int32_t frame, Rgba tint) {
    // iOS Entity::draw 0x2f828: hidden sprites return before both counters;
    // flashing precedes draw-delay suppression and only overrides sprite RGB.
    if (frame < 0)
        return {false, tint};
    if (e.flashTicks != 0) {
        e.flashTicks = add32(e.flashTicks, -1);
        const double phase = double(float(uint32_t(e.flashTicks) & 7) / 7.f);
        tint.r = phase * .5 + .5;
        tint.g = phase;
        tint.b = 0;
    }
    if (e.drawDelay > 0) {
        e.drawDelay = add32(e.drawDelay, -1);
        return {false, tint};
    }
    return {true, tint};
}
void DrawFrame::entity(Entity &e, Vec2 position, Vec2 scale, int32_t frame, Rgba tint) {
    const auto sample = sampleEntityDraw(e, frame, tint);
    if (!sample.visible)
        return;
    DrawSprite draw;
    draw.position = position;
    draw.scale = scale;
    draw.frame = add32(frame, 2);
    draw.tint = sample.tint;
    draw.angle = e.angle;
    draw.depth = e.depth;
    draw.diameter = e.radius + e.radius;
    sprites_.push_back(draw);
}
void DrawFrame::tao(Tao &t) {
    if (t.arrivalStep <= 0) {
        entity(t, t.position, t.scale, t.sprite, t.tint);
        return;
    }
    const float inverse = 1.f - t.arrival;
    const double growth = double(float(double(inverse) * .3 + 1)),
                 first = double(float(double(inverse) * 4 + 1));
    Vec2 scale{first, first};
    float cursor = float(t.trailFrame) - 1.f;
    t.tint.a = double(t.arrival);
    for (int i = 0; i < 8; ++i) {
        entity(t, t.trail[uint32_t(int32_t(cursor)) & 511], scale, add32(t.sprite, 1), t.tint);
        scale.x *= growth;
        scale.y *= growth;
        cursor = float(double(cursor) + double(1.f - t.arrival) * -16.0);
    }
}
void DrawFrame::ship(Ship &s, const Campaign &game) {
    const int state = int(s.state);
    if (state < 2) {
        if (double(s.radius) <= .03) {
            tao(s);
            return;
        }
        s.tint = game.pickupTints[1];
        s.tint.a = .5;
        const int frame = add32(s.sprite, int(((uint32_t(s.age) & 7) >> 2) ^ 1));
        entity(s, s.position, s.scale, frame, s.tint);
        s.tint.unpack(0x20ff40ff);
        return;
    }
    if (state == 2) {
        entity(s, s.position, s.scale, add32(s.sprite, int(((uint32_t(s.age) & 7) >> 2) ^ 1)),
               s.tint);
        return;
    }
    if (state != 3)
        return;
    const float inverse = 1.f - s.arrival;
    double growth = double(float(double(inverse) * .3 + 1));
    double initial = double(float(double(inverse) * 4 + 1));
    Vec2 scale{initial, initial};
    float cursor = float(s.trailFrame) - 1.f;
    constexpr uint32_t colours[] = {0,          0x0000ffff, 0xff0000ff, 0x00ff00ff,
                                    0xff00ffff, 0x00ffffff, 0xffff00ff, 0xffffffff};
    for (int i = 0; i < 7; ++i) {
        if (game.ac.completed)
            s.tint = game.palette[4];
        else
            s.tint.unpack(colours[i + 1]);
        s.tint.a = double(s.arrival);
        const auto position =
            game.ac.completed ? s.trail[uint32_t(int32_t(cursor)) & 511] : s.position;
        entity(s, position, game.ac.completed ? s.scale : scale, add32(s.sprite, 1), s.tint);
        if (game.ac.completed)
            cursor -= 6.f;
        else {
            scale.x *= growth;
            scale.y *= growth;
            cursor += inverse * -16.f;
        }
    }
}
void DrawFrame::zakato(Zakato &z, const Campaign &game) {
    const float radius = z.radius;
    z.radius = .01f;
    z.tint = game.palette[6];
    float brightness = .9f;
    for (int i = 127; i != 77; i -= 5) {
        z.tint.a = double(brightness * z.fade);
        entity(z, z.trail[(z.trailFrame + uint32_t(i)) & 127], z.scale, 2, z.tint);
        brightness += -.03f;
    }
    z.radius = radius;
    z.tint.white(1);
    if (!z.fading)
        entity(z, z.position, z.scale, z.sprite, z.tint);
}
void DrawFrame::prepare(Campaign &game) {
    sprites_.clear();
    using gr::reconstruction::List;
    for (auto list : {List::L3b8, List::L3ac, List::L3c4, List::L3c8, List::L3c0, List::L3bc,
                      List::L3b0, List::L3cc})
        for (auto *node = game.lists.first(list); node; node = node->next) {
            auto *e = dynamic_cast<Entity *>(node);
            if (!e)
                continue;
            if (auto *s = dynamic_cast<Ship *>(e)) {
                ship(*s, game);
                continue;
            }
            if (auto *z = dynamic_cast<Zakato *>(e)) {
                zakato(*z, game);
                continue;
            }
            if (auto *bolt = dynamic_cast<LaserBolt *>(e)) {
                for (int i = 0; i < 30; ++i) {
                    DrawSprite draw;
                    draw.position = bolt->position;
                    draw.frame = bolt->frames[size_t(i)];
                    draw.tint = bolt->drawTint();
                    draw.beamRow = i;
                    draw.beamRadius = bolt->radius;
                    draw.chargeFraction = float(bolt->age) / float(game.ac.laserCharge);
                    draw.charging = bolt->age < game.ac.laserCharge;
                    sprites_.push_back(draw);
                }
                continue;
            }
            if (auto *t = dynamic_cast<Tao *>(e)) {
                tao(*t);
                continue;
            }
            entity(*e, e->position, e->scale, e->sprite, e->tint);
        }
}
void DrawFrame::prepare(Starfield &field) {
    sprites_.clear();
    for (auto &s : field.stars())
        if (s.visible())
            entity(s, s.position, s.scale, s.sprite, s.tint);
}
} // namespace gr::reconstruction::ios
