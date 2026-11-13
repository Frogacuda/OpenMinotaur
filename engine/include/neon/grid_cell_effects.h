// Shared gridCell::run state machine. Original Deflex and Minotron methods
// are instruction-identical (Deflex 0x44e04); adapters supply the cell and palette.
// Scheduling, construction and drawing are deliberately outside this helper.
#pragma once
#include "neon/math.h"
namespace neon::grid {

// Shared gridCell::smash: six draws from the caller's original global stream.
template <class Cell, class RandomRange> void smashCell(Cell &cell, RandomRange range) {
    cell.rotVel.x = range(-0.1, 0.1);
    cell.rotVel.y = range(-0.1, 0.1);
    cell.rotVel.z = range(-0.1, 0.1);
    cell.vel.x = range(-0.1, 0.1) * 50.0;
    cell.vel.y = range(-0.1, 0.1) * 50.0;
    cell.vel.z = range(-0.1, 0.1) * 10.0;
    cell.scale = 16.0f;
    cell.decay = 0.98f;
    cell.fgState = 2;
}
template <class Cell, class Colour> void runCellEffects(Cell &cell, const Colour *palette) {
    auto &bgState = cell.bgState;
    auto &fgState = cell.fgState;
    auto &scale = cell.scale;
    auto &decay = cell.decay;
    auto &ch = cell.ch;
    auto &fgOn = cell.fgOn;
    auto &bgOn = cell.bgOn;
    auto &col = cell.col;
    auto &bg = cell.bg;
    auto &fgFrom = cell.fgFrom;
    auto &bgFrom = cell.bgFrom;
    auto &fadeTo = cell.fadeTo;
    auto &bgTo = cell.bgTo;
    auto &fgSeq = cell.fgSeq;
    auto &bgSeq = cell.bgSeq;
    auto &fgT = cell.fgT;
    auto &bgT = cell.bgT;
    auto &fgSpeed = cell.fgSpeed;
    auto &bgSpeed = cell.bgSpeed;
    auto &rot = cell.rot;
    auto &rotVel = cell.rotVel;
    auto &offset = cell.offset;
    auto &vel = cell.vel;
    auto &fadeFlags = cell.fadeFlags;
    auto &fgAngle = cell.fgAngle;
    auto &bgIcon = cell.bgIcon;
    auto clerpTo = [](const Colour &a, const Colour &b, float t) { return clerp(a, b, t); };
    int pass = 0;
    int state = bgState;
    for (;;) {
        bool next = true;
        switch (state) {
        case 0: {
            scale = scale * decay;
            float s = scale, t;
            if (double(s) < 0.1) {
                ch = 0x20;
                fgOn = 0;
                fgState = -1;
                scale = 0.0f;
                t = 1.0f;
            } else if (s > 1.0f) {
                t = 0.0f;
            } else {
                t = 1.0f - s;
            }
            col = clerpTo(fgFrom, fadeTo, t);
            bg = clerpTo(bgFrom, fadeTo, t);
            break;
        }
        case 1:
            if (pass == 1) {
                if (!fgSeq)
                    return;
                fgT += fgSpeed;
                int v = fgSeq[int(fgT)];
                if (v < -1) {
                    col = fgFrom;
                    fgSeq = nullptr;
                    fgState = -1;
                    return;
                }
                if (v == -1) {
                    fgT = 0.0f;
                    v = fgSeq[0];
                }
                col = palette[v];
                return;
            }
            if (bgSeq) {
                bgT += bgSpeed;
                int v = bgSeq[int(bgT)];
                if (v < -1) {
                    bg = bgFrom;
                    bgSeq = nullptr;
                    bgState = -1;
                } else {
                    if (v == -1) {
                        fgT = 0.0f; // (the original resets the glyph phase here)
                        v = bgSeq[0];
                    }
                    bg = palette[v];
                }
                pass = 1;
                state = fgState;
                next = false;
            }
            break;
        case 2:
            rot = rot + rotVel;
            offset = offset + vel;
            scale = scale * decay;
            if (double(scale) < 0.01) {
                fgOn = 0;
                bgOn = 0;
            }
            break;
        case 3:
            if (pass != 0 || !(fadeFlags & 2))
                break;
            if (bgT < 1.0f) {
                bgT = bgT + bgSpeed;
                if (bgT > 1.0f) {
                    fadeFlags &= ~3u;
                    bgT = 1.0f;
                }
                bg = clerpTo(bgFrom, bgTo, bgT);
            }
            pass = 1;
            state = fgState;
            next = false;
            break;
        case 4:
            if (pass != 1)
                break;
            fgT = fgT + fgSpeed;
            if (fgT > 1.0f)
                fgT = 1.0f;
            col = clerpTo(fgFrom, fadeTo, fgT);
            if (ch == 101) {
                if (fgAngle > 0.0f) {
                    fgAngle = float(double(fgAngle) - 0.1);
                    if (fgAngle < 0.0f)
                        fgAngle = 0.0f;
                }
            } else if (ch == 102) {
                if (double(fgAngle) < 1.570796327) {
                    fgAngle = float(double(fgAngle) + 0.1);
                    if (double(fgAngle) > 1.570796327)
                        fgAngle = 1.5707964f;
                }
            }
            break;
        case 5:
            if (pass != 1)
                break;
            fgT = fgT + fgSpeed;
            if (fgT > 1.0f) {
                fgT = 1.0f;
                bgIcon = 0;
                fgState = -1;
                ch = 0;
                return;
            }
            bgIcon = 13 - int(fgT * 8.0f);
            return;
        default:
            break;
        }
        if (!next)
            continue;
        if (++pass > 1)
            return;
        state = fgState;
    }
}

} // namespace neon::grid
