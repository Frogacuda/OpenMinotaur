#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_campaign.h"
#include <cassert>
#include <iostream>
using namespace gr::reconstruction;
using namespace gr::reconstruction::ios;
int main() {
    {
        Campaign g([](int) { return Vec2{32, 16}; });
        auto *cannon = dynamic_cast<BottomLaser *>(g.lists.first(List::L3bc));
        assert(cannon);
        LaserBolt bolt(cannon->world);
        bolt.initialize(*cannon);
        LaserZap zap(cannon->world);
        zap.initialize(*cannon);
        const auto same = [](Rgba a, Rgba b) {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        };
        // Original iOS flashes row at 0x6a24c: each RGBW colour lasts four
        // global frames. The travelling zap and warning keep gcol[4].
        constexpr uint32_t fired[] = {0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffffffff};
        for (uint32_t frame = 0; frame < 32; ++frame) {
            std::array<Rgba, 33> colours{};
            colours[4] = unpackRgba((frame & 12) == 0 ? 0xffffffff : 0xff00ffff);
            colours[6] = unpackRgba(fired[(frame & 15) / 4]);
            g.updatePalette(colours);
            bolt.age = 0;
            auto expected = colours[4];
            expected.a = 0;
            assert(same(bolt.drawTint(), expected));
            bolt.age = g.ac.laserCharge - 1;
            expected.a = double(float(bolt.age) / float(g.ac.laserCharge)) * .7;
            assert(same(bolt.drawTint(), expected));
            bolt.age = g.ac.laserCharge;
            assert(same(bolt.drawTint(), colours[6]));
            bolt.age = g.ac.laserCharge + 1;
            assert(same(bolt.drawTint(), colours[6]));
            const auto randomState = g.random.state();
            for (int redraw = 0; redraw < 5; ++redraw)
                assert(same(bolt.drawTint(), colours[6]));
            assert(g.random.state() ==
                   randomState); // host redraws cannot shift gameplay randomness
            zap.initialize(*cannon);
            zap.run();
            assert(same(zap.tint, colours[4]));
        }
    }
    {
        LevelProgram p;
        int count = 0, total = 0;
        p.step(0, [&](int n, int t) {
            assert(t == 7);
            ++count;
            total += n;
        });
        assert(count == 0 && p.variables['Z' - 'A'] == 270 && p.variables['V' - 'A'] == 2);
        p.step(10, [&](int n, int t) {
            assert(t == 7 && n == 8);
            ++count;
            total += n;
        });
        assert(count == 1 && p.wait == 500);
        for (int i = 0; i < 500; ++i)
            p.step(10, [&](int, int) { assert(false); });
        assert(p.wait == 0);
        for (int i = 0; i < 10000 && p.wait >= 0; ++i)
            p.step(0, [&](int n, int t) {
                assert(t == 7);
                ++count;
                total += n;
            });
        assert(count == 7 && total == 60 && p.variables['V' - 'A'] == 4);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 1, 600);
        std::array<Rgba, 33> colours{};
        g.updatePalette(colours);
        for (int i = 0; i < 270; ++i) {
            g.ac.powers[5] = 100000;
            g.tick({});
        }
        bool found = false;
        for (auto *n = g.lists.first(List::L3bc); n; n = n->next)
            if (auto *b = dynamic_cast<LaserBolt *>(n)) {
                found = true;
                assert(b->age == 0 && b->radius == .05f);
            }
        assert(found && g.ac.laserFrozen && g.launches() >= 1);
        for (int i = 0; i < 100; ++i) {
            g.ac.powers[5] = 100000;
            g.tick({});
        }
        assert(!g.ac.laserFrozen); // zap crosses the frozen bolt and releases emitters
        bool pod = false;
        for (auto *n = g.lists.first(List::L3c4); n; n = n->next)
            pod |= dynamic_cast<Pod *>(n) != nullptr;
        assert(pod);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 1);
        for (int i = 0; i < 12000 && g.state() != Campaign::State::Complete &&
                        g.state() != Campaign::State::GameOver;
             ++i)
            g.tick({});
        assert(g.state() == Campaign::State::Complete || g.state() == Campaign::State::GameOver);
        std::cout << "Stationary scenario: ticks=" << g.clock << " state=" << int(g.state())
                  << " score=" << g.points() << '\n';
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 1);
        for (int i = 0; i < 3000 && g.state() != Campaign::State::GameOver; ++i) {
            if (auto *ship = g.currentShip(); ship && ship->state == Ship::State::Playing) {
                g.ac.powers[5] = 0;
                ship->radius = .02f;
                ship->foom();
            }
            g.tick({});
        }
        assert(g.state() == Campaign::State::GameOver && g.ac.lives == 0 && g.ac.gameOver);
    }
    {
        Campaign g([](int) { return Vec2{32, 16}; }, 123);
        // Screen power is an original mechanism; keeping it active here lets
        // the scenario exercise the actual entire first-grid script/finish.
        for (int i = 0; i < 12000 && g.state() != Campaign::State::Complete; ++i) {
            g.ac.powers[5] = 100000;
            g.ac.powers[7] = 100000;
            g.tick({});
        }
        assert(g.state() == Campaign::State::Complete && g.launches() == 7 && g.ac.lives == 4 &&
               g.points() > 0);
    }
    std::cout << "Original first-grid script, concrete laser/pod interactions, life exhaustion and "
                 "complete wave/finish integration passed\n";
}
