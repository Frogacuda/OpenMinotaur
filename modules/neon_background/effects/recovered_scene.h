#pragma once
#include "pc_particles.h"
#include "pc_preset.h"
#include "pc_tunnel.h"
#include <map>
namespace neon::pc {
struct RecoveredScene {
    PresetPlayback preset;
    std::array<bool, 16> reachable{};
    std::array<int, 4> stages{};
    std::map<size_t, TunnelPlayback> tunnels;
    std::map<size_t, ParticlePlayback> particles;
    std::array<uint32_t, 256> palette{};
    std::array<uint8_t, 92> masks{};
    Matrix4 neutralView{};
    std::array<unsigned, 92> constants{};
    // Raw studies supply their established subset; complete scenes validate the graph.
    RecoveredScene(const std::filesystem::path &, const std::string &, bool inspectGraph = true,
                   const AssetReader &reader = {});
    void reset();
};
} // namespace neon::pc
