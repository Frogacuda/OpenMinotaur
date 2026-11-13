#pragma once
#include "recovered_scene.h"
#include "response_animation.h"
#include "scene_authoring.h"
namespace neon::pc {
// CPU templates, descriptors and authored values shared by editor and backends.
// A renderer can bind its active playback to avoid ticking hidden templates.
class AuthoringModel : public SceneAuthoring {
  public:
    std::filesystem::path data;
    AssetReader reader;
    PresetPlayback synth, cube, colour, zomg, planet, travelA, travelB;
    TunnelProfiles profiles;
    std::array<TunnelPlayback, 2> tunnels;
    uint64_t sceneTick = 0;
    std::array<std::vector<ReferenceControl>, 19> edits;
    std::array<std::string, 19> sceneNames;
    std::vector<RecoveredScene> extraScenes;
    std::map<int, neon::effects::ResponseBank> responses;
    bool responseTarget(int mode, const std::string &key) const;
    explicit AuthoringModel(const std::filesystem::path &, AssetReader reader = {});
    void bindPlayback(int mode, PresetPlayback *);
    void reset();
    void tick();
    float phase(int) const;
    std::vector<ReferenceControl> &controls(int) override;
    void clearEdits(int) override;
    void refreshControls(int) override;
    std::string sceneName(int) const override;
    void sceneName(int, const std::string &) override;
    std::vector<std::pair<int, std::string>> layers(int) const override;
    std::string validateAuthoring(int) override;
    void writeEdits(std::ostream &) const override;
    void readEdits(std::istream &) override;
    void readEdits(std::istream &, int activeMode);
    void addAuthoringControls(int);
    void initializeExtraScenes();
    std::vector<ShapeSettings> authoredScene(int);
    std::string validateScene(int);
    void refreshExtraLive();
    void tickExtraScenes();
    PresetPlayback &composition(int);
    void applyScene(int, ShapeSettings &, ShapeSettings &, ShapeSettings &);
    float sceneControl(int, const char *, float fallback = 0);
    void tickScenes();
    void refreshLive();
    void apply(int, ShapeSettings &, ShapeSettings &, ShapeSettings &, ShapeSettings &);
    static int graphIndex(int mode) {
        return mode == 17 ? 2 : mode >= 19 ? mode - 9 : mode - 7;
    }

  private:
    float authored(ReferenceControl &, double seconds, int mode);
    int externalMode_ = -1;
    PresetPlayback *external_ = nullptr;
};
} // namespace neon::pc
