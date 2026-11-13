#pragma once
namespace minos::menuArt {
// Appended to the original 96-entry map. Entity sprite numbers omit the two
// whole-page entries; the renderer adds them in the ordinary sprite path.
inline constexpr int NewGame = 94, HighScores = 95, Exit = 96;
inline constexpr int Touch1 = 97, TouchOff = 100, TiltOn = 101, TiltOff = 102;
} // namespace minos::menuArt

namespace minos::menuLayout {
inline constexpr double PauseY = -.18;
inline constexpr double ControlsY = .12;
} // namespace minos::menuLayout
