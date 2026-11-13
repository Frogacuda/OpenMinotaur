#pragma once
#include "stage_camera.h"
#include "stage_runtime.h"

namespace goatup2 {
// Prepared normalized geometry; the shared renderer scales it into its target.
// Tile UVs address the original 512-square grunt atlas directly. Sprite frames
// already include the source's two whole-page entries.
struct StageDraw {
    neon::vec2d position, size;
    neon::colour colour;
    double angle = 0;
    int frame = 2;
    bool tile = false;
    bool brushTile = false, editorObject = false;
    uint32_t brushWord = 0;
    double u = 0, v = 0, du = 0, dv = 0;
    unsigned flags = 3;
};
// The 3-D point list embedded at original game +119c. This differs from the
// earlier titles' particle stars; it shares the original 2-D sprite submission.
class StageStarfield {
  public:
    struct Point {
        double x, y, z, size;
        uint32_t colour;
        int sprite;
    };
    void reset(neon::DarwinRandom &incomingRandom); // 0x4ca24 -> 0x42e58
    void advance(bool paused);                      // early 0x42f28, independently of live actors
    void setCamera(neon::vec2d p) {
        cameraPosition_ = p;
    }
    void append(std::vector<StageDraw> &, const std::array<neon::colour, 32> &) const;
    const std::vector<Point> &points() const {
        return points_;
    }
    double cameraDepth() const {
        return depth_;
    }

  private:
    std::vector<Point> points_; // 0x4ffb0 depth order; 98 stars and two saucers
    double depth_ = 0;
    neon::vec2d cameraPosition_{0, 0}; // +30/+38; retained when points are rebuilt
};
class StagePresentation {
  public:
    StagePresentation();
    StageCamera camera;
    StageStarfield stars;
    void attach(StageRuntime &, int visibleColumns, int visibleRows);
    void requestCamera(const StageRuntime &);
    // Called once after locomotion's actor preparation, not on host redraws.
    // Tile decay/hazard writes and draw counters belong to this operation.
    void prepare(StageRuntime &);
    void prepareEditor(StageRuntime &, const std::vector<StageDraw> &, bool overview);
    void prepareMenu(StageRuntime &, const std::vector<StageDraw> &, bool preview = false);
    void appendOverlay(const std::vector<StageDraw> &overlay) {
        draws_.insert(draws_.end(), overlay.begin(), overlay.end());
    }
    const std::vector<StageDraw> &draws() const {
        return draws_;
    }
    uint32_t terrainFrames = 0;
    int visibleColumns_ = 0, visibleRows_ = 0;
    bool editor = false, attract = false;
    bool rightHandedTouch = false;

  private:
    std::vector<StageDraw> draws_;
    std::array<neon::colour, 32> tilePalette_;
    std::array<float, 4> phase_{0, 0, 0, 0};
    std::array<neon::vec2d, 4> orbit_;
    bool overview_ = false;
    void actors(StageRuntime &, StageActor *);
    void actor(StageRuntime &, StageActor &);
    void baseDraw(StageActor &, neon::vec2d scale, neon::colour tint);
    void scoreDraw(FloatingScoreActor &);
    void terrain(StageRuntime &, bool preview = false);
    neon::vec2d overviewOrigin(const StageRuntime &) const;
    void hudGraphics(StageRuntime &);
    bool tileWord(StageRuntime &, int column, int row, uint32_t word, StageDraw &,
                  int mutationIndex = -1);
    bool tile(StageRuntime &, int column, int row, StageDraw &);
    void decayTile(StageRuntime &, int physicalIndex, uint32_t raw, bool visible = false);
};
// Source 0x423c8's per-frame palettes, using the maintained engine's matching
// 0x9548 rules. This changes only the first fourteen owner palette slots.
void updateStagePalette(StageRuntime &, uint32_t frame);
} // namespace goatup2
