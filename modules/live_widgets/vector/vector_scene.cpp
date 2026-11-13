#include "vector_scene.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace neon::vectors {
namespace {
constexpr float pi = 3.141592653589793f;
bool finite(Point p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

} // namespace
Point Transform::apply(Point p) const {
    float c = std::cos(rotation), s = std::sin(rotation);
    p.x *= scale.x;
    p.y *= scale.y;
    return {position.x + c * p.x - s * p.y, position.y + s * p.x + c * p.y};
}
Canvas::Canvas(std::size_t capacity) : capacity_(capacity) {
    strokes_.reserve(capacity);
}
void Canvas::clear() {
    strokes_.clear();
    dropped_ = 0;
}
void Canvas::line(Point a, Point b, Colour colour, float energy, float width) {
    if (!finite(a) || !finite(b) || !std::isfinite(energy) || !std::isfinite(width) ||
        !std::isfinite(colour.r) || !std::isfinite(colour.g) || !std::isfinite(colour.b))
        throw std::runtime_error("Non-finite vector stroke");
    if (width <= 0 || energy <= 0)
        return;
    if (strokes_.size() == capacity_) {
        ++dropped_;
        return;
    }
    strokes_.push_back({a, b, colour, energy, width});
}
void Canvas::dot(Point p, Colour c, float e, float w) {
    line(p, p, c, e, w);
}
void Canvas::path(const std::vector<Point> &points, bool closed, Colour c, float e, float w) {
    for (std::size_t i = 1; i < points.size(); i++)
        line(points[i - 1], points[i], c, e, w);
    if (closed && points.size() > 1)
        line(points.back(), points.front(), c, e, w);
}
void Canvas::arc(Point p, float radius, float start, float sweep, Colour c, float e, float w) {
    if (!finite(p) || !std::isfinite(radius) || !std::isfinite(start) || !std::isfinite(sweep) ||
        radius < 0 || std::abs(sweep) > 2 * pi + .001f)
        throw std::runtime_error("Invalid vector arc");
    if (radius == 0) {
        dot(p, c, e, w);
        return;
    }
    int segments = std::clamp(
        int(std::ceil(std::abs(sweep) * std::sqrt(std::max(radius, 1.f) / 1.4f))), 1, 128);
    Point prior{p.x + radius * std::cos(start), p.y + radius * std::sin(start)};
    for (int i = 1; i <= segments; i++) {
        float a = start + sweep * i / segments;
        Point next{p.x + radius * std::cos(a), p.y + radius * std::sin(a)};
        line(prior, next, c, e, w);
        prior = next;
    }
}
void Canvas::text(Point p, const std::string_view &text, float height, Colour c, float energy,
                  float width) {
    TextStyle style;
    style.height = height;
    style.colour = c;
    style.energy = energy;
    style.width = width;
    style.replacement = false;
    this->text(p, text, style);
}
void Canvas::text(Point p, std::string_view text, const TextStyle &style,
                  const FontRegistry &fonts) {
    if (!finite(p))
        throw std::runtime_error("Invalid vector text origin");
    auto layout = layoutText(fonts, text, style, capacity_ - strokes_.size());
    for (auto stroke : layout.strokes)
        line({p.x + stroke.a.x, p.y + stroke.a.y}, {p.x + stroke.b.x, p.y + stroke.b.y},
             stroke.colour, stroke.energy, stroke.width);
    if (layout.metrics.truncated)
        ++dropped_;
}
void Canvas::object(const Object &object) {
    for (auto &stroke : object.strokes)
        line(object.transform.apply(stroke.a), object.transform.apply(stroke.b), stroke.colour,
             stroke.energy * object.opacity, stroke.width);
}

std::vector<Object> readObjects(std::istream &input) {
    std::string header;
    int version;
    if (!(input >> header >> version) || header != "NEONVECTORS" || version < 1 || version > 2)
        throw std::runtime_error("Invalid vector-object header");
    std::string row;
    std::getline(input, row);
    std::vector<Object> objects;
    Canvas builder;
    bool inside = false;
    std::size_t total = 0, number = 1;
    while (std::getline(input, row)) {
        ++number;
        std::istringstream line(row);
        std::string command;
        if (!(line >> command) || command[0] == '#')
            continue;
        if (command == "object") {
            if (inside || objects.size() >= 64)
                throw std::runtime_error("Vector object nesting/count invalid");
            Object object;
            if (!(line >> std::quoted(object.name) >> object.transform.position.x >>
                  object.transform.position.y >> object.transform.scale.x >>
                  object.transform.scale.y >> object.transform.rotation >> object.opacity) ||
                object.name.size() > 128 || !finite(object.transform.position) ||
                !finite(object.transform.scale) || !std::isfinite(object.transform.rotation) ||
                !std::isfinite(object.opacity) || object.opacity < 0 || object.opacity > 1)
                throw std::runtime_error("Invalid vector object transform");
            objects.push_back(std::move(object));
            builder.clear();
            inside = true;
        } else if (command == "end") {
            if (!inside)
                throw std::runtime_error("Vector end without object");
            objects.back().strokes = builder.strokes();
            total += builder.strokes().size();
            if (builder.dropped() || total > BeamMesh::MaxStrokes)
                throw std::runtime_error("Vector file exceeds 512-stroke mobile budget");
            inside = false;
        } else {
            if (!inside)
                throw std::runtime_error("Vector primitive outside object");
            Point a, b;
            Colour c;
            float energy, width, radius, start, sweep, height;
            std::string text;
            if (command == "line") {
                if (!(line >> a.x >> a.y >> b.x >> b.y >> c.r >> c.g >> c.b >> energy >> width))
                    throw std::runtime_error("Invalid vector line");
                builder.line(a, b, c, energy, width);
            } else if (command == "dot") {
                if (!(line >> a.x >> a.y >> c.r >> c.g >> c.b >> energy >> width))
                    throw std::runtime_error("Invalid vector dot");
                builder.dot(a, c, energy, width);
            } else if (command == "arc") {
                if (!(line >> a.x >> a.y >> radius >> start >> sweep >> c.r >> c.g >> c.b >>
                      energy >> width))
                    throw std::runtime_error("Invalid vector arc");
                builder.arc(a, radius, start, sweep, c, energy, width);
            } else if (command == "textfont" && version >= 2) {
                TextStyle style;
                if (!(line >> std::quoted(style.font) >> a.x >> a.y >> style.height >>
                      std::quoted(text) >> style.colour.r >> style.colour.g >> style.colour.b >>
                      style.energy >> style.width) ||
                    text.size() > 256)
                    throw std::runtime_error("Invalid font text");
                builder.text(a, text, style);
            } else if (command == "text") {
                if (!(line >> a.x >> a.y >> height >> std::quoted(text) >> c.r >> c.g >> c.b >>
                      energy >> width) ||
                    text.size() > 256)
                    throw std::runtime_error("Invalid vector text");
                builder.text(a, text, height, c, energy, width);
            } else
                throw std::runtime_error("Unknown vector command on line " +
                                         std::to_string(number));
        }
        std::string extra;
        if (line >> extra)
            throw std::runtime_error("Extra vector data on line " + std::to_string(number));
    }
    if (inside)
        throw std::runtime_error("Unclosed vector object");
    return objects;
}
void writeObjects(std::ostream &out, const std::vector<Object> &objects) {
    out << "NEONVECTORS 1\n" << std::setprecision(9);
    for (auto &object : objects) {
        auto &t = object.transform;
        out << "object " << std::quoted(object.name) << ' ' << t.position.x << ' ' << t.position.y
            << ' ' << t.scale.x << ' ' << t.scale.y << ' ' << t.rotation << ' ' << object.opacity
            << '\n';
        for (auto &s : object.strokes)
            out << "line " << s.a.x << ' ' << s.a.y << ' ' << s.b.x << ' ' << s.b.y << ' '
                << s.colour.r << ' ' << s.colour.g << ' ' << s.colour.b << ' ' << s.energy << ' '
                << s.width << '\n';
        out << "end\n";
    }
    if (!out)
        throw std::runtime_error("Cannot write vector objects");
}

BeamMesh::BeamMesh() {
    vertices_.reserve(MaxStrokes * HistoryFrames * 6);
    previous_.reserve(MaxStrokes);
    combined_.reserve(MaxStrokes * HistoryFrames);
    for (auto &frame : history_)
        frame.strokes.reserve(MaxStrokes);
}
void BeamMesh::reset() {
    vertices_.clear();
    previous_.clear();
    for (auto &frame : history_) {
        frame.strokes.clear();
        frame.time = -1;
    }
    lastTime_ = lastSample_ = -1;
    stats_ = {};
}
const std::vector<Vertex> &BeamMesh::build(const std::vector<Stroke> &strokes, double time,
                                           int width, int height, const BeamStyle &style) {
    if (width < 1 || height < 1 || !std::isfinite(time) || !std::isfinite(style.width) ||
        !std::isfinite(style.spread) || !std::isfinite(style.persistenceMs) ||
        !std::isfinite(style.colour) || !std::isfinite(style.intensity) ||
        !std::isfinite(style.halo) || !std::isfinite(style.whiteHot) || style.width <= 0 ||
        style.intensity < 0 || style.halo < 0 || style.spread < 1 || style.spread > 8)
        throw std::runtime_error("Invalid vector view/style");
    if (time < lastTime_)
        reset();
    if (lastTime_ >= 0 && time - lastSample_ >= 1. / 60 - 1.e-6) {
        for (std::size_t i = history_.size() - 1; i > 0; i--) {
            history_[i].strokes.swap(history_[i - 1].strokes);
            history_[i].time = history_[i - 1].time;
        }
        history_[0].strokes.assign(previous_.begin(), previous_.end());
        history_[0].time = lastTime_;
        lastSample_ = time;
    }
    if (lastSample_ < 0)
        lastSample_ = time;
    previous_.assign(strokes.begin(), strokes.begin() + std::min(strokes.size(), MaxStrokes));
    lastTime_ = time;
    vertices_.clear();
    stats_ = {};
    stats_.dropped = strokes.size() > MaxStrokes ? strokes.size() - MaxStrokes : 0;
    float weights[HistoryFrames] = {1, 0, 0, 0}, sum = 1;
    float tau = std::max(0.f, style.persistenceMs) * .001f;
    if (tau > 0)
        for (std::size_t i = 0; i < history_.size(); i++)
            if (history_[i].time >= 0 && time - history_[i].time < .3) {
                weights[i + 1] = float(std::exp(-(time - history_[i].time) / tau));
                sum += weights[i + 1];
            }
    combined_.assign(previous_.begin(), previous_.end());
    for (auto &stroke : combined_)
        stroke.energy *= weights[0] / sum;
    for (std::size_t frame = 0; frame < history_.size(); frame++)
        if (weights[frame + 1] > 0) {
            auto &old = history_[frame].strokes;
            for (std::size_t i = 0; i < old.size(); i++) {
                float contribution = old[i].energy * weights[frame + 1] / sum;
                if (i < previous_.size() &&
                    std::memcmp(&old[i], &previous_[i], sizeof(Stroke)) == 0)
                    combined_[i].energy += contribution;
                else {
                    auto stroke = old[i];
                    stroke.energy = contribution;
                    combined_.push_back(stroke);
                }
            }
            ++stats_.historyFrames;
        }
    append(combined_, 1, width, height, style);
    return vertices_;
}
void BeamMesh::append(const std::vector<Stroke> &strokes, float weight, int width, int height,
                      const BeamStyle &style) {
    float sx = width / 640.f, sy = height / 480.f, scale = height / 720.f;
    for (auto &stroke : strokes) {
        Point a{stroke.a.x * sx, stroke.a.y * sy}, b{stroke.b.x * sx, stroke.b.y * sy};
        if (!finite(a) || !finite(b) || !std::isfinite(stroke.energy) ||
            !std::isfinite(stroke.width) || !std::isfinite(stroke.colour.r) ||
            !std::isfinite(stroke.colour.g) || !std::isfinite(stroke.colour.b))
            throw std::runtime_error("Non-finite vector mesh");
        float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy),
              nx = length > 1.e-6f ? dx / length : 1, ny = length > 1.e-6f ? dy / length : 0;
        float beam = std::clamp(style.width * stroke.width * scale, .35f, 16.f),
              radius = std::min(
                  64.f,
                  std::max(2.f,
                           beam * (style.halo > 0 ? std::clamp(style.spread, 1.f, 8.f) : 1.f) * 2));
        if (std::max(a.x, b.x) + radius < 0 || std::min(a.x, b.x) - radius > width ||
            std::max(a.y, b.y) + radius < 0 || std::min(a.y, b.y) - radius > height)
            continue;
        Colour mono{1, 1, 1};
        float mix = std::clamp(style.colour, 0.f, 1.f);
        Colour c{mono.r + (stroke.colour.r - mono.r) * mix,
                 mono.g + (stroke.colour.g - mono.g) * mix,
                 mono.b + (stroke.colour.b - mono.b) * mix};
        Vertex q[4];
        for (int k = 0; k < 4; k++) {
            float u = (k == 1 || k == 2) ? length + radius : -radius, v = k >= 2 ? radius : -radius;
            q[k] = {a.x + nx * u - ny * v,
                    a.y + ny * u + nx * v,
                    u,
                    v,
                    length,
                    beam,
                    c.r,
                    c.g,
                    c.b,
                    std::clamp(stroke.energy, 0.f, 8.f) * weight};
        }
        for (int k : {0, 1, 2, 2, 3, 0})
            vertices_.push_back(q[k]);
        ++stats_.strokes;
        stats_.triangles += 2;
        stats_.coveredPixels += (length + 2 * radius) * 2 * radius;
    }
}

} // namespace neon::vectors
