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
};
struct Parameters {
    ScreenFraming framing{};
    vectors::BeamStyle beam{};
    float yaw = 0, pitch = 0, exposure = 1;
    float renderScale = 1; // explicit quality/cost control; destination stays native
    int vectorScene = 1;
    bool sourceOnly = false;
    int componentPreviewId = 0; // inspection only; zero presents the full component scene
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
    std::size_t targetBytes = 0, uploadedBytes = 0;
    unsigned droppedTicks = 0;
    unsigned textureBuilds = 0;
    uint64_t totalTextureBuilds = 0;
    unsigned shapeBuilds = 0;
    uint64_t totalShapeBuilds = 0;
};
// Owns no window/context/game. Construct/update on CPU; render/release with the
// host's GLES 3 context current. Destroy before that context is destroyed.
class Runtime {
  public:
    explicit Runtime(const std::filesystem::path &data, pc::AssetReader reader = {});
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    void advance(double elapsedSeconds);
    // Recommended game-host path: advance fixed ticks and render each feedback
    // update, bounded to four ticks, then leave the final image in destination.
    void frame(double elapsedSeconds, const Destination &);
    void reset();
    void render(const Destination &);
    // Call after game composition for native-resolution text. Empty by default.
    void renderOverlays(const Destination &);
    OverlayTextLayer &overlays();
    void releaseGraphics();
    void abandonGraphics();
    void setVectorObjects(std::vector<vectors::Object>);
    const std::vector<vectors::Object> &vectorObjects() const;
    Parameters &parameters();
    pc::GeometryStudy &geometryStudy();
    pc::AuthoringModel &authoring();
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
    void writeScene(std::ostream &, double playbackRate = 1) const;
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace neon::effects
