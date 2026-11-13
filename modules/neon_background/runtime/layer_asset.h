#pragma once
#include "effect_runtime.h"
#include <filesystem>
namespace neon::effects {
enum class LayerAssetKind { Recovered, Components };
struct LayerAsset {
    std::string name = "Saved layer", notes, scene;
    LayerAssetKind kind = LayerAssetKind::Recovered;
    int root = -1;
    static LayerAsset capture(Runtime &, std::string, int recoveredLayer = -1, int component = 0);
    void loadPreview(Runtime &) const;
    void updateFromPreview(Runtime &);
    void write(std::ostream &) const;
    static LayerAsset read(std::istream &);
    static LayerAsset open(const std::filesystem::path &);
    void save(const std::filesystem::path &) const;
};
// Copies dependencies and remaps component/animation IDs. Read bindings remain host-owned.
// materialTarget: existing consumer ID; primitive: 0..4 creates a new textured primitive.
int importLayerAsset(Runtime &, const LayerAsset &, int materialTarget = 0, int primitive = -1);
} // namespace neon::effects
