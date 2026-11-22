#include "authoring_model.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
namespace neon::pc {
namespace {
std::string sourceKey(size_t n, int ch) {
    return "cycle.driver." + std::to_string(n) + "." + std::to_string(ch) + ".";
}
} // namespace
void AuthoringModel::initializeCycleControls(int onlyMode) {
    for (int mode = 7; mode <= lastJukeboxPreset; ++mode) {
        if ((mode == 18 || mode == 21 || isStudy(mode)) || (onlyMode >= 0 && mode != onlyMode))
            continue;
        auto &p = composition(mode);
        auto &list = controls(mode);
        std::set<std::pair<size_t, int>> sources;
        auto add = [&](std::string key, std::string label, float low, float high, float original,
                       int layer, bool integer = false) {
            ReferenceControl e;
            e.key = key;
            e.label = label;
            e.minimum = std::min(low, original);
            e.maximum = std::max(high, original);
            e.value = e.live = e.current = original;
            e.layer = layer;
            e.integer = integer;
            if (key.rfind("cycle.driver.", 0) == 0)
                e.cycleSource = key.substr(0, key.find_last_of('.') + 1);
            list.push_back(e);
        };
        for (size_t ri = 0; ri < p.routes.size(); ++ri) {
            const auto &r = p.routes[ri];
            if (p.retainedInJukebox(r))
                continue;
            std::string target = "node." + std::to_string(r.node) + (r.floating ? ".f." : ".i.") +
                                 std::to_string(r.slot);
            auto found = std::find_if(list.begin(), list.end(),
                                      [&](const auto &e) { return e.key == target; });
            if (found == list.end())
                continue;
            found->cycleSource = sourceKey(r.node, r.channel);
            std::string label = found->label;
            float low = std::min(found->minimum, r.minimum),
                  high = std::max(found->maximum, r.maximum);
            int layer = p.nodes[r.node].stack;
            for (auto part :
                 {std::pair<const char *, float>{"lower", r.minimum}, {"upper", r.maximum}}) {
                add("cycle.range." + std::to_string(ri) + "." + part.first,
                    label + " / cycle " + part.first, low, high, part.second, layer);
                list.back().cycleSource = sourceKey(r.node, r.channel);
            }
            int channel = r.channel;
            for (int depth = 0; depth < 32; ++depth) {
                if (!sources.emplace(r.node, channel).second)
                    break;
                const auto &c = p.nodes[r.node].savedChannels[channel];
                int other = (c.flags >> 5) & 3;
                if (other == channel % 4)
                    break;
                channel = channel - channel % 4 + other;
            }
        }
        for (auto pair : sources) {
            auto n = pair.first;
            int channel = pair.second;
            const auto &c = p.nodes[n].savedChannels[channel];
            std::string targets;
            for (const auto &r : p.savedRoutes)
                if (r.node == n && r.channel == channel) {
                    std::string key = "node." + std::to_string(n) + (r.floating ? ".f." : ".i.") +
                                      std::to_string(r.slot);
                    auto it = std::find_if(list.begin(), list.end(),
                                           [&](const auto &e) { return e.key == key; });
                    if (it != list.end()) {
                        auto pos = it->label.find_last_of('/');
                        if (!targets.empty())
                            targets += " + ";
                        targets += it->label.substr(pos == std::string::npos ? 0 : pos + 2);
                    }
                }
            if (targets.empty())
                targets = "upstream lane dependency";
            std::string prefix = "Layer " + std::to_string(p.nodes[n].stack) + " / node " +
                                 std::to_string(n) + " channel " + std::to_string(channel) + " / " +
                                 targets + " / ";
            auto key = sourceKey(n, channel);
            int layer = p.nodes[n].stack;
            add(key + "speed", prefix + "cycle speed multiplier", -8, 8, 1, layer);
            add(key + "phase", prefix + "phase offset (turns)", 0, 1, 0, layer);
            add(key + "amplitude", prefix + "source amplitude", -4, 4, c.amplitude, layer);
            add(key + "offset", prefix + "source offset", -4, 4, c.offset, layer);
            add(key + "wave", prefix + "waveform", 0, 31, c.waveform, layer, true);
            const char *waves[] = {"Sine",
                                   "Absolute sine",
                                   "Cosine",
                                   "Absolute cosine",
                                   "Sawtooth (-1 to 1)",
                                   "Triangle",
                                   "Square",
                                   "Original random (unrecovered)",
                                   "Descending ramp",
                                   "Ascending ramp",
                                   "Constant"};
            for (int j = 0; j < 10; ++j)
                if (j != 7 || c.waveform == 7)
                    list.back().choices.push_back({j, waves[j]});
            if (c.waveform >= 10)
                list.back().choices.push_back({c.waveform, "Original external/retained source"});
            add(key + "clock", prefix + "clock division", 0, 3, c.clock, layer, true);
            list.back().choices = {
                {0, "Base clock"}, {1, "Base / 4"}, {2, "Base / 16"}, {3, "Base / 128"}};
            add(key + "lead", prefix + "original tick phase lead", -10000, 10000, c.rate, layer);
            add(key + "invert", prefix + "invert", 0, 1, (c.flags & 2) ? 1.f : 0.f, layer, true);
            list.back().choices = {{0, "Normal"}, {1, "Inverted"}};
            add(key + "wrap", prefix + "source boundary", 0, 1, (c.flags & 4) ? 1.f : 0.f, layer,
                true);
            list.back().choices = {{0, "Clamp"}, {1, "Wrap"}};
        }
    }
}
void AuthoringModel::applyCycles(int mode, PresetPlayback &p) {
    if (mode < 7 || mode > lastJukeboxPreset || (mode == 18 || mode == 21 || isStudy(mode)))
        return;
    p.integrateSpeed = false;
    for (auto &n : p.nodes)
        n.channels = n.savedChannels;
    p.routes = p.savedRoutes;
    for (auto &e : controls(mode)) {
        if (!e.enabled || e.key.rfind("cycle.", 0) != 0)
            continue;
        std::string key = e.key;
        for (char &c : key)
            if (c == '.')
                c = ' ';
        std::istringstream in(key);
        std::string cycle, kind, property;
        size_t n;
        int channel;
        in >> cycle >> kind >> n;
        if (kind == "range") {
            in >> property;
            auto &r = p.routes.at(n);
            if (property == "lower")
                r.minimum = e.value;
            else
                r.maximum = e.value;
        } else if (kind == "driver") {
            in >> channel >> property;
            auto &c = p.nodes.at(n).channels.at(channel);
            if (property == "speed")
                c.speed = e.value;
            else if (property == "phase")
                c.phaseShift = e.value;
            else if (property == "amplitude")
                c.amplitude = e.value;
            else if (property == "offset")
                c.offset = e.value;
            else if (property == "wave")
                c.waveform = uint8_t(std::lround(e.value));
            else if (property == "clock")
                c.clock = uint8_t(std::lround(e.value));
            else if (property == "lead")
                c.rate = e.value;
            else if (property == "invert")
                c.flags = (c.flags & ~2) | (e.value >= .5f ? 2 : 0);
            else if (property == "wrap")
                c.flags = (c.flags & ~4) | (e.value >= .5f ? 4 : 0);
        }
    }
    std::map<std::pair<size_t, int>, float> speeds;
    for (const auto &route : p.routes) {
        const auto &channel = p.nodes[route.node].channels[route.channel];
        if (channel.speed != 1)
            speeds[{route.node, route.channel}] = channel.speed;
    }
    auto bank = responses.find(mode);
    if (bank != responses.end())
        for (const auto &route : p.routes) {
            auto key = "clock." + std::to_string(route.node) + "." + std::to_string(route.channel) +
                       ".speed";
            if (bank->second.targets(key))
                speeds[{route.node, route.channel}] =
                    p.nodes[route.node].channels[route.channel].speed *
                    bank->second.apply(key, 1, p.ticks / 60., 0, 8);
        }
    auto graph = graphs.find(mode);
    if (graph != graphs.end()) {
        float speed = sceneControl(mode, "rings.speed", 1);
        std::set<std::pair<size_t, int>> ringDrivers;
        for (auto driver : graph->second.speedDrivers)
            for (const auto &route : p.routes)
                if (p.nodes[route.node].stack == driver.first && route.channel == driver.second) {
                    ringDrivers.insert({route.node, route.channel});
                }
        for (auto key : ringDrivers) {
            if (!speeds.count(key))
                speeds[key] = p.nodes[key.first].channels[key.second].speed;
            speeds[key] *= speed;
        }
    }
    // Continue clocks already integrated, including a return to baseline speed.
    for (const auto &entry : groupClocks)
        if (std::get<0>(entry.first) == mode) {
            auto key = std::make_pair(std::get<1>(entry.first), std::get<2>(entry.first));
            if (!speeds.count(key))
                speeds[key] = p.nodes[key.first].channels[key.second].speed;
        }
    for (auto driver : speeds) {
        auto &node = p.nodes[driver.first.first];
        auto &channel = node.channels[driver.first.second];
        if (channel.clock >= 4)
            continue;
        const auto &clock = p.stacks[node.stack].clock.clocks()[channel.clock];
        double position = clock.position + clock.delta * channel.rate;
        float combined = driver.second;
        auto &phase = groupClocks[{mode, driver.first.first, driver.first.second}];
        if (!phase.started || position < phase.position) {
            phase.phase = position * combined;
            phase.started = true;
        } else
            phase.phase += (position - phase.position) * phase.speed;
        phase.position = position;
        phase.speed = combined;
        channel.speed = combined;
        channel.phaseShift += float(phase.phase - position * combined);
    }
}
std::string AuthoringModel::validateCycles(int mode) {
    auto &p = composition(mode);
    auto routes = p.routes;
    auto nodes = p.nodes;
    auto clocks = groupClocks;
    applyCycles(mode, p);
    std::string error;
    for (auto &r : p.routes)
        if (r.minimum > r.maximum) {
            error = "Cycle lower limit must not exceed its upper limit.";
            break;
        }
    p.routes = std::move(routes);
    p.nodes = std::move(nodes);
    groupClocks = std::move(clocks);
    return error;
}
} // namespace neon::pc
