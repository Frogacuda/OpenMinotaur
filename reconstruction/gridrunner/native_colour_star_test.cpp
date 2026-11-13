#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_star.h"
#include "native_draw.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
bool same(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a && a.phase == b.phase &&
           a.speed == b.speed && a.amplitude == b.amplitude && a.base == b.base;
}
int main() {
    {
        Rgba c;
        c.phase = {1, 2, 3};
        c.speed = {.1f, .2f, .3f};
        c.amplitude = {-1, 2, 3};
        c.base = {-2, -3, -4};
        const auto original = c;
        c.unpack(0x12345678);
        assert(c.r == 18. / 255 && c.a == 120. / 255 && c.phase == original.phase &&
               c.base == original.base);
        c.white(-2);
        assert(c.a == 0 && c.r == 1 && c.phase == original.phase);
        c.white(2);
        assert(c.a == 1);
        c.white(std::numeric_limits<double>::quiet_NaN());
        assert(std::isnan(c.a)); // iOS comparisons preserve NaN
        const auto copy = c;
        assert(copy.phase == c.phase && copy.speed == c.speed && copy.amplitude == c.amplitude);
        c.phase = {0, 1.5707964f, -1.5707964f};
        c.speed = {};
        c.amplitude = {2, -2, 3};
        c.base = {-1, .5f, 2};
        c.a = -.25;
        c.generate();
        assert(c.r == 0 && c.g == -1.5 && c.b == 2 && c.a == -.25); // no clamping or alpha update
        c.phase = {16777216.f, 0, 0};
        c.speed = {1, 0, 0};
        c.generate();
        assert(c.phase[0] == 16777216.f); // float accumulation
        c.phase = {7, 8, 9};
        c.speed = {.25f, -.5f, 1};
        c.generate();
        assert(c.phase[0] == 7.25f && c.phase[1] == 7.5f && c.phase[2] == 10); // no phase wrap
    }
    {
        for (uint32_t f = 0; f < 128; ++f) {
            const auto c = dynamicColour(0, .25f, f);
            assert(c.a == .25);
            assert(c.r == double(float(std::abs(int(f & 31) - 15)) * .0625f) * .9 + .1);
            assert(c.g == double(float(std::abs(int((f & 63) >> 1) - 15)) * .0625f) * .9 + .1);
            assert(c.b == double(float(std::abs(int((f & 15) * 2) - 15)) * .0625f) * .9 + .1);
            for (int type = 1; type <= 13; ++type) {
                const auto q = dynamicColour(type, .25f, f);
                assert(q.a == (type == 6 ? 1 : .25));
            }
            assert(dynamicColour(4, 1, f).g == ((f & 12) ? 0 : 1));
            assert(dynamicColour(8, 1, f).g == ((f & 14) ? 0 : 1));
            assert(dynamicColour(12, 1, f).r == ((f & 8) ? 1 : 0));
        }
        assert(dynamicColour(-1, .2f, 9).a == 1 && dynamicColour(14, .2f, 9).r == 1);
        assert(dynamicColour(9, 1, 0).b == 0 && dynamicColour(9, 1, 2).b == 1);
        assert(dynamicColour(13, 1, 0).b == 1 && dynamicColour(13, 1, 8).b == 0);
    }
    {
        DarwinRandom r(1);
        Star s;
        s.initialize(r);
        assert(s.tint.r == 1 && s.tint.g == 0 && s.tint.b == 1 && s.tint.a == 1);
        assert(s.position.x == double(282475249) / 2147483647. - .5);
        assert(s.position.y == 1.2 * (double(1622650073) / 2147483647.) - .6);
        assert(s.age == (984943658 & 31) && s.sprite == 2 && s.radius == .005f &&
               r.state() == 984943658);
        s.age = 14;
        assert(s.visible());
        s.run();
        assert(!s.visible());
        s.age = 31;
        s.run();
        assert(s.visible());
        s.position.y = .6 - .004;
        s.run();
        assert(s.position.y == .6);
        s.run();
        assert(s.position.y == -.6);
        s.age = INT32_MAX;
        s.run();
        assert(s.age == INT32_MIN);
    }
    {
        DarwinRandom r(1);
        Starfield stars(r);
        assert(r.state() == 463482574);
        // Original insertion reverses constructor order: the first star is last.
        assert(stars.stars()[99].position.x == double(282475249) / 2147483647. - .5);
        const auto state = r.state();
        for (auto &s : stars.stars())
            s.age = 16;
        auto &star = stars.stars()[0];
        star.age = 0;
        star.flashTicks = 3;
        star.drawDelay = 1;
        star.tint.phase = {1, 2, 3};
        DrawFrame frame;
        frame.prepare(stars);
        assert(frame.sprites().empty() && star.flashTicks == 2 && star.drawDelay == 0);
        frame.prepare(stars);
        assert(frame.sprites().size() == 1 && frame.sprites()[0].frame == 4 &&
               star.flashTicks == 1);
        assert(frame.sprites()[0].tint.phase == star.tint.phase &&
               frame.sprites()[0].diameter == .005f + .005f);
        const auto saved = frame.sprites()[0];
        for (int i = 0; i < 30; ++i)
            assert(same(saved.tint, frame.sprites()[0].tint));
        assert(r.state() == state);
        stars.tick();
        assert(r.state() == state);
        Campaign first([](int) { return Vec2{32, 16}; }, 999, 0, 3, false, 0, false, 0, &r);
        assert(&first.random == &r && r.state() == state);
        first.globalRandom();
        const auto after = r.state();
        assert(after != state);
        Campaign second([](int) { return Vec2{32, 16}; }, 1, 0, 3, false, 0, false, 0, &r);
        assert(&second.random == &r &&
               r.state() == after); // a new campaign must not reseed the global stream
    }
    std::cout << "Full colour copies/cycler precision, iOS palette cases, star "
                 "RNG/order/wrap/blink/draw and session stream continuity passed\n";
}
