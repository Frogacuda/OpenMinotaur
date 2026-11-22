#include "gridrunner_widgets.h"
#include "bundled_assets.h"
#include "gridrunner_widget_document.h"
#include "widget_renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace mino {
namespace {
using namespace neon::widgets;
constexpr const char *pickupNames[] = {"Rear Fire", "Spread Fire", "Four-Way Fire", "Pod Sweep",
                                      "All-Around Fire", "Shield", "Time Warp", "Disease"};

int64_t integer(const PresentationState &state, const std::string &key) {
    const auto it = state.values.find(key);
    if (it != state.values.end())
        if (const auto *value = std::get_if<int64_t>(&it->second))
            return *value;
    return 0;
}

std::string scoreText(int64_t score) {
    std::string text = std::to_string(score);
    for (int index = int(text.size()) - 3; index > 0; index -= 3)
        text.insert(size_t(index), 1, ',');
    return text;
}

Value colour(const PresentationState &state, int source) {
    auto tint = state.colours.find(source);
    if (tint == state.colours.end())
        return Value::Array{1, 1, 1, 1};
    const auto &rgb = tint->second;
    return Value::Array{rgb[0], rgb[1], rgb[2], 1};
}

Value::Object item(const PresentationState &state, const PresentationState::CollectionItem &data) {
    return {{"id", data.id}, {"label", pickupNames[data.colourSource]},
            {"tint", colour(state, data.colourSource)}};
}

bool widgetAsset(const std::string &path, std::vector<uint8_t> &bytes) {
    return readBundledAsset("gridrunner", path, bytes);
}

struct OverlayLayout {
    Viewport viewport;
    RenderTarget target;
    double scale;
};

std::optional<OverlayLayout> overlayLayout(unsigned framebuffer, int width, int height, int x,
                                            int y, int gameplayWidth, int gameplayHeight,
                                            int turns) {
    if (turns != 0 || width <= height || gameplayWidth >= gameplayHeight || height <= 0)
        return {};
    const double units = height / 1000.;
    const double edge = std::max(0., (width - height * 16. / 9.) / 2.);
    const double rail = std::min(x - edge, width - edge - x - gameplayWidth);
    const double scale = std::min(1.16, (rail / units - 44) / 304);
    if (scale < .55)
        return {};
    const Rect gameplay{double(x), double(y), double(gameplayWidth), double(gameplayHeight)};
    return OverlayLayout{{width, height, 0, gameplay},
                         {framebuffer, width, height, {0, 0, double(width), double(height)}, gameplay},
                         scale};
}
} // namespace

struct GridrunnerWidgets::Impl {
    Runtime runtime;
    Renderer renderer;
    bool loaded = false, failed = false;
    int warmWidth = 0, warmHeight = 0;
    double warmScale = 0;
    uint64_t session = 0, sequence = 0;
    std::array<int64_t, 8> peak{};
    bool loggedCache = false;
    bool sampled = false, visible = false;
    std::array<int, 6> lastLayout{};

    void prepare(const OverlayLayout &layout);
};

GridrunnerWidgets::GridrunnerWidgets() : impl_(std::make_unique<Impl>()) {}
GridrunnerWidgets::~GridrunnerWidgets() = default;

void GridrunnerWidgets::Impl::prepare(const OverlayLayout &layout) {
    if (!loaded) {
        if (!Renderer::loadGlesEntryPoints([](const char *name) {
                return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            }))
            throw std::runtime_error("GLES sampler entry point unavailable");
        runtime.document(Value::parse(gridrunnerWidgetDocument));
        renderer.assets(runtime.document(), widgetAsset);
        loaded = true;
    }
    const auto &[viewport, target, scale] = layout;
    if (warmWidth == viewport.width && warmHeight == viewport.height && warmScale == scale)
        return;
    if (warmWidth == viewport.width && warmHeight == viewport.height)
        renderer.release();
    Runtime warm;
    warm.document(runtime.document());
    warm.observe({"gridrunner", 0, 1, {{"railScale", scale}}});
    renderer.warmCached(warm.update(0, viewport), target);
    // Warm every log height and the names absent from the full sample.
    Value::Array otherPickups;
    for (int count = 0; count < 5; ++count) {
        if (count > 0) {
            const int kind = count - 1;
            otherPickups.emplace_back(item({}, {uint64_t(kind), kind, kind}));
        }
        warm.observe({"gridrunner", 0, uint64_t(count + 2), {{"railScale", scale},
                       {"pickups", otherPickups}, {"effects", Value::Array{}}}});
        renderer.warmCached(warm.update(0, viewport), target);
    }
    warmWidth = viewport.width;
    warmHeight = viewport.height;
    warmScale = scale;
}

void GridrunnerWidgets::prepare(int width, int height, int x, int y, int gameplayWidth,
                                int gameplayHeight, int turns) {
    auto layout = overlayLayout(0, width, height, x, y, gameplayWidth, gameplayHeight, turns);
    if (!layout || impl_->failed)
        return;
    try {
        impl_->prepare(*layout);
    } catch (const std::exception &error) {
        SDL_Log("Gridrunner widget warm-up failed: %s", error.what());
        impl_->failed = true;
    }
}

bool GridrunnerWidgets::draw(const Core &core, bool refresh, unsigned framebuffer, int width,
                            int height, int x, int y, int gameplayWidth, int gameplayHeight,
                            int turns) {
    auto &i = *impl_;
    auto layout = overlayLayout(framebuffer, width, height, x, y, gameplayWidth, gameplayHeight,
                                turns);
    if (i.failed || !layout) {
        i.sampled = false;
        return false;
    }
    const auto &[viewport, target, scale] = *layout;
    const std::array<int, 6> geometry{width, height, x, y, gameplayWidth, gameplayHeight};
    try {
        if (!refresh && i.sampled && i.lastLayout == geometry) {
            if (i.visible)
                i.renderer.renderCached(i.runtime.frame(), target);
            return i.visible;
        }
        const auto state = core.widgetPresentationState();
        i.sampled = true;
        i.visible = state.gameplay;
        i.lastLayout = geometry;
        if (!i.visible)
            return false;
        i.prepare(*layout);
        if (i.session != state.session) {
            i.session = state.session;
            i.peak.fill(0);
        }
        const auto checkpoint = integer(state, "widgets.nextCheckpoint");
        Value::Object values{{"railScale", scale},
                             {"checkpointVisible", checkpoint != 0},
                             {"nextCheckpoint", "Grid " + std::to_string(checkpoint)},
                             {"checkpointScore", state.values.count("widgets.checkpointScore")
                                                     ? scoreText(integer(state, "widgets.checkpointScore"))
                                                     : "--"}};
        values["stats"] = Value::Array{
            Value::Object{{"id", "ships-lost"}, {"label", "Ships Lost"},
                          {"value", integer(state, "widgets.shipsLost")}},
            Value::Object{{"id", "deathless"}, {"label", "Deathless Grids"},
                          {"value", std::to_string(integer(state, "widgets.deathless")) + "/5"}},
            Value::Object{{"id", "swarmers"}, {"label", "Swarmers Shot"},
                          {"value", std::to_string(integer(state, "widgets.swarmers")) + "/50"}},
            Value::Object{{"id", "deflections"}, {"label", "Deflections"},
                          {"value", std::to_string(integer(state, "widgets.deflections")) + "/501"}}};
        auto &pickups = values["pickups"].array();
        auto log = state.collections.find("widgets.pickups");
        if (log != state.collections.end())
            for (const auto &pickup : log->second)
                pickups.emplace_back(item(state, pickup));
        auto &effects = values["effects"].array();
        auto active = state.collections.find("widgets.effects");
        std::array<bool, 8> seen{};
        if (active != state.collections.end())
            for (const auto &effect : active->second) {
                const auto kind = size_t(effect.colourSource);
                seen[kind] = true;
                i.peak[kind] = std::max({i.peak[kind], effect.value,
                                        integer(state, "widgets.duration." + std::to_string(kind))});
                auto row = item(state, effect);
                row["remaining"] = effect.value;
                row["duration"] = i.peak[kind];
                // The simulation subtracts two units at 60 ticks per second.
                const auto tenths = (effect.value + 11) / 12;
                row["timeText"] = std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
                effects.emplace_back(std::move(row));
            }
        for (size_t kind = 0; kind < seen.size(); ++kind)
            if (!seen[kind])
                i.peak[kind] = 0;
        i.runtime.observe({"gridrunner", state.session, ++i.sequence, std::move(values)});
        i.renderer.renderCached(i.runtime.update(0, viewport), target);
        if (!i.loggedCache) {
            const auto &stats = i.renderer.statistics();
            SDL_Log("Gridrunner widgets: %zu raster bakes and %zu texture uploads after warm-up",
                    stats.rasterBakes, stats.textureUploads);
            i.loggedCache = true;
        }
        return true;
    } catch (const std::exception &error) {
        SDL_Log("Gridrunner widget prototype unavailable: %s", error.what());
        i.failed = true;
        return false;
    }
}

void GridrunnerWidgets::release() {
    impl_->renderer.release();
    impl_ = std::make_unique<Impl>();
}

void GridrunnerWidgets::abandonGraphics() {
    impl_->renderer.abandonGraphics();
    impl_->warmWidth = impl_->warmHeight = 0;
    impl_->sampled = false;
}
} // namespace mino
