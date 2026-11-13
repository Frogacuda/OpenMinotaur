#include "original_frontend.h"
#include <cmath>
namespace gridrunner {
int hitButton(const std::vector<UiButton> &buttons, double x, double y) {
    for (size_t i = buttons.size(); i > 0; --i) {
        const auto &b = buttons[i - 1];
        const double margin = b.label || b.action == UiAction::Acceleration ? 1 : 1.5;
        if (b.alpha > .1 && std::abs(x - b.x) < b.rx * margin && std::abs(y - b.y) < b.ry * margin)
            return int(i - 1);
    }
    return -1;
}
int moveFocus(const std::vector<UiButton> &buttons, int current, int dx, int dy) {
    if (buttons.empty())
        return -1;
    if (current < 0 || current >= int(buttons.size()))
        return 0;
    const auto &from = buttons[size_t(current)];
    double best = 1e9;
    int pick = current;
    for (size_t i = 0; i < buttons.size(); ++i) {
        const auto &b = buttons[i];
        if (b.alpha <= .1)
            continue;
        double x = b.x - from.x, y = b.y - from.y, along = x * dx + y * dy;
        if (along < .001)
            continue;
        double perpendicular = std::abs(x * dy - y * dx), cost = along + perpendicular * 3;
        if (cost < best) {
            best = cost;
            pick = int(i);
        }
    }
    return pick;
}
} // namespace gridrunner
