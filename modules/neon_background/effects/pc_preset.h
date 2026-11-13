#pragma once
#include "asset_reader.h"
#include "pc_animation.h"
#include "pc_geometry.h"
#include <filesystem>
#include <string>
namespace neon::pc {
struct ParameterRoute {
    size_t node;
    bool floating;
    int slot, channel;
    float minimum, maximum;
};
struct PresetNode {
    int stack, type;
    unsigned inputMode;
    ShapeSettings saved, effective;
    Channels channels;
    Values values;
};
struct PresetStack {
    std::vector<size_t> nodes;
    Sequencer clock;
    float tempo = 0;
};
// Executes saved controller routes without inventing values for missing sources.
// Renderers consume effective nodes in their original stack/node order.
class PresetPlayback {
  public:
    static PresetPlayback load(const std::filesystem::path &, const std::filesystem::path &routes,
                               const AssetReader &reader = {});
    void tick();
    void reset();
    bool retainedInJukebox(const ParameterRoute &) const;
    std::vector<PresetNode> nodes;
    std::array<PresetStack, 16> stacks;
    std::vector<ParameterRoute> routes;
    std::vector<size_t> unresolvedRoutes;
    unsigned ticks = 0;
    int outputStack = 0; // Saved header +0x90; final presentation uses selector 31+this.
};
} // namespace neon::pc
