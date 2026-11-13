#include "pc_preset.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace neon::pc {
namespace {
uint32_t word(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
float scalar(const uint8_t *p) {
    auto bits = word(p);
    float f;
    std::memcpy(&f, &bits, 4);
    if (!std::isfinite(f))
        throw std::runtime_error("Nonfinite preset value");
    return f;
}
} // namespace
PresetPlayback PresetPlayback::load(const std::filesystem::path &path,
                                    const std::filesystem::path &routing,
                                    const AssetReader &reader) {
    auto in = assetStream(path, reader);
    if (!in)
        throw std::runtime_error("Cannot open original preset");
    std::vector<uint8_t> raw{std::istreambuf_iterator<char>(in), {}};
    constexpr size_t extent = 0x86c;
    if (raw.size() < extent || raw.size() % extent || word(raw.data() + 0x98) != 5)
        throw std::runtime_error("Invalid v5 preset extent/version");
    PresetPlayback p;
    p.outputStack = int(word(raw.data() + 0x90));
    if (p.outputStack < 0 || p.outputStack >= 16)
        throw std::runtime_error("Invalid original output stack");
    int stack = 0;
    size_t program = 0;
    for (; program < 128 && raw[program] != 254; program++) {
        if (raw[program] == 255) {
            if (++stack > 16)
                throw std::runtime_error("Invalid stack separators");
            continue;
        }
        if (stack >= 16 || (p.nodes.size() + 2) * extent > raw.size())
            throw std::runtime_error("Preset node outside record extent");
        auto record = raw.data() + (p.nodes.size() + 1) * extent;
        PresetNode node{};
        node.stack = stack;
        node.type = int(word(record));
        if (node.type != raw[program])
            throw std::runtime_error("Preset program/record mismatch");
        node.inputMode = word(record + 4);
        for (int i = 0; i < 16; i++) {
            node.saved.integers[i] = int32_t(word(record + 0xc + i * 4));
            node.saved.floats[i] = scalar(record + 0x4c + i * 4);
        }
        node.effective = node.saved;
        node.channels = loadChannels(record, extent);
        if (node.type == 0) {
            if (!p.stacks[stack].nodes.empty())
                throw std::runtime_error("Unexpected CONTROL placement");
            p.stacks[stack].tempo = node.saved.floats[0];
        }
        p.stacks[stack].nodes.push_back(p.nodes.size());
        p.nodes.push_back(node);
    }
    if (program == 128 || (p.nodes.size() + 1) * extent != raw.size())
        throw std::runtime_error("Preset terminator/record count mismatch");
    auto routeFile = assetStream(routing, reader);
    if (!routeFile)
        throw std::runtime_error("Cannot open original parameter routes");
    std::string bank;
    ParameterRoute r{};
    while (routeFile >> r.node >> bank >> r.slot >> r.channel >> r.minimum >> r.maximum) {
        r.floating = bank == "float";
        if ((!r.floating && bank != "integer") || r.node >= p.nodes.size() || r.slot < 0 ||
            r.slot > 15 || r.channel < 0 || r.channel > 31 || !std::isfinite(r.minimum) ||
            !std::isfinite(r.maximum) || r.minimum > r.maximum)
            throw std::runtime_error("Invalid parameter route");
        p.routes.push_back(r);
    }
    if (!routeFile.eof())
        throw std::runtime_error("Malformed parameter route");
    return p;
}
void PresetPlayback::reset() {
    ticks = 0;
    unresolvedRoutes.clear();
    for (auto &s : stacks)
        s.clock = {};
    for (auto &n : nodes) {
        n.effective = n.saved;
        n.values = {};
    }
}
bool PresetPlayback::retainedInJukebox(const ParameterRoute &r) const {
    // 00420dd0..004210fb: menu input mode 0 becomes mode 4. When mode >=4,
    // routes to controller waveforms 10..22 skip the destination write entirely.
    // A zero returned by the waveform evaluator is NOT a request to map midpoint.
    const auto &n = nodes.at(r.node);
    unsigned mode = n.inputMode == 0 ? 4 : n.inputMode;
    unsigned wave = n.channels.at(r.channel).waveform;
    return mode >= 4 && wave >= 10 && wave <= 22;
}
void PresetPlayback::tick() {
    for (auto &s : stacks)
        if (!s.nodes.empty())
            s.clock.tick(s.tempo);
    for (auto &n : nodes)
        n.values =
            evaluate(n.channels, stacks[n.stack].clock.clocks(), n.values, n.inputMode, true);
    unresolvedRoutes.clear();
    for (size_t i = 0; i < routes.size(); i++) {
        auto &r = routes[i];
        if (retainedInJukebox(r))
            continue;
        auto &n = nodes[r.node];
        auto value = n.values[r.channel];
        if (!value) {
            unresolvedRoutes.push_back(i);
            continue;
        }
        if (r.floating)
            n.effective.floats[r.slot] = mapFloat(*value, r.minimum, r.maximum);
        else
            n.effective.integers[r.slot] =
                mapInteger(*value, int32_t(r.minimum), int32_t(r.maximum));
    }
    ++ticks;
}
} // namespace neon::pc
