#include "collection_title.h"
#include "effect_runtime.h"
#include "presentation_assets.h"
#include "title_presenter.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cmath>
#include <set>
#include <sstream>
namespace mino {
namespace {
const CoreInfo info_ = {"title", "Minotaur Collection", 1024, 768, 4.f / 3, 4.f / 3, ""};
bool widgetRead(const std::string &name, std::vector<uint8_t> &bytes) {
    return readPresentationAsset("title/" + name, bytes);
}
bool effectRead(const std::filesystem::path &name, std::vector<uint8_t> &bytes) {
    return readPresentationAsset("neon/" + name.generic_string(), bytes);
}
} // namespace
struct CollectionTitle::Impl {
    Host *host = nullptr;
    SurfaceInfo surface{};
    bool paused = false, buttons = false, begun = false;
    std::set<uint32_t> pointers;
    std::unique_ptr<neon::widgets::TitleSequence> sequence;
    neon::widgets::TitlePresenter presenter;
    std::unique_ptr<neon::effects::Runtime> background;
};
CollectionTitle::CollectionTitle() : impl_(std::make_unique<Impl>()) {}
CollectionTitle::~CollectionTitle() = default;
const CoreInfo &CollectionTitle::info() const {
    return info_;
}
bool CollectionTitle::init(Host &host, const SurfaceInfo &surface) {
    auto &i = *impl_;
    i.host = &host;
    i.surface = surface;
    try {
        auto resolve = [](const char *name) -> void * {
            return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
        };
        if (!neon::widgets::Renderer::loadGlesEntryPoints(resolve) ||
            !neon::effects::Runtime::loadGlesEntryPoints(resolve))
            return false;
        std::vector<uint8_t> bytes;
        if (!widgetRead("Minotaur-Title.widgetml", bytes))
            return false;
        i.sequence = std::make_unique<neon::widgets::TitleSequence>(
            neon::widgets::Value::parse(std::string(bytes.begin(), bytes.end())));
        i.presenter.assets(*i.sequence, widgetRead);
        i.background =
            std::make_unique<neon::effects::Runtime>(std::filesystem::path{}, effectRead);
        if (!readPresentationAsset("Vaporwave.neongles", bytes))
            return false;
        std::istringstream scene(std::string(bytes.begin(), bytes.end()));
        i.background->readScene(scene);
        return true;
    } catch (const std::exception &e) {
        host.log("Collection title unavailable: %s", e.what());
        shutdown();
        return false;
    }
}
void CollectionTitle::resize(const SurfaceInfo &surface) {
    impl_->surface = surface;
}
void CollectionTitle::pads(const PadState *pads, int count) {
    auto &i = *impl_;
    i.buttons = false;
    for (int n = 0; pads && n < count; ++n)
        i.buttons |= pads[n].connected && pads[n].buttons != 0;
}
void CollectionTitle::touch(const TouchEvent &event) {
    auto &i = *impl_;
    if (event.phase == TouchPhase::Down)
        i.pointers.insert(event.id);
    else if (event.phase == TouchPhase::Cancel)
        i.pointers.clear();
    else if (event.phase == TouchPhase::Up)
        i.pointers.erase(event.id);
    if (i.sequence && i.begun && !i.paused)
        i.sequence->input(i.buttons, !i.pointers.empty());
}
void CollectionTitle::frame(double elapsed) {
    auto &i = *impl_;
    if (!i.sequence || !i.background)
        return;
    try {
        if (!i.begun) {
            i.sequence->begin(i.buttons, !i.pointers.empty());
            i.begun = true;
        }
        if (!i.paused)
            i.sequence->input(i.buttons, !i.pointers.empty());
        const int w = i.surface.width, h = i.surface.height;
        if (w <= 0 || h <= 0)
            return;
        const auto viewport = i.sequence->presentationViewport(w, h);
        const int viewW = int(viewport.width), viewH = int(viewport.height);
        const int x = int(viewport.x), y = int(viewport.y);
        if (viewW != w || viewH != h) {
            glBindFramebuffer(GL_FRAMEBUFFER, i.surface.framebuffer);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        const double dt = i.paused ? 0 : elapsed;
        i.sequence->update(dt, {viewW, viewH, 0, {}, {}});
        const neon::widgets::RenderTarget target{
            i.surface.framebuffer,
            w,
            h,
            {double(x), double(y), double(viewW), double(viewH)},
            {},
            0};
        i.presenter.render(*i.sequence, target, [&](const auto &) {
            i.background->frame(dt, {i.surface.framebuffer, w, h, {x, y, viewW, viewH}, {}, 0});
        });
        if (i.sequence->takeLaunchRequest())
            i.host->requestQuit();
    } catch (const std::exception &e) {
        i.host->log("Collection title rendering failed: %s", e.what());
        i.host->requestQuit();
    }
}
uint64_t CollectionTitle::framePeriodNs() const {
    const double rate = impl_->sequence ? impl_->sequence->maxFrameRate() : 0;
    return rate >= 1 && rate <= 1000 ? uint64_t(std::ceil(1e9 / rate)) : 0;
}
void CollectionTitle::pause(bool paused) {
    impl_->paused = paused;
}
bool CollectionTitle::back() {
    auto &i = *impl_;
    if (i.sequence && i.begun) {
        i.sequence->input(false, false);
        i.sequence->input(true, false);
    }
    return true;
}
void CollectionTitle::abandonGraphics() {
    auto &i = *impl_;
    i.presenter.abandonGraphics();
    if (i.background)
        i.background->abandonGraphics();
}
void CollectionTitle::shutdown() {
    auto &i = *impl_;
    if (SDL_GL_GetCurrentContext()) {
        i.presenter.release();
        if (i.background)
            i.background->releaseGraphics();
    } else
        abandonGraphics();
    i.background.reset();
    i.sequence.reset();
    i.pointers.clear();
}
} // namespace mino
