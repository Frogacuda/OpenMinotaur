// display.h - owns the game's offscreen surface and presents it to the window:
// aspect-preserving letterbox (fit or integer), selectable filter, and the
// inverse mapping used to turn window touches into surface coordinates.
#pragma once
#include "core_api.h"
#include "display_layout.h"
#include "settings.h"
#ifndef MINO_NO_PRESENTATION_FILTERS
#include "royale_pipeline.h"
#endif

#include <functional>
#include <string>

namespace mino {

class Display {
  public:
    bool init(bool glES);
    void shutdown();
    void abandonGraphics();
    bool graphicsValid() const;

    // Recompute surface size and letterbox for the current window and settings.
    // Returns true if the surface or input geometry changed; notify resize().
    // The launcher owns the optional Minorola texture and shares it with the launch transition.
    bool configure(int windowW, int windowH, NativeFormat native, const DisplaySettings &s,
                   unsigned minorolaTexture = 0);

    const SurfaceInfo &surface() const {
        return surface_;
    }
    const std::string &error() const {
        return error_;
    }
    unsigned gameplayTexture() const {
        return colorTex_;
    }
    const Rect &viewport() const {
        return view_;
    }
    int presentationTurns() const {
        return layout_.turns;
    }
    const DisplayLayout &presentationLayout() const {
        return layout_;
    }
    unsigned minorolaTexture() const {
        return minorolaTexture_;
    }

    // Bind the surface FBO and its viewport, ready for the core to draw.
    void beginFrame();
    // Draw the surface into the window's default framebuffer.
    void present(unsigned framebuffer = 0);
    bool liveBackgroundRequested() const {
        return backdropMode_ == Backdrop::Neon && (view_.w < winW_ || view_.h < winH_);
    }
    bool neonSelected() const {
        return backdropMode_ == Backdrop::Neon;
    }
    using BackgroundPass = std::function<bool(unsigned, int, int, int, int, int, int, int)>;
    void backgroundPass(BackgroundPass pass) {
        backgroundPass_ = std::move(pass);
    }
    void bezelOverlay(BackgroundPass pass) {
        bezelOverlay_ = std::move(pass);
    }

    // Window pixel position -> surface pixel position (unclamped; may lie outside).
    void windowToSurface(float wx, float wy, float *sx, float *sy) const;
    void setBezel(int width, int height, const unsigned char *rgba, bool tiled = true);
    MotionState nativeMotion(MotionState value) const {
        return layout_.nativeMotion(value);
    }
    PadState nativePad(PadState p) const {
        return layout_.nativePad(p);
    }

    std::string adaptShader(const std::string &src, int stage) const;

  private:
    friend struct RoyaleChecks;
    void createSurface(int w, int h);
    void destroySurface();
    unsigned compile(const char *vs, const char *fs);

    BackgroundPass backgroundPass_;
    BackgroundPass bezelOverlay_;
    bool es_ = false;
    bool directCRT_ = false;
    SurfaceInfo surface_;
    unsigned colorTex_ = 0, depthRb_ = 0;
    unsigned presentProg_ = 0, quadVbo_ = 0, vao_ = 0;
    int uTex_ = -1, uTexSize_ = -1, uOutSize_ = -1, uSharp_ = -1, uClockwise_ = -1;
    int uTime_ = -1, uPass_ = -1, uMode_ = -1, uScanlines_ = -1;
    int uPortraitScreen_ = -1, uRasterSize_ = -1;
    DisplayLayout layout_;
#ifndef MINO_NO_PRESENTATION_FILTERS
    TubeLayout tube_;
    RoyalePipeline royale_;
#endif
    std::string error_;
    Rect view_;
    int winW_ = 0, winH_ = 0;
    ScreenMode screenMode_ = ScreenMode::HiRes;
    Backdrop backdropMode_ = Backdrop::Colour;
    int backdropTurns_ = 0;
    bool bezelTiled_ = true;
    unsigned bezelTex_ = 0;
    unsigned minorolaTexture_ = 0;
    int bezelW_ = 1, bezelH_ = 1;
    std::string bezelKey_;
    Filter filter_ = Filter::SharpBilinear;
    uint32_t backdrop_ = 0; // letterbox colour, 0xRRGGBB
};

} // namespace mino
