#pragma once
// Independently authored vector objects. No emulator or platform dependencies.
#include "vector_font.h"
#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace neon::vectors {
class Canvas {
  public:
    explicit Canvas(std::size_t capacity = 512);
    void clear();
    void line(Point, Point, Colour = {}, float energy = 1, float width = 1);
    void dot(Point, Colour = {}, float energy = 1, float width = 1);
    void path(const std::vector<Point> &, bool closed = false, Colour = {}, float energy = 1,
              float width = 1);
    void arc(Point centre, float radius, float start, float sweep, Colour = {}, float energy = 1,
             float width = 1);
    void text(Point, const std::string_view &, float height, Colour = {}, float energy = 1,
              float width = 1);
    void text(Point, std::string_view, const TextStyle &, const FontRegistry & = defaultFonts());
    void object(const Object &);
    const std::vector<Stroke> &strokes() const {
        return strokes_;
    }
    std::size_t dropped() const {
        return dropped_;
    }

  private:
    std::size_t capacity_, dropped_ = 0;
    std::vector<Stroke> strokes_;
};

struct BeamStyle {
    // Reference pixels at 720px output height. Energy is radiance, not a width.
    float width = 2.0f, intensity = 1.5f, halo = .38f, spread = 4.f, persistenceMs = 35.f;
    float colour = 0, whiteHot = .2f;
};
struct Vertex {
    float x, y, along, across, length, width, r, g, b, energy;
};
static_assert(sizeof(Vertex) == 40, "Vector vertex layout must remain portable");
struct MeshStats {
    std::size_t strokes = 0, triangles = 0, historyFrames = 0, dropped = 0;
    double coveredPixels = 0;
};
class BeamMesh {
  public:
    static constexpr std::size_t MaxStrokes = 512, HistoryFrames = 4;
    BeamMesh();
    void reset();
    // Logical object coordinates span 640x480. Stroke widths stay screen relative.
    const std::vector<Vertex> &build(const std::vector<Stroke> &, double seconds, int width,
                                     int height, const BeamStyle &);
    const MeshStats &stats() const {
        return stats_;
    }

  private:
    struct Frame {
        std::vector<Stroke> strokes;
        double time = -1;
    };
    std::array<Frame, HistoryFrames - 1> history_;
    std::vector<Stroke> previous_, combined_;
    std::vector<Vertex> vertices_;
    double lastTime_ = -1, lastSample_ = -1;
    MeshStats stats_{};
    void append(const std::vector<Stroke> &, float weight, int, int, const BeamStyle &);
};
std::vector<Object> readObjects(std::istream &);
void writeObjects(std::ostream &, const std::vector<Object> &);
} // namespace neon::vectors
