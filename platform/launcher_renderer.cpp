#include "launcher.h"
#include "game_catalog.h"
#include "gridrunner_choices.h"
#include "about_screen.h"
#include "build_info.h"
#include <algorithm>
#include "ui_assets.h"
#include <glad/gl.h>
#include <cstddef>
#include <sstream>

namespace mino {
void Launcher::quad(const MenuRect &r, float u0, float v0, float u1, float v1, Colour t, Colour b) {
    const auto transformed = drawTransform_.apply(r);
    Vertex a{transformed.x, transformed.y, u0, v0, t.r, t.g, t.b, t.a},
        c{transformed.x + transformed.w, transformed.y + transformed.h, u1, v1, b.r, b.g, b.b, b.a};
    Vertex v{transformed.x + transformed.w, transformed.y, u1, v0, t.r, t.g, t.b, t.a},
        w{transformed.x, transformed.y + transformed.h, u0, v1, b.r, b.g, b.b, b.a};
    vertices_.insert(vertices_.end(), {a, v, c, a, c, w});
}
void Launcher::draw(unsigned texture, bool rounded, float gloss, bool gray, float sdfWidth,
                    bool circle, ImageMode mode) {
    if (vertices_.empty())
        return;
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1f(uRounded_, mode == ImageMode::Capture  ? 5.f
                           : mode == ImageMode::Halo   ? 4.f
                           : mode == ImageMode::Shadow ? 3.f
                           : circle                    ? 2.f
                           : rounded                   ? 1.f
                                                       : 0.f);
    glUniform1f(uGloss_, gloss);
    glUniform1f(uGray_, gray ? 1.0f : 0.0f);
    glUniform1f(uSdfWidth_, sdfWidth);
    // Keep even unusually long messages within the allocated VBO.
    for (size_t i = 0; i < vertices_.size(); i += 4092) {
        size_t count = std::min(size_t(4092), vertices_.size() - i);
        glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(count * sizeof(Vertex)),
                        vertices_.data() + i);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
    }
    vertices_.clear();
}
void Launcher::rect(const MenuRect &r, Colour top, Colour bottom, bool rounded, float gloss,
                    float cornerRadius) {
    quad(r, 0, 0, 1, 1, top, bottom);
    roundEdge(r, cornerRadius);
    draw(white_, rounded, gloss);
}
void Launcher::roundEdge(const MenuRect &r, float cornerRadius) {
    // The shader's radius is 0.18 in units of side. Most existing controls use
    // proportional corners; Settings groups request a fixed canvas-space radius.
    const float side = cornerRadius > 0 ? cornerRadius / .18f : std::max(1.f, std::min(r.w, r.h));
    glUniform2f(uRoundScale_, r.w / side, r.h / side);
    float pixels =
        std::min(r.w * surface_.width / layout_.width, r.h * surface_.height / layout_.height);
    if (cornerRadius > 0)
        pixels = side * std::min(surface_.width / layout_.width, surface_.height / layout_.height);
    glUniform1f(uRoundEdge_, 0.75f / std::max(pixels * drawTransform_.scale, 1.0f));
}
float Launcher::textWidth(const std::string &value, float size, bool bold) const {
    const auto &face = (notesWriting_ ? ui::notesFontFace() : ui::fontFace(bold));
    float width = 0;
    for (unsigned char c : value) {
        if (c < 32 || c > 126)
            c = '?';
        width += face.glyphs[size_t(c - 32)].advance;
    }
    return width * size / ui::kFontEm;
}
void Launcher::text(const std::string &value, float x, float y, float size, Colour colour,
                    bool centered, bool bold) {
    const auto &face = (notesWriting_ ? ui::notesFontFace() : ui::fontFace(bold));
    const float scale = size / ui::kFontEm;
    if (centered)
        x -= textWidth(value, size, bold) * 0.5f;
    float baseline = y + face.ascent * scale;
    for (unsigned char c : value) {
        if (c < 32 || c > 126)
            c = '?';
        const auto &g = face.glyphs[size_t(c - 32)];
        if (g.w && g.h) {
            quad({x + g.xOffset * scale, baseline + g.yOffset * scale, g.w * scale, g.h * scale},
                 float(g.x) / ui::kFontAtlasWidth, float(g.y) / ui::kFontAtlasHeight,
                 float(g.x + g.w) / ui::kFontAtlasWidth, float(g.y + g.h) / ui::kFontAtlasHeight,
                 colour, colour);
        }
        x += g.advance * scale;
    }
    float pixelScale = float(surface_.height) / layout_.height * drawTransform_.scale;
    // One physical pixel of antialiasing, independent of window size/DPI.
    float smoothing = ui::kFontEm / (4 * ui::kFontSpread * size * std::max(pixelScale, 0.01f));
    draw(notesWriting_ ? notesFont_ : font_, false, false, false,
         std::clamp(smoothing, 0.002f, 0.25f));
}
void Launcher::paragraph(const std::string &message, float y, float size) {
    const float maxWidth = layout_.width - 90;
    std::istringstream in(message);
    std::string line, word;
    auto emit = [&]() {
        text(line, layout_.width / 2, y, size, {0.87f, 0.9f, 0.95f});
        y += size * 1.5f;
        line.clear();
    };
    while (in >> word) {
        if (!line.empty() && textWidth(line + " " + word, size) > maxWidth)
            emit();
        if (!line.empty())
            line += ' ';
        for (char c : word) {
            if (!line.empty() && textWidth(line + c, size) > maxWidth)
                emit();
            line += c;
        }
    }
    if (!line.empty())
        emit();
}
void Launcher::icon(const MenuRect &r, unsigned texture, const char *fallback, bool selected,
                    Colour top, Colour bottom, bool gray) {
    const float padding = r.w * .13f;
    quad({r.x - padding + 1, r.y - padding + 5, r.w + 2 * padding, r.h + 2 * padding}, 0, 0, 1, 1,
         {0, 0, 0, .55f}, {0, 0, 0, .55f});
    draw(white_, false, false, false, 0, false, ImageMode::Shadow);
    if (selected) {
        const float pulse = .5f + .5f * std::sin(float(selectionTime_) * 6.28318530718f / 1.8f);
        const Colour glow{1, .85f, .08f, .45f + .30f * pulse};
        const float spread = r.w * .28f;
        quad({r.x - spread, r.y - spread, r.w + 2 * spread, r.h + 2 * spread}, 0, 0, 1, 1, glow,
             glow);
        draw(white_, false, 0, false, 0, false, ImageMode::Halo);
    }
    if (texture) {
        quad(r, 0, 0, 1, 1, {1, 1, 1}, {1, 1, 1});
        roundEdge(r);
        draw(texture, true, 1.38f, gray);
    } else {
        if (gray) {
            float t = top.r * 0.299f + top.g * 0.587f + top.b * 0.114f;
            float b = bottom.r * 0.299f + bottom.g * 0.587f + bottom.b * 0.114f;
            top = {t, t, t, top.a};
            bottom = {b, b, b, bottom.a};
        }
        rect(r, top, bottom, true, 1.38f);
        text(fallback, r.cx(), r.y + 21, 27, {1, 1, 1}, true, true);
    }
}
void Launcher::wrappedText(const std::string &value, float x, float y, float width, float size,
                           Colour colour) {
    std::istringstream words(value);
    std::string word, line;
    while (words >> word) {
        const auto next = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(next, size) > width) {
            text(line, x, y, size, colour, false);
            y += size * 1.35f;
            line = word;
        } else
            line = next;
    }
    if (!line.empty())
        text(line, x, y, size, colour, false);
}
} // namespace mino
