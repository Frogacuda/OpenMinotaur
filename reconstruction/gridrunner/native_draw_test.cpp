#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_draw.h"
#include <cassert>
#include <climits>
#include <iostream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
bool same(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
int main() {
    {
        Entity e;
        e.tint = {.1, .2, .3, .4};
        e.flashTicks = 10;
        e.drawDelay = 2;
        auto s = sampleEntityDraw(e, -1, e.tint);
        assert(!s.visible && e.flashTicks == 10 && e.drawDelay == 2); // hidden-frame early return
        s = sampleEntityDraw(e, 4, e.tint);
        const double first = double(1.f / 7.f);
        assert(!s.visible && e.flashTicks == 9 && e.drawDelay == 1);
        assert(same(s.tint, {first * .5 + .5, first, 0, .4}));
        s = sampleEntityDraw(e, 4, e.tint);
        assert(!s.visible && e.flashTicks == 8 && e.drawDelay == 0 && same(s.tint, {.5, 0, 0, .4}));
        s = sampleEntityDraw(e, 4, e.tint);
        assert(s.visible && e.flashTicks == 7 && same(s.tint, {1, 1, 0, .4}));
        assert(same(e.tint, {.1, .2, .3, .4})); // override belongs to the sprite
        for (int remaining = 6; remaining >= 0; --remaining) {
            s = sampleEntityDraw(e, 4, e.tint);
            const double f = double(float(remaining) / 7.f);
            assert(s.visible && e.flashTicks == remaining && same(s.tint, {f * .5 + .5, f, 0, .4}));
        }
        assert(same(sampleEntityDraw(e, 4, e.tint).tint, e.tint));
        e.drawDelay = -1;
        assert(sampleEntityDraw(e, 4, e.tint).visible && e.drawDelay == -1);
        e.flashTicks = INT_MIN;
        s = sampleEntityDraw(e, 4, e.tint);
        assert(e.flashTicks == INT_MAX && same(s.tint, {1, 1, 0, .4})); // explicit ARM wrap
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        g.lists.clear();
        auto e = std::make_unique<Entity>();
        auto *original = e.get();
        e->sprite = 9;
        e->flashTicks = 2;
        e->tint = {.2, .3, .4, .6};
        e->position = {.125, .25};
        g.lists.prepend(List::L3c4, std::move(e));
        DrawFrame frame;
        const auto random = g.random.state();
        frame.prepare(g);
        assert(frame.sprites().size() == 1 && original->flashTicks == 1);
        const auto saved = frame.sprites()[0];
        for (int redraw = 0; redraw < 20; ++redraw) {
            assert(same(frame.sprites()[0].tint, saved.tint));
            assert(original->flashTicks == 1);
        }
        assert(g.random.state() == random);
        frame.prepare(g);
        assert(original->flashTicks == 0 && same(frame.sprites()[0].tint, {.5, 0, 0, .6}));
        frame.prepare(g);
        assert(same(frame.sprites()[0].tint, original->tint));
        original->tint = {1, 0, 1, 1};
        g.lists.clear(); // snapshot has no borrowed object pointers
        assert(same(frame.sprites()[0].tint, {.2, .3, .4, .6}));
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        g.lists.clear();
        auto t = std::make_unique<Tao>();
        auto *original = t.get();
        t->sprite = 7;
        t->initTrail();
        t->arrival = .5f;
        t->flashTicks = 10;
        t->drawDelay = 2;
        g.lists.prepend(List::L3bc, std::move(t));
        DrawFrame frame;
        frame.prepare(g);
        // Tao invokes the base draw eight times; the first two suppressed
        // calls still consume flash ticks. The remaining six retain trail alpha.
        assert(original->flashTicks == 2 && original->drawDelay == 0 &&
               frame.sprites().size() == 6);
        assert(same(frame.sprites()[0].tint, {1, 1, 0, .5}));
        const double last = double(2.f / 7.f);
        assert(same(frame.sprites()[5].tint, {last * .5 + .5, last, 0, .5}));
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        g.lists.clear();
        auto s = std::make_unique<Ship>();
        auto *original = s.get();
        s->initialize(false);
        s->state = Ship::State::Playing;
        s->radius = .04f;
        s->flashTicks = 1;
        g.pickupTints[1] = {0, 0, 1, 1};
        g.lists.prepend(List::L3b0, std::move(s));
        DrawFrame frame;
        frame.prepare(g);
        assert(frame.sprites().size() == 1 && same(frame.sprites()[0].tint, {.5, 0, 0, .5}));
        assert(
            same(original->tint, unpackRgba(0x20ff40ff))); // original shield restores ship colour
        original->state = Ship::State::Departing;
        original->arrival = .75f;
        original->flashTicks = 9;
        frame.prepare(g);
        assert(frame.sprites().size() == 7 && original->flashTicks == 2);
        for (const auto &sprite : frame.sprites())
            assert(sprite.tint.b == 0 && sprite.tint.a == .75);
        original->state = Ship::State::Hidden;
        const int before = original->flashTicks;
        frame.prepare(g);
        assert(frame.sprites().empty() && original->flashTicks == before);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        auto &world = dynamic_cast<BottomLaser *>(g.lists.first(List::L3bc))->world;
        g.lists.clear();
        auto b = std::make_unique<LaserBolt>(world);
        auto *original = b.get();
        b->frames.fill(11);
        b->flashTicks = 5;
        b->drawDelay = 2;
        g.lists.prepend(List::L3bc, std::move(b));
        DrawFrame frame;
        frame.prepare(g);
        assert(frame.sprites().size() == 30 && original->flashTicks == 5 &&
               original->drawDelay == 2);
        // LaserBolt's custom draw never calls the base draw and must not gain
        // its counter or colour overrides through a blanket renderer change.
        for (int i = 0; i < 30; ++i)
            assert(frame.sprites()[size_t(i)].beamRow == i);
    }
    std::cout << "Base draw flash/delay, hidden frames, trail/ship dispatch, custom beam exclusion "
                 "and redraw independence passed\n";
}
