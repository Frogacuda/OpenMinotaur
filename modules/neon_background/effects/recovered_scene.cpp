#include "recovered_scene.h"
#include <algorithm>
#include <fstream>
#include <queue>
#include <stdexcept>
namespace neon::pc {
RecoveredScene::RecoveredScene(const std::filesystem::path &data, const std::string &name,
                               bool inspectGraph, const AssetReader &reader)
    : preset(PresetPlayback::load(data / "presets" / (name + ".grpreset"),
                                  data / "reference" / (name + ".routes.tsv"), reader)) {
    auto paletteFile = assetStream(data / "reference/pob-palette.bin", reader);
    auto maskFile = assetStream(data / "reference/shader-samplers.bin", reader);
    if (!paletteFile.read((char *)palette.data(), sizeof palette) ||
        !maskFile.read((char *)masks.data(), sizeof masks))
        throw std::runtime_error("Missing recovered scene metadata");
    auto cameraFile = assetStream(data / "reference/neutral-spatial-camera.bin", reader);
    if (!cameraFile.read((char *)neutralView.data(), 64))
        throw std::runtime_error("Missing recovered particle camera");
    for (size_t i = 0; i < constants.size(); i++) {
        auto f =
            assetStream(data / "reference/shaders" / (std::to_string(i) + ".constants"), reader);
        f >> constants[i];
    }
    if (!inspectGraph)
        return;
    std::queue<int> todo;
    todo.push(preset.outputStack);
    while (!todo.empty()) {
        int stack = todo.front();
        todo.pop();
        if (reachable[stack])
            continue;
        reachable[stack] = true;
        if (preset.stacks[stack].nodes.empty())
            throw std::runtime_error("Visible scene reads an empty stack");
        for (auto index : preset.stacks[stack].nodes) {
            auto &n = preset.nodes[index];
            auto &i = n.saved.integers;
            int shader = -1;
            std::array<int, 3> inputs{};
            if (n.type == 9) {
                shader = i[2];
                inputs = {i[5], i[3], i[6]};
            } else if (n.type == 6) {
                shader = i[2];
                inputs = {31 + stack, i[3], 0};
            } else if (n.type == 1) {
                shader = i[9];
                inputs = {i[6], i[7], 0};
                if (i[14])
                    throw std::runtime_error("POB object mesh is not recovered for this scene");
                particles.emplace(index, ParticlePlayback{});
            } else if (n.type == 19) {
                shader = i[6];
                inputs = {i[4], i[5], i[8]};
            } else if (n.type == 15) {
                shader = i[15];
                inputs = {i[3], i[4], i[12]};
                tunnels.emplace(index, TunnelPlayback{});
            } else if (n.type != 0 && n.type != 7 && n.type != 12) {
                bool overwritten = false;
                for (auto later : preset.stacks[stack].nodes)
                    if (later > index && preset.nodes[later].type == 9)
                        overwritten = true;
                if (!overwritten)
                    throw std::runtime_error("Unsupported module in visible scene");
            }
            if (shader >= 0) {
                if (shader >= int(masks.size()))
                    throw std::runtime_error("Invalid scene shader");
                for (int s = 0; s < 3; s++)
                    if (masks[shader] & (1 << s)) {
                        auto b = originalTextureBinding(inputs[s]);
                        if (b.kind == TextureSourceKind::Stack)
                            todo.push(b.index);
                        else if (b.kind == TextureSourceKind::Retained)
                            throw std::runtime_error(
                                "Unresolved retained sampler in visible scene");
                    }
            }
        }
    }
    stages.fill(preset.outputStack);
    stages[0] = preset.outputStack;
    std::vector<int> spatial;
    for (int stack = 0; stack < 16; stack++)
        if (reachable[stack])
            for (auto index : preset.stacks[stack].nodes)
                if (preset.nodes[index].type == 1 || preset.nodes[index].type == 15 ||
                    preset.nodes[index].type == 19)
                    spatial.push_back(stack);
    if (!spatial.empty())
        stages[1] = spatial.back();
    else
        for (int stack = 0; stack < 16; stack++)
            if (reachable[stack] && stack != stages[0]) {
                stages[1] = stack;
                break;
            }
    if (spatial.size() > 1)
        stages[2] = spatial.front();
    else
        for (int stack = 0; stack < 16; stack++)
            if (reachable[stack] && stack != stages[0] && stack != stages[1]) {
                stages[2] = stack;
                break;
            }
    for (int stack = 0; stack < 16; stack++)
        if (reachable[stack] && stack != stages[0] && stack != stages[1] && stack != stages[2]) {
            stages[3] = stack;
            break;
        }
}
void RecoveredScene::reset() {
    preset.reset();
    for (auto &t : tunnels)
        t.second.reset();
    for (auto &p : particles)
        p.second.reset();
}
} // namespace neon::pc
