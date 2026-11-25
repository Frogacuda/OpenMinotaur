#include "collection_title.h"
#include "display.h"
#include "effect_runtime.h"
#include "launcher.h"
#include "presentation_assets.h"
#include "shell_audio.h"
#include "title_presenter.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <glad/gl.h>
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

constexpr double kHandoffSeconds = .95;

float ease(float from, float to, float time) {
    const float t = std::clamp((time - from) / (to - from), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Capture the real first iOX frame once. The wallpaper's crop and the bezel's placement
// come from the same display layout that the live launcher will use at handoff.
class LauncherFlight {
  public:
    void draw(Launcher &launcher, int width, int height, const DisplaySettings &settings, bool es,
              int titleTurns, float horizon, float progress) {
        if (failed_ || !prepare(launcher, width, height, settings, es))
            return;
        const float swing = std::sin(3.14159265f * progress) * (1 - progress);
        const float turn = titleTurns < 0 ? -1.f : 1.f;
        const float sweep = std::min(width, height) * .13f * swing;
        const float vertical = horizon * (1 - ease(0, .91f, progress)) + sweep * .32f;
        const float angle = -.8f * (1 - ease(.06f, .87f, progress)) + .4f * swing;
        const float p = progress;
        const float approach = p * p * p * (10 + p * (-15 + 6 * p));
        const float scale = .014f + .986f * approach;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        glColorMask(1, 1, 1, 1);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(image_, 0);
        glUniform2f(output_, float(width), float(height));
        glUniform2f(offset_, titleTurns ? -vertical * turn : sweep,
                    titleTurns ? sweep * turn : vertical);
        glUniform2f(rotation_, std::cos(angle) * scale, std::sin(angle) * scale);
        glUniform1f(opacity_, ease(0, .22f, progress));

        const auto layer = [&](const Rect &r, unsigned texture, int turns, bool png) {
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform4f(bounds_, float(r.x), float(r.y), float(r.w), float(r.h));
            glUniform1i(turns_, turns);
            glUniform1i(topRows_, png);
            glUniform1f(textureAlpha_, png ? 1.f : 0.f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        };
        const auto &layout = destination_.presentationLayout();
        Rect bezel = layout.minorolaFrame;
        unsigned artwork = destination_.minorolaTexture();
        int bezelTurns = layout.minorolaTurns;
        if (!bezel.w && layout.view.w == width && layout.view.h == height) {
            // Native 4:3 and 3:4 still see the handset fly. Its entire rim is
            // outside the output by the time the full-screen menu takes over.
            bezel = minorolaAround(layout.view);
            artwork = launcher.minorolaArtwork();
            bezelTurns = height > width ? 1 : layout.turns > 0 ? 2 : 0;
        }
        const bool cropped = bezel.w > width || bezel.h > height;
        if (artwork && cropped)
            layer(bezel, artwork, bezelTurns, true);
        layer(layout.view, destination_.gameplayTexture(), layout.turns, false);
        if (artwork && !cropped)
            layer(bezel, artwork, bezelTurns, true);
        glDisable(GL_BLEND);
    }

    void release(bool abandon) {
        if (abandon)
            destination_.abandonGraphics();
        else {
            destination_.shutdown();
            glDeleteProgram(program_);
            glDeleteBuffers(1, &vbo_);
            glDeleteVertexArrays(1, &vao_);
        }
        program_ = vao_ = vbo_ = 0;
        width_ = height_ = 0;
        failed_ = false;
    }

  private:
    bool prepare(Launcher &launcher, int width, int height, const DisplaySettings &settings,
                 bool es) {
        if (width <= 0 || height <= 0)
            return false;
        const auto native = launcher.nativeFormat();
        if (width == width_ && height == height_ && native == native_ &&
            settings.orientation == orientation_)
            return true;
        if (!program_ && (!destination_.init(es) || !makeProgram())) {
            release(false);
            failed_ = true;
            SDL_Log("iOX title flight could not initialize its compositor");
            return false;
        }
        launcher.configureDisplay(destination_, width, height, settings);
        if (!destination_.graphicsValid()) {
            release(false);
            failed_ = true;
            SDL_Log("iOX title flight could not allocate its wallpaper surface");
            return false;
        }
        destination_.beginFrame();
        launcher.captureHomeEntrance(destination_.surface());
        glBindTexture(GL_TEXTURE_2D, destination_.gameplayTexture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        width_ = width;
        height_ = height;
        native_ = native;
        orientation_ = settings.orientation;
        return true;
    }

    bool makeProgram() {
        const char *vertex = R"(
attribute vec2 aUV;
uniform vec2 uOutput, uOffset, uRotation;
uniform vec4 uBounds;
uniform int uTurns, uTopRows;
varying vec2 vUV;
void main() {
    vec2 p = uBounds.xy + aUV * uBounds.zw - uOutput * 0.5;
    p = mat2(uRotation.x,uRotation.y,-uRotation.y,uRotation.x) * p + uOffset;
    gl_Position = vec4(p / uOutput * vec2(2.0,-2.0), 0.0, 1.0);
    vec2 uv = uTurns == 2 ? 1.0-aUV : uTurns == 1 ? vec2(aUV.y,1.0-aUV.x)
              : uTurns == -1 ? vec2(1.0-aUV.y,aUV.x) : aUV;
    vUV = uTopRows != 0 ? uv : vec2(uv.x,1.0-uv.y);
})";
        const char *fragment = R"(
precision mediump float;
uniform sampler2D uImage;
uniform float uOpacity, uTextureAlpha;
varying vec2 vUV;
void main() {
    vec4 c = texture2D(uImage,vUV);
    gl_FragColor = vec4(c.rgb,mix(1.0,c.a,uTextureAlpha)*uOpacity);
})";
        const auto compile = [&](unsigned type, const char *code) {
            const auto adapted = destination_.adaptShader(code, type == GL_VERTEX_SHADER ? 0 : 1);
            const char *source = adapted.c_str();
            const unsigned shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            return shader;
        };
        const unsigned vs = compile(GL_VERTEX_SHADER, vertex),
                       fs = compile(GL_FRAGMENT_SHADER, fragment);
        program_ = glCreateProgram();
        glAttachShader(program_, vs);
        glAttachShader(program_, fs);
        glBindAttribLocation(program_, 0, "aUV");
        glLinkProgram(program_);
        glDeleteShader(vs);
        glDeleteShader(fs);
        int linked = 0;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024]{};
            glGetProgramInfoLog(program_, sizeof log, nullptr, log);
            SDL_Log("iOX title flight shader: %s", log);
            return false;
        }
        image_ = glGetUniformLocation(program_, "uImage");
        bounds_ = glGetUniformLocation(program_, "uBounds");
        output_ = glGetUniformLocation(program_, "uOutput");
        offset_ = glGetUniformLocation(program_, "uOffset");
        rotation_ = glGetUniformLocation(program_, "uRotation");
        turns_ = glGetUniformLocation(program_, "uTurns");
        topRows_ = glGetUniformLocation(program_, "uTopRows");
        opacity_ = glGetUniformLocation(program_, "uOpacity");
        textureAlpha_ = glGetUniformLocation(program_, "uTextureAlpha");
        const float quad[] = {0, 0, 1, 0, 0, 1, 1, 1};
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        return true;
    }

    Display destination_;
    NativeFormat native_{};
    Orientation orientation_ = Orientation::Auto;
    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int width_ = 0, height_ = 0;
    int image_ = -1, bounds_ = -1, output_ = -1, offset_ = -1, rotation_ = -1;
    int turns_ = -1, topRows_ = -1, opacity_ = -1, textureAlpha_ = -1;
    bool failed_ = false;
};
} // namespace
struct CollectionTitle::Impl {
    std::filesystem::path shaderCache;
    Host *host = nullptr;
    ShellAudio *audio = nullptr;
    SurfaceInfo surface{};
    bool highQuality = true;
    bool paused = false, buttons = false, begun = false;
    std::set<uint32_t> pointers;
    std::unique_ptr<neon::widgets::TitleSequence> sequence;
    neon::widgets::TitlePresenter presenter;
    std::unique_ptr<neon::effects::Runtime> background;
    LauncherFlight flight;
    double handoffTime = -1, handoffBrightness = 1;
    bool exitReady = false;
};
CollectionTitle::CollectionTitle() : impl_(std::make_unique<Impl>()) {}
CollectionTitle::~CollectionTitle() = default;
const CoreInfo &CollectionTitle::info() const {
    return info_;
}
void CollectionTitle::setShaderCacheDirectory(const std::filesystem::path &path) {
    impl_->shaderCache = path;
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
        if (!readPresentationAsset("Vaporwave.neonb", bytes))
            return false;
        std::istringstream scene(std::string(bytes.begin(), bytes.end()));
        i.background = neon::effects::Runtime::loadBaked(scene, {}, effectRead);
        i.background->setShaderCacheDirectory(i.shaderCache);
        i.background->parameters().renderScale = i.highQuality ? 1.f : .5f;
        return true;
    } catch (const std::exception &e) {
        host.log("Collection title unavailable: %s", e.what());
        shutdown();
        return false;
    }
}
void CollectionTitle::setHighQuality(bool high) {
    impl_->highQuality = high;
    if (impl_->background)
        impl_->background->parameters().renderScale = high ? 1.f : .5f;
}
void CollectionTitle::setShellAudio(ShellAudio &audio) {
    impl_->audio = &audio;
}
void CollectionTitle::updateInput(bool buttons, bool pointer) {
    auto &i = *impl_;
    const bool wasDismissing = i.sequence->dismissing();
    i.sequence->input(buttons, pointer);
    if (i.audio && !wasDismissing && i.sequence->dismissing())
        i.audio->play(ShellAudio::Cue::Launch);
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
        updateInput(i.buttons, !i.pointers.empty());
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
            updateInput(i.buttons, !i.pointers.empty());
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
        if (i.sequence->dismissing()) {
            if (i.handoffTime < 0) {
                i.handoffTime = 0;
                i.handoffBrightness = i.sequence->opacity();
            } else
                i.handoffTime = std::min(kHandoffSeconds, i.handoffTime + std::clamp(dt, 0., .1));
        }
        i.sequence->update(dt, {viewW, viewH, 0, {}, {}});
        const neon::widgets::RenderTarget target{
            i.surface.framebuffer,
            w,
            h,
            {double(x), double(y), double(viewW), double(viewH)},
            {},
            0};
        const float progress = float(i.handoffTime / kHandoffSeconds);
        i.presenter.render(
            *i.sequence, target,
            [&](const auto &) {
                i.background->frame(dt, {i.surface.framebuffer, w, h, {x, y, viewW, viewH}, {}, 0});
            },
            progress < 0 ? -1 : i.handoffBrightness * (1 - ease(.52f, 1, progress)));
        if (i.audio && i.sequence->opacity() >= 1.0)
            i.audio->markTitleVisible();
        i.exitReady |= i.sequence->takeLaunchRequest();
        if (i.exitReady && i.handoffTime >= kHandoffSeconds)
            i.host->requestQuit();
    } catch (const std::exception &e) {
        i.host->log("Collection title rendering failed: %s", e.what());
        i.host->requestQuit();
    }
}
bool CollectionTitle::handoffActive() const {
    return impl_->handoffTime >= 0;
}
void CollectionTitle::presentHandoff(Launcher &launcher, const Display &screen,
                                     const DisplaySettings &settings) {
    auto &i = *impl_;
    if (i.handoffTime >= 0 && i.sequence) {
        const auto sky = i.sequence->presentationViewport(i.surface.width, i.surface.height);
        // The authored grid meets the sky below center, measured in short-edge units.
        const float horizon = .17f * float(std::min(sky.width, sky.height));
        i.flight.draw(launcher, screen.viewport().w, screen.viewport().h, settings, i.surface.glES,
                      screen.presentationTurns(), horizon, float(i.handoffTime / kHandoffSeconds));
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
        updateInput(true, false);
    }
    return true;
}
void CollectionTitle::abandonGraphics() {
    auto &i = *impl_;
    i.flight.release(true);
    i.presenter.abandonGraphics();
    if (i.background)
        i.background->abandonGraphics();
}
void CollectionTitle::shutdown() {
    auto &i = *impl_;
    if (SDL_GL_GetCurrentContext()) {
        i.flight.release(false);
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
