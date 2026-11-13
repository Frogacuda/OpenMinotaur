#include "reconstruction/deflex/particles.h"
#include "cores/goatup/effects.h"
#include "cores/superoxwars/effects.h"
#include "neon/particles.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <list>
#include <stdexcept>
#include <vector>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace deflex_reference {
using namespace deflex;
neon::colour dynamicColour(int type, float alpha, int32_t age) {
    const auto c =
        neon::paletteColour(type, alpha, uint32_t(age), {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
#include "fixtures/particles-reference/deflex.h"
} // namespace deflex_reference
namespace goat_reference {
using goatup::Vec2;
using goatup::ActorDraw;
#include "fixtures/particles-reference/goatup.h"
struct LegacyEffects {
    neon::DarwinRandom &random;
    std::list<LegacyGoatParticle> particles;
    explicit LegacyEffects(neon::DarwinRandom &r) : random(r) {}
    void splash(int count, float speed, float spread, Vec2 at, uint32_t from, uint32_t to) {
        const float low = float(speed + float(double(speed) / -10.)),
                    high = float(speed + float(double(speed) / 10.));
        for (int n = 0; n < count; ++n) {
            Vec2 velocity{0, double(float(random.range(double(low), double(high))))};
            velocity.rotateBy(random.range(double(-spread), double(spread)));
            LegacyGoatParticle p;
            p.position = at;
            p.velocity = velocity;
            p.from.unpackRGBA(from);
            p.to.unpackRGBA(to);
            particles.push_front(p);
        }
    }
    void tick() {
        for (auto it = particles.begin(); it != particles.end();) {
            it->run();
            if (it->dead)
                it = particles.erase(it);
            else
                ++it;
        }
    }
};
} // namespace goat_reference
void sameColour(const neon::colour &a, const neon::colour &b) {
    CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
    for (int i = 0; i < 3; ++i)
        CHECK(a.phase[i] == b.phase[i] && a.speed[i] == b.speed[i] && a.amp[i] == b.amp[i] &&
              a.base[i] == b.base[i]);
}
struct Recorder : deflex::ActorRenderer {
    std::vector<deflex::ActorSprite> records;
    void emit(const deflex::ActorSprite &s) override {
        records.push_back(s);
    }
};
void sameSprite(const deflex::ActorSprite &a, const deflex::ActorSprite &b) {
    CHECK(a.position.x == b.position.x && a.position.y == b.position.y && a.size.x == b.size.x &&
          a.size.y == b.size.y);
    CHECK(a.frame == b.frame && a.depth == b.depth && a.angle == b.angle &&
          a.orFlags == b.orFlags && a.region == b.region);
    CHECK(a.regionOrigin.x == b.regionOrigin.x && a.regionOrigin.y == b.regionOrigin.y &&
          a.regionSize.x == b.regionSize.x && a.regionSize.y == b.regionSize.y);
    sameColour(a.colour, b.colour);
}
namespace sow_reference {
struct LegacyFragment : superoxwars::Fragment {
    using superoxwars::Fragment::Fragment;
    // Independent reference body for the complete fragment update.
    void oldRun() {
        PlayerBullet::run();
        travel = float(double(travel) + velocity.length());
        angle = travel * spin;
        clock += lifetimeRate;
        velocity.y += double(gravity);
        if (clock > 1)
            dead = true;
    }
};
} // namespace sow_reference
int main() {
    try {
        int comparisons = 0;
        deflex::CellGrid grid;
        grid.setSize(4, 4);
        grid.displacement[0] = {{0, 0}, .5};
        for (int fd : {-1, 0, 3, 14, 99})
            for (int td : {-1, 0, 4})
                for (float life : {0.f, .01f, .5f, 1.f})
                    for (float fade : {0.f, .01f, .03f})
                        for (int variant = 0; variant < 4; ++variant) {
                            deflex::Particle a;
                            deflex_reference::LegacyParticle b;
                            a.fromDynamic = fd;
                            a.toDynamic = td;
                            a.life = life;
                            a.fade = fade;
                            a.pos = {.1, .2};
                            a.vel = {.03, .04};
                            a.drag = {.98, .97};
                            a.angle = .25f;
                            a.from = {.1, .3, .5, .7};
                            a.to = {.9, .7, .5, 0};
                            a.from.phase[0] = 1.25f;
                            a.gridDisplacement = variant == 1;
                            a.fragment = variant >= 2;
                            a.freezeMovement = variant - 2;
                            a.fragmentLife = life;
                            a.fragmentFade = fade;
                            a.regionOrigin = {3, 4};
                            a.regionSize = {5, 6};
                            static_cast<deflex::Particle &>(b) = a;
                            for (int frame = 0; frame < 5; ++frame) {
                                a.run(grid);
                                b.run(grid);
                                CHECK(a.life == b.life && a.dead == b.dead && a.age == b.age &&
                                      a.wrapped == b.wrapped && a.depth == b.depth);
                                CHECK(a.pos.x == b.pos.x && a.pos.y == b.pos.y &&
                                      a.vel.x == b.vel.x && a.vel.y == b.vel.y &&
                                      a.fragmentLife == b.fragmentLife);
                                sameColour(a.from, b.from);
                                sameColour(a.to, b.to);
                                Recorder ar, br;
                                a.draw(ar, {640, 480}, frame & 1);
                                b.draw(br, {640, 480}, frame & 1);
                                CHECK(ar.records.size() == br.records.size());
                                for (size_t n = 0; n < ar.records.size(); ++n)
                                    sameSprite(ar.records[n], br.records[n]);
                                ++comparisons;
                            }
                        }
        for (uint32_t seed : {1u, 17u, 0x12345678u}) {
            neon::DarwinRandom ar(seed), br(seed);
            goatup::Effects actual(ar);
            goat_reference::LegacyEffects expected(br);
            for (int frame = 0; frame < 90; ++frame) {
                if (frame == 0 || frame == 15 || frame == 37) {
                    actual.splash(7, .003f, .75f, {.1, .2}, 0xff008080, 0x0080ff00);
                    expected.splash(7, .003f, .75f, {.1, .2}, 0xff008080, 0x0080ff00);
                }
                actual.tickParticles();
                expected.tick();
                std::vector<goatup::ActorDraw> draw;
                actual.drawParticles(draw);
                CHECK(draw.size() == expected.particles.size() &&
                      size_t(actual.particles()) == draw.size());
                size_t index = 0;
                for (const auto &p : expected.particles) {
                    auto b = p.draw();
                    const auto &a = draw[index++];
                    CHECK(a.position.x == b.position.x && a.position.y == b.position.y &&
                          a.scale.x == b.scale.x && a.scale.y == b.scale.y && a.frame == b.frame &&
                          a.radius == b.radius && a.particle == b.particle);
                    sameColour(a.colour, b.colour);
                }
                CHECK(ar.state() == br.state() && ar.draws() == br.draws());
                ++comparisons;
            }
        }
        superoxwars::World world(17);
        for (float life : {0.f, .01f, .5f, 1.f})
            for (float fade : {0.f, .01f, .1f}) {
                superoxwars::Explosion effect(world, {.1, .2}, .03f, fade, 1.06f);
                effect.opacity = life;
                float opacity = life;
                bool dead = false;
                auto scale = effect.scale;
                const auto draws = world.random.draws();
                for (int frame = 0; frame < 20; ++frame) {
                    // Saved reference Explosion::run scalar update.
                    opacity -= fade;
                    if (opacity < 0) {
                        opacity = 0;
                        dead = true;
                    }
                    scale = scale * double(1.06f);
                    effect.run();
                    CHECK(effect.opacity == opacity && effect.dead == dead &&
                          effect.scale.x == scale.x && effect.scale.y == scale.y &&
                          effect.colour.a == double(opacity));
                    CHECK(effect.position.x == .1 && effect.position.y == .2 &&
                          world.random.draws() == draws);
                    ++comparisons;
                }
            }
        // Independent callback-order and terminal-tick contract.
        struct P {
            float life = .01f, fade = .02f;
            bool dead = false;
            int fromDynamic = 2, toDynamic = 3, age = 41;
            neon::colour from, to;
        } p;
        std::vector<int> calls;
        neon::particles::updateAfterIntegration(p, [&](int type, float alpha, int age) {
            CHECK(age == 41);
            calls.push_back(type);
            return neon::colour(alpha, double(type), 0, alpha);
        });
        CHECK((calls == std::vector<int>{2, 3}) && p.dead && p.life == 0 && p.from.a == 1 &&
              p.to.a == 0);
        // Staged binary32 source interpolation, independent of the shared helper.
        for (float low : {-.5f, 0.f, .3f, 1.f})
            for (float high : {-.25f, 0.f, 2.7f, 10.f})
                for (float life : {0.f, .01f, .5f, 1.f, 1.1f}) {
                    const auto a = neon::particles::scaleFrame(life, low, high);
                    volatile float progress = 1.f - life, difference = high - low,
                                   product = difference * progress, result = product + low;
                    CHECK(a.progress == progress && a.scale == result);
                }
        superoxwars::World fragmentWorld(17);
        fragmentWorld.bulletTiles = [](superoxwars::PlayerBullet &) {};
        for (float phase : {0.f, .9f, 1.f, 1.1f})
            for (float step : {0.f, .01f, .1f, -.02f}) {
                superoxwars::Fragment a(fragmentWorld);
                sow_reference::LegacyFragment b(fragmentWorld);
                for (auto *f : {&a, static_cast<superoxwars::Fragment *>(&b)}) {
                    f->clock = phase;
                    f->lifetimeRate = step;
                    f->position = {.1, -.55};
                    f->velocity = {.001, -.002};
                    f->spin = 17;
                    f->gravity = .0006f;
                    f->arcStep = .01f;
                    f->arcRemaining = .02f;
                }
                for (int frame = 0; frame < 5; ++frame) {
                    a.run();
                    b.oldRun();
                    CHECK(a.clock == b.clock && a.dead == b.dead && a.travel == b.travel &&
                          a.angle == b.angle);
                    CHECK(a.position.x == b.position.x && a.position.y == b.position.y &&
                          a.velocity.x == b.velocity.x && a.velocity.y == b.velocity.y);
                    CHECK(a.arcStep == b.arcStep && a.arcRemaining == b.arcRemaining &&
                          a.repeatClock == b.repeatClock);
                    CHECK(a.colour.r == b.colour.r && a.colour.g == b.colour.g &&
                          a.colour.b == b.colour.b && a.colour.a == b.colour.a);
                    ++comparisons;
                }
            }
        std::cout << "Native particles: " << comparisons
                  << " Deflex/GoatUp/SOW comparisons and independent source contracts passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
