// Coordinates inside the iOX canvas. The display host owns output aspect and
// letterboxing; changing those must not create a second hit-testing transform.
#pragma once
#include "launcher_layout.h"
#include <algorithm>

namespace mino::iox {
struct CanvasTransform {
    float scaleX, scaleY;
    CanvasTransform(const LauncherLayout &canvas, int surfaceWidth, int surfaceHeight)
        : scaleX(float(std::max(surfaceWidth, 1)) / canvas.width),
          scaleY(float(std::max(surfaceHeight, 1)) / canvas.height) {}
    float canvasX(float surfaceX) const {
        return surfaceX / scaleX;
    }
    float canvasY(float surfaceY) const {
        return surfaceY / scaleY;
    }
};
} // namespace mino::iox
