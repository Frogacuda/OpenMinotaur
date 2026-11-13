#pragma once
#include "vector/vector_font.h"
#include "vector/vector_scene.h"
namespace neon::effects {
struct OverlayText {
    std::string text;
    vectors::TextStyle style{};
    vectors::Point position{.5f, .5f}; // normalized native canvas, before game's final turn
    vectors::Point offset{};           // native output pixels
    bool enabled = true, bezelOnly = false;
};
struct OverlayTextStatistics {
    size_t entries = 0, strokes = 0, missingGlyphs = 0, droppedEntries = 0, layoutBuilds = 0;
    uint64_t totalLayoutBuilds = 0;
};
struct OverlayTextFrame {
    std::vector<vectors::Stroke> overGame, bezel;
};
// Text primitives and font support only. No score/achievement/widget logic.
class OverlayTextLayer {
  public:
    vectors::FontRegistry &fonts() {
        return fonts_;
    }
    const vectors::FontRegistry &fonts() const {
        return fonts_;
    }
    void set(const std::string &id, OverlayText);
    bool remove(const std::string &id);
    void clear();
    const OverlayText *find(const std::string &id) const;
    const OverlayTextFrame &prepare(int nativeWidth, int nativeHeight);
    const OverlayTextStatistics &statistics() const {
        return stats_;
    }
    vectors::BeamStyle &beamStyle() {
        return beam_;
    }
    void write(std::ostream &) const;
    void read(std::istream &); // transactional; fonts must be registered first
  private:
    struct Entry {
        OverlayText text;
        vectors::TextLayout layout;
        uint64_t fontRevision = 0;
        bool dirty = true;
    };
    std::map<std::string, Entry> entries_;
    vectors::FontRegistry fonts_;
    OverlayTextFrame frame_;
    OverlayTextStatistics stats_{};
    size_t pendingBuilds_ = 0;
    vectors::BeamStyle beam_{1.5f, 1.5f, .28f, 3.f, 0, 1.f, .15f};
};
} // namespace neon::effects
