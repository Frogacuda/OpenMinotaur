#pragma once
#include "value.h"
#include <array>
#include <memory>
#include <set>
namespace neon::widgets {
struct Rect {
    double x = 0, y = 0, width = 0, height = 0;
};
struct Matrix {
    double a = 1, b = 0, c = 0, d = 1, x = 0, y = 0;
    std::array<double, 2> point(double u, double v) const {
        return {a * u + c * v + x, b * u + d * v + y};
    }
};
struct Viewport {
    int width = 1280, height = 720, turns = 0;
    Rect gameplay{}, safe{};
};
struct Snapshot {
    std::string game;
    uint64_t session = 0, sequence = 0;
    Value::Object values;
};
struct Draw {
    std::string path, kind, asset, font, text, fit = "contain", fillDirection = "right";
    Matrix transform;
    Rect bounds{}, clip{};
    std::array<double, 4> color{1, 1, 1, 1};
    double fill = 1, lineWidth = 1, fontSize = 24;
    Value points, frame;
    std::string align = "left";
    bool closed = false, bezelOnly = false;
};
struct Signal {
    std::string group, name;
};
struct Frame {
    std::vector<Draw> draws;
    std::vector<std::string> signals;
    std::vector<Signal> scopedSignals;
    Value::Object outputs;
    size_t evaluated = 0, instances = 0, truncated = 0;
    double seconds = 0;
};
// CPU-only. document() accepts WidgetML 1 (formatted JSON, .widgetml); see
// modules/live_widgets/README.md for centered units, placement and the host viewport contract.
// Legacy canvas-based documents retain their original coordinate behavior.
// No light-synth response bank, oscillator, game code, SDL or graphics ownership.
class Runtime {
  public:
    Runtime();
    ~Runtime();
    Runtime(Runtime &&) noexcept;
    Runtime &operator=(Runtime &&) noexcept;
    void document(Value);
    const Value &document() const;
    static void validate(const Value &);
    bool observe(const Snapshot &);
    void event(std::string, std::string groupPath = {});
    void enabled(bool);
    bool enabled() const;
    void reset();
    void resetGroup(const std::string &path);
    const Frame &update(double elapsed, const Viewport & = {});
    const Frame &frame() const;
    double time() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace neon::widgets
