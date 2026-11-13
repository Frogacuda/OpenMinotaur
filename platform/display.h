// display.h - owns the game's offscreen surface and presents it to the window:
// aspect-preserving letterbox (fit or integer), selectable filter, and the
// inverse mapping used to turn window touches into surface coordinates.
#pragma once
#include "core_api.h"
#include "settings.h"
#include "display_layout.h"
#ifndef MINO_NO_PRESENTATION_FILTERS
#include "royale_pipeline.h"
#endif

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
    bool configure(int windowW, int windowH, NativeFormat native, const DisplaySettings &s);

    const SurfaceInfo &surface() const {
        return surface_;
    }
    const std::string &error() const {
        return error_;
    }
    const Rect &viewport() const {
        return view_;
    }

    // Bind the surface FBO and its viewport, ready for the core to draw.
    void beginFrame();
    // Draw the surface into the window's default framebuffer.
    void present(unsigned framebuffer = 0);

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

    bool es_ = false;
    bool directCRT_ = false;
    SurfaceInfo surface_;
    unsigned colorTex_ = 0, depthRb_ = 0;
    unsigned presentProg_ = 0, quadVbo_ = 0, vao_ = 0;
    int uTex_ = -1, uTexSize_ = -1, uOutSize_ = -1, uSharp_ = -1, uClockwise_ = -1;
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
    int bezelW_ = 1, bezelH_ = 1;
    std::string bezelKey_;
    Filter filter_ = Filter::SharpBilinear;
    uint32_t backdrop_ = 0; // letterbox colour, 0xRRGGBB
};

} // namespace mino
