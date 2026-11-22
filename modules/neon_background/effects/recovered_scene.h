#pragma once
#include "pc_particles.h"
#include "pc_preset.h"
#include "pc_tunnel.h"
#include <map>
#include <set>
namespace neon::pc {
struct RecoveredScene {
    PresetPlayback preset;
    std::array<bool, 16> reachable{};
    std::array<int, 4> stages{};
    std::map<size_t, TunnelPlayback> tunnels;
    std::map<size_t, ParticlePlayback> particles;
    std::array<uint32_t, 256> palette{};
    std::array<uint8_t, 96> masks{};
    bool hasCubes = false;
    std::set<size_t> nonDrawingShapes;
    Matrix4 neutralView{};
    std::array<unsigned, 96> constants{};
    // Raw studies supply their established subset; complete scenes validate the graph.
    RecoveredScene() = default;
    RecoveredScene(const std::filesystem::path &, const std::string &, bool inspectGraph = true,
                   const AssetReader &reader = {});
    bool spatialCamera() const {
        if (hasCubes || !tunnels.empty() || !particles.empty())
            return true;
        for (const auto &node : preset.nodes)
            if (node.type == 19 && reachable[node.stack])
                return true;
        return false;
    }
    void rebuildGraph();
    void reset();
};
} // namespace neon::pc
