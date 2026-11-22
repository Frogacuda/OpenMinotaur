#include "authoring_model.h"
#include "pc_texture_bindings.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace neon::pc {
void AuthoringModel::installGraph(int mode, const GraphRecipe &recipe) {
    if (mode < 22 || mode > lastJukeboxPreset || isStudy(mode) || recipe.layers.empty() ||
        recipe.layers.size() > 16 || recipe.aliases.size() > 32 ||
        recipe.speedDrivers.size() > 32 || recipe.output < 0 || recipe.output > 15)
        throw std::runtime_error("Invalid saved graph recipe");
    auto validName = [](const std::string &name) {
        return std::find(addedJukeboxPresets.begin(), addedJukeboxPresets.end(), name) !=
                   addedJukeboxPresets.end() ||
               (name == "r1-c6" || name == "r1-c8") || name == "minotaur-rescue" ||
               name == "minotron-bezel" || name == "superoxwars-bezel" || name == "fiveaday-bezel";
    };
    std::map<std::string, RecoveredScene> sources;
    std::map<std::pair<std::string, int>, int> bindings;
    std::set<int> targets;
    for (const auto &layer : recipe.layers) {
        if (!validName(layer.preset) || layer.source < 0 || layer.source > 15 || layer.target < 0 ||
            layer.target > 15 || !targets.insert(layer.target).second ||
            !bindings.emplace(std::make_pair(layer.preset, layer.source), layer.target).second)
            throw std::runtime_error("Invalid saved graph layer mapping");
        if (!sources.count(layer.preset))
            sources.emplace(layer.preset, RecoveredScene(data, layer.preset, true, reader));
    }
    for (const auto &alias : recipe.aliases) {
        if (!sources.count(alias.preset) || alias.source < 0 || alias.source > 15 ||
            !targets.count(alias.target) ||
            !bindings.emplace(std::make_pair(alias.preset, alias.source), alias.target).second)
            throw std::runtime_error("Invalid saved graph input alias");
    }
    RecoveredScene assembled = sources.begin()->second;
    assembled.preset = PresetPlayback{};
    assembled.nonDrawingShapes.clear();
    auto layers = recipe.layers;
    std::sort(layers.begin(), layers.end(), [](auto &a, auto &b) { return a.target < b.target; });
    for (const auto &layer : layers) {
        const auto &original = sources.at(layer.preset);
        const auto &stack = original.preset.stacks[layer.source];
        if (stack.nodes.empty())
            throw std::runtime_error("Cannot import an empty recovered layer");
        auto &dest = assembled.preset.stacks[layer.target];
        dest.tempo = stack.tempo;
        auto selector = [&](int value) {
            auto binding = originalTextureBinding(value);
            if (binding.kind != TextureSourceKind::Stack)
                return value;
            auto match = bindings.find({layer.preset, binding.index});
            if (match == bindings.end() && original.preset.stacks[binding.index].nodes.empty())
                return value; // Unused sampler slots may retain an unbound stack selector.
            if (match == bindings.end())
                throw std::runtime_error("Saved graph is missing a referenced input layer");
            return 31 + match->second;
        };
        for (auto index : stack.nodes) {
            auto node = original.preset.nodes[index];
            node.stack = layer.target;
            auto &i = node.saved.integers;
            std::vector<int> slots;
            if (node.type == 9)
                slots = {5, 3, 6};
            else if (node.type == 6)
                slots = {3};
            else if (node.type == 1)
                slots = {6, 7};
            else if (node.type == 15)
                slots = {3, 4, 12};
            else if (node.type == 19)
                slots = {4, 5, 8};
            else if (node.type == 11)
                slots = {0, 1, 2, 3, 4, 5, 12, 13};
            for (int slot : slots)
                i[slot] = selector(i[slot]);
            auto target = assembled.preset.nodes.size();
            node.effective = node.saved;
            dest.nodes.push_back(target);
            assembled.preset.nodes.push_back(node);
            if (original.nonDrawingShapes.count(index))
                assembled.nonDrawingShapes.insert(target);
            for (auto route : original.preset.savedRoutes)
                if (route.node == index) {
                    route.node = target;
                    assembled.preset.routes.push_back(route);
                }
        }
    }
    if (recipe.mixOutput >= 0) {
        if (recipe.mixOutput > 15 || recipe.mixBase < 0 || !targets.count(recipe.mixBase) ||
            !targets.count(recipe.output) || targets.count(recipe.mixOutput))
            throw std::runtime_error("Invalid saved graph crossfade");
        auto &stack = assembled.preset.stacks[recipe.mixOutput];
        stack.tempo = 140;
        auto append = [&](PresetNode node) {
            node.stack = recipe.mixOutput;
            node.effective = node.saved;
            stack.nodes.push_back(assembled.preset.nodes.size());
            assembled.preset.nodes.push_back(node);
        };
        PresetNode control{};
        control.type = 0;
        control.saved.floats[0] = 140;
        append(control);
        for (int input : {recipe.mixBase, recipe.output}) {
            PresetNode mix{};
            mix.type = 9;
            mix.saved.integers[1] = 255;
            mix.saved.integers[2] = 26;
            mix.saved.integers[4] = 128;
            mix.saved.integers[5] = 31 + input;
            append(mix);
        }
        PresetNode terminal{};
        terminal.type = 7;
        terminal.saved.floats[0] = terminal.saved.floats[1] = terminal.saved.floats[4] =
            terminal.saved.floats[7] = 1;
        append(terminal);
    }
    assembled.preset.savedRoutes = assembled.preset.routes;
    assembled.preset.outputStack = recipe.mixOutput >= 0 ? recipe.mixOutput : recipe.output;
    assembled.rebuildGraph();
    assembled.reset();
    for (auto driver : recipe.speedDrivers)
        if (driver.first < 0 || driver.first > 15 || driver.second < 0 || driver.second > 31 ||
            std::none_of(assembled.preset.routes.begin(), assembled.preset.routes.end(),
                         [&](auto &r) {
                             return assembled.preset.nodes[r.node].stack == driver.first &&
                                    r.channel == driver.second;
                         }))
            throw std::runtime_error("Invalid saved graph speed driver");
    extraScenes.at(graphIndex(mode)) = std::move(assembled);
    graphs[mode] = recipe;
    if (externalMode_ == mode) {
        externalMode_ = -1;
        external_ = nullptr;
    }
    groupClocks.clear();
    auto &list = edits[editIndex(mode)];
    list.clear();
    buildExtraControls(mode);
    initializeCycleControls(mode);
    auto add = [&](const char *key, const char *label, float high, float value) {
        ReferenceControl c;
        c.key = key;
        c.label = label;
        c.minimum = 0;
        c.maximum = high;
        c.value = c.live = c.current = value;
        c.layer = recipe.mixOutput;
        list.push_back(c);
    };
    if (recipe.mixOutput >= 0)
        add("rings.amount", "Rings / fade amount", 1, 1);
    if (!recipe.speedDrivers.empty())
        add("rings.speed", "Rings / animation speed", 8, 1);
    auto &defaults = defaultValues[editIndex(mode)];
    defaults.clear();
    for (auto &e : list)
        defaults.push_back(e.value);
    // Graph installation rebuilds descriptors after imported images were read.
    // Restore their choices before validating the graph's saved sampler edits.
    setImages(images);
    refreshExtraLive();
}
void AuthoringModel::writeGraphs(std::ostream &out) const {
    out << "GRAPHS 1 " << graphs.size() << '\n';
    for (auto &pair : graphs) {
        const auto &g = pair.second;
        out << pair.first << ' ' << g.output << ' ' << g.mixBase << ' ' << g.mixOutput << ' '
            << g.layers.size() << ' ' << g.aliases.size() << ' ' << g.speedDrivers.size() << '\n';
        for (auto &l : g.layers)
            out << std::quoted(l.preset) << ' ' << l.source << ' ' << l.target << '\n';
        for (auto &a : g.aliases)
            out << std::quoted(a.preset) << ' ' << a.source << ' ' << a.target << '\n';
        for (auto d : g.speedDrivers)
            out << d.first << ' ' << d.second << '\n';
    }
}
void AuthoringModel::readGraphs(std::istream &in) {
    std::string tag;
    int version;
    size_t count;
    if (!(in >> tag >> version >> count) || tag != "GRAPHS" || version != 1 || count > 16)
        throw std::runtime_error("Invalid saved graph header");
    for (size_t n = 0; n < count; ++n) {
        GraphRecipe g;
        int mode;
        size_t layers, aliases, drivers;
        if (!(in >> mode >> g.output >> g.mixBase >> g.mixOutput >> layers >> aliases >> drivers) ||
            layers > 16 || aliases > 32 || drivers > 32 || graphs.count(mode))
            throw std::runtime_error("Invalid saved graph record");
        for (size_t i = 0; i < layers; ++i) {
            GraphLayer l;
            if (!(in >> std::quoted(l.preset) >> l.source >> l.target))
                throw std::runtime_error("Invalid graph layer");
            g.layers.push_back(l);
        }
        for (size_t i = 0; i < aliases; ++i) {
            GraphAlias a;
            if (!(in >> std::quoted(a.preset) >> a.source >> a.target))
                throw std::runtime_error("Invalid graph alias");
            g.aliases.push_back(a);
        }
        for (size_t i = 0; i < drivers; ++i) {
            int layer, channel;
            if (!(in >> layer >> channel))
                throw std::runtime_error("Invalid graph driver");
            g.speedDrivers.push_back({layer, channel});
        }
        installGraph(mode, g);
    }
}
} // namespace neon::pc
