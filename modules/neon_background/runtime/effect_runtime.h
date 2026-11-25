#pragma once
#include "authoring_model.h"
#include "component_scene.h"
#include "geometry_model.h"
#include "overlay_text.h"
#include "read_bindings.h"
#include "screen_layout.h"
#include "vector/vector_scene.h"
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <vector>
namespace neon::effects {
struct Rect {
    int x = 0, y = 0, width = 0, height = 0;
}; // top-left pixels in destination
struct Destination {
    unsigned framebuffer = 0;
    int width = 0, height = 0;
    Rect viewport{}, exclude{};
    int clockwiseTurns =
        0; // host's resolved game presentation turn; exclude stays in display pixels
    // Exclusive hosts re-establish their own state after this pass. General
    // embedding preserves the caller's state by default.
    bool preserveHostState = true;
    // Opt in only when this pass precedes the host's opaque game/UI composition.
    // False retains the exclusion rectangle's existing destination pixels.
    bool clearDestination = false;
    // Batch flat bezel strips into one draw when this pass clears the viewport.
    bool batchBezel = false;
};
struct Parameters {
    ScreenFraming framing{};
    vectors::BeamStyle beam{};
    float yaw = 0, pitch = 0, exposure = 1;
    float hueShift = 0;                                   // Hue rotation in turns, 0..1.
    std::array<float, 5> studyControls{.35f, 1, 1, 1, 1}; // Phase, motion, radius, detail, UV zoom.
    bool studyPhaseOverride = false;
    float renderScale = 1; // explicit quality/cost control; destination stays native
    int vectorScene = 1;
    bool copyElision = true; // Disable for renderer equivalence diagnostics.
    bool skipTransparentMixerPasses = false;
    bool sourceOnly = false;
    int componentPreviewId = 0; // inspection only; zero presents the full component scene
    int nodePreview = -1;       // transient node inspection; never stored in scene files
    int layerPreview = -1;      // recovered stack inspection; -1 restores authored composition
    int previewStage = 0;       // 0 final, 1 spatial, 2 source, 3 auxiliary; inspection only
    float titleSize = 1.35f, titlePeriod = 18, titleHue = 0, titleFilter = .8f, titleOpacity = 1;
    int titleSprite = 0;
};
struct PresetInfo {
    int id;
    const char *name;
};
struct Statistics {
    unsigned drawCalls = 0, triangles = 0, targets = 0;
    unsigned layerCopies = 0;
    std::size_t targetBytes = 0, uploadedBytes = 0;
    unsigned droppedTicks = 0;
    unsigned textureBuilds = 0;
    uint64_t totalTextureBuilds = 0;
    unsigned imageUploads = 0;
    uint64_t totalImageUploads = 0;
    uint64_t shaderCompilations = 0, shaderCacheHits = 0;
    unsigned shapeBuilds = 0;
    uint64_t totalShapeBuilds = 0;
};
// Owns no window/context/game. Construct/update on CPU; render/release with the
// host's GLES 3 context current. Destroy before that context is destroyed.
class Runtime {
  public:
    explicit Runtime(const std::filesystem::path &data, pc::AssetReader reader = {});
    ~Runtime();
    void writeBaked(std::ostream &, double playbackRate = 1,
                    const std::vector<std::string> *exports = nullptr);
    static std::unique_ptr<Runtime> loadBaked(std::istream &,
                                              const std::filesystem::path &data = {},
                                              pc::AssetReader reader = {},
                                              double *playbackRate = nullptr);
    std::vector<std::string> bakedResources(const std::vector<int> &additionalShaders = {},
                                            const std::vector<int> &additionalTextures = {});

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    // Borrowed parent-rendered texture, refreshed before rendering the child.
    void clearExternalLayers();
    void setExternalLayer(int layer, unsigned texture, bool expanded = true, bool flipY = false);
    void advance(double elapsedSeconds);
    // Advance fixed ticks and render each feedback update, bounded to four ticks.
    // Use frameSampled when the host requires one render per presentation.
    void frame(double elapsedSeconds, const Destination &);
    // Advance clocks by elapsed time and execute each feedback graph once.
    void frameSampled(double elapsedSeconds, const Destination &);
    // Recompose completed targets without evaluating or rendering the effect graph.
    bool presentCached(const Destination &);
    // Inserts consume emission slots; existing particles retain their sprite.
    bool insertSprite(int node, int atlasFrame, unsigned count, float speed = 1,
                      int colourSource = -1);
    void setSpriteColours(const std::map<int, std::array<float, 3>> &);
    void reset();
    void render(const Destination &);
    // Call after game composition for native-resolution text. Empty by default.
    void renderOverlays(const Destination &);
    OverlayTextLayer &overlays();
    void releaseGraphics();
    void abandonGraphics();
    // Warm finite shader alternatives with the host context current; preserve GL state.
    bool prepareShaderChoices(const std::string &target, const std::vector<float> &choices);
    void setShaderCacheDirectory(const std::filesystem::path &);
    // Borrowed native gameplay texture; the host owns its lifetime and sampling state.
    void setGameplayTexture(unsigned texture, int width, int height);
    void setVectorObjects(std::vector<vectors::Object>);
    const std::vector<vectors::Object> &vectorObjects() const;
    Parameters &parameters();
    pc::GeometryStudy &geometryStudy();
    pc::AuthoringModel &authoring();
    // Non-owning rendered source, valid until its payload changes or resources release.
    Runtime *layerRuntime(int component);
    ComponentScene &components();
    ResponseBank &responses();
    std::vector<ResponseParameter> responseParameters();
    bool triggerEvent(const std::string &, float strength = 1);
    void setInput(const std::string &, float value);
    void clearInput(const std::string &);
    ReadBindings &readBindings();
    ReadReport observe(const ReadSnapshot &);
    ReadReport inspect(const ReadSource &);
    const ReadReport &lastReadReport() const;
    void resetReads();
    // Portable editor/game interchange. Loading is CPU-only and transactional;
    // starts playback at tick zero and returns the saved playback multiplier.
    void writeScene(std::ostream &, double playbackRate = 1,
                    const std::filesystem::path &assetFolder = {}) const;
    void saveSceneFile(const std::filesystem::path &, double playbackRate = 1) const;
    double loadSceneFile(const std::filesystem::path &);
    const ImageAssets &imageAssets() const;
    void setImageAssets(ImageAssets);
    double readScene(std::istream &);
    const Statistics &statistics() const;
    double time() const;
    static const std::vector<PresetInfo> &presets();
    static bool loadGlesEntryPoints(void *(*resolve)(const char *));
    void selectPreset(int id);
    int selectedPreset() const;
    const std::filesystem::path &dataPath() const;
    bool validateShaderLibrary(std::ostream &);

  private:
    struct PlaybackOnly {};
    Runtime(const std::filesystem::path &, pc::AssetReader, PlaybackOnly);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace neon::effects
