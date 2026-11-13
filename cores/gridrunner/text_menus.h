// One layout for drawing, touch hit regions and directional focus. Game Over
// is deliberately absent: tapping it is the same as tapping the background.
#pragma once
#include "original_frontend.h"
#include <cmath>
#include <string>
namespace gridrunner {
inline constexpr int kNameDelete = 36, kNameDone = 37;
inline std::string nameSlots(const std::string &name, size_t limit) {
    const auto text = name.substr(0, limit);
    return text + std::string(limit - text.size(), '_');
}
inline bool appendName(std::string &name, char letter, size_t limit, int &selected) {
    if (name.size() >= limit)
        return false;
    name += letter;
    if (name.size() == limit)
        selected = kNameDone;
    return true;
}
inline int moveNameFocus(const std::vector<UiButton> &buttons, int current, int dx, int dy) {
    if (dy == 0 && dx != 0 && (current == kNameDelete || current == kNameDone))
        return dx > 0 ? kNameDone : kNameDelete;
    return moveFocus(buttons, current, dx, dy);
}
// iOS setup 3f610 / UI runs 44d10, 451e8, 45368, 458b0.
// Sprite indices include the resource loader's two literal entries.
inline std::vector<UiButton> pauseMenu(bool expertMode, double fade = 1, double aspect = .75) {
    std::vector<UiButton> buttons = {
        {UiAction::Pause, 3, 0, -.2, .08, .08},
        {UiAction::Scores, 98, 0, -.065, .17, .032, 1},
        {UiAction::Title, 99, 0, .04, .17, .032, fade},
        {UiAction::Exit, 83, 0, .145, .06 * 2.4, .06, fade},
        {UiAction::Acceleration, expertMode ? 101 : 100, 0, .295, .08, .08, fade}};
    // Preserve the parent's artwork proportions on Classic's landscape surface.
    for (auto &button : buttons)
        button.rx *= .75 / aspect;
    return buttons;
}
inline std::vector<UiButton> scoreMenu() {
    return {{UiAction::Back, 0, 0, 34. / 36 - .5, .18, .03, 1, 1, "Back"},
            {UiAction::Previous, 0, -.28, 31. / 36 - .5, .19, .03, 1, .85, "Previous"},
            {UiAction::Next, 0, .28, 31. / 36 - .5, .19, .03, 1, .85, "Next"}};
}
inline bool pauseHotspot(double x, double y) {
    return std::abs(x) < .12 && std::abs(y + .2) < .12;
}
} // namespace gridrunner
