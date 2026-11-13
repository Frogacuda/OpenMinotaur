// settings.h - global settings with optional per-game overrides, stored as a
// small INI file:  [global] key=value ...  [minotron] key=value ...
//
// Every setting is read through get(game, key): the game's own section
// wins, then [global], then the built-in default. The launcher will edit
// [global] (carried into every game); explicit per-game INI overrides remain available.
//
// Display keys: filter (sharp | bilinear | nearest), scale (fit | integer),
// aspect (original | ipad | fill), internal_scale (1-6 | native),
// orientation (bestfit/native | landscape | portrait), screen_mode
// (hires | smooth | sharp | scanlines | crt | crt2), backdrop (colour | image | neon | widgets).
// Shell keys: window_mode, fullscreen_resolution, wallpaper (0-3).
// Input keys: deadzone, disable_touch, rotate_controls, mapping1 through mapping4, tilt_neutral
// (sensor-native gravity vector).
#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace mino {

enum class Filter { Nearest, Bilinear, SharpBilinear };
enum class ScaleMode { Fit, Integer };
enum class AspectMode { Original, IPad, Fill };
// Historical tokens: Landscape = Horizontal, BestFit/Auto = Native.
enum class Orientation { Auto, BestFit, Landscape, Portrait, PortraitRight };
// What fills the letterbox bars around the game.
enum class Backdrop { Colour, Image, Neon, Widgets };
enum class ScreenMode { HiRes, Smooth, Sharp, Scanlines, CRT, CRT2 };
inline bool isCRT(ScreenMode mode) {
    return mode == ScreenMode::CRT || mode == ScreenMode::CRT2;
}

struct DisplaySettings {
    bool matchViewportPixels = false;
    Filter filter = Filter::SharpBilinear;
    ScaleMode scale = ScaleMode::Fit;
    AspectMode aspect = AspectMode::IPad;
    int internalScale = 2; // surface = base size * N; 0 = match display
    Orientation orientation = Orientation::Auto;
    Backdrop backdrop = Backdrop::Colour;
    uint32_t backdropColour = 0x000000; // 0xRRGGBB
    std::string backdropImage;          // optional custom bezel in the data folder
    ScreenMode screenMode = ScreenMode::HiRes;
};

struct InputSettings {
    float deadzone = 0.20f;
    bool disableTouch = false;
    bool rotateControls = false;
};

class Settings {
  public:
    void useNativeOrientationDefault(bool native) {
        nativeOrientationDefault_ = native;
    }
    bool load(const std::string &path);
    bool save() const;

    // Effective value for a game: per-game override, else global, else default.
    std::string get(const std::string &game, const std::string &key, const std::string &def) const;
    void set(const std::string &section, const std::string &key, const std::string &value);

    DisplaySettings display(const std::string &game) const;
    InputSettings input(const std::string &game) const;

  private:
    bool nativeOrientationDefault_ = false;
    std::string path_;
    std::map<std::string, std::map<std::string, std::string>> sections_;
};

} // namespace mino
