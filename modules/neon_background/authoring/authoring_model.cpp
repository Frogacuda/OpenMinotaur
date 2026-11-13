#include "authoring_model.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
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
        auto &list = edits[mode - 2];
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
        auto &list = edits[mode - 2];
        auto add = [&](std::string key, std::string label, float low, float high) {
            ReferenceControl control;
            control.key = key;
            control.label = label;
            control.minimum = low;
            control.maximum = high;
            control.value = std::clamp(0.f, low, high);
            list.push_back(std::move(control));
        };
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
    initializeExtraScenes();
    edits[15] = edits[7];
    vectorTemplate(edits[15]);
    refreshLive();
    refreshExtraLive();
}
void AuthoringModel::addAuthoringControls(int mode) {
    auto &scene = extraScenes.at(graphIndex(mode));
    auto &list = edits[mode - 2];
    std::vector<ReferenceChoice> textures{{0, "None / unused input"},
                                          {200, "Vector objects / live generated texture"}};
    for (int root = 0; root < 30; root++)
        if (assetExists(data / "reference/roots" / (std::to_string(root) + ".argb"), reader))
            textures.push_back({root + 1, "Root " + std::to_string(root)});
    for (int slot : {5, 7, 11, 12, 20, 31, 32, 34})
        textures.push_back({47 + slot, "Literal " + std::to_string(slot)});
    for (int stack = 0; stack < 16; stack++)
        if (scene.reachable[stack])
            textures.push_back(
                {31 + stack, "Layer " + std::to_string(stack) + " (ordered / feedback)"});
    std::map<int, std::string> names;
    auto file = assetStream(data / "reference/shader-names.tsv", reader);
    std::string line;
    while (std::getline(file, line)) {
        auto tab = line.find('\t');
        if (tab != std::string::npos)
            names[std::stoi(line.substr(0, tab))] = line.substr(tab + 1);
    }
    std::vector<ReferenceChoice> shaders;
    for (int id : {0,  1,  2,  3,  4,  5,  6,  8,  11, 12, 13, 14, 15, 16, 18, 26, 29, 39, 40,
                   46, 47, 48, 49, 57, 59, 61, 69, 70, 71, 72, 73, 74, 80, 81, 83, 84, 85})
        shaders.push_back({id, std::to_string(id) + " / " + names[id]});
    auto choice = [&](std::string key, std::string label,
                      const std::vector<ReferenceChoice> &options, int initial) {
        ReferenceControl c;
        c.key = std::move(key);
        c.label = std::move(label);
        c.minimum = 0;
        c.maximum = 255;
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
        }
    outputs.push_back({16, "Vector objects / generated layer"});
    choice("scene.output", "Composition / output layer", outputs, scene.preset.outputStack);
    for (size_t index = 0; index < scene.preset.nodes.size(); index++) {
        auto &n = scene.preset.nodes[index];
        if (!scene.reachable[n.stack] ||
            (n.type != 9 && n.type != 6 && n.type != 15 && n.type != 1 && n.type != 19))
            continue;
        int shaderSlot = n.type == 9 || n.type == 6 ? 2 : n.type == 15 ? 15 : n.type == 19 ? 6 : 9;
        std::array<int, 3> slots = n.type == 9    ? std::array<int, 3>{5, 3, 6}
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
        for (int s = 0; s < 3; s++)
            if (slots[s] >= 0) {
                auto options = textures;
                if (std::none_of(options.begin(), options.end(), [&](const auto &c) {
                        return c.value == n.saved.integers[slots[s]];
                    }))
                    options.push_back(
                        {n.saved.integers[slots[s]],
                         "Original unused selector " + std::to_string(n.saved.integers[slots[s]])});
                choice(key(slots[s]), prefix + "texture input " + std::to_string(s), options,
                       n.saved.integers[slots[s]]);
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
    for (int mode = 7; mode <= 20; mode++) {
        if (mode == 17 || mode == 18)
            continue;
        auto &scene = extraScenes.at(graphIndex(mode));
        auto &list = edits[mode - 2];
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
                    "Layer " + std::to_string(n.stack) + " / node " + std::to_string(j) + " / " +
                        label,
                    lo, hi);
            };
            if (n.type == 1) {
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
                field(false, 10, "tunnel yaw", -3.1415927f, 3.1415927f);
                field(false, 11, "tunnel pitch", -3.1415927f, 3.1415927f);
                field(false, 12, "tunnel roll", -3.1415927f, 3.1415927f);
            }
            if (n.type == 19) {
                field(true, 0, "flower segments", 3, 1024);
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
                field(true, 4, "mixer shader colour", 0, 255);
                const char *names[] = {"UV width",           "UV height",
                                       "UV X offset",        "UV Y offset",
                                       "UV rotation",        "second UV width",
                                       "second UV height",   "second UV X offset",
                                       "second UV Y offset", "second UV rotation"};
                for (int k = 0; k < 10; k++)
                    field(false, k, names[k], k % 5 == 4 ? -6.2831855f : -.95f,
                          k % 5 == 4 ? 6.2831855f : 2);
            }
            if (n.type == 7) {
                for (int k = 0; k < 8; k++) {
                    field(
                        false, k < 4 ? 4 + k : 9 + k - 4,
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
        if (!scene.particles.empty() || !scene.tunnels.empty() ||
            std::any_of(scene.preset.nodes.begin(), scene.preset.nodes.end(),
                        [](auto &n) { return n.type == 19; })) {
            add("camera.yaw", "3D camera / yaw", -.8f, .8f);
            add("camera.pitch", "3D camera / pitch", -.8f, .8f);
            add("camera.roll", "3D camera / roll", -.8f, .8f);
            add("camera.x", "3D camera / horizontal position", -500, 500);
            add("camera.y", "3D camera / vertical position", -500, 500);
            add("camera.z", "3D camera / forward position", -900, 900);
            add("camera.zoom", "3D camera / lens zoom", .5f, 2);
            list.back().value = 1;
        }
        addAuthoringControls(mode);
        for (auto spec :
             std::initializer_list<std::tuple<const char *, const char *, float, float>>{
                 {"vector.width", "Vector beam / core width", .5f, 4.f},
                 {"vector.intensity", "Vector beam / brightness", .1f, 3.f},
                 {"vector.halo", "Vector beam / glow strength", 0.f, 2.f},
                 {"vector.spread", "Vector beam / glow spread", 1.f, 8.f},
                 {"vector.persistence", "Vector beam / phosphor decay ms", 0.f, 180.f},
                 {"vector.colour", "Vector beam / colour (0 tube, 1 objects)", 0.f, 1.f},
                 {"vector.hot", "Vector beam / white hot core", 0.f, 1.f},
                 {"vector.scene", "Vector beam / scene (0 HUD, 1 objects, 2 test, 3 file)", 0.f,
                  3.f},
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
                     {"title.size", "Title flight / common sprite size", .25f, 3.f},
                     {"title.period", "Title flight / colour cycle seconds", 4.f, 90.f},
                     {"title.hue", "Title flight / hue offset", 0.f, 1.f},
                     {"title.filter", "Title flight / colour filtration", 0.f, 1.f},
                     {"title.opacity", "Title flight / sprite opacity", 0.f, 1.f}}) {
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
}
std::vector<ShapeSettings> AuthoringModel::authoredScene(int mode) {
    auto &p = composition(mode);
    std::vector<ShapeSettings> settings;
    for (auto &n : p.nodes)
        settings.push_back(n.effective);
    for (auto &e : edits[mode - 2]) {
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
            e.live = e.key.rfind("vector.", 0) == 0  ? vectorDefault(e.key)
                     : e.key.rfind("title.", 0) == 0 ? titleDefault(e.key)
                     : e.key == "camera.zoom" || e.key.rfind("layer.", 0) == 0 ? 1.f
                     : e.key == "scene.output" ? float(p.outputStack)
                                               : 0.f;
            authored(e, p.ticks / 60., mode);
        }
    }
    std::array<unsigned, 16> banks{};
    auto &scene = extraScenes.at(graphIndex(mode));
    for (size_t i = 0; i < p.nodes.size(); i++)
        if (scene.reachable[p.nodes[i].stack]) {
            auto &n = p.nodes[i];
            auto &v = settings[i].integers;
            int shaderId = n.type == 9 || n.type == 6 ? v[2]
                           : n.type == 15             ? v[15]
                           : n.type == 19             ? v[6]
                           : n.type == 1              ? v[9]
                                                      : -1;
            if (shaderId >= 0 && shaderId < 92)
                banks[n.stack] |= scene.constants[shaderId];
        }
    for (auto &e : edits[mode - 2]) {
        e.applicable = !e.shaderBank || (banks[e.layer] & e.shaderBank) != 0;
        if (mode == 16 &&
            (e.key == "node.19.f.4" || e.key == "node.19.i.5" || e.key == "node.19.i.6" ||
             e.key == "node.19.i.7" || e.key == "node.19.i.9" || e.key == "node.19.i.12"))
            e.applicable = false;
    }
    return settings;
}
std::string AuthoringModel::validateScene(int mode) {
    if (mode < 7)
        return {};
    auto settings = authoredScene(mode);
    auto &scene = extraScenes.at(graphIndex(mode));
    for (size_t index = 0; index < scene.preset.nodes.size(); index++) {
        auto &n = scene.preset.nodes[index];
        if (!scene.reachable[n.stack])
            continue;
        auto &i = settings[index].integers;
        int shaderId = -1;
        std::array<int, 3> selectors{};
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
        if (shaderId >= 92)
            return "Shader is outside the recovered library.";
        for (int s = 0; s < 3; s++)
            if (scene.masks[shaderId] & (1 << s)) {
                auto binding = originalTextureBinding(selectors[s]);
                if (selectors[s] == 200)
                    continue;
                bool valid =
                    binding.kind == TextureSourceKind::Stack ? scene.reachable[binding.index]
                    : binding.kind == TextureSourceKind::Root
                        ? assetExists(data / "reference/roots" /
                                          (std::to_string(binding.index) + ".argb"),
                                      reader)
                    : binding.kind == TextureSourceKind::Alternate
                        ? (binding.index == 5 || binding.index == 7 || binding.index == 11 ||
                           binding.index == 12 || binding.index == 20 || binding.index == 31 ||
                           binding.index == 32 || binding.index == 34)
                        : false;
                if (!valid)
                    return "Node " + std::to_string(index) + " needs texture input " +
                           std::to_string(s) + " (selector " + std::to_string(selectors[s]) + ")" +
                           ". Choose a recovered texture or an existing layer before "
                           "selecting this shader.";
            }
    }
    return {};
}
void AuthoringModel::refreshExtraLive() {
    for (int mode = 7; mode <= 20; mode++)
        if (mode != 18)
            authoredScene(mode);
}
void AuthoringModel::tickExtraScenes() {
    for (int mode = 7; mode <= 20; mode++) {
        if (mode == 17 || mode == 18)
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
    for (auto &e : edits[mode - 2]) {
        float *field = nullptr;
        int *integer = nullptr;
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
    for (auto &e : edits[mode - 2])
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
    for (auto &e : edits[mode - 2]) {
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
std::vector<ReferenceControl> &AuthoringModel::controls(int mode) {
    if (mode < 2 || mode > 20 || mode == 18)
        throw std::runtime_error("No preset editor for this study");
    return edits[mode - 2];
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
    if (mode >= 7)
        authoredScene(mode);
    else
        refreshLive();
}
std::string AuthoringModel::sceneName(int mode) const {
    return sceneNames.at(mode - 2);
}
void AuthoringModel::sceneName(int mode, const std::string &name) {
    if (name.size() > 128 ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }))
        throw std::runtime_error("Scene name must be a single line, at most 128 characters");
    sceneNames.at(mode - 2) = name;
}
std::string AuthoringModel::validateAuthoring(int mode) {
    return validateScene(mode);
}
std::vector<std::pair<int, std::string>> AuthoringModel::layers(int mode) const {
    std::vector<std::pair<int, std::string>> result;
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
    out << "REFERENCEEDITS 7\n" << std::setprecision(9);
    for (size_t i = 0; i < edits.size(); i++) {
        auto &list = edits[i];
        out << std::quoted(sceneNames[i]) << '\n' << list.size() << '\n';
        for (auto &e : list)
            out << std::quoted(e.key) << ' ' << e.enabled << ' ' << e.value << ' ' << e.motion
                << ' ' << e.period << ' ' << e.depth << '\n';
    }
}
void AuthoringModel::readEdits(std::istream &in) {
    readEdits(in, -1);
}
void AuthoringModel::readEdits(std::istream &in, int activeMode) {
    auto pending = edits;
    auto names = sceneNames;
    for (auto &list : pending)
        for (auto &e : list) {
            e.enabled = false;
            e.motion = 0;
        }
    std::string magic;
    int version;
    if (!(in >> magic >> version) || magic != "REFERENCEEDITS" || version < 1 || version > 7)
        throw std::runtime_error("Invalid scene edits header");
    size_t groups = version == 1   ? 3u
                    : version == 2 ? 5u
                    : version == 3 ? 9u
                    : version == 4 ? 14u
                    : version == 5 ? 15u
                    : version == 6 ? 16u
                                   : 19u;
    for (size_t group = 0; group < groups; group++) {
        auto &list = pending[group];
        size_t count;
        if (version >= 4) {
            if (!(in >> std::quoted(names[group])) || names[group].size() > 128 ||
                std::any_of(names[group].begin(), names[group].end(),
                            [](unsigned char c) { return c < 32; }))
                throw std::runtime_error("Invalid scene name");
        }
        if (!(in >> count) || count > 512)
            throw std::runtime_error("Invalid scene edit count");
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
            if (value < found->minimum || value > found->maximum)
                throw std::runtime_error("Scene value outside supported range");
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
        refreshExtraLive();
        for (int mode = 7; mode <= 20; mode++) {
            if (mode == 18)
                continue;
            auto error = activeMode < 0 || mode == activeMode ? validateScene(mode) : std::string{};
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
