#ifdef NDEBUG
#undef NDEBUG
#endif
#define NEON_GAME minotron
#include "engine/bindings/common/neon_base.h"
#undef NEON_GAME
#define NEON_GAME minorescue
#include "cores/minorescue/engine/neon_base.h"
#undef NEON_GAME
#include "reconstruction/gridrunner/native_colour.h"
#include "reconstruction/gridrunner/native_entity.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    namespace mt = minotron::Neon_Internals;
    namespace mr = minorescue::Neon_Internals;
    namespace ios = gr::reconstruction::ios;
    mt::vec2d early{-0., 1};
    mr::vec2d rescue{-0., 1};
    ios::Vec2 late{-0., 1};
    early.rotateBy(0);
    rescue.rotateBy(0);
    late.rotateBy(0);
    assert(!std::signbit(early.x) && !std::signbit(rescue.x) && std::signbit(late.x));
    mt::colour a;
    mr::colour b;
    ios::Rgba c;
    a.phase[1] = b.phase[1] = c.phase[1] = 3.25f;
    a.speed[2] = b.speed[2] = c.speed[2] = .25f;
    a.white(2);
    b.white(2);
    c.white(2);
    assert(a.a == 1 && b.a == 2 && c.a == 1);
    assert(a.phase[1] == 3.25f && b.phase[1] == 3.25f && c.phase[1] == 3.25f);
    assert(a.speed[2] == .25f && b.speed[2] == .25f && c.speed[2] == .25f);
    for (int f = 0; f < 256; ++f) {
        auto m = mt::getDynamicColour(10, .4f, f);
        auto r = mr::getDynamicColour(10, .4f, f);
        auto g = ios::dynamicColour(10, .4f, uint32_t(f));
        assert(m.g == ((f & 4) ? 0 : 1) && r.g == ((f & 8) ? 0 : 1) && g.g == m.g);
        assert(m.a == double(.4f) && r.a == m.a && g.a == m.a);
    }
    std::cout
        << "Actual Minotron/Rescue/Gridrunner adapters: numeric variants, original Rescue timing, full colour state passed\n";
}
