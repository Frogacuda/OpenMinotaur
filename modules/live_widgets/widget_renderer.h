#pragma once
#include "widget_runtime.h"
#include <filesystem>
#include <functional>
namespace neon::widgets {
struct RenderTarget {
    unsigned framebuffer = 0;
    int width = 1280, height = 720;
    Rect viewport{0, 0, 1280, 720}, gameplay{};
    int turns = 0;
};
struct RenderStatistics {
    size_t draws = 0, triangles = 0, strokes = 0, uploads = 0, textLayouts = 0, dropped = 0;
    size_t rasterBakes = 0, textureUploads = 0;
};
struct BeamAppearance {
    float brightness = .55f;
    float halo = .30f;
    float spread = 1.75f;
    float whiteHot = .10f;
    float widthScale = .70f;
};
enum class BeamProfile { Uniform, Gameplay };
// Optional GLES3 backend. CPU semantics can be built and tested without it.
using ResourceReader = std::function<bool(const std::string &, std::vector<uint8_t> &)>;
class Renderer {
  public:
    explicit Renderer(BeamAppearance appearance = {}, BeamProfile profile = BeamProfile::Uniform);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    static bool loadGlesEntryPoints(void *(*resolve)(const char *));
    void assets(const Value &document, const std::filesystem::path &folder);
    // Resource names are relative to the widget bundle; no extraction is required.
    void assets(const Value &document, ResourceReader reader);
    // Use only after the owning GL context has been lost.
    void abandonGraphics();
    void render(const Frame &, const RenderTarget &);
    // Cached overlay path for single-line ASCII text and inline vector artwork.
    void renderCached(const Frame &, const RenderTarget &);
    void warmCached(const Frame &, const RenderTarget &);
    void release();
    const RenderStatistics &statistics() const;

  private:
    struct Impl;
    void renderFrame(const Frame &, const RenderTarget &, bool cached, bool warm = false);
    std::unique_ptr<Impl> impl_;
};
} // namespace neon::widgets
