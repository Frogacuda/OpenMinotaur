#include "launcher_transition.h"
#include "neon/render_device.h"
#include "startup_screen.h"
#include <glad/gl.h>
#include <cstdarg>

namespace mino {
namespace {
// Forward reads/shaders to the selected game's real host, but don't present or
// start a second startup clock while constructing the transient launch view.
class ArtworkHost final : public Host {
  public:
    explicit ArtworkHost(Host &target) : target_(target) {}
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        return target_.readAsset(name, out);
    }
    std::string saveDir() const override {
        return target_.saveDir();
    }
    void log(const char *, ...) override {}
    std::string adaptShader(const std::string &src, int stage) const override {
        return target_.adaptShader(src, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {}

  private:
    Host &target_;
};
} // namespace
bool LauncherTransition::prepare(Launcher &launcher, int width, int height,
                                 const DisplaySettings &settings, bool es) {
    cancel();
    if (width <= 0 || height <= 0 || !source_.init(es))
        return false;
    width_ = width;
    height_ = height;
    source_.configure(width, height, launcher.nativeFormat(), settings);
    if (!source_.error().empty()) {
        source_.shutdown();
        return false;
    }
    const char *vertex =
        R"(attribute vec2 aPos;attribute vec2 aUV;varying vec2 vUV;uniform float uScale;
void main(){vUV=aUV;gl_Position=vec4(aPos*uScale,0.0,1.0);})";
    const char *fragment = R"(precision mediump float;
varying vec2 vUV;uniform sampler2D uTexture;uniform float uAlpha;
void main(){gl_FragColor=vec4(texture2D(uTexture,vUV).rgb,uAlpha);})";
    const auto compile = [&](const char *source, unsigned type) {
        const auto adapted = source_.adaptShader(source, type == GL_VERTEX_SHADER ? 0 : 1);
        const char *text = adapted.c_str();
        const unsigned shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        int okay = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
        if (!okay) {
            char log[1024]{};
            glGetShaderInfoLog(shader, sizeof log, nullptr, log);
            SDL_Log("iOX transition shader: %s", log);
            glDeleteShader(shader);
            return 0u;
        }
        return shader;
    };
    const unsigned vs = compile(vertex, GL_VERTEX_SHADER),
                   fs = compile(fragment, GL_FRAGMENT_SHADER);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        cancel();
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aUV");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int okay = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &okay);
    if (!okay) {
        cancel();
        return false;
    }
    const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, 1,  1, 1, 1,
                          -1, -1, 0, 0, 1, 1,  1, 1, -1, 1, 0, 1};
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        cancel();
        return false;
    }
    launcher_ = &launcher;
    return true;
}
bool LauncherTransition::playArtwork(Host &host, Display &target, SDL_Window *window, bool &quit,
                                     const FrameObserver &observer) {
    if (!pending())
        return false;
    ArtworkHost artworkHost(host);
    StartupScreen artwork;
    if (!artwork.begin(artworkHost, target.surface())) {
        cancel();
        return false;
    }
    const bool shown = play(target, window, quit, observer);
    artwork.release();
    return shown;
}
bool LauncherTransition::playRendered(Display &target, SDL_Window *window, bool &quit,
                                      const FrameObserver &observer) {
    return pending() && play(target, window, quit, observer);
}
bool LauncherTransition::play(Display &target, SDL_Window *window, bool &quit,
                              const FrameObserver &observer) {
    const auto context = SDL_GL_GetCurrentContext();
    const auto start = SDL_GetTicksNS();
    bool first = true;
    do {
        const float p = first ? 0.f
                              : float(std::min(1.0, double(SDL_GetTicksNS() - start) / 1e9 /
                                                        iox::AppLaunch::seconds));
        first = false;
        // Reuse normal fitting, output rotation, CRT/filter and bezel composition.
        target.present(framebuffer_);
        launcher_->resize(source_.surface());
        launcher_->externalLaunchProgress(p);
        source_.beginFrame();
        launcher_->frame(0);
        source_.present();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
        // Startup rendering uses the current VAO. Re-establish the compositor's
        // attributes after that renderer, just as Display::present does.
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        for (unsigned attribute = 2; attribute < 5; ++attribute)
            glDisableVertexAttribArray(attribute);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void *>(2 * sizeof(float)));
        glUniform1i(glGetUniformLocation(program_, "uTexture"), 0);
        glUniform1f(glGetUniformLocation(program_, "uScale"), iox::AppLaunch::imageScale(p));
        glUniform1f(glGetUniformLocation(program_, "uAlpha"), iox::AppLaunch::imageProgress(p));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        if (observer)
            observer(p);
        SDL_GL_SwapWindow(window);
        SDL_PumpEvents();
        SDL_Event event;
        if (SDL_GL_GetCurrentContext() != context) {
            // Stop before releasing old names in SDL's replacement context.
            neon::render::NeonDevice::abandonAllGraphics();
            abandonGraphics();
            return false;
        }
        if (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0)
            quit = true;
        // Device/lifecycle/window events remain queued for the normal host loop.
        // Presses during the animation must not activate the new app on handoff.
        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        SDL_FlushEvent(SDL_EVENT_TEXT_INPUT);
        SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
        SDL_FlushEvent(SDL_EVENT_FINGER_DOWN);
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        const bool background =
            SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_EVENT_WILL_ENTER_BACKGROUND,
                           SDL_EVENT_DID_ENTER_BACKGROUND) > 0;
        if (p >= 1 || quit || background || width != width_ || height != height_)
            break;
        SDL_Delay(1);
    } while (true);
    launcher_->externalLaunchProgress(-1);
    launcher_->resize(target.surface());
    cancel();
    return true;
}
void LauncherTransition::cancel() {
    if (launcher_)
        launcher_->externalLaunchProgress(-1);
    launcher_ = nullptr;
    if (framebuffer_)
        glDeleteFramebuffers(1, &framebuffer_);
    if (texture_)
        glDeleteTextures(1, &texture_);
    if (program_)
        glDeleteProgram(program_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    framebuffer_ = texture_ = program_ = vbo_ = vao_ = 0;
    source_.shutdown();
}
} // namespace mino
