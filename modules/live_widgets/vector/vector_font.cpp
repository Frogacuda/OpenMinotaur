#include "vector_font.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace neon::vectors {
namespace {
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
bool finite(float x) {
    return std::isfinite(x);
}
bool validName(const std::string &s) {
    return !s.empty() && s.size() <= 128 &&
           std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 32; });
}
#include "font_hud.inc"
#include "font_menu.inc"
const char *extraHud(char c) {
    switch (c) {
    case '!':
        return "20 24|26 26";
    case '"':
        return "10 12|30 32";
    case '#':
        return "10 16|30 36|02 42|04 44";
    case '$':
        return "40 10 01 12 32 43 34 04|20 26";
    case '%':
        return "04 40|00 10 11 01 00|35 45 46 36 35";
    case '&':
        return "44 16 05 04 31 30 20 11 46";
    case '\'':
        return "20 22";
    case '(':
        return "30 11 15 36";
    case ')':
        return "10 31 35 16";
    case '*':
        return "02 44|42 04|20 26";
    case ',':
        return "25 16";
    case ';':
        return "21 21|25 16";
    case '<':
        return "41 03 45";
    case '>':
        return "01 43 05";
    case '=':
        return "02 42|04 44";
    case '?':
        return "01 10 30 41 42 23 24|26 26";
    case '@':
        return "44 41 30 10 01 05 16 46|34 14 12 32 34 44";
    case '[':
        return "30 10 16 36";
    case '\\':
        return "00 46";
    case ']':
        return "10 30 36 16";
    case '^':
        return "02 20 42";
    case '_':
        return "06 46";
    case '`':
        return "10 21";
    case '{':
        return "30 20 22 13 24 26 36";
    case '|':
        return "20 26";
    case '}':
        return "10 20 22 33 24 26 16";
    case '~':
        return "04 13 34 43";
    default:
        return "";
    }
}
Glyph pathGlyph(const char *p) {
    Glyph g;
    Point previous{};
    bool pen = false;
    while (*p) {
        if (*p == '|') {
            pen = false;
            ++p;
            continue;
        }
        if (*p == ' ') {
            ++p;
            continue;
        }
        Point next{float(p[0] - '0'), float(p[1] - '0')};
        if (pen)
            g.segments.push_back({previous, next});
        previous = next;
        pen = true;
        p += 2;
    }
    return g;
}
VectorFont hud() {
    VectorFont f;
    f.id = "neon-hud";
    f.name = "Neon HUD / monoline";
    for (uint32_t cp = 32; cp < 127; ++cp) {
        if (cp >= 'a' && cp <= 'z')
            continue;
        auto p = hudGlyph(char(cp));
        if (!*p)
            p = extraHud(char(cp));
        f.glyphs.emplace(cp, pathGlyph(p));
    }
    f.validate();
    return f;
}
Glyph scanGlyph(const uint8_t *rows) {
    Glyph g;
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7;) {
            if (!(rows[y] & (1 << x))) {
                ++x;
                continue;
            }
            int start = x;
            while (x < 7 && (rows[y] & (1 << x)))
                ++x;
            g.segments.push_back({{float(start), float(y)}, {float(x) - .25f, float(y)}});
        }
    return g;
}
VectorFont menu() {
    VectorFont f;
    f.id = "vectrex-menu";
    f.name = "Vectrex Menu / scan strokes";
    f.strokeWidth = .65f;
    for (uint32_t cp = 32; cp < 96; ++cp)
        f.glyphs.emplace(cp, scanGlyph(menuRows[cp - 32]));
    // Additional punctuation is independently drawn, outside the original uppercase set.
    for (auto cp : {'{', '|', '}', '~', '`'})
        f.glyphs.emplace(uint32_t(cp), pathGlyph(extraHud(cp)));
    for (auto pair : {std::pair<uint32_t, int>{0x2191, 0x61}, {0x2193, 0x63}, {0x25cf, 0x65}})
        f.glyphs.emplace(pair.first, scanGlyph(menuRows[pair.second - 32]));
    f.validate();
    return f;
}
std::vector<uint32_t> decode(std::string_view text) {
    need(text.size() <= 16384, "Text exceeds UTF-8 byte budget");
    std::vector<uint32_t> out;
    for (size_t i = 0; i < text.size();) {
        unsigned char first = text[i];
        uint32_t cp = first;
        size_t count = 1;
        if (first >= 0xc2 && first <= 0xdf) {
            count = 2;
            cp = first & 31;
        } else if (first >= 0xe0 && first <= 0xef) {
            count = 3;
            cp = first & 15;
        } else if (first >= 0xf0 && first <= 0xf4) {
            count = 4;
            cp = first & 7;
        } else if (first >= 128) {
            cp = 0xfffd;
        }
        bool ok = i + count <= text.size();
        for (size_t j = 1; j < count && ok; ++j) {
            auto byte = (unsigned char)text[i + j];
            ok = (byte & 0xc0) == 0x80;
            cp = (cp << 6) | (byte & 63);
        }
        if (!ok || (count == 2 && cp < 128) || (count == 3 && cp < 2048) ||
            (count == 4 && cp < 65536) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            cp = 0xfffd;
            count = 1;
        }
        i += count;
        if (cp == '\r') {
            if (i < text.size() && text[i] == '\n')
                ++i;
            cp = '\n';
        }
        out.push_back(cp);
        need(out.size() <= 4096, "Text exceeds codepoint budget");
    }
    return out;
}
} // namespace
const Glyph *VectorFont::find(uint32_t cp) const {
    if (uppercase && cp >= 'a' && cp <= 'z')
        cp -= 32;
    auto at = glyphs.find(cp);
    return at == glyphs.end() ? nullptr : &at->second;
}
void VectorFont::validate() const {
    need(validName(id) && validName(this->name), "Invalid font identity");
    for (auto n : {capHeight, ascent, descent, lineAdvance, defaultAdvance, strokeWidth})
        need(finite(n) && n >= 0 && n <= 4096, "Invalid font metrics");
    need(capHeight >= .01f && lineAdvance > 0 && defaultAdvance > 0 && strokeWidth > 0 &&
             glyphs.size() <= 512 && glyphs.count('?'),
         "Invalid font coverage/metrics");
    size_t total = 0;
    for (auto &item : glyphs) {
        need(item.first <= 0x10ffff && !(item.first >= 0xd800 && item.first <= 0xdfff) &&
                 finite(item.second.advance) && item.second.advance > 0 &&
                 item.second.advance <= 4096,
             "Invalid glyph");
        total += item.second.segments.size();
        need(total <= 4096, "Font stroke budget exceeded");
        for (auto segment : item.second.segments)
            for (float n : {segment.a.x, segment.a.y, segment.b.x, segment.b.y})
                need(finite(n) && std::abs(n) <= 4096, "Invalid font coordinate");
    }
}
void VectorFont::write(std::ostream &out) const {
    validate();
    out << std::setprecision(9) << "NEONFONT 1\n"
        << std::quoted(id) << ' ' << std::quoted(name) << '\n'
        << capHeight << ' ' << ascent << ' ' << descent << ' ' << lineAdvance << ' '
        << defaultAdvance << ' ' << strokeWidth << ' ' << uppercase << '\n'
        << glyphs.size() << '\n';
    for (auto &item : glyphs) {
        out << item.first << ' ' << item.second.advance << ' ' << item.second.segments.size()
            << '\n';
        for (auto s : item.second.segments)
            out << s.a.x << ' ' << s.a.y << ' ' << s.b.x << ' ' << s.b.y << '\n';
    }
    need(bool(out), "Could not write vector font");
}
VectorFont VectorFont::read(std::istream &in) {
    VectorFont f;
    std::string magic;
    int version, upper;
    size_t count;
    need(bool(in >> magic >> version) && magic == "NEONFONT" && version == 1,
         "Invalid font header");
    need(bool(in >> std::quoted(f.id) >> std::quoted(f.name) >> f.capHeight >> f.ascent >>
              f.descent >> f.lineAdvance >> f.defaultAdvance >> f.strokeWidth >> upper >> count) &&
             upper >= 0 && upper <= 1 && count <= 512,
         "Invalid font metadata");
    f.uppercase = upper != 0;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        uint32_t cp;
        Glyph g;
        size_t size;
        need(bool(in >> cp >> g.advance >> size) && size <= 4096 && total + size <= 4096 &&
                 !f.glyphs.count(cp),
             "Invalid font glyph count/key");
        total += size;
        for (size_t j = 0; j < size; ++j) {
            FontSegment s;
            need(bool(in >> s.a.x >> s.a.y >> s.b.x >> s.b.y), "Invalid font stroke");
            g.segments.push_back(s);
        }
        f.glyphs.emplace(cp, std::move(g));
    }
    f.validate();
    return f;
}
FontRegistry::FontRegistry() {
    add(hud());
    add(menu());
}
void FontRegistry::add(VectorFont f, bool replace) {
    f.validate();
    need(replace || !fonts_.count(f.id), "Font ID already registered");
    auto id = f.id;
    fonts_[id] = std::move(f);
    ++revision_;
}
const VectorFont *FontRegistry::find(const std::string &id) const {
    auto it = fonts_.find(id);
    return it == fonts_.end() ? nullptr : &it->second;
}
std::vector<std::string> FontRegistry::ids() const {
    std::vector<std::string> ids;
    for (auto &f : fonts_)
        ids.push_back(f.first);
    return ids;
}
const FontRegistry &defaultFonts() {
    static const FontRegistry registry;
    return registry;
}
bool sameTextStyle(const TextStyle &a, const TextStyle &b) {
    return a.font == b.font && a.height == b.height && a.tracking == b.tracking &&
           a.lineSpacing == b.lineSpacing && a.wrapWidth == b.wrapWidth && a.align == b.align &&
           a.vertical == b.vertical && a.colour.r == b.colour.r && a.colour.g == b.colour.g &&
           a.colour.b == b.colour.b && a.energy == b.energy && a.width == b.width &&
           a.replacement == b.replacement;
}
TextLayout layoutText(const FontRegistry &fonts, std::string_view text, const TextStyle &style,
                      size_t budget) {
    auto font = fonts.find(style.font);
    need(font != nullptr, "Unknown vector font");
    return layoutText(*font, text, style, budget);
}
TextLayout layoutText(const VectorFont &font, std::string_view text, const TextStyle &style,
                      size_t budget) {
    for (auto n : {style.height, style.tracking, style.lineSpacing, style.wrapWidth, style.colour.r,
                   style.colour.g, style.colour.b, style.energy, style.width})
        need(finite(n) && n >= 0 && n <= 8192, "Invalid text style");
    need(style.height > 0 && style.lineSpacing > 0 && style.width > 0 && int(style.align) >= 0 &&
             int(style.align) <= 2 && int(style.vertical) >= 0 && int(style.vertical) <= 3 &&
             budget <= 65536,
         "Invalid text layout");
    TextLayout result;
    if (text.empty())
        return result;
    const float scale = style.height / font.capHeight;
    auto advance = [&](uint32_t cp) {
        auto g = font.find(cp);
        return (g ? g->advance : font.defaultAdvance) * scale + style.tracking;
    };
    std::vector<std::vector<uint32_t>> lines(1);
    auto width = [&](const auto &line) {
        float value = 0;
        for (auto cp : line)
            value += advance(cp);
        return line.empty() ? 0 : std::max(0.f, value - style.tracking);
    };
    auto add = [&](uint32_t cp) {
        auto &line = lines.back();
        line.push_back(cp);
        if (style.wrapWidth > 0 && line.size() > 1 && width(line) > style.wrapWidth) {
            auto space = std::find(line.rbegin(), line.rend(), ' ');
            if (space != line.rend() && cp != ' ') {
                size_t split = line.size() - 1 - size_t(space - line.rbegin());
                std::vector<uint32_t> tail(line.begin() + split + 1, line.end());
                line.resize(split);
                while (!line.empty() && line.back() == ' ')
                    line.pop_back();
                lines.push_back(std::move(tail));
            } else if (cp == ' ') {
                line.pop_back();
                lines.emplace_back();
            } else {
                line.pop_back();
                lines.push_back({cp});
            }
        }
        need(lines.size() <= 256, "Text exceeds line budget");
    };
    for (auto cp : decode(text)) {
        if (cp == '\n') {
            lines.emplace_back();
            need(lines.size() <= 256, "Text exceeds line budget");
        } else if (cp == '\t') {
            int count = 4 - int(lines.back().size() % 4);
            for (int i = 0; i < count; ++i)
                add(' ');
        } else
            add(cp);
    }
    float lineAdvance = font.lineAdvance * scale * style.lineSpacing;
    float height = style.height + (lines.size() - 1) * lineAdvance,
          y = style.vertical == TextVertical::Middle     ? -height / 2
              : style.vertical == TextVertical::Bottom   ? -height
              : style.vertical == TextVertical::Baseline ? -font.ascent * scale
                                                         : 0;
    float maximum = 0, minX = std::numeric_limits<float>::max(), minY = minX, maxX = -minX,
          maxY = -minX;
    bool emit = true;
    for (auto &line : lines) {
        float w = width(line);
        maximum = std::max(maximum, w);
        float x = style.align == TextAlign::Center  ? -w / 2
                  : style.align == TextAlign::Right ? -w
                                                    : 0;
        for (auto cp : line) {
            ++result.metrics.glyphs;
            auto g = font.find(cp);
            if (!g) {
                ++result.metrics.missingGlyphs;
                if (style.replacement)
                    g = font.find('?');
            }
            if (g) {
                if (emit && result.strokes.size() + g->segments.size() > budget) {
                    emit = false;
                    result.metrics.truncated = true;
                }
                for (auto segment : g->segments) {
                    Point a{x + segment.a.x * scale, y + segment.a.y * scale},
                        b{x + segment.b.x * scale, y + segment.b.y * scale};
                    minX = std::min({minX, a.x, b.x});
                    maxX = std::max({maxX, a.x, b.x});
                    minY = std::min({minY, a.y, b.y});
                    maxY = std::max({maxY, a.y, b.y});
                    result.metrics.hasInk = true;
                    if (emit)
                        result.strokes.push_back(
                            {a, b, style.colour, style.energy, style.width * font.strokeWidth});
                }
            }
            x += advance(cp);
        }
        y += lineAdvance;
    }
    result.metrics.lines = lines.size();
    result.metrics.advance = {style.align == TextAlign::Center  ? -maximum / 2
                              : style.align == TextAlign::Right ? -maximum
                                                                : 0,
                              y - lineAdvance - (lines.size() - 1) * lineAdvance, maximum, height};
    if (result.metrics.hasInk)
        result.metrics.ink = {minX, minY, maxX - minX, maxY - minY};
    return result;
}
} // namespace neon::vectors
