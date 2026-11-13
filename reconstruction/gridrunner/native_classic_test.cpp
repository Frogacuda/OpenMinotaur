#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_classic.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gr::reconstruction::ios {
struct ClassicSourceChecks {
    static void localGoal() {
        Classic g;
        g.start();
        g.high_ = 50000;
        g.addScore(50000);
        assert(g.achievements_.empty());
        g.addScore(1);
        assert(g.achievements_ == std::vector<int>{0});
        g.addScore(1);
        assert(g.achievements_.size() == 1);
        g.start();
        g.high_ = 0;
        g.addScore(50001);
        assert(g.achievements_ == std::vector<int>{0});
    }
    static uint32_t bits(float x) {
        uint32_t n;
        std::memcpy(&n, &x, 4);
        return n;
    }
    static void tables(const char *path) {
        std::ifstream in(path);
        assert(in);
        std::string line;
        std::getline(in, line);
        int count = 0;
        while (std::getline(in, line)) {
            std::istringstream row(line);
            int grid, body, delay, n;
            uint32_t speed, zap;
            row >> grid >> body >> delay >> n >> std::hex >> speed >> zap;
            assert(row);
            const auto r = vicRules(grid);
            assert(r.body == body && r.delay == delay && r.count == n);
            assert(bits(r.speed) == speed && bits(r.zapSpeed) == zap);
            ++count;
        }
        assert(count == 32);
    }
    static void observed(const char *path) {
        std::map<std::string, uint32_t> actual;
        const auto store = [&](const char *name, const char *field, uint32_t value) {
            actual[std::string(name) + "/" + field] = value;
        };
        for (bool wrap : {false, true}) {
            Classic g;
            g.start();
            g.zapY_ = wrap ? 18.f : 2.f;
            g.doZapper();
            const char *name = wrap ? "zap-wrap" : "zap-normal";
            store(name, "x", bits(g.zapX_));
            store(name, "y", bits(g.zapY_));
            store(name, "timer", bits(g.zapTimer_));
            store(name, "row0", g.screen_.get(0, 0));
            store(name, "row2", g.screen_.get(0, 2));
            store(name, "bottom", g.screen_.get(1, 19));
        }
        for (const char *name : {"edge", "double-seven", "bottom-wrap"}) {
            Classic g;
            g.start();
            auto &p = g.pedes_[0];
            p.length = 0;
            p.speed = .2f;
            p.head = {std::string(name) == "double-seven" ? 10.9f : 21.95f,
                      std::string(name) == "bottom-wrap" ? 18.f : 5.f};
            if (std::string(name) == "double-seven")
                g.screen_.put(11, 5, 0x707);
            g.doPedes();
            store(name, "x", bits(p.head.x));
            store(name, "y", bits(p.head.y));
            store(name, "speed", bits(p.speed));
        }
        for (bool top : {true, false}) {
            Classic g;
            g.start();
            g.fireTimer_ = 3;
            g.bullet_ = {4.5f, top ? 2.2f : 5.5f};
            g.doBull();
            const char *name = top ? "bullet-top" : "bullet-live";
            store(name, "x", bits(g.bullet_.x));
            store(name, "y", bits(g.bullet_.y));
            store(name, "ink", g.screen_.get(4, top ? 1 : 4));
        }
        {
            Classic g;
            g.start();
            g.zap_ = 1;
            g.zapX_ = 12.2f;
            g.zapY_ = 5.3f;
            g.doZapper();
            store("zap-active", "x", bits(g.zap_));
        }
        for (int c : {11, 13, 16}) {
            Classic g;
            g.start();
            g.ship_ = {9.95f, 17};
            g.screen_.put(10, 17, 0x700 | c);
            g.doShip(.025, 0);
            const std::string name = "ship-pod-" + std::to_string(c);
            store(name.c_str(), "x", bits(g.ship_.x));
            store(name.c_str(), "y", bits(g.ship_.y));
            store(name.c_str(), "oldInk", g.screen_.get(9, 17));
            store(name.c_str(), "podInk", g.screen_.get(10, 17));
        }
        {
            Classic g;
            g.start();
            for (int i = 0; i < 88; ++i)
                g.explodeShip();
            store("explosion-88", "radius", bits(g.transition_));
            store("explosion-88", "lives", g.lives_);
        }
        std::ifstream in(path);
        assert(in);
        std::string line;
        int count = 0;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line.rfind("case\t", 0) == 0)
                continue;
            std::string name, field;
            uint32_t expected;
            std::istringstream row(line);
            row >> name >> field >> std::hex >> expected;
            assert(row);
            const auto found = actual.find(name + "/" + field);
            if (found == actual.end() || found->second != expected) {
                std::cerr << "Original Classic mismatch: " << name << "/" << field << " expected "
                          << std::hex << expected << " actual "
                          << (found == actual.end() ? 0 : found->second) << "\n";
                throw std::runtime_error("original Classic fixture divergence");
            }
            ++count;
        }
        assert(count == 42 && actual.size() == 42);
    }
    static void screen() {
        Classic title;
        constexpr int lineFrames[] = {0x307, 0x307, 0x308, 0x308, 0x307, 0x307, 0x308, 0x308};
        for (uint32_t frame = 0; frame < 8; ++frame) {
            title.title(0, frame);
            for (int x = 3; x < 18; ++x)
                for (int y : {10, 12})
                    assert(title.screen().get(x, y) == lineFrames[frame]);
        }
        VicScreen s;
        s.clear();
        s.floating(10.2f, 5.5f, 0x34e, 0x201, 1.2, -5.1f);
        assert(s.get(10, 5) == 0x1034e);
        auto draw = s.draw();
        assert(draw.size() == 2);
        assert(draw[0].ink == 0x201 && draw[0].x == 10 && draw[0].y == 5);
        assert(draw[1].ink == 0x34e && draw[1].x == double(10.2f - .5f) && draw[1].y == 5);
        // One record per cell; writes from bullets/pods replace earlier heads.
        s.floating(10.5f, 5.5f, 0x70e);
        assert(s.draw().size() == 2 && s.draw().back().ink == 0x70e);
        s.put(10, 5, 0x201);
        assert(s.draw().size() == 1);
        s.stringAt(0, 0, "1234567890", 0x25);
        assert(s.get(0, 0) == 0x40 && s.get(8, 0) == 0x48 && s.get(9, 0) == 0x14);
        s.valueAt(0, 1, 7);
        assert(s.get(0, 1) == 0x14 && s.get(1, 1) == 0x1b);
    }
    static void motion() {
        Classic g;
        g.start();
        g.remaining_ = 0;
        auto &p = g.pedes_[0];
        p.head = {21.95f, 5};
        p.speed = .2f;
        p.length = 0;
        g.doPedes();
        assert(p.head.x == 21.95f && p.head.y == 6 && p.speed == -.2f);
        p.head = {1.05f, 18};
        p.speed = -.2f;
        g.doPedes();
        assert(p.head.x == 1 && p.head.y == 13 && p.speed == .2f);
        p.head = {10.9f, 5};
        p.speed = .2f;
        g.screen_.put(11, 5, 0x707);
        g.doPedes();
        // Both entries for character 7 execute, so this drops two rows and
        // restores the heading. A set-membership shortcut would be wrong.
        assert(p.head.y == 7 && p.speed == .2f);
        p.head = {10, 5};
        p.speed = .2f;
        p.length = 0;
        g.doPedes();
        const auto &f = g.screen_.floats[5 * VicScreen::stride + 10];
        assert(f.ink == 0x34e && f.y == 5 && f.scaleX == 1.2 && f.scaleY == 1.2);
        assert(f.angle == p.head.x * -.5f);
        g.pedes_ = {};
        g.spawnTimer_ = 80;
        assert(!g.doPedes() && g.spawnTimer_ == 0);
        g.remaining_ = 2;
        g.runGR();
        assert(g.pedes_[0].head.x == 20 && g.remaining_ == 1);
        g.pedes_[0].head.x = -1;
        g.spawnTimer_ = 0;
        g.runGR();
        assert(g.pedes_[0].head.x == 1);
    }
    static void splits() {
        Classic g;
        g.start();
        auto &p = g.pedes_[0];
        p.head = {10, 5};
        p.speed = .2f;
        p.length = 4;
        p.x.fill(-1);
        for (int j = 0; j < 4; ++j) {
            p.x[j] = float(9 - j);
            p.y[j] = 5;
            p.direction[j] = .2f;
        }
        g.splitSnake(8, 5);
        assert(g.score() == 100 && p.length == 1);
        assert(g.pedes_[1].head.x == 7 && g.pedes_[1].length == 1 && g.pedes_[1].x[0] == 6);
        g.splitSnake(10, 5);
        assert(g.score() == 500 && p.head.x == 9 && p.length == 0);
        g.splitSnake(9, 5);
        assert(g.score() == 900 && p.head.x < 0);
        // No free slot: the struck snake still truncates and produces a pod.
        for (auto &q : g.pedes_) {
            q.head = {20, 18};
            q.length = 0;
        }
        p.head = {10, 5};
        p.length = 4;
        p.x[0] = 9;
        p.y[0] = 5;
        g.splitSnake(9, 5);
        assert(p.length == 0 && g.screen_.get(9, 5) == 0x70e);
    }
    static void podsBulletsShip() {
        Classic g;
        g.start();
        g.screen_.put(3, 5, 0x70c);
        g.updateGrid();
        assert((g.screen_.get(3, 5) & 255) == 11 && (g.screen_.get(3, 5) & 0x10000));
        g.updateGrid();
        assert(g.bombs_[0].x == 3.5f && g.bombs_[0].y == 5.5f);
        g.doBombs();
        assert((g.screen_.get(3, 5) & 0xffff) == 0x11e);
        // Controlled original execution resolves the misleading iOS boundary
        // decompilation: bullets expire above row 2.
        g.bullet_ = {4.5f, 2.2f};
        g.fireTimer_ = 3;
        g.doBull();
        assert(g.bullet_.x < 0 && g.bullet_.y == float(double(2.2f) - .8));
        g.bullet_ = {4.5f, 5.5f};
        g.doBull();
        assert((g.screen_.get(4, 4) & 0xffff) == 0x104);
        g.bullet_ = {6.5f, 5.5f};
        g.screen_.put(6, 4, 0x710);
        g.doBull();
        assert(g.score() == 10 && g.bullet_.x < 0 && g.fireTimer_ == 0);
        assert((g.screen_.get(6, 4) & 0xffff) == 0x201);
        g.ship_ = {10, 17};
        g.doShip(.1, -.05);
        assert(g.ship_.x == 10.4f && g.ship_.y == 16.8f);
        assert((g.screen_.get(10, 16) & 0xffff) == 0x502);
        g.screen_.put(10, 16, 0x109);
        g.doShip(0, 0);
        assert(g.state_ == Classic::State::Dying);
        g.start();
        g.zapY_ = 2;
        g.zapX_ = 1;
        g.doZapper();
        assert(g.zapY_ == float(2. + .1) && g.zapX_ == float(1. + .1));
        assert((g.screen_.get(0, 2) & 0xffff) == 0x105 && (g.screen_.get(1, 19) & 0xffff) == 0x106);
    }
    static void campaign() {
        for (int grid = 1; grid <= 32; ++grid) {
            Classic g;
            g.start(grid);
            g.tick(0, 0, 1);
            assert(g.grid() == grid && g.remaining_ == vicRules(grid).count - 1);
            for (uint32_t i = 2; i < 16000 && g.state() != Classic::State::GameOver; ++i)
                g.tick(std::sin(i * .01) * .01, 0, i);
        }
        Classic g;
        g.start(32);
        g.remaining_ = 0;
        g.pedes_ = {};
        g.tick(0, 0, 1);
        assert(g.grid() == 1 && g.lives() == 6 && g.state() == Classic::State::BetweenGrids);
        for (int i = 0; i < 249; ++i)
            g.tick(0, 0, uint32_t(i + 2));
        assert(g.state() == Classic::State::BetweenGrids);
        g.tick(0, 0, 251);
        assert(g.state() == Classic::State::Playing);
        g.start();
        g.lives_ = 1;
        g.screen_.put(11, 18, 0x11e);
        g.doShip(0, 0);
        int i = 0;
        while (g.state() == Classic::State::Dying && i < 100)
            g.tick(0, 0, uint32_t(++i));
        assert(g.lives() == 0 && g.state() == Classic::State::BetweenGrids);
        for (int n = 0; n < 250; ++n)
            g.tick(0, 0, uint32_t(++i));
        assert(g.state() == Classic::State::GameOver);
        DarwinRandom shared(1);
        Classic h(&shared);
        h.sound(0);
        assert(shared.state() == 16807 && h.sounds_[0] == 25);
        h.sound(1);
        h.sound(1);
        assert(h.sounds_.size() == 2);
        h.sound(5);
        assert(h.sounds_.size() == 2);
    }
};
} // namespace gr::reconstruction::ios
int main(int argc, char **argv) {
    try {
        using Checks = gr::reconstruction::ios::ClassicSourceChecks;
        if (argc > 1)
            Checks::tables(argv[1]);
        if (argc > 2)
            Checks::observed(argv[2]);
        Checks::localGoal();
        Checks::screen();
        Checks::motion();
        Checks::splits();
        Checks::podsBulletsShip();
        Checks::campaign();
        std::cout << "Classic source checks: tables, cell records, movement, splits, shots, zaps, "
                     "lives and 32 grids passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
