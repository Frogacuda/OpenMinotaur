#include "layer_asset.h"
#include "primitive_mesh.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
namespace neon::effects {
namespace {
void need(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
void validate(const LayerAsset &a) {
    need(!a.name.empty() && a.name.size() <= 128 && a.notes.size() <= 2048 &&
             a.scene.size() <= 1048576 && !a.scene.empty() && int(a.kind) >= 0 && int(a.kind) <= 1,
         "Invalid layer asset metadata");
    need(a.kind == LayerAssetKind::Recovered ? (a.root >= -1 && a.root < 16)
                                             : (a.root >= 0 && a.root <= 100000),
         "Invalid layer asset root");
}
} // namespace
LayerAsset LayerAsset::capture(Runtime &effect, std::string name, int layer, int component) {
    LayerAsset a;
    a.name = std::move(name);
    a.kind = (effect.selectedPreset() == 18 || effect.selectedPreset() == 21)
                 ? LayerAssetKind::Components
                 : LayerAssetKind::Recovered;
    a.root = a.kind == LayerAssetKind::Components ? component : layer;
    if (a.kind == LayerAssetKind::Components && component)
        effect.components().at(component);
    std::ostringstream out;
    effect.writeScene(out);
    a.scene = out.str();
    validate(a);
    return a;
}
void LayerAsset::loadPreview(Runtime &effect) const {
    validate(*this);
    std::istringstream input(scene);
    effect.readScene(input);
    if (kind == LayerAssetKind::Components) {
        need(effect.selectedPreset() == 18 || effect.selectedPreset() == 21,
             "Component asset contains another scene type");
        if (root)
            effect.components().at(root);
        effect.parameters().componentPreviewId = root;
    } else {
        need(effect.selectedPreset() != 18 && effect.selectedPreset() != 21,
             "Recovered asset contains a component workspace");
        if (root >= 0) {
            auto layers = effect.authoring().layers(effect.selectedPreset());
            need(std::any_of(layers.begin(), layers.end(),
                             [&](const auto &p) { return p.first == root; }),
                 "Layer root does not exist in source scene");
        }
        effect.parameters().layerPreview = root;
    }
}
void LayerAsset::updateFromPreview(Runtime &effect) {
    if (kind == LayerAssetKind::Components && root)
        effect.components().at(root);
    std::ostringstream out;
    effect.writeScene(out);
    auto candidate = *this;
    candidate.scene = out.str();
    validate(candidate);
    scene = std::move(candidate.scene);
}
void LayerAsset::write(std::ostream &out) const {
    validate(*this);
    out << "NEONLAYER 1\n"
        << int(kind) << ' ' << root << ' ' << std::quoted(name) << ' ' << std::quoted(notes) << '\n'
        << std::quoted(scene) << '\n';
    need(bool(out), "Could not save layer asset");
}
LayerAsset LayerAsset::read(std::istream &in) {
    LayerAsset a;
    std::string magic;
    int version, kind;
    need(bool(in >> magic >> version >> kind >> a.root >> std::quoted(a.name) >>
              std::quoted(a.notes) >> std::quoted(a.scene)) &&
             magic == "NEONLAYER" && version == 1,
         "Invalid layer asset document");
    a.kind = LayerAssetKind(kind);
    validate(a);
    in >> std::ws;
    need(in.eof(), "Unexpected layer asset data");
    return a;
}
LayerAsset LayerAsset::open(const std::filesystem::path &file) {
    need(std::filesystem::file_size(file) <= 2200000, "Layer asset file is too large");
    std::ifstream in(file, std::ios::binary);
    need(bool(in), "Cannot open layer asset");
    return read(in);
}
void LayerAsset::save(const std::filesystem::path &file) const {
    validate(*this);
    std::ofstream out(file, std::ios::binary);
    need(bool(out), "Cannot write layer asset");
    write(out);
}
int importLayerAsset(Runtime &destination, const LayerAsset &asset, int target, int primitive) {
    // Work against a CPU-only copy. Any invalid graph, resource budget or nested payload
    // leaves the destination untouched; no graphics is allocated during import.
    std::ostringstream saved;
    destination.writeScene(saved);
    std::istringstream read(saved.str());
    // Runtime data directory is exposed explicitly, not inferred from executable paths.
    Runtime candidate(destination.dataPath());
    candidate.readScene(read);
    if (candidate.selectedPreset() != 18 && candidate.selectedPreset() != 21)
        candidate.selectPreset(18);
    Runtime source(destination.dataPath());
    asset.loadPreview(source);
    auto &scene = candidate.components();
    auto &bank = candidate.responses();
    int imported = 0;
    if (asset.kind == LayerAssetKind::Components && !target && primitive < 0) {
        std::set<int> needed;
        std::function<void(int)> visit = [&](int id) {
            if (!needed.insert(id).second)
                return;
            auto &c = source.components().at(id);
            for (int input : {c.source, c.sourceB, c.sourceC})
                if (input >= 10000)
                    visit(input - 10000);
        };
        if (asset.root)
            visit(asset.root);
        else
            for (auto &c : source.components().items)
                visit(c.id);
        std::map<int, int> ids;
        for (auto &c : source.components().items)
            if (needed.count(c.id))
                ids[c.id] = scene.nextId++;
        for (auto c : source.components().items)
            if (needed.count(c.id)) {
                int old = c.id;
                c.id = ids.at(old);
                c.name = asset.name + " / " + c.name;
                if (c.name.size() > 128)
                    c.name.resize(128);
                for (auto p : {&c.source, &c.sourceB, &c.sourceC})
                    if (*p >= 10000)
                        *p = 10000 + ids.at(*p - 10000);
                if (c.kind == ComponentKind::Vector && c.profile == 3 && c.vectorDocument.empty()) {
                    std::ostringstream vectors;
                    neon::vectors::writeObjects(vectors, source.vectorObjects());
                    c.vectorDocument = vectors.str();
                }
                if (!asset.root || old == asset.root) {
                    imported = c.id;
                    if (asset.root)
                        c.visible = true;
                }
                scene.items.push_back(std::move(c));
            }
        for (auto sequence : source.responses().sequences) {
            sequence.targets.erase(std::remove_if(sequence.targets.begin(), sequence.targets.end(),
                                                  [&](auto &t) {
                                                      if (t.key.rfind("component.", 0) != 0)
                                                          return true;
                                                      size_t dot = t.key.find('.', 10);
                                                      if (dot == std::string::npos)
                                                          return true;
                                                      int id =
                                                          std::stoi(t.key.substr(10, dot - 10));
                                                      if (!ids.count(id))
                                                          return true;
                                                      t.key = "component." +
                                                              std::to_string(ids.at(id)) +
                                                              t.key.substr(dot);
                                                      return false;
                                                  }),
                                   sequence.targets.end());
            if (!sequence.targets.empty()) {
                sequence.name = asset.name + " / " + sequence.name;
                if (sequence.name.size() > 128)
                    sequence.name.resize(128);
                bank.sequences.push_back(std::move(sequence));
            }
        }
        bank.rebuild();
    } else {
        Component layer = Component::make(ComponentKind::Layer, scene.nextId++);
        layer.name = asset.name;
        layer.layerScene = asset.scene;
        layer.layerOutput = asset.kind == LayerAssetKind::Recovered ? asset.root : -1;
        layer.layerComponent = asset.kind == LayerAssetKind::Components ? asset.root : -1;
        layer.visible = !target && primitive < 0;
        imported = layer.id;
        if (target) {
            auto at = std::find_if(scene.items.begin(), scene.items.end(),
                                   [&](auto &c) { return c.id == target; });
            need(at != scene.items.end(), "Select a component to receive the material");
            scene.items.insert(at, layer);
            scene.at(target).source = 10000 + layer.id;
            imported = target;
        } else {
            scene.items.push_back(layer);
            if (primitive >= 0) {
                auto c = Component::make(ComponentKind::Primitive, scene.nextId++);
                c.profile = primitive;
                c.source = 10000 + layer.id;
                c.name = std::string("Textured ") + primitiveName(primitive);
                imported = c.id;
                scene.items.push_back(c);
            }
        }
    }
    need(scene.validate().empty(), scene.validate().c_str());
    std::ostringstream document;
    candidate.writeScene(document);
    Runtime validation(destination.dataPath());
    std::istringstream check(document.str());
    validation.readScene(check);
    if (destination.selectedPreset() != 18 && destination.selectedPreset() != 21)
        destination.selectPreset(18);
    destination.components() = std::move(scene);
    destination.responses() = std::move(bank);
    destination.parameters().componentPreviewId = 0;
    destination.parameters().layerPreview = -1;
    return imported;
}
} // namespace neon::effects
