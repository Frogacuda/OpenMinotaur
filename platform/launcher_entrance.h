// Home choreography, expressed in canvas space and seconds, independent of GL.
#pragma once
#include "launcher_layout.h"
#include <algorithm>
#include <cmath>

namespace mino::iox {
struct LayerTransform {
    float anchorX = 0, anchorY = 0, scale = 1, dx = 0, dy = 0;
    MenuRect apply(MenuRect r) const {
        return {anchorX + (r.x - anchorX) * scale + dx, anchorY + (r.y - anchorY) * scale + dy,
                r.w * scale, r.h * scale};
    }
};

class HomeEntrance {
  public:
    static constexpr double iconsSeconds = 1.0, barsSeconds = 0.5;
    static constexpr double barsStart = iconsSeconds - barsSeconds;
    void start() {
        elapsed_ = 0;
    }
    void seek(double seconds) {
        elapsed_ = std::clamp(seconds, 0.0, iconsSeconds);
    }
    void finish() {
        elapsed_ = iconsSeconds;
    }
    bool active() const {
        return elapsed_ < iconsSeconds;
    }
    double elapsed() const {
        return elapsed_;
    }
    void advance(double dt) {
        if (std::isfinite(dt) && dt > 0)
            elapsed_ = std::min(iconsSeconds, elapsed_ + dt);
        // Repeated 1/60 steps must finish on the 60th frame despite FP rounding.
        if (elapsed_ > iconsSeconds - 1e-9)
            finish();
    }
    float iconsProgress() const {
        const float remaining = 1 - float(std::clamp(elapsed_ / iconsSeconds, 0.0, 1.0));
        return 1 - remaining * remaining * remaining; // Shared cubic ease-out; no overshoot.
    }
    float barsProgress() const {
        return float(std::clamp((elapsed_ - barsStart) / barsSeconds, 0.0, 1.0));
    }
    LayerTransform dock(const LauncherLayout &layout) const {
        const float remaining = 1 - iconsProgress();
        return layout.portrait
                   ? LayerTransform{0, 0, 1, 0, (layout.height - layout.dock.y + 16) * remaining}
                   : LayerTransform{0, 0, 1, (layout.width - layout.dock.x + 16) * remaining, 0};
    }
    LayerTransform icon(const LauncherLayout &layout, int index) const {
        if (index >= 9)
            return dock(layout); // Dock and its icons form one rigid layer.
        const auto &target = layout.items[size_t(index)];
        const int row = index / 3, column = index % 3;
        const float remaining = 1 - iconsProgress();
        float originX, originY;
        if (layout.portrait) {
            // Diagram: origins span three display widths; row spacing is 3/4
            // of the display height. Keep even enlarged labels/shadows outside.
            originX = (float(column) - 0.5f) * layout.width;
            originY =
                std::min((-1.625f + float(row) * 0.75f) * layout.height, -target.h * 0.75f - 32);
        } else {
            // Same spread on the horizontal axis: icons enter from the left.
            originX =
                std::min((-1.625f + float(column) * 0.75f) * layout.width, -target.w * 0.75f - 32);
            originY = (float(row) - 0.5f) * layout.height;
        }
        return {target.cx(), target.cy(), 1 + 0.5f * remaining, (originX - target.cx()) * remaining,
                (originY - target.cy()) * remaining};
    }

  private:
    double elapsed_ = iconsSeconds;
};
} // namespace mino::iox
