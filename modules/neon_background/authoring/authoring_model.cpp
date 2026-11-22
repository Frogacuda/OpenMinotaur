#include "authoring_model.h"
#include "pc_texture_bindings.h"
#include "pc_texture_catalog.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace neon::pc {
namespace {
float baseAuthored(ReferenceControl &e, double seconds) {
    float v = e.enabled ? e.value : e.live;
    if (e.enabled && e.motion && !e.choices.size()) {
        float phase = float(std::fmod(seconds / std::max(.25f, e.period), 1.));
        float wave = e.motion == 1   ? std::sin(phase * 6.2831855f)
                     : e.motion == 2 ? 1 - 4 * std::abs(phase - .5f)
                                     : phase * 2 - 1;
        v += wave * e.depth * (e.maximum - e.minimum);
    }
    e.current = std::clamp(v, e.minimum, e.maximum);
    if (!e.enabled)
        e.current = e.live;
    return e.current;
}
float titleDefault(const std::string &key) {
    return key == "title.size"      ? 1.35f
           : key == "title.period"  ? 18.f
           : key == "title.filter"  ? .8f
           : key == "title.opacity" ? 1.f
                                    : 0.f;
}
float vectorDefault(const std::string &key) {
    return key == "vector.width"         ? 2.f
           : key == "vector.intensity"   ? 1.5f
           : key == "vector.halo"        ? .38f
           : key == "vector.spread"      ? 4.f
           : key == "vector.persistence" ? 35.f
           : key == "vector.hot"         ? .2f
           : key == "vector.background"  ? 1.f
                                         : 0.f;
}
void vectorTemplate(std::vector<ReferenceControl> &list) {
    for (auto &e : list) {
        if (e.key == "node.7.i.5" || e.key == "vector.enabled" || e.key == "vector.scene" ||
            e.key == "vector.colour" || e.key == "vector.placement") {
            e.enabled = true;
            e.value = e.key == "node.7.i.5" ? 200.f : 1.f;
        }
    }
}
} // namespace
float AuthoringModel::authored(ReferenceControl &e, double seconds, int mode) {
    float baseline = baseAuthored(e, seconds);
    auto bank = responses.find(mode);
    if (bank != responses.end())
        e.current = bank->second.apply(e.key, baseline, seconds, e.minimum, e.maximum);
    return e.current;
}
bool AuthoringModel::responseTarget(int mode, const std::string &key) const {
    auto bank = responses.find(mode);
    return bank != responses.end() && bank->second.targets(key);
}
AuthoringModel::AuthoringModel(const std::filesystem::path &path, AssetReader read)
    : data(path), reader(std::move(read)),
      synth(PresetPlayback::load(path / "presets/r2-c8.grpreset",
                                 path / "reference/r2-c8.routes.tsv", reader)),
      cube(PresetPlayback::load(path / "presets/r6-c15.grpreset",
                                path / "reference/r6-c15.routes.tsv", reader)),
      colour(PresetPlayback::load(path / "presets/r6-c11.grpreset",
                                  path / "reference/r6-c11.routes.tsv", reader)),
      zomg(PresetPlayback::load(path / "presets/r4-c3.grpreset",
                                path / "reference/r4-c3.routes.tsv", reader)),
      planet(PresetPlayback::load(path / "presets/r5-c13.grpreset",
                                  path / "reference/r5-c13.routes.tsv", reader)),
      travelA(PresetPlayback::load(path / "presets/r1-c6.grpreset",
                                   path / "reference/r1-c6.routes.tsv", reader)),
      travelB(PresetPlayback::load(path / "presets/r1-c4.grpreset",
                                   path / "reference/r1-c4.routes.tsv", reader)),
      profiles(TunnelProfiles::load(path / "reference/tunnel", reader)) {
    if (synth.outputStack != 3 || cube.outputStack != 0 || colour.outputStack != 5 ||
        zomg.outputStack != 4 || planet.outputStack != 5)
        throw std::runtime_error("Original output routing changed");
    for (int mode = 2; mode <= 4; mode++) {
        auto &list = edits[editIndex(mode)];
        auto add = [&](std::string key, std::string label, float low, float high) {
            ReferenceControl control;
            control.key = key;
            control.label = label;
            control.minimum = low;
            control.maximum = high;
            control.value = std::clamp(0.f, low, high);
            list.push_back(std::move(control));
        };
        const char *uv[] = {"UV width",
                            "UV height",
                            "UV horizontal offset",
                            "UV vertical offset",
                            "UV rotation",
                            "Second UV width",
                            "Second UV height",
                            "Second UV horizontal offset",
                            "Second UV vertical offset",
                            "Second UV rotation"};
        for (int i = 0; i < 10; i++)
            add("source.uv." + std::to_string(i), std::string("Source / ") + uv[i],
                i == 4 || i == 9 ? -6.2831855f : -.95f, i == 4 || i == 9 ? 6.2831855f : 2.f);
        const char *gal[] = {"sample count",    "phase X",           "phase Y",
                             "orbit radius",    "phase increment X", "phase increment Y",
                             "radius change X", "radius change Y"};
        const char *zm[] = {"sample count",   "light intensity", "colour intensity", "field zoom",
                            "sample spacing", "parameter G",     "parameter B",      "parameter A"};
        for (int i = 0; i < (mode == 3 ? 5 : 8); i++)
            add("source.constant." + std::to_string(i),
                std::string("Source / ") + (mode == 3 ? zm[i] : gal[i]), 0, 1);
        add("source.alpha", "Source / opacity", 0, 255);
        add("source.blue", "Source / shader intensity", 0, 255);
        add("final.alpha", mode == 4 ? "Planet / rim cutoff" : "Composition / opacity", 0, 255);
        add("final.blue",
            mode == 4 ? "Planet / ray origin distance" : "Composition / colour remap strength", 0,
            255);
        if (mode == 4) {
            const char *names[] = {"centre X",         "centre Y",         "centre depth",
                                   "light depth",      "texture repeat X", "texture repeat Y",
                                   "texture offset X", "texture offset Y"};
            for (int i = 0; i < 8; i++)
                add("final.constant." + std::to_string(i), std::string("Planet / ") + names[i], 0,
                    1);
        }
        add("camera.yaw", "Image-plane tilt / yaw", -.7f, .7f);
        add("camera.pitch", "Image-plane tilt / pitch", -.7f, .7f);
        add("camera.roll", "Image-plane tilt / roll", -1, 1);
        add("camera.x", "Image-plane tilt / horizontal position", -.5f, .5f);
        add("camera.y", "Image-plane tilt / vertical position", -.5f, .5f);
        add("camera.zoom", "Image-plane tilt / zoom", .5f, 2);
        list.back().value = list.back().live = 1;
    }
    for (int mode = 5; mode <= 6; mode++) {
        auto &list = edits[editIndex(mode)];
        auto add = [&](std::string key, std::string label, float low, float high) {
            ReferenceControl control;
            control.key = key;
            control.label = label;
            control.minimum = low;
            control.maximum = high;
            control.value = std::clamp(0.f, low, high);
            list.push_back(std::move(control));
        };
        if (mode == 5) {
            add("tunnel.ripple.enabled", "Tunnel / ripple texture layer enabled", 0, 1);
            list.back().integer = true;
            list.back().value = list.back().live = list.back().current = 1;
            list.back().choices = {{0, "Off / use wall texture"}, {1, "Original ripple layer"}};
        }
        add("tunnel.mesh.sides", "Tunnel / mesh sides around", 8, 128);
        list.back().integer = true;
        list.back().value = list.back().live = 8;
        add("tunnel.mesh.roundness", "Tunnel / corner rounding", 0, 1);
        list.back().value = list.back().live = 1;
        add("tunnel.speed", "Tunnel / travel speed multiplier", .1f, 3);
        add("tunnel.yaw", "Tunnel / path yaw", -5, 5);
        add("tunnel.pitch", "Tunnel / path pitch", -5, 5);
        add("tunnel.roll", "Tunnel / roll", -3.1415927f, 3.1415927f);
        add("tunnel.repeatX", "Tunnel / texture repeats around", 1, 16);
        add("tunnel.repeatY", "Tunnel / texture repeats along", 1, 32);
        add("tunnel.spacing", "Tunnel / ring spacing", 40, 350);
        add("tunnel.alpha", "Tunnel / depth-fade opacity", 0, 255);
        add("tunnel.colour", "Tunnel / shader colour strength", 0, 255);
        add("tunnel.profileA", "Tunnel / original profile A", 0, 13);
        add("tunnel.profileB", "Tunnel / original profile B", 0, 13);
        add("tunnel.morph", "Tunnel / profile morph", 0, 1);
        add("tunnel.deformX", "Tunnel / relative X deformation", -.25f, .25f);
        add("tunnel.deformY", "Tunnel / relative Y deformation", -.25f, .25f);
        add("tunnel.deformZ", "Tunnel / relative Z deformation", -.25f, .25f);
        const char *names[] = {"sample count",    "phase X",           "phase Y",
                               "orbit radius",    "phase increment X", "phase increment Y",
                               "radius change X", "radius change Y"};
        const char *glargle[] = {"sample count",          "texture 0 step X", "texture 0 step Y",
                                 "unused by this shader", "texture 1 step X", "texture 1 step Y",
                                 "texture 2 step X",      "texture 2 step Y"};
        for (int i = 0; i < 8; i++)
            if (mode == 5 || i != 3)
                add("source.constant." + std::to_string(i),
                    std::string("Wall texture / ") + (mode == 5 ? names[i] : glargle[i]), 0, 1);
        add("source.alpha", "Wall texture / opacity", 0, 255);
        add("source.blue", "Wall texture / shader intensity", 0, 255);
        add("camera.yaw", "3D camera / yaw", -.8f, .8f);
        add("camera.pitch", "3D camera / pitch", -.8f, .8f);
        add("camera.roll", "3D camera / roll", -.8f, .8f);
        add("camera.x", "3D camera / horizontal position", -350, 350);
        add("camera.y", "3D camera / vertical position", -350, 350);
        add("camera.z", "3D camera / forward position", -900, 900);
        add("camera.zoom", "3D camera / lens zoom", .5f, 2);
        list.back().value = list.back().live = 1;
    }
    {
        auto &list = edits[19];
        auto &original = cube.nodes[24].saved;
        auto add = [&](std::string key, std::string label, float low, float high, float initial,
                       bool integer = false) {
            ReferenceControl e;
            e.key = key;
            e.label = "Cube / " + label;
            e.minimum = std::min(low, initial);
            e.maximum = std::max(high, initial);
            e.value = e.live = e.current = initial;
            e.integer = integer;
            e.layer = 0;
            list.push_back(e);
        };
        add("cube.subdivisions", "Face subdivisions per edge", 2, 30,
            float(original.integers[10] + 2), true);
        add("cube.i.8", "Texture repeats per face", 1, 32, float(original.integers[8]), true);
        add("cube.i.6", "Opacity", 0, 255, float(original.integers[6]), true);
        add("cube.i.9", "Shader colour", 0, 255, float(original.integers[9]), true);
        add("cube.mirrorUV", "Mirrored texture coordinates", 0, 1, float(original.integers[14] & 1),
            true);
        list.back().choices = {{0, "Repeat"}, {1, "Mirror"}};
        const char *labels[] = {"Horizontal position",
                                "Vertical position",
                                "Half width",
                                "Half height",
                                "Depth",
                                "Half depth",
                                "Pitch",
                                "Yaw",
                                "Roll",
                                "Surface curvature",
                                "Surface displacement",
                                "X deformation",
                                "Y deformation",
                                "Z deformation"};
        for (int slot = 0; slot < 14; ++slot) {
            bool dimension = slot == 2 || slot == 3 || slot == 5;
            float low = dimension                             ? .001f
                        : slot == 0 || slot == 1 || slot == 4 ? -5000.f
                        : slot >= 6 && slot <= 8              ? -3.141593f
                                                              : -4.f;
            float high = dimension                             ? 10000.f
                         : slot == 0 || slot == 1 || slot == 4 ? 12000.f
                         : slot >= 6 && slot <= 8              ? 3.141593f
                                                               : 4.f;
            add("cube.f." + std::to_string(slot), labels[slot], low, high, original.floats[slot]);
        }
    }
    initializeExtraScenes();
    edits[15] = edits[7];
    vectorTemplate(edits[15]);
    initializeCycleControls();
    for (size_t group = 0; group < edits.size(); ++group)
        for (const auto &e : edits[group])
            defaultValues[group].push_back(e.value);
    refreshLive();
    refreshExtraLive();
}
void AuthoringModel::setImages(neon::effects::ImageAssets catalog) {
    catalog.prepare();
    images = std::move(catalog);
    for (auto &bank : edits)
        for (auto &e : bank)
            if (std::any_of(e.choices.begin(), e.choices.end(),
                            [](auto &c) { return c.value == 200; })) {
                e.choices.erase(
                    std::remove_if(
                        e.choices.begin(), e.choices.end(),
                        [](auto &c) { return neon::effects::importedImageSelector(c.value); }),
                    e.choices.end());
                for (auto &a : images.items)
                    e.choices.push_back({a.selector, "Imported / " + a.name});
                e.maximum = images.items.empty()
                                ? std::max(255.f, float(47 + literalTextureSlots.back()))
                                : 9999.f;
            }
}
void AuthoringModel::addAuthoringControls(int mode) {
    auto &scene = extraScenes.at(graphIndex(mode));
    auto &list = edits[editIndex(mode)];
    std::vector<ReferenceChoice> textures{{0, "Keep previous sampler binding"},
                                          {202, "Current gameplay image"},
                                          {200, "Vector objects / live generated texture"}};
    for (int root = 0; root < 30; root++)
        if (assetExists(data / "reference/roots" / (std::to_string(root) + ".argb"), reader))
            textures.push_back({root + 1, "Root " + std::to_string(root)});
    for (int slot : literalTextureSlots)
        textures.push_back({47 + slot, "Literal " + std::to_string(slot) + " / " +
                                           literalOriginalNames.at(slot - 1)});
    std::vector<ReferenceChoice> layerInputs;
    for (auto &layer : layers(mode))
        layerInputs.push_back(
            {31 + layer.first, "Layer output / " + layer.second + " (ordered / feedback)"});
    textures.insert(textures.begin() + 2, layerInputs.begin(), layerInputs.end());
    std::map<int, std::string> names;
    auto file = assetStream(data / "reference/shader-names.tsv", reader);
    std::string line;
    while (std::getline(file, line)) {
        auto tab = line.find('\t');
        if (tab != std::string::npos)
            names[std::stoi(line.substr(0, tab))] = line.substr(tab + 1);
    }
    std::vector<ReferenceChoice> shaders;
    for (int id : {0,  1,  2,  3,  4,  5,  6,  8,  11, 12, 13, 14, 15, 16, 18, 26, 29, 32, 39, 40,
                   46, 47, 48, 49, 57, 59, 61, 69, 70, 71, 72, 73, 74, 80, 81, 82, 83, 84, 85, 19,
                   28, 45, 50, 52, 53, 67, 68, 87, 9,  17, 20, 31, 33, 44, 55, 56, 60, 66, 88, 23})
        shaders.push_back({id, std::to_string(id) + " / " + names[id]});
    shaders.push_back({92, "92 / Barrel Concave (one axis)"});
    shaders.push_back({93, "93 / Barrel Convex (one axis)"});
    shaders.push_back({94, "94 / Barrel Concave (single surface)"});
    shaders.push_back({95, "95 / Barrel Convex (single surface)"});
    auto choice = [&](std::string key, std::string label,
                      const std::vector<ReferenceChoice> &options, int initial) {
        ReferenceControl c;
        c.key = std::move(key);
        c.label = std::move(label);
        c.minimum = std::min(0.f, float(initial));
        c.maximum = std::max(255.f, float(initial));
        for (const auto &option : options) {
            c.minimum = std::min(c.minimum, float(option.value));
            c.maximum = std::max(c.maximum, float(option.value));
        }
        c.value = c.live = c.current = float(initial);
        c.integer = true;
        c.choices = options;
        list.push_back(std::move(c));
    };
    std::vector<ReferenceChoice> outputs;
    for (int stack = 0; stack < 16; stack++)
        if (scene.reachable[stack]) {
            outputs.push_back({stack, "Layer " + std::to_string(stack)});
            choice("layer." + std::to_string(stack),
                   "Layer " + std::to_string(stack) + " / enabled", {{0, "Muted"}, {1, "Enabled"}},
                   1);
            list.back().layer = stack;
            choice("blend." + std::to_string(stack),
                   "Layer " + std::to_string(stack) + " / draw blend override",
                   {{0, "Original material behaviour"},
                    {1, "Add light"},
                    {2, "Alpha over"},
                    {3, "Replace"},
                    {4, "Multiply"},
                    {5, "Screen"}},
                   0);
            list.back().maximum = 5;
            list.back().layer = stack;
        }
    outputs.push_back({16, "Vector objects / generated layer"});
    choice("scene.output", "Composition / output layer", outputs, scene.preset.outputStack);
    std::vector<ReferenceChoice> nodeOutputs{{-1, "Complete layer"}};
    for (size_t node = 0; node < scene.preset.nodes.size(); ++node) {
        const auto &n = scene.preset.nodes[node];
        if (scene.reachable[n.stack] && n.type != 0 && n.type != 7 && n.type != 12 &&
            n.type != 14 && !scene.nonDrawingShapes.count(node))
            nodeOutputs.push_back({int(node), "Layer " + std::to_string(n.stack) +
                                                  " / output through node " +
                                                  std::to_string(node)});
    }
    choice("scene.node", "Composition / node output endpoint", nodeOutputs, -1);
    list.back().minimum = -1;
    for (size_t index = 0; index < scene.preset.nodes.size(); index++) {
        auto &n = scene.preset.nodes[index];
        if (!scene.reachable[n.stack] || (n.type != 9 && n.type != 6 && n.type != 15 &&
                                          n.type != 1 && n.type != 19 && n.type != 11))
            continue;
        if (n.type == 9 && n.saved.integers[2] == 81) {
            ReferenceControl e;
            e.key = "warp." + std::to_string(index) + ".amount";
            e.label = "Layer " + std::to_string(n.stack) + " / node " + std::to_string(index) +
                      " / Texture displacement strength";
            e.minimum = 0;
            e.maximum = 2;
            e.value = e.live = e.current = 1;
            e.layer = n.stack;
            list.push_back(e);
        }
        int shaderSlot = n.type == 11                 ? 7
                         : n.type == 9 || n.type == 6 ? 2
                         : n.type == 15               ? 15
                         : n.type == 19               ? 6
                                                      : 9;
        std::array<int, 3> slots = n.type == 11   ? std::array<int, 3>{0, 12, 13}
                                   : n.type == 9  ? std::array<int, 3>{5, 3, 6}
                                   : n.type == 6  ? std::array<int, 3>{-1, 3, -1}
                                   : n.type == 15 ? std::array<int, 3>{3, 4, 12}
                                   : n.type == 19 ? std::array<int, 3>{4, 5, 8}
                                                  : std::array<int, 3>{6, 7, -1};
        auto key = [&](int slot) {
            return "node." + std::to_string(index) + ".i." + std::to_string(slot);
        };
        std::string prefix =
            "Stack " + std::to_string(n.stack) + " / node " + std::to_string(index) + " / ";
        auto programs = shaders;
        if (std::none_of(programs.begin(), programs.end(),
                         [&](const auto &c) { return c.value == n.saved.integers[shaderSlot]; }))
            programs.push_back(
                {n.saved.integers[shaderSlot],
                 std::to_string(n.saved.integers[shaderSlot]) + " / original program"});
        choice(key(shaderSlot), prefix + "shader", programs, n.saved.integers[shaderSlot]);
        if (n.type == 11)
            for (int face = 1; face < 6; ++face)
                choice(key(face), prefix + "face " + std::to_string(face + 1) + " texture",
                       textures, n.saved.integers[face]);
        for (int s = 0; s < 3; s++)
            if (slots[s] >= 0) {
                auto options = textures;
                if (std::none_of(options.begin(), options.end(), [&](const auto &c) {
                        return c.value == n.saved.integers[slots[s]];
                    }))
                    options.push_back(
                        {n.saved.integers[slots[s]],
                         "Original unused selector " + std::to_string(n.saved.integers[slots[s]])});
                choice(key(slots[s]), prefix + "original texture binding " + std::to_string(s),
                       options, n.saved.integers[slots[s]]);
            }
    }
    for (size_t node = 0; node < scene.preset.nodes.size(); ++node) {
        auto &n = scene.preset.nodes[node];
        if (!scene.reachable[n.stack] || (n.type != 9 && n.type != 6 && n.type != 15 &&
                                          n.type != 19 && n.type != 11 && n.type != 1))
            continue;
        for (int input = 0; input < 3; ++input) {
            auto options = textures;
            options.front().label = "Original binding / follow preset";
            choice("input." + std::to_string(node) + "." + std::to_string(input),
                   "Layer " + std::to_string(n.stack) + " / node " + std::to_string(node) +
                       " / texture input " + std::to_string(input + 1),
                   options, 0);
            list.back().layer = n.stack;
        }
        if (n.type == 9 && (n.saved.integers[2] == 67 || n.saved.integers[2] == 68)) {
            choice(
                "sampling." + std::to_string(node) + ".0.scrollspace",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(node) +
                    " / Radial scroll units",
                {{0, "Reference frame (original)"}, {1, "Sampled texture widths (seamless wrap)"}},
                0);
            list.back().maximum = 1;
            list.back().layer = n.stack;
        }
        for (int input = 0; input < 3; ++input)
            for (int part = 0; part < 3; ++part) {
                std::string key = "sampling." + std::to_string(node) + "." + std::to_string(input) +
                                  "." + std::to_string(part);
                choice(key,
                       "Layer " + std::to_string(n.stack) + " / node " + std::to_string(node) +
                           " / input " + std::to_string(input + 1) +
                           (part == 0   ? " filter"
                            : part == 1 ? " U addressing"
                                        : " V addressing"),
                       part == 0 ? std::vector<ReferenceChoice>{{0, "Texture default"},
                                                                {1, "Nearest"},
                                                                {2, "Linear"}}
                                 : std::vector<ReferenceChoice>{{0, "Texture default"},
                                                                {1, "Repeat"},
                                                                {2, "Mirrored repeat"},
                                                                {3, "Clamp"}},
                       0);
                list.back().maximum = part == 0 ? 2.f : 3.f;
                list.back().layer = n.stack;
            }
    }
    for (size_t node = 0; node < scene.preset.nodes.size(); ++node) {
        const auto &n = scene.preset.nodes[node];
        if (!scene.reachable[n.stack] || (n.type != 9 && n.type != 6 && n.type != 1 &&
                                          n.type != 15 && n.type != 19 && n.type != 11))
            continue;
        for (int input = 0; input < 3; ++input) {
            auto key = "sampling." + std::to_string(node) + "." + std::to_string(input) + ".3";
            choice(key,
                   "Layer " + std::to_string(n.stack) + " / node " + std::to_string(node) +
                       " / input " + std::to_string(input + 1) + " layer domain",
                   {{0, "Automatic"}, {1, "Reference texture tile"}, {2, "Extended scene surface"},
                    {3, "Extended U / reference V"}, {4, "Reference U / extended V"}},
                   0);
            list.back().maximum = 4;
            list.back().layer = n.stack;
        }
    }
    for (auto &e : list)
        if (e.key.rfind("node.", 0) == 0) {
            size_t stop = e.key.find('.', 5);
            size_t node = size_t(std::stoul(e.key.substr(5, stop - 5)));
            int slot = std::stoi(e.key.substr(stop + 3));
            e.integer = e.key[stop + 1] == 'i';
            e.layer = scene.preset.nodes[node].stack;
            for (auto &r : scene.preset.routes)
                if (r.node == node && r.slot == slot && r.floating != e.integer) {
                    e.retained = scene.preset.retainedInJukebox(r);
                    e.driven = !e.retained;
                }
        }
}
void AuthoringModel::initializeExtraScenes() {
    for (const char *name : {"r1-c8", "r4-c10", "r2-c13", "r6-c1", "r1-c10", "r5-c8", "r1-c0",
                             "r3-c1", "r5-c0", "r1-c8", "r1-c12", "r2-c14"})
        extraScenes.emplace_back(data, name, true, reader);
    for (auto name : addedJukeboxPresets)
        extraScenes.emplace_back(data, name, true, reader);
    for (int mode = 7; mode <= lastJukeboxPreset; mode++) {
        if (mode == 17 || mode == 18 || mode == 21 || isStudy(mode))
            continue;
        buildExtraControls(mode);
    }
}
void AuthoringModel::buildExtraControls(int mode) {
    auto &scene = extraScenes.at(graphIndex(mode));
    auto &list = edits[editIndex(mode)];
    auto add = [&](std::string key, std::string label, float lo, float hi) {
        ReferenceControl control;
        control.key = key;
        control.label = label;
        control.minimum = lo;
        control.maximum = hi;
        control.value = std::clamp(0.f, lo, hi);
        list.push_back(std::move(control));
    };
    for (size_t j = 0; j < scene.preset.nodes.size(); j++) {
        auto &n = scene.preset.nodes[j];
        if (!scene.reachable[n.stack])
            continue;
        auto field = [&](bool integer, int slot, const char *label, float lo, float hi) {
            add("node." + std::to_string(j) + (integer ? ".i." : ".f.") + std::to_string(slot),
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) + " / " + label,
                lo, hi);
        };
        if (n.type == 11) {
            field(true, 6, "cube opacity", 0, 255);
            field(true, 9, "cube shader colour", 0, 255);
            field(true, 10, "cube subdivision order", 0, 28);
            field(true, 8, "cube face repeats", 1, 32);
            field(true, 14, "mirrored cube UV", 0, 1);
            const char *labels[] = {"cube X",
                                    "cube Y",
                                    "cube half width",
                                    "cube half height",
                                    "cube depth",
                                    "cube half depth",
                                    "cube pitch",
                                    "cube yaw",
                                    "cube roll",
                                    "cube curvature",
                                    "cube displacement",
                                    "cube deformation X",
                                    "cube deformation Y",
                                    "cube deformation Z"};
            for (int k = 0; k < 14; ++k)
                field(false, k, labels[k],
                      k == 2 || k == 3 || k == 5 ? .001f
                      : k >= 6 && k <= 8         ? -3.141593f
                      : k >= 9                   ? -4.f
                                                 : -10000.f,
                      k == 2 || k == 3 || k == 5 ? 15000.f
                      : k >= 6 && k <= 8         ? 3.141593f
                      : k >= 9                   ? 4.f
                                                 : 20000.f);
        }
        if (n.type == 1) {
            add("sprite." + std::to_string(j) + ".enabled",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / use sprite sequence",
                0, 1);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 0;
            list.back().integer = true;
            list.back().choices = {{0, "Off / original sprite"}, {1, "On / sequential atlas"}};
            add("sprite." + std::to_string(j) + ".columns",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / atlas columns",
                1, 16);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 1;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".rows",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / atlas rows",
                1, 16);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 1;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".baseFirst",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / base first frame",
                0, 255);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 0;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".baseCount",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / base frame count",
                1, 256);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 1;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".rareFirst",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / rare first frame",
                0, 255);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 0;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".rareCount",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / rare frame count",
                0, 256);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 0;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".rareEvery",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / rare emission interval",
                1, 1024);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 10;
            list.back().integer = true;
            add("sprite." + std::to_string(j) + ".paletteTint",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / palette tint amount",
                0, 1);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 1;
            add("sprite." + std::to_string(j) + ".filter",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Sprite sequence / colour filtration",
                0, 1);
            list.back().layer = n.stack;
            list.back().value = list.back().live = list.back().current = 0;

            field(true, 0, "particle population", 1, 512);
            field(true, 1, "particles per emission", 1, 16);
            field(true, 2, "emission interval", 0, 120);
            field(true, 3, "emitter width", 10, 15000);
            field(true, 4, "emitter height", 10, 15000);
            field(true, 5, "palette index", 0, 255);
            field(true, 8, "particle opacity", 0, 255);
            field(true, 10, "spin at birth", -8192, 8192);
            field(true, 11, "spin variation", 0, 8192);
            field(true, 12, "particle blue tint", 0, 255);
            field(false, 0, "emitter X", -4000, 4000);
            field(false, 1, "emitter Y", -4000, 4000);
            field(false, 2, "travel depth", -20000, -500);
            field(false, 3, "sprite width", .02f, 4);
            field(false, 4, "sprite height", .02f, 4);
            field(false, 6, "velocity X", -50, 50);
            field(false, 8, "velocity Y", -50, 50);
            field(false, 10, "travel velocity", 1, 100);
            field(false, 7, "velocity X variation", 0, 20);
            field(false, 9, "velocity Y variation", 0, 20);
            field(false, 11, "travel velocity variation", 0, 20);
        }
        if (n.type == 15) {
            add("mesh." + std::to_string(j) + ".sides",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / mesh sides around",
                8, 128);
            list.back().layer = n.stack;
            list.back().integer = true;
            list.back().value = list.back().live = 8;
            add("mesh." + std::to_string(j) + ".roundness",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / corner rounding",
                0, 1);
            list.back().layer = n.stack;
            list.back().value = list.back().live = 1;
            field(true, 2, "tunnel travel step", 0, 8388608);
            field(true, 6, "ring spacing", 40, 350);
            field(true, 7, "profile A", 0, 13);
            field(true, 10, "profile B", 0, 13);
            field(true, 8, "texture repeats around", 1, 16);
            field(true, 9, "texture repeats along", 1, 32);
            field(true, 13, "tunnel opacity", 0, 255);
            field(true, 14, "tunnel shader colour", 0, 255);
            field(false, 0, "path pitch", -5, 5);
            field(false, 1, "path yaw", -5, 5);
            field(false, 2, "profile morph", 0, 1);
            field(false, 3, "texture scroll", -1, 1);
            field(false, 4, "relative X deformation", -.25f, .25f);
            field(false, 5, "relative Y deformation", -.25f, .25f);
            field(false, 6, "relative Z deformation", -.25f, .25f);
            field(false, 7, "tunnel horizontal position", -5000, 5000);
            field(false, 8, "tunnel vertical position", -5000, 5000);
            field(false, 9, "tunnel forward position", -5000, 12000);
            field(false, 10, "tunnel yaw", -3.1415927f, 3.1415927f);
            field(false, 11, "tunnel pitch", -3.1415927f, 3.1415927f);
            field(false, 12, "tunnel roll", -3.1415927f, 3.1415927f);
        }
        if (n.type == 19) {
            field(true, 0, "flower segments", 3, 2048);
            field(true, 1, "angular phase", 0, 16383);
            field(true, 2, "flower opacity", 0, 255);
            field(true, 3, "angular texture repeats", 1, 32);
            field(true, 7, "flower colour / shader mix", 0, 255);
            field(true, 9, "flower depth / UV mode", 0, 3);
            list.back().integer = true;
            list.back().choices = {{0, "Single depth / flowing UV"},
                                   {1, "Reflected depth / flowing UV"},
                                   {2, "Single depth / folded UV"},
                                   {3, "Reflected depth / folded UV"}};
            const char *fn[] = {"inner radius",     "middle radius", "outer radius",
                                "forward position", "middle depth",  "outer depth",
                                "flower yaw",       "flower pitch",  "flower roll",
                                "X deformation",    "Y deformation", "Z deformation"};
            for (int k = 0; k < 12; k++)
                field(false, k, fn[k],
                      k < 3   ? 0
                      : k < 6 ? -1200
                      : k < 9 ? -3.141593f
                              : -1,
                      k < 3   ? 1200
                      : k < 6 ? 2000
                      : k < 9 ? 3.141593f
                              : 1);
        }
        if (n.type == 9 || n.type == 6) {
            field(true, 1, n.type == 6 ? "feedback retention" : "mixer opacity", 0, 255);
            field(true, 4, "shader effect parameter", 0, 255);
            const char *names[] = {"UV width",          "UV height",          "UV X offset",
                                   "UV Y offset",       "UV rotation",        "second UV width",
                                   "second UV height",  "second UV X offset", "second UV Y offset",
                                   "second UV rotation"};
            for (int k = 0; k < 10; k++)
                field(false, k, names[k], k % 5 == 4 ? -6.2831855f : (k % 5 >= 2 ? -8.f : -.95f),
                      k % 5 == 4 ? 6.2831855f : (k % 5 >= 2 ? 8.f : 2.f));
        }
        if (n.type == 9 || n.type == 6 || n.type == 1 || n.type == 11 || n.type == 15 ||
            n.type == 19) {
            for (const char *part : {"red", "green", "blue", "hue", "saturation", "brightness"}) {
                const std::string channel(part);
                const bool hue = channel == "hue";
                add("colour." + std::to_string(j) + "." + channel,
                    "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                        " / Colour / " + channel + (hue ? " (degrees)" : ""),
                    hue ? -180.f : 0.f,
                    hue                       ? 180.f
                    : channel == "saturation" ? 2.f
                                              : 4.f);
                auto &control = list.back();
                control.layer = n.stack;
                control.value = control.live = control.current = hue ? 0.f : 1.f;
            }
        }
        if (n.type == 9 || n.type == 6) {
            add("colour." + std::to_string(j) + ".floor",
                "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) +
                    " / Colour / Masked-light floor (Burnthru)",
                0, .25f);
            auto &e = list.back();
            e.layer = n.stack;
            e.value = e.live = e.current = 0;
        }
        if (n.type == 7) {
            for (int k = 0; k < 8; k++) {
                field(false, k < 4 ? 4 + k : 9 + k - 4,
                      ("shader constant " + std::string(k < 4 ? "C25." : "C26.") + "rgba"[k % 4])
                          .c_str(),
                      0, 1);
                if (mode == 20 && j == 21) {
                    const char *names[] = {"sphere centre X",   "sphere centre Y",
                                           "sphere depth",      "light depth",
                                           "surface repeats U", "surface repeats V",
                                           "surface scroll U",  "surface scroll V"};
                    list.back().label = "Layer 5 / Planet2 / " + std::string(names[k]);
                }
                list.back().shaderBank = k < 4 ? 1 : 2;
                list.back().layer = n.stack;
            }
        }
    }
    if (scene.spatialCamera()) {
        add("camera.yaw", "3D camera / yaw", -.8f, .8f);
        add("camera.pitch", "3D camera / pitch", -.8f, .8f);
        add("camera.roll", "3D camera / roll", -.8f, .8f);
        add("camera.x", "3D camera / horizontal position", -500, 500);
        add("camera.y", "3D camera / vertical position", -500, 500);
        add("camera.z", "3D camera / forward position", -900, 900);
        add("camera.zoom", "3D camera / lens zoom", .5f, 2);
        list.back().value = 1;
    } else {
        add("camera.yaw", "Image-plane camera / yaw", -.7f, .7f);
        add("camera.pitch", "Image-plane camera / pitch", -.7f, .7f);
        add("camera.roll", "Image-plane camera / roll", -1, 1);
        add("camera.x", "Image-plane camera / horizontal position", -2, 2);
        add("camera.y", "Image-plane camera / vertical position", -2, 2);
        add("camera.zoom", "Image-plane camera / zoom", .1f, 4);
        list.back().value = 1;
    }
    addAuthoringControls(mode);
    for (auto spec : std::initializer_list<std::tuple<const char *, const char *, float, float>>{
             {"vector.width", "Vector beam / core width", .5f, 4.f},
             {"vector.intensity", "Vector beam / brightness", .1f, 3.f},
             {"vector.halo", "Vector beam / glow strength", 0.f, 2.f},
             {"vector.spread", "Vector beam / glow spread", 1.f, 8.f},
             {"vector.persistence", "Vector beam / phosphor decay ms", 0.f, 180.f},
             {"vector.colour", "Vector beam / colour (0 tube, 1 objects)", 0.f, 1.f},
             {"vector.hot", "Vector beam / white hot core", 0.f, 1.f},
             {"vector.scene", "Vector beam / scene (0 HUD, 1 objects, 2 test, 3 file)", 0.f, 3.f},
             {"vector.background", "Vector beam / background visibility", 0.f, 1.f}}) {
        add(std::get<0>(spec), std::get<1>(spec), std::get<2>(spec), std::get<3>(spec));
        list.back().value = vectorDefault(list.back().key);
        list.back().integer =
            list.back().key == "vector.scene" || list.back().key == "vector.background";
    }
    add("vector.placement", "Vector beam / placement", 0, 2);
    list.back().integer = true;
    list.back().choices = {
        {0, "Overlay"}, {1, "Texture source only"}, {2, "Texture source and overlay"}};
    add("vector.enabled", "Vector beam / enabled", 0, 1);
    list.back().integer = true;
    list.back().choices = {{0, "Off"}, {1, "On"}};
    if (mode == 16) {
        for (auto spec :
             std::initializer_list<std::tuple<const char *, const char *, float, float>>{
                 {"title.size", "Title flight / common sprite size", .25f, 3},
                 {"title.period", "Title flight / colour cycle seconds", 4, 90},
                 {"title.hue", "Title flight / hue offset", 0, 1},
                 {"title.filter", "Title flight / colour filtration", 0, 1},
                 {"title.opacity", "Title flight / sprite opacity", 0, 1}}) {
            add(std::get<0>(spec), std::get<1>(spec), std::get<2>(spec), std::get<3>(spec));
            list.back().value = titleDefault(list.back().key);
            list.back().layer = 4;
        }
        add("title.sprite", "Title flight / sprite assortment", 0, 7);
        auto &e = list.back();
        e.layer = 4;
        e.integer = true;
        e.choices = {{0, "All seven sprites"},         {1, "Gridrunner ship"},
                     {2, "Rescue rainbow Minotaur"},   {3, "GoatUp goat"},
                     {4, "Minotron facing down"},      {5, "Minos ship"},
                     {6, "Five a Day / Ox Wars ship"}, {7, "Deflex first-level animal"}};
    }
}
std::vector<ShapeSettings> AuthoringModel::authoredScene(int mode) {
    auto &p = composition(mode);
    std::vector<ShapeSettings> settings;
    for (auto &n : p.nodes)
        settings.push_back(n.effective);
    for (auto &e : edits[editIndex(mode)]) {
        if (e.key.rfind("cycle.", 0) == 0) {
            e.current = e.enabled ? e.value : e.live;
            continue;
        }
        if (e.key.rfind("node.", 0) == 0) {
            size_t stop = e.key.find('.', 5);
            auto node = size_t(std::stoul(e.key.substr(5, stop - 5)));
            int slot = std::stoi(e.key.substr(stop + 3));
            bool integer = e.key[stop + 1] == 'i';
            e.live = integer ? float(settings.at(node).integers.at(slot))
                             : settings.at(node).floats.at(slot);
            float value = authored(e, p.ticks / 60., mode);
            if (e.enabled || responseTarget(mode, e.key)) {
                if (integer)
                    settings[node].integers[slot] = int(std::lround(value));
                else
                    settings[node].floats[slot] = value;
            }
        } else {
            e.live = (e.key.rfind("rings.", 0) == 0 || e.key.rfind("warp.", 0) == 0) ? 1.f
                     : e.key.rfind("mesh.", 0) == 0
                         ? (e.key.find("roundness") != std::string::npos ? 1.f : 8.f)
                     : e.key.rfind("vector.", 0) == 0 ? vectorDefault(e.key)
                     : e.key.rfind("sprite.", 0) == 0
                         ? (e.key.find("rareEvery") != std::string::npos
                                ? 10.f
                                : (e.key.find("columns") != std::string::npos ||
                                           e.key.find("rows") != std::string::npos ||
                                           e.key.find("baseCount") != std::string::npos ||
                                           e.key.find("paletteTint") != std::string::npos
                                       ? 1.f
                                       : 0.f))
                     : e.key.rfind("title.", 0) == 0                           ? titleDefault(e.key)
                     : e.key == "camera.zoom" || e.key.rfind("layer.", 0) == 0 ? 1.f
                     : e.key.rfind("colour.", 0) == 0 ? ((e.key.find(".hue") != std::string::npos ||
                                                          e.key.find(".floor") != std::string::npos)
                                                             ? 0.f
                                                             : 1.f)
                     : e.key == "scene.node"          ? -1.f
                     : e.key == "scene.output"        ? float(p.outputStack)
                                                      : 0.f;
            authored(e, p.ticks / 60., mode);
        }
    }
    std::array<unsigned, 16> banks{};
    std::array<bool, 16> barrelAxes{};
    auto &scene = extraScenes.at(graphIndex(mode));
    for (size_t i = 0; i < p.nodes.size(); i++)
        if (scene.reachable[p.nodes[i].stack]) {
            auto &n = p.nodes[i];
            auto &v = settings[i].integers;
            int shaderId = n.type == 9 || n.type == 6 ? v[2]
                           : n.type == 15             ? v[15]
                           : n.type == 19             ? v[6]
                           : n.type == 11             ? v[7]
                           : n.type == 1              ? v[9]
                                                      : -1;
            if (shaderId >= 0 && shaderId < 96)
                banks[n.stack] |= scene.constants[shaderId];
            if (shaderId == 94 || shaderId == 95)
                barrelAxes[n.stack] = true;
        }
    for (auto &e : edits[editIndex(mode)]) {
        e.applicable = !e.shaderBank || (banks[e.layer] & e.shaderBank) != 0;
        if (e.key.rfind("node.", 0) == 0 && e.key.size() > 4 &&
            e.key.compare(e.key.size() - 4, 4, ".f.5") == 0) {
            auto dot = e.key.find('.', 5);
            size_t index = std::stoul(e.key.substr(5, dot - 5));
            if (p.nodes.at(index).type == 7) {
                e.maximum = barrelAxes[p.nodes[index].stack] ? 4.f : 1.f;
                float value = authored(e, p.ticks / 60., mode);
                if (e.enabled || responseTarget(mode, e.key))
                    settings[index].floats[5] = value;
            }
        }

        if (e.key.rfind("colour.", 0) == 0 && e.key.size() > 6 &&
            e.key.compare(e.key.size() - 6, 6, ".floor") == 0) {
            auto end = e.key.find('.', 7);
            auto node = size_t(std::stoul(e.key.substr(7, end - 7)));
            e.applicable = settings.at(node).integers[2] == 4;
        }
        if (e.key.rfind("node.", 0) == 0 && e.key.size() > 4 &&
            e.key.compare(e.key.size() - 4, 4, ".i.4") == 0) {
            const auto stop = e.key.find('.', 5);
            const auto index = size_t(std::stoul(e.key.substr(5, stop - 5)));
            if (p.nodes[index].type == 9 || p.nodes[index].type == 6) {
                const int shader = settings[index].integers[2];
                const bool unused =
                    shader == 26 || shader == 92 || shader == 93 || shader == 94 || shader == 95;
                const bool channelSwap = shader >= 12 && shader <= 17;
                e.label =
                    "Layer " + std::to_string(p.nodes[index].stack) + " / node " +
                    std::to_string(index) +
                    (channelSwap ? " / Channel-swap mask strength" : " / Shader effect parameter") +
                    (unused        ? " (unused by this shader)"
                     : channelSwap ? ""
                                   : " (shader-specific)");
                e.applicable = e.applicable && !unused;
            }
        }

        if (mode == 16 &&
            (e.key == "node.19.f.4" || e.key == "node.19.i.5" || e.key == "node.19.i.6" ||
             e.key == "node.19.i.7" || e.key == "node.19.i.9" || e.key == "node.19.i.12"))
            e.applicable = false;
    }
    auto graph = graphs.find(mode);
    if (graph != graphs.end() && graph->second.mixOutput >= 0) {
        float amount = sceneControl(mode, "rings.amount", 1);
        auto &mix = p.stacks[graph->second.mixOutput].nodes;
        settings[mix[1]].integers[1] = int(std::lround(255 * (1 - amount)));
        settings[mix[2]].integers[1] = int(std::lround(255 * amount));
    }
    return settings;
}
std::vector<std::array<int, 3>> AuthoringModel::materialOverrides(int mode) const {
    auto &scene = extraScenes.at(graphIndex(mode));
    std::vector<std::array<int, 3>> overrides(scene.preset.nodes.size());
    for (const auto &e : edits[editIndex(mode)])
        if (e.key.rfind("input.", 0) == 0) {
            auto stop = e.key.find('.', 6);
            auto node = std::stoul(e.key.substr(6, stop - 6));
            int input = std::stoi(e.key.substr(stop + 1));
            overrides.at(node).at(input) = int(e.current);
        }
    return overrides;
}
std::string AuthoringModel::prepareMaterialEdit(int mode, const std::string &key) {
    if (mode < 7 || mode == 18 || mode == 21 || isStudy(mode) || key.rfind("node.", 0) != 0)
        return {};
    auto stop = key.find('.', 5);
    size_t index = std::stoul(key.substr(5, stop - 5));
    auto &scene = extraScenes.at(graphIndex(mode));
    auto &node = scene.preset.nodes.at(index);
    int slot = node.type == 9 || node.type == 6 ? 2
               : node.type == 15                ? 15
               : node.type == 19                ? 6
               : node.type == 11                ? 7
               : node.type == 1                 ? 9
                                                : -1;
    if (key != "node." + std::to_string(index) + ".i." + std::to_string(slot))
        return {};
    auto settings = authoredScene(mode);
    int shader = settings[index].integers[slot];
    if (shader < 0 || shader >= 96)
        return {};
    auto overrides = materialOverrides(mode);
    auto inputs = resolvedSamplerInputs(scene.preset, index, &settings, &overrides);
    auto valid = [&](int selector) {
        if (selector == 200 || selector == 202 || images.find(selector))
            return true;
        auto source = originalTextureBinding(selector);
        if (source.kind == TextureSourceKind::Stack)
            return scene.reachable[source.index];
        if (source.kind == TextureSourceKind::Root)
            return assetExists(data / "reference/roots" / (std::to_string(source.index) + ".argb"),
                               reader);
        if (source.kind == TextureSourceKind::Alternate) {
            auto stem = std::to_string(source.index);
            return assetExists(data / "reference/literals" /
                                   ((source.index < 10 ? "0" : "") + stem + ".png"),
                               reader) ||
                   assetExists(data / "reference/literals" / (stem + ".jpg"), reader);
        }
        return false;
    };
    int fallback = 3;
    for (int input : inputs)
        if (valid(input)) {
            fallback = input;
            break;
        }
    bool filled = false;
    for (int sampler = 0; sampler < 3; ++sampler)
        if ((scene.masks[shader] & (1 << sampler)) && !valid(inputs[sampler]))
            for (auto &e : controls(mode))
                if (e.key == "input." + std::to_string(index) + "." + std::to_string(sampler)) {
                    e.enabled = true;
                    e.value = float(fallback);
                    e.motion = 0;
                    filled = true;
                }
    return filled ? "New shader inputs reuse the available source. Change Texture input above."
                  : std::string{};
}
std::string AuthoringModel::validateScene(int mode) {
    if (mode < 7)
        return {};
    auto settings = authoredScene(mode);
    auto overrides = materialOverrides(mode);
    auto &scene = extraScenes.at(graphIndex(mode));
    for (size_t index = 0; index < scene.preset.nodes.size(); index++) {
        auto &n = scene.preset.nodes[index];
        if (!scene.reachable[n.stack])
            continue;
        auto &i = settings[index].integers;
        if (n.type == 1) {
            auto read = [&](const char *key) {
                return sceneControl(mode, ("sprite." + std::to_string(index) + "." + key).c_str());
            };
            if (read("enabled") > .5f) {
                const int capacity = int(read("columns")) * int(read("rows"));
                if (capacity < 1 || capacity > 256 ||
                    read("baseFirst") + read("baseCount") > capacity ||
                    read("rareFirst") + read("rareCount") > capacity)
                    return "Sprite sequence frame ranges exceed the atlas grid";
            }
        }
        int shaderId = -1;
        std::array<int, 3> selectors{};
        if (n.type == 11) {
            shaderId = i[7];
            selectors = {i[0], i[12], i[13]};
        }
        if (n.type == 9) {
            shaderId = i[2];
            selectors = {i[5], i[3], i[6]};
        }
        if (n.type == 6) {
            shaderId = i[2];
            selectors = {31 + n.stack, i[3], 0};
        }
        if (n.type == 15) {
            shaderId = i[15];
            selectors = {i[3], i[4], i[12]};
        }
        if (n.type == 19) {
            shaderId = i[6];
            selectors = {i[4], i[5], i[8]};
        }
        if (n.type == 1) {
            shaderId = i[9];
            selectors = {i[6], i[7], 0};
        }
        if (shaderId < 0)
            continue;
        selectors = resolvedSamplerInputs(scene.preset, index, &settings, &overrides);
        if (shaderId >= 96)
            return "Shader is outside the recovered library.";
        for (int face = 0; face < (n.type == 11 ? 6 : 1); ++face) {
            if (n.type == 11) {
                if (i[face] == 0)
                    continue;
                selectors[0] = overrides[index][0] > 0 ? overrides[index][0] : i[face];
            }
            for (int s = 0; s < 3; s++)
                if (scene.masks[shaderId] & (1 << s)) {
                    auto binding = originalTextureBinding(selectors[s]);
                    if (selectors[s] == 200 || selectors[s] == 202)
                        continue;
                    bool valid =
                        neon::effects::importedImageSelector(selectors[s])
                            ? images.find(selectors[s]) != nullptr
                        : binding.kind == TextureSourceKind::Stack ? scene.reachable[binding.index]
                        : binding.kind == TextureSourceKind::Root
                            ? assetExists(data / "reference/roots" /
                                              (std::to_string(binding.index) + ".argb"),
                                          reader)
                        : binding.kind == TextureSourceKind::Alternate
                            ? (assetExists(
                                   data / "reference/literals" /
                                       ((binding.index < 10 ? std::string("0") : std::string()) +
                                        std::to_string(binding.index) + ".png"),
                                   reader) ||
                               assetExists(data / "reference/literals" /
                                               (std::to_string(binding.index) + ".jpg"),
                                           reader))
                            : false;
                    if (!valid)
                        return "Node " + std::to_string(index) + " needs texture input " +
                               std::to_string(s) +
                               ". Choose a recovered texture or an existing layer before "
                               "selecting this shader.";
                }
        }
    }
    return {};
}
void AuthoringModel::refreshExtraLive() {
    for (int mode = 7; mode <= lastJukeboxPreset; mode++)
        if (mode != 18 && mode != 21 && !isStudy(mode))
            authoredScene(mode);
}
void AuthoringModel::tickExtraScenes() {
    for (int mode = 7; mode <= lastJukeboxPreset; mode++) {
        if (mode == 17 || mode == 18 || mode == 21 || isStudy(mode))
            continue;
        auto &scene = extraScenes.at(graphIndex(mode));
        scene.preset.tick();
        auto settings = authoredScene(mode);
        for (auto &t : scene.tunnels)
            t.second.tick(settings[t.first], profiles);
        for (auto &p : scene.particles)
            p.second.tick(settings[p.first], scene.palette);
    }
}
PresetPlayback &AuthoringModel::composition(int mode) {
    if (external_ && mode == externalMode_)
        return *external_;
    if (mode == 0)
        return synth;
    if (mode == 1)
        return cube;
    if (mode >= 7)
        return extraScenes.at(graphIndex(mode)).preset;
    return mode == 2   ? colour
           : mode == 3 ? zomg
           : mode == 4 ? planet
           : mode == 5 ? travelA
                       : travelB;
}
void AuthoringModel::applyScene(int mode, ShapeSettings &tunnel, ShapeSettings &source,
                                ShapeSettings &terminal) {
    for (auto &e : edits[editIndex(mode)]) {
        float *field = nullptr;
        int *integer = nullptr;
        if (e.key == "tunnel.ripple.enabled") {
            e.live = 1;
            authored(e, composition(mode).ticks / 60., mode);
            continue;
        }
        if (e.key.rfind("tunnel.mesh.", 0) == 0) {
            e.live = e.key == "tunnel.mesh.sides" ? 8.f : 1.f;
            authored(e, composition(mode).ticks / 60., mode);
            continue;
        }
        if (e.key == "tunnel.speed") {
            e.live = 1;
            auto value = authored(e, composition(mode).ticks / 60., mode);
            if (e.enabled || responseTarget(mode, e.key))
                tunnel.integers[2] = int(std::lround(tunnel.integers[2] * value));
            continue;
        }
        if (e.key == "tunnel.profileA")
            integer = &tunnel.integers[7];
        if (e.key == "tunnel.profileB")
            integer = &tunnel.integers[10];
        if (e.key == "tunnel.morph")
            field = &tunnel.floats[2];
        if (e.key == "tunnel.deformX")
            field = &tunnel.floats[4];
        if (e.key == "tunnel.deformY")
            field = &tunnel.floats[5];
        if (e.key == "tunnel.deformZ")
            field = &tunnel.floats[6];
        if (e.key == "tunnel.yaw")
            field = &tunnel.floats[1];
        if (e.key == "tunnel.pitch")
            field = &tunnel.floats[0];
        if (e.key == "tunnel.roll")
            field = &tunnel.floats[12];
        if (e.key == "tunnel.repeatX")
            integer = &tunnel.integers[8];
        if (e.key == "tunnel.repeatY")
            integer = &tunnel.integers[9];
        if (e.key == "tunnel.spacing")
            integer = &tunnel.integers[6];
        if (e.key == "tunnel.alpha")
            integer = &tunnel.integers[13];
        if (e.key == "tunnel.colour")
            integer = &tunnel.integers[14];
        if (e.key == "source.alpha")
            integer = &source.integers[1];
        if (e.key == "source.blue")
            integer = &source.integers[4];
        if (e.key.rfind("source.constant.", 0) == 0) {
            int i = std::stoi(e.key.substr(16));
            field = &terminal.floats[i < 4 ? 4 + i : 9 + i - 4];
        }
        if (field) {
            e.live = *field;
            if (e.enabled || responseTarget(mode, e.key))
                *field = authored(e, composition(mode).ticks / 60., mode);
            else
                authored(e, composition(mode).ticks / 60., mode);
        }
        if (integer) {
            e.live = float(*integer);
            if (e.enabled || responseTarget(mode, e.key))
                *integer = int(std::lround(authored(e, composition(mode).ticks / 60., mode)));
            else
                authored(e, composition(mode).ticks / 60., mode);
        }
        if (e.key.rfind("camera.", 0) == 0) {
            e.live = e.key == "camera.zoom" ? 1.f : 0.f;
            authored(e, composition(mode).ticks / 60., mode);
        }
    }
}
float AuthoringModel::sceneControl(int mode, const char *key, float fallback) {
    for (auto &e : controls(mode))
        if (e.key == key && (e.enabled || responseTarget(mode, e.key)))
            return e.current;
    return fallback;
}
void AuthoringModel::tickScenes() {
    ++sceneTick;
    for (int mode = 5; mode <= 6; mode++) {
        auto &p = composition(mode);
        p.tick();
        auto t = p.nodes[mode == 5 ? 18 : 17].effective, s = p.nodes[10].effective,
             c = p.nodes[11].effective;
        applyScene(mode, t, s, c);
        tunnels[mode - 5].tick(t, profiles);
    }
}
void AuthoringModel::refreshLive() {
    for (int mode = 2; mode <= 4; mode++) {
        auto &p = composition(mode);
        int source = mode == 3   ? 10
                     : mode == 4 ? 7
                                 : 13,
            final = mode == 3   ? 17
                    : mode == 4 ? 14
                                : 16;
        auto a = p.nodes[source].effective, b = p.nodes[source + 1].effective,
             c = p.nodes[final].effective, d = p.nodes[final + 1].effective;
        apply(mode, a, b, c, d);
    }
    for (int mode = 5; mode <= 6; mode++) {
        auto &p = composition(mode);
        auto t = p.nodes[mode == 5 ? 18 : 17].effective, s = p.nodes[10].effective,
             c = p.nodes[11].effective;
        applyScene(mode, t, s, c);
    }
}
void AuthoringModel::apply(int mode, ShapeSettings &source, ShapeSettings &terminal,
                           ShapeSettings &final, ShapeSettings &finalTerminal) {
    for (auto &e : edits[editIndex(mode)]) {
        float *field = nullptr;
        int *integer = nullptr;
        auto index = [&](size_t start) { return std::stoi(e.key.substr(start)); };
        if (e.key.rfind("source.uv.", 0) == 0)
            field = &source.floats[index(10)];
        else if (e.key.rfind("source.constant.", 0) == 0) {
            int i = index(16);
            field = &terminal.floats[i < 4 ? 4 + i : 9 + i - 4];
        } else if (e.key.rfind("final.constant.", 0) == 0) {
            int i = index(15);
            field = &finalTerminal.floats[i < 4 ? 4 + i : 9 + i - 4];
        } else if (e.key == "source.alpha")
            integer = &source.integers[1];
        else if (e.key == "source.blue")
            integer = &source.integers[4];
        else if (e.key == "final.alpha")
            integer = &final.integers[1];
        else if (e.key == "final.blue")
            integer = &final.integers[4];
        if (field) {
            e.live = *field;
            if (e.enabled || responseTarget(mode, e.key))
                *field = authored(e, composition(mode).ticks / 60., mode);
            else
                authored(e, composition(mode).ticks / 60., mode);
        }
        if (integer) {
            e.live = float(*integer);
            if (e.enabled || responseTarget(mode, e.key))
                *integer = int(std::lround(authored(e, composition(mode).ticks / 60., mode)));
            else
                authored(e, composition(mode).ticks / 60., mode);
        }
        if (e.key.rfind("camera.", 0) == 0) {
            e.live = e.key == "camera.zoom" ? 1.f : 0.f;
            authored(e, composition(mode).ticks / 60., mode);
        }
    }
}
ShapeSettings AuthoringModel::authoredCube() {
    auto &playback = composition(1);
    auto settings = playback.nodes[24].effective;
    for (auto &e : edits[19]) {
        if (e.key == "cube.subdivisions") {
            e.live = float(settings.integers[10] + 2);
            float value = authored(e, playback.ticks / 60., 1);
            if (e.enabled || responseTarget(1, e.key))
                settings.integers[10] = int(std::lround(value)) - 2;
        } else if (e.key == "cube.mirrorUV") {
            e.live = float(settings.integers[14] & 1);
            float value = authored(e, playback.ticks / 60., 1);
            if (e.enabled || responseTarget(1, e.key))
                settings.integers[14] = (settings.integers[14] & ~1) | (value >= .5f ? 1 : 0);
        } else {
            bool integer = e.key[5] == 'i';
            int slot = std::stoi(e.key.substr(7));
            e.live = integer ? float(settings.integers[slot]) : settings.floats[slot];
            float value = authored(e, playback.ticks / 60., 1);
            if (e.enabled || responseTarget(1, e.key)) {
                if (integer)
                    settings.integers[slot] = int(std::lround(value));
                else
                    settings.floats[slot] = value;
            }
        }
    }
    return settings;
}
std::vector<ReferenceControl> &AuthoringModel::controls(int mode) {
    if (mode == 1)
        return edits[19];
    if (mode < 2 || mode > lastJukeboxPreset || (mode == 18 || mode == 21 || isStudy(mode)))
        throw std::runtime_error("No preset editor for study " + std::to_string(mode));
    return edits[editIndex(mode)];
}
void AuthoringModel::clearEdits(int mode) {
    for (auto &e : controls(mode)) {
        e.enabled = false;
        e.motion = 0;
    }
    if (mode == 17)
        vectorTemplate(edits[15]);
    refreshControls(mode);
}
void AuthoringModel::refreshControls(int mode) {
    if (mode == 1)
        authoredCube();
    else if (mode >= 7)
        authoredScene(mode);
    else
        refreshLive();
}
std::string AuthoringModel::sceneName(int mode) const {
    return sceneNames.at(editIndex(mode));
}
void AuthoringModel::sceneName(int mode, const std::string &name) {
    if (name.size() > 128 ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }))
        throw std::runtime_error("Scene name must be a single line, at most 128 characters");
    sceneNames.at(editIndex(mode)) = name;
}
std::string AuthoringModel::validateAuthoring(int mode) {
    if (mode >= 7 && mode <= lastJukeboxPreset && mode != 18 && mode != 21 && !isStudy(mode)) {
        for (const auto &field : controls(mode))
            if ((field.enabled || field.motion) &&
                (field.value < field.minimum || field.value > field.maximum))
                return "Scene value outside shader-specific range";
        auto error = validateCycles(mode);
        if (!error.empty())
            return error;
    }
    if (mode == 1) {
        authoredCube();
        return {};
    }
    return validateScene(mode);
}
std::vector<std::pair<int, std::string>> AuthoringModel::layers(int mode) const {
    std::vector<std::pair<int, std::string>> result;
    if (mode == 1)
        return {{0, "Layer 0 / subdivided cube"}};
    if (mode < 7)
        return result;
    auto &scene = extraScenes.at(graphIndex(mode));
    for (int stack = 0; stack < 16; stack++)
        if (scene.reachable[stack]) {
            std::string line = "Layer " + std::to_string(stack) + ": ";
            for (auto index : scene.preset.stacks[stack].nodes) {
                int t = scene.preset.nodes[index].type;
                const char *label = t == 9    ? "Mixer"
                                    : t == 6  ? "Feedback"
                                    : t == 1  ? "Particles"
                                    : t == 11 ? "Cube"
                                    : t == 19 ? "Flower"
                                    : t == 15 ? "Tunnel"
                                              : nullptr;
                if (label) {
                    if (line.back() != ' ')
                        line += " -> ";
                    line += label;
                    line += " [" + std::to_string(index) + "]";
                }
            }
            if (stack == scene.preset.outputStack)
                line += " (original output)";
            result.push_back({stack, line});
        }
    return result;
}
void AuthoringModel::writeEdits(std::ostream &out) const {
    out << "REFERENCEEDITS 10\n" << edits.size() << "\n" << std::setprecision(9);
    for (size_t i = 0; i < edits.size(); i++) {
        auto &list = edits[i];
        // Preserve disabled edits too, but omit untouched defaults from every
        // other preset bank. Adding library entries must not inflate every layer.
        auto changed = [&](size_t j) {
            const auto &e = list[j];
            return e.enabled || e.motion || e.value != defaultValues[i][j] || e.period != 4 ||
                   e.depth != .1f;
        };
        size_t count = 0;
        for (size_t j = 0; j < list.size(); ++j)
            count += changed(j);
        out << std::quoted(sceneNames[i]) << '\n' << count << '\n';
        for (size_t j = 0; j < list.size(); ++j)
            if (changed(j)) {
                const auto &e = list[j];
                out << std::quoted(e.key) << ' ' << e.enabled << ' ' << e.value << ' ' << e.motion
                    << ' ' << e.period << ' ' << e.depth << '\n';
            }
    }
}
void AuthoringModel::readEdits(std::istream &in) {
    auto pending = edits;
    auto names = sceneNames;
    std::set<int> changedModes;
    for (size_t g = 0; g < edits.size(); ++g)
        for (const auto &e : edits[g])
            if (e.enabled || e.motion)
                changedModes.insert(modeForGroup(int(g)));
    for (auto &list : pending)
        for (auto &e : list) {
            e.enabled = false;
            e.motion = 0;
        }
    std::string magic;
    int version;
    if (!(in >> magic >> version) || magic != "REFERENCEEDITS" || version < 1 || version > 10)
        throw std::runtime_error("Invalid scene edits header");
    size_t groups = version == 1   ? 3u
                    : version == 2 ? 5u
                    : version == 3 ? 9u
                    : version == 4 ? 14u
                    : version == 5 ? 15u
                    : version == 6 ? 16u
                    : version == 7 ? 19u
                                   : 20u;
    if (version == 8)
        groups = 21;
    if (version == 9 && (!(in >> groups) || groups < 21 || groups > pending.size()))
        throw std::runtime_error("Invalid legacy scene group count");
    if (version == 10 && (!(in >> groups) || (groups < 20 || groups > pending.size())))
        throw std::runtime_error("Invalid scene group count");
    for (size_t group = 0; group < groups; group++) {
        auto &list = pending[group];
        size_t count;
        if (version >= 4) {
            if (!(in >> std::quoted(names[group])) || names[group].size() > 128 ||
                std::any_of(names[group].begin(), names[group].end(),
                            [](unsigned char c) { return c < 32; }))
                throw std::runtime_error("Invalid scene name");
        }
        if (!(in >> count) || count > 4096)
            throw std::runtime_error("Invalid scene edit count");
        if (count)
            changedModes.insert(modeForGroup(int(group)));
        std::vector<std::string> seen;
        for (size_t i = 0; i < count; i++) {
            std::string key;
            int enabled, motion = 0;
            float value, period = 4, depth = .1f;
            if (!(in >> std::quoted(key) >> enabled >> value) ||
                (version >= 4 && !(in >> motion >> period >> depth)) || enabled < 0 ||
                enabled > 1 || !std::isfinite(value) || motion < 0 || motion > 3 ||
                !std::isfinite(period) || period < .25f || period > 60 || !std::isfinite(depth) ||
                depth < 0 || depth > 1 || std::find(seen.begin(), seen.end(), key) != seen.end())
                throw std::runtime_error("Invalid scene edit record");
            seen.push_back(key);
            auto found =
                std::find_if(list.begin(), list.end(), [&](const auto &e) { return e.key == key; });
            if (found == list.end()) {
                if (enabled)
                    throw std::runtime_error("Scene uses an unavailable parameter: " + key);
                continue;
            }
            bool deferredAxis = false;
            if (key.rfind("node.", 0) == 0 && key.size() > 4 &&
                key.compare(key.size() - 4, 4, ".f.5") == 0) {
                const int mode = modeForGroup(int(group));
                auto dot = key.find('.', 5);
                size_t node = std::stoul(key.substr(5, dot - 5));
                deferredAxis = mode >= 7 && mode != 18 && mode != 21 && !isStudy(mode) &&
                               node < composition(mode).nodes.size() &&
                               composition(mode).nodes[node].type == 7 && value >= 0 && value <= 4;
            }
            if (!deferredAxis && (value < found->minimum || value > found->maximum))
                throw std::runtime_error("Scene value outside supported range");
            if (key.rfind("cycle.", 0) == 0 && motion)
                throw std::runtime_error("Cycle settings cannot have a second authored oscillator");
            if (!found->choices.empty() &&
                (motion || (enabled && std::none_of(found->choices.begin(), found->choices.end(),
                                                    [&](const auto &c) {
                                                        return c.value == int(std::lround(value));
                                                    }))))
                throw std::runtime_error("Invalid discrete scene choice");
            found->enabled = enabled != 0;
            found->value = value;
            found->motion = motion;
            found->period = period;
            found->depth = depth;
        }
    }
    if (version < 6)
        vectorTemplate(pending[15]);
    auto old = edits;
    auto oldNames = sceneNames;
    edits = std::move(pending);
    sceneNames = std::move(names);
    try {
        refreshLive();
        // Original default banks are already initialized. Only changed banks need
        // re-evaluation and validation when loading a sparse scene or layer asset.
        for (int mode : changedModes) {
            if (mode < 7 || mode == 18 || mode == 21 || isStudy(mode))
                continue;
            refreshControls(mode);
            for (const auto &field : controls(mode))
                if ((field.enabled || field.motion) &&
                    (field.value < field.minimum || field.value > field.maximum))
                    throw std::runtime_error("Scene value outside shader-specific range");
            auto error = validateCycles(mode);
            if (error.empty())
                error = validateScene(mode);
            if (!error.empty())
                throw std::runtime_error(error);
        }
    } catch (...) {
        edits = std::move(old);
        sceneNames = std::move(oldNames);
        refreshLive();
        refreshExtraLive();
        throw;
    }
}
void AuthoringModel::bindPlayback(int mode, PresetPlayback *playback) {
    externalMode_ = mode;
    external_ = playback;
}
void AuthoringModel::reset() {
    synth.reset();
    cube.reset();
    colour.reset();
    zomg.reset();
    planet.reset();
    travelA.reset();
    travelB.reset();
    for (auto &t : tunnels)
        t.reset();
    for (auto &scene : extraScenes)
        scene.reset();
    refreshLive();
    refreshExtraLive();
}
void AuthoringModel::tick() {
    synth.tick();
    cube.tick();
    colour.tick();
    zomg.tick();
    planet.tick();
    tickScenes();
    tickExtraScenes();
    refreshLive();
    refreshExtraLive();
}
float AuthoringModel::phase(int mode) const {
    if (external_ && mode == externalMode_) {
        auto &p = *external_;
        if (mode >= 7)
            return float(p.ticks % 600) / 600;
        return mode >= 5 ? (mode == 5 ? p.nodes[11].effective.floats[6]
                                      : (p.nodes[10].effective.floats[4] + 6.2831855f) / 12.566371f)
               : mode == 0 ? p.nodes[10].effective.floats[5]
               : mode == 1 ? (p.nodes[24].effective.floats[8] + 3.1415927f) / 6.2831855f
                           : (p.nodes[mode == 3   ? 10
                                      : mode == 4 ? 7
                                                  : 13]
                                  .effective.floats[mode == 2 ? 4 : 9] +
                              6.2831855f) /
                                 12.566371f;
    }
    if (mode >= 7)
        return float(const_cast<AuthoringModel *>(this)->composition(mode).ticks % 600) / 600;
    return mode >= 5
               ? (mode == 5 ? travelA.nodes[11].effective.floats[6]
                            : (travelB.nodes[10].effective.floats[4] + 6.2831855f) / 12.566371f)
           : mode == 0 ? synth.nodes[10].effective.floats[5]
           : mode == 1 ? (cube.nodes[24].effective.floats[8] + 3.1415927f) / 6.2831855f
                       : (const_cast<AuthoringModel *>(this)
                              ->composition(mode)
                              .nodes[mode == 3   ? 10
                                     : mode == 4 ? 7
                                                 : 13]
                              .effective.floats[mode == 2 ? 4 : 9] +
                          6.2831855f) /
                             12.566371f;
}
} // namespace neon::pc
