// core_api.h - the contract between the shared platform layer and each game core.
//
// The platform owns the window, GL context, display filtering, input devices,
// audio device, settings and file locations. A core owns one game's state and
// shared-engine service instances, rendering into an offscreen surface the platform
// provides. Cores never talk to SDL directly.
#pragma once
#include "neon/device_motion.h"
#include "neon/render_host.h"
#include "neon/input.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace mino {

constexpr int kMaxPads = neon::input::MaxControllers;
constexpr int kAudioRate = 44100; // stereo, interleaved float, fixed

// ---------------------------------------------------------------- input ----

// Positional names are canonical. A/B/X/Y remain source aliases for existing
// modules and configuration bits; they do not identify controller hardware.
enum PadButton : uint32_t {
    PAD_SOUTH = neon::input::South,
    PAD_EAST = neon::input::East,
    PAD_WEST = neon::input::West,
    PAD_NORTH = neon::input::North,
    PAD_A = PAD_SOUTH,
    PAD_B = PAD_EAST,
    PAD_X = PAD_WEST,
    PAD_Y = PAD_NORTH,
    PAD_LB = neon::input::LeftShoulder,
    PAD_RB = neon::input::RightShoulder,
    PAD_START = neon::input::Start,
    PAD_BACK = neon::input::Back,
    PAD_UP = neon::input::Up,
    PAD_DOWN = neon::input::Down,
    PAD_LEFT = neon::input::Left,
    PAD_RIGHT = neon::input::Right,
    PAD_LSTICK = neon::input::LeftStick,
    PAD_RSTICK = neon::input::RightStick
};
// Stable player slots; disconnected devices supply a neutral state. Axes have
// the user dead zone applied by the platform, before game-specific response.
using PadState = neon::input::ControllerState;
using MotionState = neon::input::MotionState;

using TouchPhase = neon::input::TouchPhase;

// Touch positions are mapped into surface pixels (origin top-left) but NOT
// clamped: touches in the letterbox bars arrive with coordinates outside
// 0..width / 0..height, extending the touch area into the bars. Cores that
// need on-screen geometry (menus) just treat those as misses.
using TouchEvent = neon::input::TouchEvent;

// --------------------------------------------------------------- render ----

// The offscreen surface a core draws into. framebuffer is the GL FBO the core
// must treat as its "screen" (where the original bound renderbuffer 0 / the
// EAGL view's framebuffer). The platform scales it to the display afterwards.
struct SurfaceInfo {
    int width = 0, height = 0;
    uint32_t framebuffer = 0;
    bool glES = false; // true: OpenGL ES 3 context, false: desktop GL 3.3 core
    // Upright displayed game aspect, independent of render resolution/filter.
    // Zero allows standalone hosts to fall back to the surface dimensions.
    float displayAspect = 0;
};

// --------------------------------------------------------------- host ------

// Opt-in asynchronous file selection. Callback ownership survives a core being
// closed; modules poll the result on their own thread under this mutex.
struct FileSelection {
    std::mutex mutex;
    bool done = false;
    std::string path;
};
class Host : public neon::RenderHost {
  public:
    virtual ~Host() = default;

    // Read-only game resources supplied by the host (atlas, WAVs, XML...).
    virtual bool readAsset(const std::string &name, std::vector<uint8_t> &out) const = 0;
    // Writable per-game directory for saves and high scores (ends in '/').
    virtual std::string saveDir() const = 0;

    virtual void log(const char *fmt, ...) = 0;

    // Translate a GLSL ES 1.00 shader (as written for the original iOS engine)
    // into whatever the current context accepts. stage: 0 = vertex, 1 = fragment.
    virtual std::string adaptShader(const std::string &src, int stage) const = 0;

    // Guard state shared with mixAudio(), which runs on the audio thread.
    virtual void lockAudio() = 0;
    virtual void unlockAudio() = 0;

    // Leave the game: return to the collection or close a standalone application.
    virtual void requestQuit() = 0;
    // Main/core thread only. Lifetime tracking must never gate gameplay or local
    // notifications. The default leaves achievement records unchanged.
    // The host supplies the catalog game namespace; IDs must remain permanent.
    virtual void unlockAchievement(const std::string &) {}
    // Presentation only: collection records for the current game namespace.
    // Never seed original eligibility/session gates from this lifetime mask.
    virtual uint32_t originalAchievementDisplayMask() const {
        return 0;
    }
    // Original goal index N has stable local ID "original-N". Observe the full
    // source mask, not just a mailbox byte that multiple awards can overwrite.
    void observeOriginalAchievements(uint32_t bits) {
        for (unsigned n = 0; n < 32; ++n)
            if (bits & (uint32_t(1) << n))
                unlockAchievement("original-" + std::to_string(n));
    }
    // During synchronous initialization, present a startup image already drawn
    // by the core before loading the remaining assets. No timer/thread implied.
    virtual bool presentStartupFrame() {
        return false;
    }
    // Optional device accelerometer. A core releases it on shutdown. Hosts
    // without sensors return false and provide an unavailable motion sample.
    virtual bool enableMotion(bool) {
        return false;
    }
    // Already corrected for OS/device orientation and the presentation turn.
    // Modules consume native game axes (X right, Y up), independent of the
    // controller-rotation preference; they must not rotate this again.
    virtual MotionState motion() const {
        return {};
    }
    // Effective touchscreen availability, including the launcher touch policy.
    // A mouse alone is not a touchscreen. Hosts explicitly report touchscreen availability.
    virtual bool touchControlsAvailable() const {
        return false;
    }
    // Menu focus capability only; it does not create a gamepad or gameplay input.
    virtual bool keyboardNavigationAvailable() const {
        return false;
    }
    virtual std::shared_ptr<FileSelection> chooseDataFile() {
        return {};
    }
    virtual bool readDataFile(const std::string &, std::vector<uint8_t> &, size_t) const {
        return false;
    }
};

// --------------------------------------------------------------- core ------

struct CoreInfo {
    const char *id;                          // "minotron"
    const char *title;                       // "Minotron: 2112"
    int baseWidth, baseHeight;               // native surface at 1x, e.g. 480x320 or 480x640
    float minAspect, maxAspect;              // width/height range the game lays out correctly
    const char *atlasFile;                   // primary atlas resource name
    const char *assetGameId = nullptr;       // optional shared resource identity
    const char *saveGameId = nullptr;        // optional stable save identity across core renames
    const char *achievementGameId = nullptr; // optional shared catalog achievement identity
};

// Runtime presentation geometry, independent of the game/asset/save identity.
// Some cores may change this at a game-specific presentation boundary.
// Separately launched Gridrunner variants each retain their own fixed format.
// The platform observes changes between frames; cores never resize the window
// or override the player's orientation preference themselves.
struct NativeFormat {
    int width, height;
    float minAspect, maxAspect;
    bool operator==(const NativeFormat &b) const {
        return width == b.width && height == b.height && minAspect == b.minAspect &&
               maxAspect == b.maxAspect;
    }
    bool operator!=(const NativeFormat &b) const {
        return !(*this == b);
    }
};

class Core {
  public:
    virtual ~Core() = default;
    virtual const CoreInfo &info() const = 0;
    virtual NativeFormat nativeFormat() const {
        const auto &i = info();
        return {i.baseWidth, i.baseHeight, i.minAspect, i.maxAspect};
    }

    // GL context is current for init/resize/frame/shutdown.
    virtual bool init(Host &host, const SurfaceInfo &surface) = 0;
    virtual void resize(const SurfaceInfo &surface) = 0;
    virtual void frame(double dt) = 0; // update + draw into surface
    virtual void touch(const TouchEvent &e) = 0;
    virtual void pads(const PadState *pads, int count) = 0; // once per frame
    virtual void mixAudio(float *out, int frames) = 0;      // audio thread
    virtual void pause(bool paused) = 0;                    // app backgrounded / menu open
    virtual void save() = 0;                                // persist now (may be killed)
    // Esc / Android back: true if the game used it (e.g. paused itself);
    // otherwise the platform leaves the game.
    virtual bool back() {
        return false;
    }
    virtual void shutdown() = 0;
};

struct CoreEntry {
    const CoreInfo *info;
    Core *(*create)();
};

// Defined in platform/core_registry.cpp: every core compiled into this build.
const CoreEntry *coreRegistry(int *count);
const CoreEntry *findCore(const std::string &id);

} // namespace mino
