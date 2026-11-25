#pragma once
#include "vector_types.h"
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string_view>
namespace neon::vectors {
struct FontSegment {
    Point a, b;
};
struct Glyph {
    float advance = 6;
    std::vector<FontSegment> segments;
};
struct VectorFont {
    std::string id, name;
    float capHeight = 6, ascent = 6, descent = 0, lineAdvance = 9, defaultAdvance = 6,
          strokeWidth = 1;
    bool uppercase = true;
    std::map<uint32_t, Glyph> glyphs;
    const Glyph *find(uint32_t codepoint) const;
    void validate() const;
    void write(std::ostream &) const;
    static VectorFont read(std::istream &);
};
class FontRegistry {
  public:
    FontRegistry(); // neon-hud and vectrex-menu, no system font/ROM dependency
    void add(VectorFont, bool replace = false);
    const VectorFont *find(const std::string &) const;
    uint64_t revision() const {
        return revision_;
    }
    std::vector<std::string> ids() const;

  private:
    std::map<std::string, VectorFont> fonts_;
    uint64_t revision_ = 0;
};
const FontRegistry &defaultFonts();
enum class TextAlign { Left, Center, Right };
enum class TextVertical { Top, Middle, Bottom, Baseline };
struct TextStyle {
    std::string font = "neon-hud";
    float height = 16, tracking = 0, lineSpacing = 1, wrapWidth = 0;
    TextAlign align = TextAlign::Left;
    TextVertical vertical = TextVertical::Top;
    Colour colour{};
    float energy = 1, width = 1;
    bool replacement = true;
};
struct TextBounds {
    float x = 0, y = 0, width = 0, height = 0;
};
struct TextMetrics {
    TextBounds advance{}, ink{};
    size_t lines = 0, glyphs = 0, missingGlyphs = 0;
    bool hasInk = false, truncated = false;
};
struct TextLayout {
    std::vector<Stroke> strokes;
    TextMetrics metrics;
};
// UTF-8; bounds exclude glow. Wraps words, hard-breaks long words; tabs use four spaces.
// Stroke budget truncates at a complete glyph, never a partial character.
TextLayout layoutText(const VectorFont &, std::string_view, const TextStyle &,
                      size_t strokeBudget = 512);
TextLayout layoutText(const FontRegistry &, std::string_view, const TextStyle &,
                      size_t strokeBudget = 512);
bool sameTextStyle(const TextStyle &, const TextStyle &);
} // namespace neon::vectors
