#include "neon/particles.h"
#include "reconstruction/minos/ship.h"
#include "reconstruction/gridrunner/native_effects.h"
#include "cores/fiveaday/world.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace frozen {
using fiveaday::unpack;
using fiveaday::interpolatePacked;
#include "fixtures/mechanisms-reference/particle_updates.h"
} // namespace frozen
int main() {
    try {
        using neon::particles::ProgressEnd;
        int comparisons = 0;
        // Independent rule expectations include signed zero, endpoints, NaN, infinities.
        for (float p :
             {-.1f, -0.f, 0.f, .5f, 1.f, std::nextafter(1.f, 2.f), 2.f,
              std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            for (auto rule :
                 {ProgressEnd::AboveAndClamp, ProgressEnd::Above, ProgressEnd::AtOrAbove})
                for (bool wasDead : {false, true}) {
                    float a = p;
                    bool dead = wasDead;
                    neon::particles::retireProgress(a, dead, rule);
                    bool terminal = rule == ProgressEnd::AtOrAbove ? p >= 1 : p > 1;
                    CHECK(dead == (wasDead || terminal));
                    float expected = terminal && rule == ProgressEnd::AboveAndClamp ? 1.f : p;
                    CHECK(std::memcmp(&a, &expected, 4) == 0);
                    ++comparisons;
                }
        for (float p : {-.1f, -0.f, 0.f, .1f, .2f, std::nextafter(.2f, 0.f),
                        std::nextafter(.2f, 1.f), 1.f, std::numeric_limits<float>::quiet_NaN()})
            for (double cutoff : {0., .2})
                for (auto rule :
                     {neon::particles::LifeEnd::Below, neon::particles::LifeEnd::AtOrBelow})
                    for (bool wasDead : {false, true}) {
                        float a = p;
                        bool dead = wasDead;
                        neon::particles::retireLife(a, dead, rule, cutoff);
                        bool ended = rule == neon::particles::LifeEnd::Below ? double(p) < cutoff
                                                                             : double(p) <= cutoff;
                        float expected = ended ? 0.f : p;
                        CHECK(dead == (wasDead || ended));
                        CHECK(std::memcmp(&a, &expected, 4) == 0);
                        ++comparisons;
                    }
        minos::PlayState game;
        game.scrollDelta = {.003, -.007};
        for (bool paused : {false, true})
            for (float phase : {0.f, .9f, 1.f, 1.1f})
                for (float rate : {0.f, .01f, .1f, -.02f}) {
                    game.paused = paused;
                    minos::Explo a;
                    frozen::minosExplo b;
                    a.g = &game;
                    a.t = phase;
                    a.rate = rate;
                    a.pos = {.1, .2};
                    a.from = .3f;
                    a.to = 2.7f;
                    a.baseScale = {1.25, -.75};
                    static_cast<minos::Explo &>(b) = a;
                    for (int frame = 0; frame < 5; ++frame) {
                        a.run();
                        b.oldRun();
                        CHECK(a.t == b.t && a.dead == b.dead && a.pos.x == b.pos.x &&
                              a.pos.y == b.pos.y && a.scale.x == b.scale.x &&
                              a.scale.y == b.scale.y && a.col.a == b.col.a);
                        ++comparisons;
                    }
                }
        fiveaday::World world(17);
        world.camera = {.002, -.003};
        for (int sprite : {6, 29, 51})
            for (float phase : {0.f, .9f, 1.f, 1.1f})
                for (float rate : {0.f, .01f, .1f, -.02f}) {
                    fiveaday::Explosion a(world);
                    frozen::fiveExplosion b(world);
                    a.progress = phase;
                    a.progressStep = rate;
                    a.frame = sprite;
                    a.position = {.2, .3};
                    a.velocity = {.001, .005};
                    b.progress = a.progress;
                    b.progressStep = a.progressStep;
                    b.frame = a.frame;
                    b.position = a.position;
                    b.velocity = a.velocity;
                    b.radius = a.radius;
                    b.growth = a.growth;
                    b.colour = a.colour;
                    for (int frame = 0; frame < 5; ++frame) {
                        a.run();
                        b.oldRun();
                        CHECK(a.progress == b.progress && a.dead == b.dead &&
                              a.position.x == b.position.x && a.position.y == b.position.y &&
                              a.radius == b.radius);
                        CHECK(a.colour.r == b.colour.r && a.colour.g == b.colour.g &&
                              a.colour.b == b.colour.b && a.colour.a == b.colour.a);
                        ++comparisons;
                    }
                }
        std::array<gr::reconstruction::ios::Rgba, 33> palette{};
        palette[1] = {.1, .2, .3, .4};
        for (int colour : {-1, 0, 1})
            for (float phase : {0.f, .9f, 1.f, 1.1f})
                for (float rate : {0.f, .01f, .1f, -.02f}) {
                    gr::reconstruction::ios::Xpart a(palette);
                    frozen::gridXpart b(palette);
                    a.fade = phase;
                    a.fadeStep = rate;
                    a.growth = 1.07f;
                    a.colour = colour;
                    a.position = {.2, .3};
                    a.velocity = {.004, -.003};
                    static_cast<gr::reconstruction::ios::Entity &>(b) = a;
                    b.fade = a.fade;
                    b.fadeStep = a.fadeStep;
                    b.growth = a.growth;
                    b.colour = a.colour;
                    for (int frame = 0; frame < 5; ++frame) {
                        a.run();
                        b.oldRun();
                        CHECK(a.fade == b.fade && a.dead == b.dead && a.scale.x == b.scale.x &&
                              a.scale.y == b.scale.y && a.position.x == b.position.x &&
                              a.position.y == b.position.y);
                        CHECK(a.tint.r == b.tint.r && a.tint.g == b.tint.g &&
                              a.tint.b == b.tint.b && a.tint.a == b.tint.a);
                        ++comparisons;
                    }
                }
        std::cout << "Growing particle phases: " << comparisons
                  << " source-rule and full update comparisons passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
