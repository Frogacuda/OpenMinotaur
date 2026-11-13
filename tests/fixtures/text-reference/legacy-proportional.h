struct LegacyLine : proportionalLine {
    void frozenUpdate() { // 0x43398
        phase = phase + phaseRate;
        if (state == 1) {
            alpha = 1.0f;
            if (0 < hold && --hold == 0) {
                t = 1.0f;
                state = 2;
            }
            return;
        }
        if (state == 2) {
            pos = away.lerp(to, t);
            curScale = vec2d(scale.x * outScale.x, scale.y * outScale.y).lerp(scale, t);
            curSpacing = float(double(outCurve) +
                               (double(spacing) - double(outCurve)) * double(t)); // interpolate
            t = t - outRate;
            if (t <= 0.0f) {
                t = 0;
                text = nullptr;
            }
            alpha = t;
        } else if (state == 0) {
            pos = from.lerp(to, t);
            curScale = vec2d(scale.x * inScale.x, scale.y * inScale.y).lerp(scale, t);
            curSpacing = float(double(inCurve) + (double(spacing) - double(inCurve)) * double(t));
            t = t + inRate;
            if (1.0f <= t) {
                t = 1.0f;
                state = 1;
            }
            alpha = t;
        }
    }
};
struct LegacyGrid : cellGrid {
    void oldRaw(sprite *s, int gx, int gy, int gw, int gh, uint32_t rgba) { // 0x43600
        if (noProp)
            return; // +0x3104
        if (0x1fc4 < s->batchCount) {
            s->setupPurge();
            s->batchCount = 0;
            s->batchPtr = stack->verts;
        }
        const double u = double(float(gx)) * 0.001953125, v = double(float(gy)) * 0.001953125;
        const float u0 = float(u), v0 = float(v);
        const float v1 = float(v + double(float(gh)) * 0.001953125 * 0.95);
        const float u1 = float(u + double(float(gw)) * 0.001953125);
        const uint32_t c = ((rgba >> 16 & 0xffu) | ((rgba >> 8 & 0xffu) | (rgba & 0xffu) << 8) << 8)
                               << 8 |
                           rgba >> 24;
        const double x = s->pos.x, y = s->pos.y, w = s->width, h = s->height;
        const float z = float(s->z);
        NeonVertex *o = s->batchPtr;
        auto V = [&](double px, double py, float uu, float vv) {
            NeonVertex &n = *o++;
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
    void oldProp(sprite *s, LegacyLine *l) { // 0x43a44 (instructions)
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
        float total = 0, avg = 0;
        if (0 < int(int8_t(txt[0]))) {
            int n = 0, w = 0;
            if (l->font < 0) {
                for (; 0 < int(int8_t(txt[n])); n++) {
                    const unsigned c = unsigned(uint8_t(txt[n]));
                    w += kPropFontLineX[(c & 0xc0) * 2 + (c & 0x3f) * 2 + 1];
                }
            } else {
                for (; 0 < int(int8_t(txt[n])); n++)
                    w += 14;
            }
            total = float(w);
            avg = float(w) / float(n);
        }
        const float width = float(sx * double(sp + total));
        s->pos = vec2d(l->pos.x * 512.0, l->pos.y * 512.0);
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
                gy = kPropFontLineY[row];
                const int k = row * 0x80 + int(c & 0x3f) * 2;
                gx = kPropFontLineX[k];
                gw = kPropFontLineX[k + 1];
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
                oldRaw(s, gx, gy, gw, 0x12, uint32_t(a) | (col & 0xffffff00u));
            }
            s->pos.x = s->pos.x + (gap + s->width);
            ph = next;
        }
        if (!noProp)
            l->frozenUpdate();
    }
    void oldQuick(const char *s, vec2d p, vec2d sc, int hold, int row, int colourSeq,
                  float phaseRate, float charStep) { // 0x430f8
        proportionalLine *l = lines[row];
        if (!l)
            return;
        l->from = p;
        l->away = p;
        l->pos = p;
        l->to = p;
        l->seq = colourSeq >= 0 && colourSeq < 7 ? kColourSequences[colourSeq] : nullptr;
        l->seqLen = colourSeq >= 0 && colourSeq < 7 ? kColourSequenceLengths[colourSeq] : 0;
        l->scale = sc;
        l->inScale = vec2d(0.8, 0.8);
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
    void oldClear() { // 0x430ac
        for (int i = 0; i < rowsN; i++)
            if (lines[i] && lines[i]->text)
                lines[i]->text = nullptr;
    }
};
