#pragma once
#include <cstdint>
namespace neon::text {
// Source generations differ on hold-state alpha. Update samples geometry at
// OLD progress, then advances it. The caller retains active/pause/draw ordering.
enum class HoldAlpha { Preserve, Opaque };
template <class Line, class Point, class Clear>
void updateLine(Line &l, float &progress, Point &currentScale, float &currentSpacing,
                HoldAlpha holdAlpha, Clear clear) {
    l.phase = l.phase + l.phaseRate;
    if (l.state == 1) {
        if (holdAlpha == HoldAlpha::Opaque)
            l.alpha = 1.f;
        if (l.hold > 0 && --l.hold == 0) {
            progress = 1.f;
            l.state = 2;
        }
        return;
    }
    if (l.state != 0 && l.state != 2)
        return;
    if (l.state == 2) {
        l.pos = l.away.lerp(l.to, progress);
        currentScale =
            Point(l.scale.x * l.outScale.x, l.scale.y * l.outScale.y).lerp(l.scale, progress);
        currentSpacing =
            float(double(l.outCurve) + (double(l.spacing) - double(l.outCurve)) * double(progress));
        progress = progress - l.outRate;
        if (progress <= 0.f) {
            progress = 0.f;
            clear();
        }
        l.alpha = progress;
    } else {
        l.pos = l.from.lerp(l.to, progress);
        currentScale =
            Point(l.scale.x * l.inScale.x, l.scale.y * l.inScale.y).lerp(l.scale, progress);
        currentSpacing =
            float(double(l.inCurve) + (double(l.spacing) - double(l.inCurve)) * double(progress));
        progress = progress + l.inRate;
        if (progress >= 1.f) {
            progress = 1.f;
            l.state = 1;
        }
        l.alpha = progress;
    }
}
// Borrowed text storage and module-owned colour sequences; existing range
// checks and row ownership stay in the binding.
template <class Line, class Point, class Sequences, class Lengths>
void quick(Line *l, const char *s, Point p, Point sc, int hold, int colourSeq, float phaseRate,
           float charStep, const Sequences &sequences, const Lengths &lengths) {
    l->from = p;
    l->away = p;
    l->pos = p;
    l->to = p;
    l->seq = colourSeq >= 0 && colourSeq < 7 ? sequences[colourSeq] : nullptr;
    l->seqLen = colourSeq >= 0 && colourSeq < 7 ? lengths[colourSeq] : 0;
    l->scale = sc;
    l->inScale = Point(0.8, 0.8);
    l->outScale = l->inScale;
    l->outCurve = 1.0f;
    l->inCurve = 1.0f;
    l->spacing = 1.0f;
    l->phaseRate = phaseRate;
    l->charStep = charStep;
    l->hold = hold;
    l->text = s;
    l->state = 0;
    l->phase = 0;
    l->t = 0;
    l->alpha = 0;
}

template <class Grid, class Sprite>
void rawGlyph(Grid &grid, Sprite *s, int gx, int gy, int gw, int gh, uint32_t rgba) { // 0x43600
    if (grid.noProp)
        return; // +0x3104
    if (0x1fc4 < s->batchCount) {
        s->setupPurge();
        s->batchCount = 0;
        s->batchPtr = grid.stack->verts;
    }
    const double u = double(float(gx)) * 0.001953125, v = double(float(gy)) * 0.001953125;
    const float u0 = float(u), v0 = float(v);
    const float v1 = float(v + double(float(gh)) * 0.001953125 * 0.95);
    const float u1 = float(u + double(float(gw)) * 0.001953125);
    const uint32_t c =
        ((rgba >> 16 & 0xffu) | ((rgba >> 8 & 0xffu) | (rgba & 0xffu) << 8) << 8) << 8 | rgba >> 24;
    const double x = s->pos.x, y = s->pos.y, w = s->width, h = s->height;
    const float z = float(s->z);
    auto *o = s->batchPtr;
    auto V = [&](double px, double py, float uu, float vv) {
        auto &n = *o++;
        n.x = float(px);
        n.y = -float(py);
        n.z = z;
        n.u0 = n.u1 = n.u2 = uu;
        n.v0 = n.v1 = n.v2 = vv;
        n.colour = c;
    };
    V(x, y, u0, v0);
    V(x, y + h, u0, v1);
    V(x + w, y + h, u1, v1);
    V(x + w, y + h, u1, v1);
    V(x + w, y, u1, v0);
    V(x, y, u0, v0);
    s->batchPtr = o;
    s->batchCount += 6;
}

// Clears borrowed text pointers only; storage and line animation remain owned by
// the binding. The exclusive count preserves the original rowsN traversal.
template <class Lines> void clearBorrowed(Lines &lines, int count) {
    for (int i = 0; i < count; ++i)
        if (lines[i] && lines[i]->text)
            lines[i]->text = nullptr;
}

struct Measurement {
    float total, average;
};
// Source strings stop at the first non-positive SIGNED byte, not merely NUL.
template <class Width> Measurement measureSignedText(const char *text, Width width) {
    int sum = 0, count = 0;
    for (; int(int8_t(text[count])) > 0; ++count)
        sum += width(uint8_t(text[count]));
    return {float(sum), count ? float(sum) / float(count) : 0.f};
}

// Fixed 512-coordinate text canvas, signed-byte termination, atlas metrics and
// colour phase order recovered from the matching propLine paths. UV routing and
// font tables stay with the module. emitGlyph must submit synchronously.
template <class Sprite, class Line, class FontX, class FontY, class EmitGlyph>
void drawLine(Sprite *s, Line *l, const FontX &fontX, const FontY &fontY, bool suppressed,
              EmitGlyph emitGlyph) {
    using Point = decltype(s->pos);
    // 0x43a44 (instructions)
    const char *txt = l ? l->text : nullptr;
    if (!txt)
        return;
    double sy, sx;
    float sp;
    if (l->state == 1) {
        sy = l->scale.y;
        sx = l->scale.x;
        sp = l->spacing;
    } else {
        sy = l->curScale.y;
        sx = l->curScale.x;
        sp = l->curSpacing;
    }
    float ph = l->phase;
    const auto measured = measureSignedText(txt, [&](unsigned c) {
        return l->font < 0 ? fontX[(c & 0xc0) * 2 + (c & 0x3f) * 2 + 1] : 14;
    });
    const float total = measured.total, avg = measured.average;
    const float width = float(sx * double(sp + total));
    s->pos = Point(l->pos.x * 512.0, l->pos.y * 512.0);
    if (l->align == 1)
        s->pos.x = s->pos.x - double(width * 0.5f + avg * 0.5f);
    else if (l->align == 3)
        s->pos.x = s->pos.x - double(width);
    s->height = sy * 18.0;
    s->pos.y = s->pos.y + s->height * -0.5;
    const double gap = sx * double(sp);
    for (int i = 0; 0 < int(int8_t(txt[i])); i++) {
        const unsigned c = unsigned(uint8_t(txt[i]));
        const int ci = int(int8_t(txt[i]));
        int gx, gy, gw;
        float next;
        bool draw = true;
        int idx;
        if (l->font < 0) {
            const int row = int((c >> 6) & 3);
            gy = fontY[row];
            const int k = row * 0x80 + int(c & 0x3f) * 2;
            gx = fontX[k];
            gw = fontX[k + 1];
            s->width = sx * double(float(gw));
            next = ph + l->charStep;
            draw = -1 < gx;
            idx = draw && l->seqLen ? int(ph) % l->seqLen : 0;
        } else {
            gy = l->font;
            if (ci < 0x20)
                gx = 0;
            else {
                gx = int(c & 0x1f) << 4;
                gy += ((ci - 0x20) >> 5) * 0x12;
            }
            s->width = sx * 14.0;
            gw = 0xe;
            idx = l->seqLen ? int(ph) % l->seqLen : 0;
            next = ph + l->charStep;
        }
        if (draw) {
            const uint32_t col = l->seq ? l->seq[idx] : l->rgba;
            const int a = int(float(int(col & 0xff)) * l->alpha);
            emitGlyph(s, gx, gy, gw, 0x12, uint32_t(a) | (col & 0xffffff00u));
        }
        s->pos.x = s->pos.x + (gap + s->width);
        ph = next;
    }
    if (!suppressed)
        l->update();
}
} // namespace neon::text
