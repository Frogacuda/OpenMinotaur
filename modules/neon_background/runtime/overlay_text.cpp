#include "overlay_text.h"
#include <cmath>
#include <iomanip>
#include <stdexcept>
namespace neon::effects {
namespace {
void need(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
bool equal(const OverlayText &a, const OverlayText &b) {
    return a.text == b.text && vectors::sameTextStyle(a.style, b.style) &&
           a.position.x == b.position.x && a.position.y == b.position.y &&
           a.offset.x == b.offset.x && a.offset.y == b.offset.y && a.enabled == b.enabled &&
           a.bezelOnly == b.bezelOnly;
}
} // namespace
void OverlayTextLayer::set(const std::string &id, OverlayText text) {
    need(!id.empty() && id.size() <= 128 && text.text.size() <= 16384,
         "Invalid overlay text identity/size");
    for (float p : {text.position.x, text.position.y, text.offset.x, text.offset.y})
        need(std::isfinite(p) && std::abs(p) <= 65536, "Invalid overlay text position");
    need(entries_.count(id) || entries_.size() < 64, "Overlay text entry budget exceeded");
    auto it = entries_.find(id);
    if (it != entries_.end() && equal(it->second.text, text))
        return;
    bool geometry = it == entries_.end() || it->second.text.text != text.text ||
                    !vectors::sameTextStyle(it->second.text.style, text.style);
    vectors::TextLayout layout;
    if (geometry)
        layout = vectors::layoutText(fonts_, text.text, text.style, vectors::BeamMesh::MaxStrokes);
    auto &entry = entries_[id];
    entry.text = std::move(text);
    if (geometry) {
        entry.layout = std::move(layout);
        entry.fontRevision = fonts_.revision();
        entry.dirty = false;
        ++pendingBuilds_;
        ++stats_.totalLayoutBuilds;
    }
}
bool OverlayTextLayer::remove(const std::string &id) {
    return entries_.erase(id) != 0;
}
void OverlayTextLayer::clear() {
    entries_.clear();
    frame_ = {};
    pendingBuilds_ = 0;
}
const OverlayText *OverlayTextLayer::find(const std::string &id) const {
    auto it = entries_.find(id);
    return it == entries_.end() ? nullptr : &it->second.text;
}
const OverlayTextFrame &OverlayTextLayer::prepare(int width, int height) {
    need(width > 0 && height > 0, "Invalid overlay dimensions");
    frame_.overGame.clear();
    frame_.bezel.clear();
    auto total = stats_.totalLayoutBuilds;
    stats_ = {};
    stats_.totalLayoutBuilds = total;
    stats_.layoutBuilds = pendingBuilds_;
    pendingBuilds_ = 0;
    for (auto &item : entries_) {
        auto &e = item.second;
        if (!e.text.enabled)
            continue;
        ++stats_.entries;
        if (e.dirty || e.fontRevision != fonts_.revision()) {
            e.layout = vectors::layoutText(fonts_, e.text.text, e.text.style,
                                           vectors::BeamMesh::MaxStrokes);
            e.fontRevision = fonts_.revision();
            e.dirty = false;
            ++stats_.layoutBuilds;
            ++stats_.totalLayoutBuilds;
        }
        stats_.missingGlyphs += e.layout.metrics.missingGlyphs;
        // Keep whole entries atomic; a long label cannot consume a later label's partial glyphs.
        if (e.layout.metrics.truncated ||
            stats_.strokes + e.layout.strokes.size() > vectors::BeamMesh::MaxStrokes) {
            ++stats_.droppedEntries;
            continue;
        }
        auto &target = e.text.bezelOnly ? frame_.bezel : frame_.overGame;
        for (auto stroke : e.layout.strokes) {
            auto move = [&](vectors::Point &p) {
                p.x += e.text.position.x * width + e.text.offset.x;
                p.y += e.text.position.y * height + e.text.offset.y;
            };
            move(stroke.a);
            move(stroke.b);
            target.push_back(stroke);
        }
        stats_.strokes += e.layout.strokes.size();
    }
    return frame_;
}
void OverlayTextLayer::write(std::ostream &out) const {
    out << std::setprecision(9) << "NEONOVERLAY 1 " << entries_.size() << '\n';
    for (auto &item : entries_) {
        auto &e = item.second.text;
        auto &s = e.style;
        out << std::quoted(item.first) << ' ' << std::quoted(e.text) << ' ' << std::quoted(s.font)
            << ' ' << e.position.x << ' ' << e.position.y << ' ' << e.offset.x << ' ' << e.offset.y
            << ' ' << e.enabled << ' ' << e.bezelOnly << '\n'
            << s.height << ' ' << s.tracking << ' ' << s.lineSpacing << ' ' << s.wrapWidth << ' '
            << int(s.align) << ' ' << int(s.vertical) << ' ' << s.colour.r << ' ' << s.colour.g
            << ' ' << s.colour.b << ' ' << s.energy << ' ' << s.width << ' ' << s.replacement
            << '\n';
    }
    need(bool(out), "Could not save overlay text");
}
void OverlayTextLayer::read(std::istream &in) {
    std::string magic;
    int version;
    size_t count;
    need(bool(in >> magic >> version >> count) && magic == "NEONOVERLAY" && version == 1 &&
             count <= 64,
         "Invalid overlay header");
    OverlayTextLayer pending;
    pending.fonts_ = fonts_;
    for (size_t i = 0; i < count; ++i) {
        std::string id;
        OverlayText e;
        int enabled, bezel, align, vertical, replacement;
        auto &s = e.style;
        need(bool(in >> std::quoted(id) >> std::quoted(e.text) >> std::quoted(s.font) >>
                  e.position.x >> e.position.y >> e.offset.x >> e.offset.y >> enabled >> bezel >>
                  s.height >> s.tracking >> s.lineSpacing >> s.wrapWidth >> align >> vertical >>
                  s.colour.r >> s.colour.g >> s.colour.b >> s.energy >> s.width >> replacement) &&
                 enabled >= 0 && enabled <= 1 && bezel >= 0 && bezel <= 1 && replacement >= 0 &&
                 replacement <= 1 && !pending.find(id),
             "Invalid overlay text definition");
        e.enabled = enabled != 0;
        e.bezelOnly = bezel != 0;
        s.align = vectors::TextAlign(align);
        s.vertical = vectors::TextVertical(vertical);
        s.replacement = replacement != 0;
        pending.set(id, std::move(e));
    }
    entries_ = std::move(pending.entries_);
    frame_ = {};
    pendingBuilds_ += pending.stats_.totalLayoutBuilds;
    stats_.totalLayoutBuilds += pending.stats_.totalLayoutBuilds;
}
} // namespace neon::effects
