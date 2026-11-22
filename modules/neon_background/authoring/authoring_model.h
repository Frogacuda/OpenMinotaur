#pragma once
#include "image_assets.h"
#include "jukebox_expansion.h"
#include "recovered_scene.h"
#include "response_animation.h"
#include "scene_authoring.h"
#include <tuple>
namespace neon::pc {
// CPU templates, descriptors and authored values shared by editor and backends.
// A renderer can bind its active playback to avoid ticking hidden templates.
struct GraphLayer {
    std::string preset;
    int source = 0, target = 0;
};
struct GraphAlias {
    std::string preset;
    int source = 0, target = 0;
};
struct GraphRecipe {
    std::vector<GraphLayer> layers;
    std::vector<GraphAlias> aliases;
    int output = 0, mixBase = -1, mixOutput = -1;
    std::vector<std::pair<int, int>> speedDrivers;
};
class AuthoringModel : public SceneAuthoring {
  public:
    neon::effects::ImageAssets images;
    void setImages(neon::effects::ImageAssets);
    std::filesystem::path data;
    AssetReader reader;
    PresetPlayback synth, cube, colour, zomg, planet, travelA, travelB;
    TunnelProfiles profiles;
    std::array<TunnelPlayback, 2> tunnels;
    uint64_t sceneTick = 0;
    std::array<std::vector<ReferenceControl>, 20 + addedJukeboxPresets.size()> edits;
    std::array<std::string, 20 + addedJukeboxPresets.size()> sceneNames;
    std::array<std::vector<float>, 20 + addedJukeboxPresets.size()> defaultValues;
    std::vector<RecoveredScene> extraScenes;
    std::map<int, neon::effects::ResponseBank> responses;
    bool responseTarget(int mode, const std::string &key) const;
    struct PlaybackOnly {};
    AuthoringModel(PlaybackOnly, const std::filesystem::path &path, AssetReader source)
        : data(path), reader(std::move(source)) {
        extraScenes.resize(graphIndex(lastJukeboxPreset) + 1);
    }
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
    std::string prepareMaterialEdit(int, const std::string &) override;
    std::vector<std::array<int, 3>> materialOverrides(int) const;
    void writeEdits(std::ostream &) const override;
    void readEdits(std::istream &) override;
    ShapeSettings authoredCube();
    void addAuthoringControls(int);
    void initializeExtraScenes();
    std::map<int, GraphRecipe> graphs;
    struct SpeedClock {
        double position = 0, phase = 0;
        float speed = 1;
        bool started = false;
    };
    std::map<std::tuple<int, size_t, int>, SpeedClock> groupClocks;
    void installGraph(int, const GraphRecipe &);
    void writeGraphs(std::ostream &) const;
    void readGraphs(std::istream &);
    void buildExtraControls(int);
    void initializeCycleControls(int onlyMode = -1);
    void applyCycles(int, PresetPlayback &);
    std::string validateCycles(int);
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
        int ordinal = presetOrdinal(mode);
        return ordinal >= 22   ? ordinal - 10
               : mode == 17    ? 2
               : ordinal >= 19 ? ordinal - 9
                               : ordinal - 7;
    }

  private:
    float authored(ReferenceControl &, double seconds, int mode);
    int externalMode_ = -1;
    PresetPlayback *external_ = nullptr;
};
} // namespace neon::pc
