#pragma once
#include "pc_preset.h"
#include <iosfwd>
#include <string>
#include <vector>
namespace neon::pc {
struct ReferenceChoice {
    int value;
    std::string label;
};
struct ReferenceControl {
    std::string key, label;
    float minimum = 0, maximum = 1, value = 0, live = 0;
    bool enabled = false;
    int motion = 0;
    float period = 4, depth = .1f, current = 0;
    bool driven = false, integer = false;
    std::vector<ReferenceChoice> choices;
    bool applicable = true;
    int shaderBank = 0, layer = -1;
    bool retained = false;
};
// Shared editor contract: no graphics API or native window dependency.
class SceneAuthoring {
  public:
    virtual ~SceneAuthoring() = default;
    virtual std::vector<ReferenceControl> &controls(int) = 0;
    virtual void clearEdits(int) = 0;
    virtual void refreshControls(int) = 0;
    virtual std::string sceneName(int) const = 0;
    virtual void sceneName(int, const std::string &) = 0;
    virtual std::vector<std::pair<int, std::string>> layers(int) const = 0;
    virtual std::string validateAuthoring(int) = 0;
    virtual void writeEdits(std::ostream &) const = 0;
    virtual void readEdits(std::istream &) = 0;
};
} // namespace neon::pc
