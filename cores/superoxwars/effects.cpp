#include "neon/score_notices.h"
#include "effects.h"
#include "neon/particles.h"
#include "projectiles.h"
#include <algorithm>
namespace superoxwars {
FloatingScore::FloatingScore(World &w, int value, Vec2 at, int side, Vec2 motion, float size)
    : GameEntity(w), digits(std::to_string(std::max(value, 0))), paletteSlot(side < 0   ? 8
                                                                             : side > 0 ? 1
                                                                                        : 0) {
    position = at;
    velocity = motion;
    radius = size;
}
void FloatingScore::run() {
    neon::notices::advanceFloating(position, velocity, opacity, -.01f, dead, [&](double alpha) {
        colour = world.palette[size_t(paletteSlot)];
        colour.a = alpha;
    });
}
void FloatingScore::prepare(std::vector<ActorDraw> &out) {
    const double step = double(radius) * scale.x * 1.2;
    Vec2 at{position.x + step * (.5 - double(digits.size()) * .5),
            position.y - double(radius) * scale.y * .6};
    for (char digit : digits) {
        out.push_back({at, scale, radius * .5f, angle, 30 + digit - '0', colour});
        at.x += step;
    }
}
Explosion::Explosion(World &w, Vec2 at, float size, float fadeStep, float expansion)
    : GameEntity(w), fade(fadeStep), growth(expansion) {
    position = retained = at;
    frame = 17;
    radius = size;
    angle = float(w.random.range(-3.141592654, 3.141592654));
}
void Explosion::run() {
    neon::particles::fadeLife(opacity, fade, dead);
    scale = scale * double(growth);
    colour.a = double(opacity);
}
void Fragment::run() {
    PlayerBullet::run();
    travel = float(double(travel) + velocity.length());
    angle = travel * spin;
    clock += lifetimeRate;
    velocity.y += double(gravity);
    neon::particles::retireProgress(clock, dead, neon::particles::ProgressEnd::Above);
}
void breakup(World &w, Vec2 pos, int count, int mode) {
    double phase = mode == 4 ? 1.570796327 : 0, second = 0;
    const double step = 6.2831853072 / double(float(count)),
                 secondStep = mode == 3 ? 31.415926536 / double(float(count)) : 0;
    for (int i = 0; i < count; ++i) {
        auto *f = new Fragment(w);
        f->position = pos;
        f->retained = pos;
        f->type = 0x2002;
        f->radius = .015f;
        f->frame = 41;
        f->scale.x = .7;
        const float a = float(phase);
        if (mode == 4) {
            f->palette = 1;
            f->frame = 29;
            f->scale.x = 1;
            const float sine = std::sin(a),
                        y = float(float(std::cos(a) * 13.f) +
                                  float(std::cos(float(phase + phase)) * -5.f)) +
                            float(std::cos(float(phase * 3.)) * -2.f);
            f->velocity = {double(.00080000004f) * std::pow(double(sine), 3.) * 16.,
                           double(-.00080000004f) * (double(y) - std::cos(phase * 4.))};
        } else {
            f->velocity = {double(float(std::cos(a) * .008f)), double(float(std::sin(a) * -.008f))};
            if (mode == 3) {
                f->palette = 8;
                f->frame = 28;
                f->scale.x = 1;
                f->velocity =
                    f->velocity * double(float(double(std::sin(float(second))) * .2 * 2.5 + 1.));
            }
        }
        f->velocity = f->velocity * w.random.range(.3, 1.);
        f->lifetimeRate = float(w.random.range(.001, .003));
        f->spin = float(w.random.range(-50, 50));
        f->angle = heading(f->velocity);
        w.add(Domain::PlayerShots, f);
        phase += step;
        second += secondStep;
    }
}
void scatter(World &w, Vec2 position, int count, int mode) {
    for (int i = 0; i < count; ++i) {
        auto *f = new Fragment(w);
        f->position = f->retained = position;
        f->type = 0x2002;
        f->radius = .02f;
        f->frame = mode == 3 ? 28 : mode == 4 ? 29 : 2;
        f->palette = mode == 3 ? 8 : mode == 4 ? 1 : 0;
        f->velocity = {0, w.random.range(-.01, -.003)};
        f->velocity.rotateBy(w.random.range(-3.141592654, 3.141592654));
        f->lifetimeRate = float(w.random.range(.001, .003));
        f->spin = float(w.random.range(-50, 50));
        f->angle = heading(f->velocity);
        f->gravity = .0006f;
        w.add(Domain::PlayerShots, f);
    }
}
} // namespace superoxwars
