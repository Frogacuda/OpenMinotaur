#pragma once
#include "pc_preset.h"
namespace neon::pc {
inline std::array<int, 3> moduleSamplerInputs(const PresetNode &n, const ShapeSettings &s) {
    const auto &i = s.integers;
    switch (n.type) {
    case 9:
        return {i[5], i[3], i[6]};
    case 6:
        return {31 + n.stack, i[3], 0};
    case 1:
        return {i[6], i[7], 0};
    case 15:
        return {i[3], i[4], i[12]};
    case 19:
        return {i[4], i[5], i[8]};
    case 22:
        return {i[8], i[1], i[3]};
    case 11: {
        int last = 0;
        for (int face = 0; face < 6; ++face)
            if (i[face] > 0)
                last = i[face];
        return {last, i[12], i[13]};
    }
    default:
        return {};
    }
}
// 0043ee80/0043eec0/0043ef00 keep a binding for selectors <= 0.
// 00421ae9..00421b1b leave the completed stack image on all three samplers.
// Resolve this logical state explicitly, independent of the host GL context.
inline std::array<int, 3>
resolvedSamplerInputs(const PresetPlayback &p, size_t node,
                      const std::vector<ShapeSettings> *values = nullptr,
                      const std::vector<std::array<int, 3>> *overrides = nullptr) {
    int stack = p.nodes.at(node).stack, previous = -1;
    for (int s = 0; s < stack; ++s)
        if (!p.stacks[s].nodes.empty())
            previous = s;
    std::array<int, 3> result{};
    if (previous >= 0)
        result.fill(31 + previous);
    for (auto index : p.stacks[stack].nodes) {
        const auto &n = p.nodes[index];
        const auto &settings = values ? values->at(index) : n.saved;
        auto inputs = moduleSamplerInputs(n, settings);
        if (overrides)
            for (int sampler = 0; sampler < 3; ++sampler)
                if (overrides->at(index)[sampler] > 0)
                    inputs[sampler] = overrides->at(index)[sampler];
        for (int sampler = 0; sampler < 3; ++sampler)
            if (inputs[sampler] > 0)
                result[sampler] = inputs[sampler];
        if (index == node)
            break;
    }
    return result;
}
} // namespace neon::pc
