#include "widgetml_layout.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neon::widgets {
namespace {
void need(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
bool numeric(const Value &v) {
    return std::holds_alternative<double>(v.data) || std::holds_alternative<int64_t>(v.data) ||
           std::holds_alternative<uint64_t>(v.data);
}
bool screenOrigin(const std::string &name) {
    return name == "screen-center" || name == "screen-left" || name == "screen-right" ||
           name == "screen-top" || name == "screen-bottom" || name == "screen-top-left" ||
           name == "screen-top-right" || name == "screen-bottom-left" ||
           name == "screen-bottom-right";
}
void origin(const Value &v, bool landscape) {
    need(std::holds_alternative<std::string>(v.data), "WidgetML placement origin must be a string");
    auto name = v.string();
    need(screenOrigin(name) || (landscape && (name == "gameplay-left" || name == "gameplay-right")),
         "WidgetML origin is unavailable in this orientation");
}
} // namespace
void validateWidgetMLCoordinates(const Value &doc) {
    need(numeric(doc.at("widgetML")) && doc.at("widgetML").number() == 1, "Expected widgetML 1");
    need(doc.at("widgetVersion").missing() && doc.at("canvas").missing(),
         "WidgetML cannot mix legacy canvas/widgetVersion coordinates");
    const auto &coordinates = doc.at("coordinates");
    need(coordinates.missing() || std::holds_alternative<Value::Object>(coordinates.data),
         "WidgetML coordinates must be an object");
    for (auto &field : coordinates.object())
        need(field.first == "shortEdge", "Unknown WidgetML coordinate setting");
    auto unit = coordinates.at("shortEdge");
    need(unit.missing() || (numeric(unit) && std::isfinite(unit.number()) && unit.number() >= 1 &&
                            unit.number() <= 8192),
         "WidgetML shortEdge must be a finite number in 1..8192");
    auto region = doc.at("region").string("viewport");
    need(region == "viewport" || region == "bezel",
         "WidgetML uses viewport coordinates; region may be viewport or bezel");
}
void validateWidgetMLPlacement(const Value &p) {
    if (p.missing())
        return;
    need(std::holds_alternative<Value::Object>(p.data),
         "WidgetML placement must be an orientation object");
    for (auto &field : p.object()) {
        need(field.first == "origin" || field.first == "landscape" || field.first == "portrait",
             "Unknown WidgetML placement orientation");
        origin(field.second, field.first == "landscape");
    }
}
WidgetMLLayout resolveWidgetMLLayout(const Value &doc, const Viewport &viewport) {
    validateWidgetMLCoordinates(doc);
    need(viewport.width > 0 && viewport.height > 0 && viewport.width <= 8192 &&
             viewport.height <= 8192,
         "Invalid WidgetML viewport");
    int turns = (viewport.turns % 4 + 4) % 4;
    double w = turns % 2 ? viewport.height : viewport.width,
           h = turns % 2 ? viewport.width : viewport.height;
    auto rotate = [&](double x, double y) -> std::array<double, 2> {
        if (turns == 1)
            return {y, viewport.width - x};
        if (turns == 2)
            return {viewport.width - x, viewport.height - y};
        if (turns == 3)
            return {viewport.height - y, x};
        return {x, y};
    };
    auto r = viewport.gameplay;
    need(std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) &&
             std::isfinite(r.height) && r.width >= 0 && r.height >= 0,
         "Invalid WidgetML gameplay rectangle");
    bool present = r.width > 0 && r.height > 0;
    if (present)
        need(r.x >= 0 && r.y >= 0 && r.x + r.width <= viewport.width &&
                 r.y + r.height <= viewport.height,
             "WidgetML gameplay rectangle is outside the viewport");
    auto a = rotate(r.x, r.y), b = rotate(r.x + r.width, r.y + r.height);
    Rect gameplay{std::min(a[0], b[0]), std::min(a[1], b[1]), std::abs(a[0] - b[0]),
                  std::abs(a[1] - b[1])};
    double scale = std::min(w, h) / doc.at("coordinates").at("shortEdge").number(1000);
    return {{scale, 0, 0, scale, w / 2, h / 2},
            {0, 0, w, h},
            gameplay,
            w / scale,
            h / scale,
            w >= h,
            present};
}
std::array<double, 2> widgetMLOrigin(const Value &placement, const WidgetMLLayout &layout,
                                     Matrix parent) {
    if (placement.missing())
        return {0, 0};
    auto name = placement.at(layout.landscape ? "landscape" : "portrait")
                    .string(placement.at("origin").string("screen-center"));
    double x = layout.screen.width / 2, y = layout.screen.height / 2;
    if (name == "gameplay-left" || name == "gameplay-right") {
        need(layout.hasGameplay,
             "WidgetML gameplay-edge placement requires a host gameplay rectangle");
        x = layout.gameplay.x + (name == "gameplay-right" ? layout.gameplay.width : 0);
    } else {
        if (name == "screen-left" || name == "screen-top-left" || name == "screen-bottom-left")
            x = 0;
        if (name == "screen-right" || name == "screen-top-right" || name == "screen-bottom-right")
            x = layout.screen.width;
        if (name == "screen-top" || name == "screen-top-left" || name == "screen-top-right")
            y = 0;
        if (name == "screen-bottom" || name == "screen-bottom-left" ||
            name == "screen-bottom-right")
            y = layout.screen.height;
    }
    double determinant = parent.a * parent.d - parent.b * parent.c;
    if (std::abs(determinant) < 1e-12)
        return {0, 0}; // Collapsed parent remains collapsed.
    x -= parent.x;
    y -= parent.y;
    return {(parent.d * x - parent.c * y) / determinant,
            (-parent.b * x + parent.a * y) / determinant};
}
} // namespace neon::widgets
