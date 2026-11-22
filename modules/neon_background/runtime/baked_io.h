// Fixed-endian, bounded scene preparation data. No native object memory is serialized.
#pragma once
#include "effect_runtime.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
namespace neon::effects::baked {
struct IO {
    std::istream *input = nullptr;
    std::ostream *output = nullptr;
    size_t *budget = nullptr;
    void consume(size_t n) {
        if (budget) {
            need(n <= *budget);
            *budget -= n;
        }
    }
    explicit IO(std::istream &in, size_t *bytes = nullptr) : input(&in), budget(bytes) {}
    explicit IO(std::ostream &out) : output(&out) {}
    void need(bool ok) const {
        if (!ok)
            throw std::runtime_error("Invalid baked Neon scene");
    }
    template <class T> void value(T &v) {
        static_assert(std::is_arithmetic_v<T>);
        if (input && !std::is_same_v<T, bool>)
            consume(sizeof(T));
        if constexpr (std::is_same_v<T, bool>) {
            uint32_t bits = v ? 1 : 0;
            value(bits);
            need(bits <= 1);
            v = bits != 0;
        } else {
            using Bits = std::conditional_t<sizeof(T) == 8, uint64_t, uint32_t>;
            Bits bits = 0;
            if (output)
                std::memcpy(&bits, &v, sizeof(T));
            for (size_t i = 0; i < sizeof(T); ++i) {
                if (output)
                    output->put(char((bits >> (8 * i)) & 255));
                else {
                    int c = input->get();
                    need(c != std::char_traits<char>::eof());
                    bits |= Bits(uint8_t(c)) << (8 * i);
                }
            }
            if (input)
                std::memcpy(&v, &bits, sizeof(T));
            if constexpr (std::is_floating_point_v<T>)
                need(std::isfinite(v));
        }
    }
    void text(std::string &s, size_t limit = 64 * 1024 * 1024) {
        uint32_t n = uint32_t(s.size());
        value(n);
        need(n <= limit);
        if (input) {
            consume(n);
            s.resize(n);
            input->read(s.data(), n);
            need(bool(*input));
        } else
            output->write(s.data(), n);
    }
    template <class T, size_t N> void array(std::array<T, N> &a) {
        for (auto &v : a)
            value(v);
    }
    template <class F> void count(size_t n, size_t max, F f) {
        uint32_t size = uint32_t(n);
        value(size);
        need(size <= max);
        f(size);
    }
};
inline void playback(IO &io, pc::RecoveredScene &scene, bool components, unsigned version = 3) {
    auto &p = scene.preset;
    io.value(p.outputStack);
    io.need(p.outputStack >= 0 && p.outputStack < 16);
    io.count(p.nodes.size(), 512, [&](size_t count) {
        if (io.input)
            p.nodes.resize(count);
        for (auto &n : p.nodes) {
            io.value(n.stack);
            io.value(n.type);
            io.value(n.inputMode);
            io.need(n.stack >= 0 && n.stack < 16);
            io.array(n.saved.integers);
            io.array(n.saved.floats);
            for (auto &c : n.savedChannels) {
                io.value(c.rate);
                io.value(c.offset);
                io.value(c.amplitude);
                io.value(c.speed);
                io.value(c.phaseShift);
                io.value(c.flags);
                io.value(c.clock);
                io.value(c.waveform);
            }
            n.effective = n.saved;
            n.channels = n.savedChannels;
        }
    });
    for (auto &s : p.stacks)
        io.value(s.tempo);
    io.count(p.savedRoutes.size(), 16384, [&](size_t count) {
        if (io.input)
            p.savedRoutes.resize(count);
        for (auto &r : p.savedRoutes) {
            uint32_t node = uint32_t(r.node);
            io.value(node);
            r.node = node;
            io.value(r.floating);
            io.value(r.slot);
            io.value(r.channel);
            io.value(r.minimum);
            io.value(r.maximum);
            io.need(r.node < p.nodes.size() && r.slot >= 0 && r.slot < 16 && r.channel >= 0 &&
                    r.channel < 32);
        }
    });
    io.array(scene.palette);
    for (size_t n = 0; n < (version == 1 ? 92 : version == 2 ? 94 : scene.masks.size()); ++n)
        io.value(scene.masks[n]);
    scene.masks[92] = scene.masks[93] = 1;
    io.array(scene.neutralView);
    for (size_t n = 0; n < (version == 1 ? 92 : version == 2 ? 94 : scene.constants.size()); ++n)
        io.value(scene.constants[n]);
    scene.constants[92] = scene.constants[93] = 1;
    scene.masks[94] = scene.masks[95] = 1;
    scene.constants[94] = scene.constants[95] = 3;
    io.count(scene.nonDrawingShapes.size(), 512, [&](size_t count) {
        auto items = scene.nonDrawingShapes;
        if (io.input)
            scene.nonDrawingShapes.clear();
        auto at = items.begin();
        for (size_t k = 0; k < count; ++k) {
            uint32_t n = io.output ? uint32_t(*at++) : 0;
            io.value(n);
            io.need(n < p.nodes.size());
            if (io.input)
                scene.nonDrawingShapes.insert(n);
        }
    });
    if (io.input) {
        for (auto &s : p.stacks)
            s.nodes.clear();
        for (size_t k = 0; k < p.nodes.size(); ++k)
            p.stacks[p.nodes[k].stack].nodes.push_back(k);
        p.routes = p.savedRoutes;
        if (components) {
            scene.reachable.fill(false);
            scene.reachable[0] = true;
            scene.stages.fill(0);
        } else
            scene.rebuildGraph();
        scene.reset();
    }
}
inline void controls(IO &io, std::vector<pc::ReferenceControl> &controls) {
    io.count(controls.size(), 16384, [&](size_t count) {
        if (io.input)
            controls.resize(count);
        for (auto &c : controls) {
            io.text(c.key, 256);
            io.text(c.label, 2048);
            for (float *v :
                 {&c.minimum, &c.maximum, &c.value, &c.live, &c.current, &c.period, &c.depth})
                io.value(*v);
            io.value(c.enabled);
            io.value(c.motion);
            io.value(c.driven);
            io.value(c.integer);
            io.value(c.applicable);
            io.value(c.shaderBank);
            io.value(c.layer);
            io.value(c.retained);
            bool discrete = !c.choices.empty();
            io.value(discrete);
            if (io.input && discrete)
                c.choices.push_back({0, {}});
        }
    });
}
inline void graph(IO &io, pc::GraphRecipe &g) {
    io.value(g.output);
    io.value(g.mixBase);
    io.value(g.mixOutput);
    io.count(g.speedDrivers.size(), 32, [&](size_t count) {
        if (io.input)
            g.speedDrivers.resize(count);
        for (auto &v : g.speedDrivers) {
            io.value(v.first);
            io.value(v.second);
        }
    });
}
} // namespace neon::effects::baked
