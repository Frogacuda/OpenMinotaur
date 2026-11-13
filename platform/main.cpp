// main.cpp - application shell: window + GL context, audio device, main loop,
// lifecycle (pause/save on background), command-line tools for desktop use.
//
//   minotaur [--game ID] [--gles] [--window WxH]
//            [--set key=value]... [--screenshot out.png --frames N] [--list]
//            [--inject-touch nx,ny[@frame[+frames]]]...
//            [--inject-pad lx,ly,buttons[@frame[+frames]]]... (isolated test input)
//            [--inject-motion x,y,z] (simulated gravity in g, test only)
#include "core_api.h"
#include "achievements.h"
#include "display.h"
#include "bundled_assets.h"
#ifndef MINO_STANDALONE_GAME
#include "launcher.h"
#include "launcher_transition.h"
#include "about_screen.h"
#include "ui_assets.h"
#endif
#include "input.h"
#ifdef MINO_COLLECTION_TITLE
#include "collection_title.h"
#endif
#include "settings.h"
#include "stb_image.h"
#include "neon/render_device.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace mino;

namespace {

struct App;

class HostImpl : public Host {
  public:
    App *app = nullptr;
    bool injectMotion = false, injectedMotionEnabled = false;
    MotionState injectedMotion;
    std::string assetGameId, saveDirPath;
    Achievements achievements;
    std::string achievementPath, achievementGameId;
    bool achievementWritePending = false;
    void flushAchievements() {
        achievementWritePending = false;
        if (achievements.dirty() && !achievements.save(achievementPath))
            log("Achievement records could not be saved; retained for lifecycle retry.");
    }
    void unlockAchievement(const std::string &id) override {
        achievementWritePending |= achievements.unlock(achievementGameId, id);
    }

    uint32_t originalAchievementDisplayMask() const override {
        uint32_t bits = 0;
        for (unsigned n = 0; n < 32; ++n)
            if (achievements.contains(achievementGameId, "original-" + std::to_string(n)))
                bits |= uint32_t(1) << n;
        return bits;
    }

    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        return readBundledAsset(assetGameId, name, out);
    }
    std::string saveDir() const override {
        return saveDirPath;
    }
    void log(const char *fmt, ...) override {
        char buf[2048];
        va_list ap;
        va_start(ap, fmt);
        SDL_vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);
        SDL_Log("[core] %s", buf);
    }
    std::string adaptShader(const std::string &src, int stage) const override;
    void lockAudio() override;
    void unlockAudio() override;
    void requestQuit() override;
    bool presentStartupFrame() override;
    bool enableMotion(bool enabled) override;
    MotionState motion() const override;
    bool touchControlsAvailable() const override;
    bool keyboardNavigationAvailable() const override;
    std::shared_ptr<FileSelection> chooseDataFile() override;
    bool readDataFile(const std::string &, std::vector<uint8_t> &, size_t) const override;
};

struct App {
    SDL_Window *window = nullptr;
    SDL_GLContext gl = nullptr;
    SDL_AudioStream *audio = nullptr;
    std::vector<float> mixBuf;
    Display display;
    Input input;
#ifndef MINO_STANDALONE_GAME
    Settings settings;
    Launcher launcher;
    LauncherTransition launchTransition;
#endif
    HostImpl host;
    Core *core = nullptr;
    bool coreReady = false;
    std::string gameId, prefDir, savesRoot;
    bool quit = false;
    bool returnRequested = false;
    bool gatePads = true; // Require neutral controls after a screen/game change.
    MappingCapture mapping;
    std::vector<std::string> resolutions;
    std::string appliedWindow;
    NativeFormat nativeFormat{0, 0, 0, 0};
    std::atomic_bool paused{false};
    bool glES = false;
    bool graphicsCheckpointed = false;
    std::atomic_bool graphicsDirty{false};
};

bool HostImpl::enableMotion(bool enabled) {
    if (injectMotion) {
        injectedMotionEnabled = enabled;
        return enabled;
    }
    return app->input.enableMotion(enabled);
}
MotionState HostImpl::motion() const {
    if (injectMotion)
        return injectedMotionEnabled ? app->display.nativeMotion(injectedMotion) : MotionState{};
    return app->display.nativeMotion(app->input.motion(app->window));
}
bool HostImpl::touchControlsAvailable() const {
    int count = 0;
    auto *devices = SDL_GetTouchDevices(&count);
    SDL_free(devices);
    return count > 0 && app->input.touchEnabled();
}
bool HostImpl::presentStartupFrame() {
    app->display.present();
    SDL_GL_SwapWindow(app->window);
    return true;
}
std::shared_ptr<FileSelection> HostImpl::chooseDataFile() {
    auto picker = std::make_shared<FileSelection>();
    auto *context = new std::shared_ptr<FileSelection>(picker);
    auto callback = [](void *user, const char *const *files, int) {
        std::unique_ptr<std::shared_ptr<FileSelection>> owner(
            static_cast<std::shared_ptr<FileSelection> *>(user));
        auto result = *owner;
        std::lock_guard<std::mutex> lock(result->mutex);
        if (files && files[0])
            result->path = files[0];
        result->done = true;
    };
    // No format filter: original banks also have extensionless generated names;
    // Android document providers likewise supply their own type metadata.
    SDL_ShowOpenFileDialog(callback, context, app->window, nullptr, 0, saveDirPath.c_str(), false);
    return picker;
}
bool HostImpl::readDataFile(const std::string &path, std::vector<uint8_t> &out,
                            size_t limit) const {
    auto *file = SDL_IOFromFile(path.c_str(), "rb");
    if (!file)
        return false;
    std::vector<uint8_t> bytes(limit + 1);
    const auto count = SDL_ReadIO(file, bytes.data(), bytes.size());
    const bool valid = count <= limit && SDL_GetIOStatus(file) != SDL_IO_STATUS_ERROR;
    SDL_CloseIO(file);
    if (!valid)
        return false;
    bytes.resize(count);
    out = std::move(bytes);
    return true;
}
std::string HostImpl::adaptShader(const std::string &src, int stage) const {
    return app->display.adaptShader(src, stage);
}
void HostImpl::lockAudio() {
    if (app->audio)
        SDL_LockAudioStream(app->audio);
}
void HostImpl::unlockAudio() {
    if (app->audio)
        SDL_UnlockAudioStream(app->audio);
}
void HostImpl::requestQuit() {
    app->returnRequested = true;
}

void SDLCALL audioCallback(void *user, SDL_AudioStream *stream, int additional, int) {
    App *app = static_cast<App *>(user);
    int frames = additional / int(sizeof(float) * 2);
    if (frames <= 0)
        return;
    app->mixBuf.assign(size_t(frames) * 2, 0.0f);
    if (app->core && !app->paused)
        app->core->mixAudio(app->mixBuf.data(), frames);
    SDL_PutAudioStreamData(stream, app->mixBuf.data(), frames * int(sizeof(float) * 2));
}

bool createContext(App &app, bool preferES) {
    auto attempt = [&](bool es) {
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        app.gl = SDL_GL_CreateContext(app.window);
        if (app.gl)
            app.glES = es;
        return app.gl != nullptr;
    };
    if (attempt(preferES) || attempt(!preferES)) {
        SDL_GL_MakeCurrent(app.window, app.gl);
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
            SDL_Log("failed to load GL functions");
            return false;
        }
        SDL_Log("GL: %s | %s | %s", glGetString(GL_VERSION), glGetString(GL_RENDERER),
                app.glES ? "ES path" : "desktop core path");
        return true;
    }
    SDL_Log("could not create a GL context: %s", SDL_GetError());
    return false;
}

#if (defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_MACOS) ||                               \
     defined(SDL_PLATFORM_LINUX)) &&                                                               \
    !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_XBOXONE) &&                            \
    !defined(SDL_PLATFORM_XBOXSERIES)
constexpr bool kDesktop = true;
#else
constexpr bool kDesktop = false;
#endif
bool HostImpl::keyboardNavigationAvailable() const {
    return kDesktop && SDL_HasKeyboard();
}
#ifndef MINO_STANDALONE_GAME
void loadMappings(App &app) {
    std::array<ControlMapping, kMaxPads> mappings{};
    for (int i = 0; i < kMaxPads; ++i)
        mappings[size_t(i)] = ControlMapping::decode(
            app.settings.get("global", "mapping" + std::to_string(i + 1), ""));
    app.input.setMappings(mappings);
    app.input.loadTiltCalibration(app.settings.get("global", "tilt_neutral", ""));
}
void refreshOptions(App &app) {
    int touches = 0;
    auto *ids = SDL_GetTouchDevices(&touches);
    SDL_free(ids);
    app.launcher.configureOptions(app.settings, kDesktop, SDL_HasMouse() || touches > 0,
                                  app.input.connectedPads(), app.resolutions);
    const auto prompts = app.input.menuPrompts();
    app.launcher.setControllerPrompts(prompts.connected, prompts.select, prompts.back);
}
void enumerateResolutions(App &app) {
    app.resolutions.clear();
    int count = 0;
    auto **modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(app.window), &count);
    for (int i = 0; i < count; ++i) {
        auto name = std::to_string(modes[i]->w) + " x " + std::to_string(modes[i]->h);
        if (std::find(app.resolutions.begin(), app.resolutions.end(), name) ==
            app.resolutions.end())
            app.resolutions.push_back(name);
    }
    SDL_free(modes);
}
void applyWindow(App &app) {
    if (!kDesktop)
        return;
    const auto mode = app.settings.get("global", "window_mode", "windowed"),
               resolution = app.settings.get("global", "fullscreen_resolution", "desktop");
    auto key = mode + resolution;
    if (key == app.appliedWindow)
        return;
    bool ok = true;
    if (mode == "windowed") {
        ok = SDL_SetWindowFullscreen(app.window, false);
        int w = 0, h = 0;
        if (resolution == "desktop") {
            const auto *native = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(app.window));
            if (native) {
                w = native->w;
                h = native->h;
            }
        } else
            SDL_sscanf(resolution.c_str(), "%d x %d", &w, &h);
        if (ok && w > 0 && h > 0)
            ok = SDL_SetWindowSize(app.window, w, h);
    } else {
        const SDL_DisplayMode *chosen = nullptr;
        int count = 0;
        auto **modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(app.window), &count);
        if (mode == "exclusive") {
            int w = 0, h = 0;
            if (resolution == "desktop") {
                auto *desktop = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(app.window));
                if (desktop) {
                    w = desktop->w;
                    h = desktop->h;
                }
            } else
                SDL_sscanf(resolution.c_str(), "%d x %d", &w, &h);
            for (int i = 0; i < count; ++i)
                if (modes[i]->w == w && modes[i]->h == h &&
                    (!chosen || modes[i]->refresh_rate > chosen->refresh_rate))
                    chosen = modes[i];
            if (!chosen)
                ok = false;
        }
        if (ok)
            ok = SDL_SetWindowFullscreenMode(app.window, chosen) &&
                 SDL_SetWindowFullscreen(app.window, true);
        SDL_free(modes);
    }
    if (!ok) {
        SDL_SetWindowFullscreen(app.window, false);
        app.settings.set("global", "window_mode", "windowed");
        app.settings.save();
        app.launcher.showMessage(
            "Display mode unavailable",
            "The display could not apply that fullscreen mode. Windowed mode has been restored.");
        app.appliedWindow = "windowed" + resolution;
    } else
        app.appliedWindow = key;
}

#endif

#ifdef MINO_STANDALONE_GAME
DisplaySettings standaloneDisplaySettings() {
    DisplaySettings defaults;
#ifdef SDL_PLATFORM_ANDROID
    defaults.orientation = Orientation::BestFit;
#else
    defaults.orientation = Orientation::Landscape;
#endif
    return defaults;
}
#endif

InputSettings gameInputSettings(const App &app) {
#ifdef MINO_STANDALONE_GAME
    return {};
#else
    return app.settings.input(app.gameId);
#endif
}

void applyOrientation(App &app) {
#ifdef MINO_STANDALONE_GAME
    const char *hint =
        orientationHint(standaloneDisplaySettings().orientation, false, app.core->nativeFormat());
#else
    const bool shell = app.core == &app.launcher || app.gameId == "title";
    const auto ds = app.settings.display(shell ? "global" : app.gameId);
    const char *hint = orientationHint(ds.orientation, shell, app.core->nativeFormat());
#endif
    SDL_SetHint(SDL_HINT_ORIENTATIONS, hint);
#ifdef SDL_PLATFORM_ANDROID
    // Changing SDL's hint alone does not update an existing Android window.
    if (app.window) {
        auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
        jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (env && activity) {
            jclass cls = env->GetObjectClass(activity);
            jmethodID method =
                env->GetMethodID(cls, "setOrientationBis", "(IIZLjava/lang/String;)V");
            jstring value = env->NewStringUTF(hint);
            if (method)
                env->CallVoidMethod(activity, method, 0, 0, JNI_TRUE, value);
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                SDL_Log("Android orientation request failed");
            }
            env->DeleteLocalRef(value);
            env->DeleteLocalRef(cls);
        }
        if (env && activity)
            env->DeleteLocalRef(activity);
    }
#endif
}

#ifndef MINO_STANDALONE_GAME
DisplaySettings shellDisplaySettings(DisplaySettings ds) {
    ds.aspect = AspectMode::IPad;
    ds.screenMode = ScreenMode::HiRes;
    ds.backdrop = Backdrop::Colour;
    ds.scale = ScaleMode::Fit;
    ds.internalScale = 0;
    ds.filter = Filter::Bilinear;
    return ds;
}
#endif
void reconfigure(App &app, bool notify = true) {
    if (!app.core)
        return;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
#ifdef MINO_STANDALONE_GAME
    const auto ds = standaloneDisplaySettings();
    const auto native = app.core->nativeFormat();
#else
    DisplaySettings ds = app.settings.display(app.core == &app.launcher ? "global" : app.gameId);
    if (app.core == &app.launcher || app.gameId == "title") {
        app.launcher.setPortrait(portraitLauncher(ds.orientation, w, h));
        ds = shellDisplaySettings(ds);
        if (app.gameId == "title") {
            ds.aspect = AspectMode::Fill;
            ds.matchViewportPixels = true;
        }
    }
    // Old bezel preferences fall back to black; image bezels are no longer offered.
    if (ds.backdrop == Backdrop::Image)
        ds.backdrop = Backdrop::Colour;
    const auto native =
        app.gameId == "title" ? app.launcher.nativeFormat() : app.core->nativeFormat();
#endif
    const bool formatChanged = native != app.nativeFormat;
    bool changed = app.display.configure(w, h, native, ds);
#ifndef MINO_STANDALONE_GAME
    if (!app.display.error().empty()) {
        app.settings.set("global", "screen_mode", "sharp");
        app.settings.save();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "CRT shader unavailable",
                                 "This graphics device could not run the CRT effect. Original "
                                 "(sharp) has been restored. Details are in the application log.",
                                 app.window);
    }
#endif
    app.nativeFormat = native;
    if (notify && (changed || formatChanged)) {
        app.input.cancelTouches(app.core);
        app.gatePads = true;
        app.core->resize(app.display.surface());
    }
}

void refreshNativeFormat(App &app) {
    if (app.gameId == "title")
        return;
    if (app.core && app.core->nativeFormat() != app.nativeFormat) {
        applyOrientation(app);
        reconfigure(app);
    }
}

#ifdef SDL_PLATFORM_WINDOWS
void toggleFullscreen(App &app) {
    const bool fullscreen = (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!fullscreen && !SDL_SetWindowFullscreenMode(app.window, nullptr))
        return;
    if (!SDL_SetWindowFullscreen(app.window, !fullscreen)) {
        SDL_Log("Fullscreen toggle failed: %s", SDL_GetError());
        return;
    }
#ifndef MINO_STANDALONE_GAME
    const std::string mode = fullscreen ? "windowed" : "borderless";
    app.settings.set("global", "window_mode", mode);
    app.settings.save();
    app.appliedWindow = mode + app.settings.get("global", "fullscreen_resolution", "desktop");
#endif
    SDL_SyncWindow(app.window);
    app.input.cancelTouches(app.core);
    app.gatePads = true;
    if (!app.paused && SDL_GL_GetCurrentContext())
        reconfigure(app);
    else
        app.graphicsDirty = true;
}
bool fullscreenShortcut(App &app, const SDL_KeyboardEvent &key) {
    if ((key.key != SDLK_RETURN && key.key != SDLK_KP_ENTER) || !(key.mod & SDL_KMOD_ALT))
        return false;
    if (!key.repeat)
        toggleFullscreen(app);
    return true;
}
#endif

#ifdef MINO_COLLECTION_TITLE
bool startCollectionTitle(App &app) {
    auto title = std::make_unique<CollectionTitle>();
    app.coreReady = false;
    app.core = title.get();
    app.gameId = "title";
    reconfigure(app, false);
    if (title->init(app.host, app.display.surface())) {
        title.release();
        app.coreReady = true;
        return true;
    }
    app.core = &app.launcher;
    app.gameId = "launcher";
    app.coreReady = true;
    reconfigure(app);
    return false;
}
#endif

void setPaused(App &app, bool p) {
    if (p == app.paused || !app.core)
        return;
    if (p) {
        app.paused = true;
        app.gatePads = true;
        app.input.cancelTouches(app.core);
        if (app.coreReady) {
            app.core->pause(true);
            app.core->save();
        }
        app.host.flushAchievements();
    } else {
        if (app.coreReady)
            app.core->pause(false);
    }
    app.paused = p;
}

bool SDLCALL lifecycleWatch(void *userdata, SDL_Event *event) {
    auto &app = *static_cast<App *>(userdata);
    if (event->type == SDL_EVENT_DID_ENTER_FOREGROUND && app.gl &&
        SDL_GL_GetCurrentContext() == app.gl)
        app.graphicsDirty = true;
    // A replacement may reuse the old context handle. Mark it before older
    // queued resize events can touch names from the discarded namespace.
    if (event->type == SDL_EVENT_RENDER_DEVICE_RESET)
        app.graphicsDirty = true;
    // SDL emits this on the rendering thread before detaching Android's EGL
    // context. Polling the queued event happens too late for GPU readback.
    if (event->type == SDL_EVENT_WILL_ENTER_BACKGROUND && app.core &&
        SDL_GL_GetCurrentContext() == app.gl && !app.graphicsCheckpointed) {
        setPaused(app, true);
        const bool captured = neon::render::NeonDevice::checkpointAllGraphics();
        app.graphicsCheckpointed = true;
        if (!captured)
            SDL_Log("Graphics checkpoint could not capture every texture");
    }
    return true;
}

bool synchronizeGraphics(App &app) {
    auto current = SDL_GL_GetCurrentContext();
    if (!current) {
        if (!SDL_GL_MakeCurrent(app.window, app.gl))
            return false;
        current = SDL_GL_GetCurrentContext();
    }
    if (current != app.gl || (app.graphicsDirty && !app.display.graphicsValid())) {
        SDL_Log("Restoring graphics after context replacement");
        app.gl = current;
        const auto failed = [&]() {
            SDL_Log("Graphics recovery failed: %s", SDL_GetError());
            app.quit = true;
            return false;
        };
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
            return failed();
        // Drop every stale name before allocating in the new namespace.
        neon::render::NeonDevice::abandonAllGraphics();
#ifdef MINO_COLLECTION_TITLE
        if (app.gameId == "title")
            static_cast<CollectionTitle *>(app.core)->abandonGraphics();
#endif
#ifndef MINO_STANDALONE_GAME
        app.launchTransition.abandonGraphics();
        app.launcher.abandonGraphics();
#endif
        app.display.abandonGraphics();
        if (!app.display.init(app.glES))
            return failed();
        reconfigure(app, false);
        // Update backbuffer/scale while target names are zero. The normal
        // resize path would otherwise reallocate restored feedback textures.
        if (app.coreReady)
            app.core->resize(app.display.surface());
        if (!neon::render::NeonDevice::restoreAllGraphics())
            return failed();
#ifndef MINO_STANDALONE_GAME
        if (!app.launcher.restoreGraphics(app.host, app.display.surface()))
            return failed();
#endif
        app.gatePads = true;
    } else if (app.graphicsDirty)
        reconfigure(app);
    app.graphicsDirty = false;
    if (app.graphicsCheckpointed) {
        neon::render::NeonDevice::discardGraphicsCheckpoints();
        app.graphicsCheckpointed = false;
    }
    return true;
}

void stopGame(App &app) {
    // Destroying the stream waits for its callback before core pointers,
    // samples or game state can be destroyed/replaced.
    if (app.audio)
        SDL_DestroyAudioStream(app.audio);
    app.audio = nullptr;
    if (!app.core)
        return;
#ifndef MINO_STANDALONE_GAME
    if (app.core == &app.launcher)
        return;
#endif
    app.input.cancelTouches(app.core);
    app.core->save();
    app.host.flushAchievements();
    app.core->shutdown();
    delete app.core;
    app.core = nullptr;
}

#ifndef MINO_STANDALONE_GAME
void showLauncher(App &app) {
    stopGame(app);
    app.core = &app.launcher;
    app.coreReady = true;
    app.gameId = "launcher";
    app.returnRequested = false;
    app.gatePads = true;
    app.launcher.returnHome();
    applyOrientation(app);
    reconfigure(app);
    // The surface can already have the right dimensions after a failed init.
    app.launcher.resize(app.display.surface());
    SDL_SetWindowTitle(app.window, app.launcher.info().title);
    app.settings.save();
}

bool startGame(App &app, const std::string &requestedId, bool animate = false,
               const LauncherTransition::FrameObserver &observer = {}) {
    const CoreEntry *entry = findCore(requestedId);
    if (!entry)
        return false;
    const std::string id = entry->info->id;
    if (entry->info->atlasFile &&
        !bundledGameAvailable(entry->info->assetGameId ? entry->info->assetGameId : id)) {
        app.launcher.selectGame(entry->info->assetGameId ? entry->info->assetGameId : id);
        app.launcher.showMessage(entry->info->title, "Bundled game resources are unavailable.");
        return false;
    }
    if (animate && app.core == &app.launcher) {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(app.window, &width, &height);
        app.launchTransition.prepare(app.launcher, width, height,
                                     shellDisplaySettings(app.settings.display("global")),
                                     app.glES);
    }
    app.input.cancelTouches(app.core);
    app.gameId = id;
    app.host.achievementGameId =
        entry->info->achievementGameId ? entry->info->achievementGameId : id;
    app.returnRequested = false;
    app.gatePads = true;
    const std::string assetId = entry->info->assetGameId ? entry->info->assetGameId
                                : entry->info->atlasFile ? id
                                                         : "minotron";
    app.host.assetGameId = assetId;
    app.host.saveDirPath =
        app.savesRoot + (entry->info->saveGameId ? entry->info->saveGameId : id) + "/";
    SDL_CreateDirectory(app.host.saveDirPath.c_str());
    app.core = entry->create();
    app.coreReady = false;
    applyOrientation(app);
    reconfigure(app, false);
    int launchWidth = 0, launchHeight = 0;
    SDL_GetWindowSizeInPixels(app.window, &launchWidth, &launchHeight);
    if (app.launchTransition.pending() && id != "gridrunner-classic") {
        app.launchTransition.playArtwork(app.host, app.display, app.window, app.quit, observer);
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(app.window, &width, &height);
        if (width != launchWidth || height != launchHeight)
            reconfigure(app, false);
    }
    while (!synchronizeGraphics(app)) {
        if (app.quit)
            return false;
        // A launch interrupted by Android's surface teardown waits for its
        // replacement before initializing a core into that context.
        SDL_PumpEvents();
        SDL_Event event;
        if (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0) {
            app.quit = true;
            return false;
        }
        SDL_Delay(16);
    }
    app.display.beginFrame();
    if (!app.core->init(app.host, app.display.surface())) {
        app.launchTransition.cancel();
        SDL_Log("core init failed: %s", id.c_str());
        showLauncher(app);
        app.launcher.showMessage("Could not start game",
                                 "Check the bundled game assets and the application log. "
                                 "Re-extract the complete release package if files are missing.");
        return false;
    }
    app.coreReady = true;
    if (app.paused)
        app.core->pause(true);
    // Classic has no independent loading image. Its actual direct-title render
    // is the destination; no new screenshot asset or invented loading screen.
    if (app.launchTransition.pending()) {
        app.display.beginFrame();
        app.core->frame(0);
        app.launchTransition.playRendered(app.display, app.window, app.quit, observer);
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(app.window, &width, &height);
        if (width != launchWidth || height != launchHeight)
            reconfigure(app);
    }
    app.settings.set("global", "last_game",
                     entry->info->assetGameId ? entry->info->assetGameId : id);
    app.settings.save();
    SDL_SetWindowTitle(app.window, entry->info->title);
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kAudioRate};
    app.audio =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, &app);
    if (app.audio)
        SDL_ResumeAudioStreamDevice(app.audio);
    else
        SDL_Log("no audio: %s", SDL_GetError());
    return true;
}

#else
bool startGame(App &app, const std::string &id) {
    const auto *entry = findCore(id);
    if (!entry)
        return false;
    app.gameId = id;
    app.returnRequested = false;
    app.gatePads = true;
    app.coreReady = false;
    app.host.assetGameId = entry->info->assetGameId ? entry->info->assetGameId : id;
    app.host.achievementGameId =
        entry->info->achievementGameId ? entry->info->achievementGameId : id;
    app.host.saveDirPath =
        app.savesRoot + (entry->info->saveGameId ? entry->info->saveGameId : id) + "/";
    SDL_CreateDirectory(app.host.saveDirPath.c_str());
    if (!app.core)
        app.core = entry->create();
    applyOrientation(app);
    reconfigure(app, false);
    app.display.beginFrame();
    if (!app.core->init(app.host, app.display.surface()))
        return false;
    app.coreReady = true;
    SDL_SetWindowTitle(app.window, entry->info->title);
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kAudioRate};
    app.audio =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, &app);
    if (app.audio)
        SDL_ResumeAudioStreamDevice(app.audio);
    return true;
}
#endif

bool saveScreenshot(App &app, const char *path) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
    std::vector<uint8_t> px(size_t(w) * h * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // The window ignores alpha; the PNG must too (else viewers show the
    // feedback's low-alpha areas as white).
    for (size_t i = 3; i < px.size(); i += 4)
        px[i] = 255;
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path, w, h, 4, px.data(), w * 4) != 0;
}

} // namespace

#ifndef MINOTAUR_SHELL_TEST
int main(int argc, char **argv) {
    App app;
    app.host.app = &app;

    std::string gameArg, screenshot, dataDirArg;
#ifdef MINO_STANDALONE_GAME
    gameArg = MINO_STANDALONE_GAME;
#endif
    std::vector<std::string> overrides;
    int shotFrames = 120, winW = 960, winH = 720;
    bool preferES = false, list = false;
    struct InjectedTouch {
        float x, y;
        int start, frames;
    };
    std::vector<InjectedTouch> injectTouches;
    struct InjectedPad {
        float x = 0, y = 0;
        unsigned buttons = 0;
        int start = 5, frames = 1;
    };
    std::vector<InjectedPad> injectPads;
#ifdef SDL_PLATFORM_ANDROID
    preferES = true;
#endif
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--game")
            gameArg = next();
        else if (a == "--data-dir") {
            dataDirArg = next();
            if (dataDirArg.empty() || dataDirArg[0] == '-') {
                SDL_Log("--data-dir requires a directory path");
                return 1;
            }
        } else if (a == "--gles")
            preferES = true;
        else if (a == "--screenshot")
            screenshot = next();
        else if (a == "--frames")
            shotFrames = SDL_atoi(next().c_str());
        else if (a == "--set") {
#ifdef MINO_STANDALONE_GAME
            SDL_Log("Standalone games use fixed presentation and input defaults");
            return 1;
#else
            overrides.push_back(next());
#endif
        } else if (a == "--list")
            list = true;
        else if (a == "--window")
            SDL_sscanf(next().c_str(), "%dx%d", &winW, &winH);
        else if (a == "--inject-motion") {
            const auto value = next();
            char trailing = 0;
            MotionState motion;
            motion.available = true;
            if (std::sscanf(value.c_str(), "%lf,%lf,%lf%c", &motion.x, &motion.y, &motion.z,
                            &trailing) != 3 ||
                !std::isfinite(motion.x) || !std::isfinite(motion.y) || !std::isfinite(motion.z)) {
                SDL_Log("--inject-motion requires three finite gravity values x,y,z");
                return 1;
            }
            app.host.injectMotion = true;
            app.host.injectedMotion = motion;
            SDL_Log("TEST MODE: simulated accelerometer %.3f,%.3f,%.3f g", motion.x, motion.y,
                    motion.z);
        } else if (a == "--inject-pad") {
            InjectedPad p;
            const std::string v = next();
            SDL_sscanf(v.c_str(), "%f,%f,%u", &p.x, &p.y, &p.buttons);
            const auto at = v.find('@');
            if (at != std::string::npos)
                SDL_sscanf(v.c_str() + at + 1, "%d+%d", &p.start, &p.frames);
            injectPads.push_back(p);
        } else if (a == "--inject-touch") { // testing: "nx,ny[@frame[+frames]]", 0..1 window coords
            InjectedTouch t{0, 0, 5, -1};
            std::string v = next();
            SDL_sscanf(v.c_str(), "%f,%f", &t.x, &t.y);
            size_t at = v.find('@');
            if (at != std::string::npos)
                SDL_sscanf(v.c_str() + at + 1, "%d+%d", &t.start, &t.frames);
            injectTouches.push_back(t);
        }
    }

    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1"); // back arrives as SDLK_AC_BACK
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    std::string prefDir = dataDirArg;
    if (prefDir.empty()) {
#ifdef MINO_STANDALONE_GAME
        char *pref = SDL_GetPrefPath("OpenMinotaur", MINO_STANDALONE_GAME);
#else
        char *pref = SDL_GetPrefPath("minotaur-engine", "fork");
#endif
        prefDir = pref ? pref : "./fork-data/";
        SDL_free(pref);
    }
    if (prefDir.back() != '/' && prefDir.back() != '\\')
        prefDir += '/';
    if (!SDL_CreateDirectory(prefDir.c_str())) {
        SDL_Log("Cannot create data directory %s: %s", prefDir.c_str(), SDL_GetError());
        SDL_Quit();
        return 1;
    }
    const std::string savesRoot = prefDir + "saves/";
    SDL_CreateDirectory(savesRoot.c_str());
    app.prefDir = prefDir;
    app.savesRoot = savesRoot;
    app.host.achievementPath = prefDir + "achievements.txt";
    if (!app.host.achievements.load(app.host.achievementPath))
        SDL_Log("Achievement file unreadable or unsupported; preserving it without overwrite.");
#ifndef MINO_STANDALONE_GAME
#ifdef SDL_PLATFORM_ANDROID
    {
        auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
        jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (env && activity) {
            jclass cls = env->GetObjectClass(activity);
            jmethodID method = env->GetMethodID(cls, "usesPortableDisplayDefaults", "()Z");
            bool portable = method && env->CallBooleanMethod(activity, method) == JNI_TRUE;
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                portable = false;
            }
            app.settings.useNativeOrientationDefault(portable);
            env->DeleteLocalRef(cls);
            env->DeleteLocalRef(activity);
        }
    }
#endif
    app.settings.load(prefDir + "settings.ini");
#ifdef SDL_PLATFORM_ANDROID
    // Remember the first-launch choice so rotation or pad changes do not reset it.
    if (app.settings.get("global", "orientation", "").empty()) {
        app.settings.set("global", "orientation",
                         app.settings.display("global").orientation == Orientation::BestFit
                             ? "bestfit"
                             : "landscape");
        app.settings.save();
    }
#endif
    for (auto &o : overrides) { // --set [section.]key=value, applied for this run
        size_t eq = o.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = o.substr(0, eq), section = "global";
        size_t dot = key.find('.');
        if (dot != std::string::npos) {
            section = key.substr(0, dot);
            key = key.substr(dot + 1);
        }
        app.settings.set(section, key, o.substr(eq + 1));
    }

#endif
    if (list) {
        SDL_Log("data folder: %s", prefDir.c_str());
#ifdef MINO_STANDALONE_GAME
        SDL_Log("standalone: %s", MINO_STANDALONE_GAME);
#else
        int count = 0;
        const auto *cores = coreRegistry(&count);
        for (int i = 0; i < count; ++i)
            SDL_Log("game: %s", cores[i].info->id);
#endif
        SDL_Quit();
        return 0;
    }

    // Normal startup always opens iOX; --game remains a direct testing path.
    if (!gameArg.empty() && gameArg != "launcher" && !findCore(gameArg)) {
        SDL_Log("unknown game id '%s'", gameArg.c_str());
        SDL_Quit();
        return 1;
    }
#ifdef MINO_STANDALONE_GAME
    const auto *standalone = findCore(gameArg);
    if (!standalone) {
        SDL_Quit();
        return 1;
    }
    app.core = standalone->create();
    app.gameId = gameArg;
#else
    app.core = &app.launcher;
    app.gameId = "launcher";
#endif
    applyOrientation(app);

    SDL_WindowFlags flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef SDL_PLATFORM_ANDROID
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    app.window = SDL_CreateWindow(app.core->info().title, winW, winH, flags);
    if (!app.window || !createContext(app, preferES))
        return 1;
    SDL_GL_SetSwapInterval(1);

    if (!app.display.init(app.glES))
        return 1;
    app.input.init();
#ifndef MINO_STANDALONE_GAME
    loadMappings(app);
    enumerateResolutions(app);
    refreshOptions(app);
    applyWindow(app);
    app.launcher.setPaths(prefDir);
    app.launcher.setAchievements(app.host.achievements);
    app.launcher.selectGame(app.settings.get("global", "last_game", "minorescue"));
    reconfigure(app, false);
    if (!app.launcher.init(app.host, app.display.surface())) {
        SDL_Log("launcher init failed");
        app.input.shutdown();
        app.display.shutdown();
        SDL_GL_DestroyContext(app.gl);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
    app.coreReady = true;
#endif
    int exitCode = 0;
#ifdef MINO_STANDALONE_GAME
    if (!startGame(app, gameArg)) {
        SDL_Log("Standalone game initialization failed");
        app.quit = true;
        exitCode = 1;
    }
#else
    if (!gameArg.empty() && gameArg != "launcher" && !startGame(app, gameArg) &&
        !screenshot.empty()) {
        app.quit = true;
        exitCode = 1;
    }
#endif

#ifdef MINO_COLLECTION_TITLE
    if (gameArg.empty() && !app.quit)
        startCollectionTitle(app);
#endif
    Uint64 last = SDL_GetTicksNS();
    SDL_AddEventWatch(lifecycleWatch, &app);
    int frameCount = 0;
    while (!app.quit) {
        if (app.returnRequested && !app.paused && SDL_GL_GetCurrentContext()) {
#ifdef MINO_STANDALONE_GAME
            app.quit = true;
            break;
#else
            showLauncher(app);
#endif
        }
        refreshNativeFormat(app);
        app.input.setTouchEnabled(
            !gameInputSettings(app).disableTouch || app.input.connectedPads() == 0, app.core);
        for (size_t k = 0; k < injectTouches.size(); k++) { // synthetic fingers for --inject-touch
            const InjectedTouch &t = injectTouches[k];
            bool down = frameCount == t.start,
                 up = t.frames >= 0 && frameCount == t.start + t.frames;
            if (!down && !up)
                continue;
            SDL_Event f{};
            f.type = down ? SDL_EVENT_FINGER_DOWN : SDL_EVENT_FINGER_UP;
            f.tfinger.fingerID = SDL_FingerID(1000 + k);
            f.tfinger.x = t.x;
            f.tfinger.y = t.y;
            f.tfinger.windowID = SDL_GetWindowID(app.window);
            SDL_PushEvent(&f);
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (SDL_GL_GetCurrentContext() &&
                (SDL_GL_GetCurrentContext() != app.gl ||
                 (app.graphicsDirty && !app.display.graphicsValid()))) {
                if (!synchronizeGraphics(app)) {
                    app.quit = true;
                    exitCode = 1;
                    break;
                }
            }
            if (!app.paused && SDL_GL_GetCurrentContext())
                refreshNativeFormat(app);
            switch (e.type) {
            case SDL_EVENT_QUIT:
                app.quit = true;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESIZED:
                if (!app.paused && SDL_GL_GetCurrentContext())
                    reconfigure(app);
                else
                    app.graphicsDirty = true;
                break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
            case SDL_EVENT_WINDOW_MINIMIZED:
                setPaused(app, true);
                break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
            case SDL_EVENT_WINDOW_RESTORED:
                app.graphicsDirty = true;
                last = SDL_GetTicksNS();
                setPaused(app, false);
                break;
            case SDL_EVENT_RENDER_DEVICE_RESET:
                app.graphicsDirty = true;
                if (!synchronizeGraphics(app)) {
                    app.quit = true;
                    exitCode = 1;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
#ifdef SDL_PLATFORM_WINDOWS
                if (fullscreenShortcut(app, e.key))
                    break;
#endif
                if (app.paused || app.returnRequested)
                    break;
#ifndef MINO_STANDALONE_GAME
                if (app.mapping.active()) {
                    if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK) {
                        app.mapping.cancel();
                        app.launcher.mappingFinished(
                            "Mapping cancelled. Previous controls retained.");
                        app.gatePads = true;
                    }
                    break;
                }
#endif
                if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK) {
                    if (!e.key.repeat && !app.core->back())
                        app.returnRequested = true;
                } else if (app.gameId == "title" && !e.key.repeat &&
                           (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE ||
                            e.key.key == SDLK_A)) {
                    app.core->back();
                }
#ifndef MINO_STANDALONE_GAME
                else if (app.core == &app.launcher) {
                    app.launcher.key(e.key.key == SDLK_LEFT    ? -1
                                     : e.key.key == SDLK_RIGHT ? 1
                                                               : 0,
                                     e.key.key == SDLK_UP     ? -1
                                     : e.key.key == SDLK_DOWN ? 1
                                                              : 0,
                                     !e.key.repeat &&
                                         (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE ||
                                          e.key.key == SDLK_A),
                                     !e.key.repeat && e.key.key == SDLK_B);
                }
#endif
                break;
            default:
                if (!app.paused && !app.returnRequested)
                    app.input.handleEvent(e, app.window, app.display, app.core);
                else if (e.type == SDL_EVENT_GAMEPAD_ADDED || e.type == SDL_EVENT_GAMEPAD_REMOVED)
                    app.input.handleEvent(e, app.window, app.display, nullptr);
                break;
            }
        }

#ifndef MINO_STANDALONE_GAME
        if (app.returnRequested)
            showLauncher(app);
#else
        if (app.returnRequested) {
            app.quit = true;
            break;
        }
#endif
        Uint64 now = SDL_GetTicksNS();
        double dt = double(now - last) / 1e9;
        last = now;
        if (app.paused) {
            SDL_Delay(50);
            continue;
        }
        if (!synchronizeGraphics(app)) {
            SDL_Delay(16);
            continue;
        }

        const auto inputSettings = gameInputSettings(app);
        app.input.updatePads(inputSettings.deadzone);
        app.input.setTouchEnabled(!inputSettings.disableTouch || app.input.connectedPads() == 0,
                                  app.core);
#ifndef MINO_STANDALONE_GAME
        refreshOptions(app);
        if (app.mapping.active()) {
            app.mapping.update(app.input.physical(app.mapping.player()));
            app.launcher.mappingProgress(app.mapping.step(), app.mapping.waitingForRelease());
            if (app.mapping.complete()) {
                app.settings.set("global", "mapping" + std::to_string(app.mapping.player() + 1),
                                 app.mapping.mapping().encode());
                app.settings.save();
                loadMappings(app);
                app.mapping.cancel();
                app.launcher.mappingFinished("Controller mapping saved.");
                app.gatePads = true;
            } else if (!app.mapping.active()) {
                app.launcher.mappingFinished(
                    "Mapping cancelled or controller disconnected. Previous controls retained.");
                app.gatePads = true;
            }
        }
#endif
        std::array<PadState, kMaxPads> neutral{};
        std::array<PadState, kMaxPads> nativePads{};
        bool held = false;
        for (int i = 0; i < kMaxPads; ++i) {
            const PadState &p = app.input.pads()[i];
            neutral[size_t(i)].connected = p.connected;
#ifdef MINO_STANDALONE_GAME
            nativePads[size_t(i)] = p;
#else
            nativePads[size_t(i)] = inputSettings.rotateControls && app.core != &app.launcher
                                        ? app.display.nativePad(p)
                                        : p;
#endif
            held |= p.connected && (p.buttons || std::abs(p.lx) > 0.25f || std::abs(p.ly) > 0.25f ||
                                    std::abs(p.rx) > 0.25f || std::abs(p.ry) > 0.25f ||
                                    p.lt > 0.25f || p.rt > 0.25f);
        }
        if (!injectPads.empty()) {
            // Explicit test mode replaces physical pads for reproducibility;
            // dispatch still crosses the normal Core::pads direct-input API.
            nativePads.fill({});
            neutral.fill({});
            held = false;
            auto &p = nativePads[0];
            p.connected = true;
            for (const auto &event : injectPads)
                if (frameCount >= event.start && frameCount < event.start + event.frames) {
                    p.lx = event.x;
                    p.ly = event.y;
                    p.buttons |= event.buttons;
                    if (frameCount == event.start)
                        p.pressed |= event.buttons;
                }
            held = p.buttons || std::abs(p.lx) > .25f || std::abs(p.ly) > .25f;
            neutral[0].connected = true;
        }
        if (app.gatePads && !held)
            app.gatePads = false;
        app.core->pads(app.gatePads || app.mapping.active() ? neutral.data() : nativePads.data(),
                       kMaxPads);
        refreshNativeFormat(app);

#ifndef MINO_STANDALONE_GAME
        if (app.core == &app.launcher) {
            const auto request = app.launcher.takeRequest();
            if (request.action == Launcher::Action::Launch) {
                startGame(app, request.gameId, true);
                last = SDL_GetTicksNS();
                dt = 1.0 / 60; // Transition/loading is not simulation catch-up.
            } else if (request.action == Launcher::Action::Quit)
                app.quit = true;
            else if (request.action == Launcher::Action::SettingsChanged) {
                app.input.cancelTouches(app.core);
                app.gatePads = true;
                applyWindow(app);
                applyOrientation(app);
                reconfigure(app);
            } else if (request.action == Launcher::Action::Remap) {
                int player = SDL_atoi(request.gameId.c_str());
                if (player >= 0 && player < kMaxPads && app.input.physical(player).connected)
                    app.mapping.begin(player);
                else
                    app.launcher.mappingFinished(
                        "Connect a controller in that player's slot and try again.");
                app.gatePads = true;
            } else if (request.action == Launcher::Action::CancelRemap) {
                app.mapping.cancel();
                app.launcher.mappingFinished("Mapping cancelled. Previous controls retained.");
                app.gatePads = true;
            } else if (request.action == Launcher::Action::CalibrateTilt) {
                app.input.cancelTouches(app.core);
                app.gatePads = true;
                if (app.input.enableMotion(true))
                    app.launcher.beginTiltCalibration();
                else
                    app.launcher.showMessage("Tilt Calibration",
                                             "No accelerometer is available on this device.");
            } else if (request.action == Launcher::Action::ConfirmTilt) {
                const bool ok = app.input.calibrateTilt();
                app.input.enableMotion(false);
                if (ok) {
                    app.settings.set("global", "tilt_neutral", app.input.tiltCalibration());
                    app.settings.save();
                }
                app.gatePads = true;
                app.launcher.showMessage(
                    "Tilt Calibration",
                    ok ? "Neutral position saved for all games."
                       : "Could not read steady gravity. Hold the device still and try again.");
            } else if (request.action == Launcher::Action::CancelTilt) {
                app.input.enableMotion(false);
                app.gatePads = true;
            } else if (request.action == Launcher::Action::ResetMapping) {
                for (int i = 1; i <= kMaxPads; ++i)
                    app.settings.set("global", "mapping" + std::to_string(i), "");
                app.settings.save();
                loadMappings(app);
                app.gatePads = true;
                app.launcher.showMessage("Controller mapping",
                                         "All controller mappings restored to default.");
            } else if (request.action == Launcher::Action::Donate) {
                app.input.cancelTouches(app.core);
                app.launcher.pause(true);
                app.gatePads = true;
                if (!SDL_OpenURL(kDonationUrl))
                    app.launcher.showMessage("Open donation page", std::string("Please visit ") +
                                                                       kDonationUrl +
                                                                       " in a web browser.");
            }
        }
#endif
        if (app.quit)
            break;
        if (!synchronizeGraphics(app)) {
            SDL_Delay(16);
            continue;
        }
        Uint64 titleFramePeriod = 0;
#ifdef MINO_COLLECTION_TITLE
        if (app.gameId == "title" && screenshot.empty())
            titleFramePeriod = static_cast<CollectionTitle *>(app.core)->framePeriodNs();
#endif
        const Uint64 titleFrameStart = titleFramePeriod ? SDL_GetTicksNS() : 0;
        app.display.beginFrame();
        app.core->frame(screenshot.empty() ? dt : 1.0 / 60.0);
        app.display.present();
        if (app.host.achievementWritePending)
            app.host.flushAchievements();

        ++frameCount;
        if (!screenshot.empty() && frameCount >= shotFrames) {
            SDL_Log(saveScreenshot(app, screenshot.c_str()) ? "screenshot written: %s"
                                                            : "screenshot failed: %s",
                    screenshot.c_str());
            app.quit = true;
        }
        SDL_GL_SwapWindow(app.window);
        if (titleFramePeriod) {
            const Uint64 spent = SDL_GetTicksNS() - titleFrameStart;
            if (spent < titleFramePeriod)
                SDL_DelayPrecise(titleFramePeriod - spent);
        }
    }

    SDL_RemoveEventWatch(lifecycleWatch, &app);
#ifndef MINO_STANDALONE_GAME
    app.launchTransition.cancel();
#endif
    stopGame(app);
#ifndef MINO_STANDALONE_GAME
    app.launcher.shutdown();
#endif
    app.core = nullptr;
#ifndef MINO_STANDALONE_GAME
    app.settings.save();
#endif
    app.input.shutdown();
    app.display.shutdown();
    SDL_GL_DestroyContext(app.gl);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return exitCode;
}
#endif
