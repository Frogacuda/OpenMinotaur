// Creator-side scene preparation. The game consumes only the generated playback asset.
#include "effect_runtime.h"
#include "live_rig.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
using namespace neon::effects;
int main(int argc, char **argv) {
    try {
        if (argc < 4 || argc > 5)
            throw std::runtime_error("Usage: neon_scene_bake DATA SCENE OUTPUT [HARNESS.nhml]");
        Runtime scene(argv[1]);
        std::ifstream input(argv[2]);
        double rate = scene.readScene(input);
        RigProgram harness;
        if (argc == 5) {
            std::ifstream source(argv[4]);
            harness = RigProgram::read(source);
        }
        for (const auto &insertion : harness.insertions)
            for (const auto &[source, frame] : insertion.frames)
                if (!scene.insertSprite(insertion.node, frame, 1))
                    throw std::runtime_error("NHML sprite insertion target/frame is unavailable");
        if (!harness.insertions.empty())
            scene.reset();
        const auto descriptors = scene.responseParameters();
        std::set<std::string> exported;
        auto target = [&](const auto &r) {
            if (std::none_of(descriptors.begin(), descriptors.end(),
                             [&](const auto &p) { return p.key == r.target; }))
                throw std::runtime_error("NHML target is unavailable in this scene: " + r.target);
            exported.insert(r.target);
        };
        for (const auto &[name, p] : harness.parameters)
            target(p);
        for (const auto &r : harness.values)
            target(r);
        for (const auto &r : harness.indexed)
            target(r);
        for (const auto &r : harness.states)
            target(r);
        for (const auto &r : harness.steps)
            target(r);
        for (const auto &r : harness.fadeIndices)
            target(r);
        for (const auto &r : harness.pulses)
            target(r);
        for (const auto &r : harness.counters)
            target(r);
        for (const auto &r : harness.cycles)
            target(r);
        for (const auto &r : harness.envelopes)
            target(r);
        auto shaderParameter = [&](const std::string &target) {
            for (const auto &[name, p] : harness.parameters)
                if (p.target == target)
                    return p.unit == "shader-id";
            return false;
        };
        auto scalar = [&](const auto &rule) {
            if (shaderParameter(rule.target))
                throw std::runtime_error(
                    "Use an explicit select statement for shader-id parameter: " + rule.target);
        };
        for (const auto &r : harness.values)
            scalar(r);
        for (const auto &r : harness.pulses)
            scalar(r);
        for (const auto &r : harness.counters)
            scalar(r);
        for (const auto &r : harness.cycles)
            scalar(r);
        for (const auto &r : harness.envelopes)
            scalar(r);
        std::vector<int> shaderChoices;
        for (const auto &r : harness.states)
            if (shaderParameter(r.target) || r.target.find(".i.2") != std::string::npos) {
                auto add = [&](float value) {
                    if (!std::isfinite(value) || value < 0 || value > 95 ||
                        value != std::floor(value))
                        throw std::runtime_error("Invalid NHML shader-id choice");
                    shaderChoices.push_back(int(value));
                };
                add(r.baseline);
                for (const auto &c : r.choices)
                    add(c.value);
            }
        for (const auto &r : harness.steps)
            scalar(r);
        for (const auto &r : harness.fadeIndices) {
            shaderChoices.push_back(int(r.baseline));
            for (double choice : r.entries)
                shaderChoices.push_back(int(choice));
        }
        std::vector<int> textureChoices;
        for (const auto &r : harness.indexed) {
            scalar(r);
            bool textureParameter = false;
            for (const auto &[name, p] : harness.parameters)
                if (p.target == r.target)
                    textureParameter = p.unit == "texture-selector";
            if (textureParameter) {
                if (r.loopStep != 0)
                    throw std::runtime_error(
                        "Texture indexed mappings cannot shift selector IDs per loop");
                auto add = [&](double value) {
                    if (value != std::floor(value) || value < 1 || value > 350 ||
                        (value >= 31 && value < 48))
                        throw std::runtime_error(
                            "Indexed texture choices must be literal or root selectors");
                    textureChoices.push_back(int(value));
                };
                add(r.baseline);
                for (double value : r.entries)
                    add(value);
            }
        }
        const auto resources = scene.bakedResources(shaderChoices, textureChoices);
        std::vector<std::string> exports(exported.begin(), exported.end());
        std::ofstream output(argv[3], std::ios::binary);
        scene.writeBaked(output, rate, &exports);
        output.close();
        std::ifstream prepared(argv[3], std::ios::binary);
        auto verify = Runtime::loadBaked(prepared);
        if (verify->responseParameters().size() != exports.size())
            throw std::runtime_error("Baked export registry differs");
        std::ofstream manifest(std::string(argv[3]) + ".json");
        manifest << std::setprecision(9);
        manifest << "{\n\"format\":\"NEONBK01\",\n\"resources\":[";
        bool first = true;
        for (const auto &resource : resources) {
            if (!first)
                manifest << ',';
            first = false;
            manifest << std::quoted(resource);
        }
        manifest << "],\n\"exports\":[";
        first = true;
        for (const auto &[name, p] : harness.parameters) {
            if (!first)
                manifest << ',';
            first = false;
            const auto descriptor = std::find_if(descriptors.begin(), descriptors.end(),
                                                 [&](const auto &d) { return d.key == p.target; });
            manifest << "{\"name\":" << std::quoted(name) << ",\"target\":" << std::quoted(p.target)
                     << ",\"unit\":" << std::quoted(p.unit) << ",\"minimum\":" << descriptor->low
                     << ",\"maximum\":" << descriptor->high
                     << ",\"baseline\":" << descriptor->baseline << '}';
        }
        manifest << "]\n}\n";
        if (!manifest)
            throw std::runtime_error("Could not write bake manifest");
        std::cout << argv[3] << ": " << resources.size() << " resources, " << exports.size()
                  << " exported parameters\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
