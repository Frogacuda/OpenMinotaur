#pragma once
#include "widget_runtime.h"

namespace neon::widgets {
// Positions and clips are in oriented, top-left-origin viewport pixels.
// Authored coordinates use equal units on both axes, centered on the screen.
struct WidgetMLLayout {
    Matrix transform;
    Rect screen, gameplay;
    double width = 0, height = 0;
    bool landscape = true, hasGameplay = false;
};
WidgetMLLayout resolveWidgetMLLayout(const Value &document, const Viewport &);
void validateWidgetMLCoordinates(const Value &document);
void validateWidgetMLPlacement(const Value &placement);
// Returns an origin in the current parent's local coordinates. Offsets are
// applied afterwards, so nested group transforms still affect their contents.
std::array<double, 2> widgetMLOrigin(const Value &placement, const WidgetMLLayout &, Matrix parent);
} // namespace neon::widgets
