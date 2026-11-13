// Independent quad body and Minos inline run/smash reference for the
// previously shared Minotron state contract (the adopted Minos/late paths
// are also compared with these frozen bodies).
struct LegacyCell final : gridCell {
    void smash();
    void run();
    int outch(NeonVertex *out, cellGrid *grid, unsigned chr, uint32_t colour);
};
void LegacyCell::smash() {
    rotVel.x = oxRNG::rand(-0.1, 0.1);
    rotVel.y = oxRNG::rand(-0.1, 0.1);
    rotVel.z = oxRNG::rand(-0.1, 0.1);
    vel.x = oxRNG::rand(-0.1, 0.1) * 50.0;
    vel.y = oxRNG::rand(-0.1, 0.1) * 50.0;
    vel.z = oxRNG::rand(-0.1, 0.1) * 10.0;
    scale = 16.0f;
    decay = 0.98f;
    fgState = 2;
}

void LegacyCell::run() {
    auto clerpTo = [](const colour &a, const colour &b, float t) { return clerp(a, b, t); };
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
                col = owner->palette[v];
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
                    bg = owner->palette[v];
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

int LegacyCell::outch(NeonVertex *out, cellGrid *grid, unsigned chr, uint32_t colour) {
    owner = grid;
    z = double(baseZ);
    const vec2d &ps = grid->passScale[pass < 0 ? 0 : pass];
    double w = width * ps.x, h = height * ps.y;
    double px = pos.x * ps.x, py = pos.y * ps.y;
    double u, v, du, dv;
    float spin;

    if (special) {
        int icon;
        if (pass == 0) {
            icon = bgIcon;
            spin = bgAngle;
        } else if (pass == 1) {
            h = w;
            icon = fgIcon;
            spin = fgAngle;
        } else {
            icon = -2;
            spin = 0.0f;
        }
        if (icon == -2) {
            u = 0.0029296875;
        } else {
            if (icon < 0)
                return 0;
            u = (double(float(icon) * 18.0f) + 1.5) * 0.001953125;
        }
        v = 0.4541015625;
        dv = 0.03125;
        du = 0.029296875;
    } else {
        unsigned c8 = chr & 0xff;
        unsigned c = c8 >= unsigned(grid->firstChar) ? c8 : unsigned(grid->firstChar);
        if (c <= 32)
            return 0;
        int idx = int(c) - grid->firstChar;
        int gw = grid->glyphW, gh = grid->glyphH;
        if (c8 > unsigned(grid->maxChar)) {
            v = 0.869140625;
        } else {
            int row = idx / 32;
            v = (double(gh * row + row + grid->fontY) - 1.0) * 0.001953125;
        }
        u = double(gw * (idx % 32) - 1) * 0.001953125;
        spin = 0.0f;
        du = double(gw) * 0.001953125;
        dv = double(gh) * 0.001953125;
        if (pass == 0) {
            // (Port fix) A background block's rectangle starts a texel
            // above and left of the glyph, so filtering blends in the
            // neighbouring row / column and every tile shows a faint seam.
            // Sample only the block's interior instead.
            const double t = 0.001953125;
            u += t * 1.5;
            v += t * 1.5;
            du -= t * 3.0;
            dv -= t * 3.0;
        }
    }

    struct P {
        double x, y, z;
    } q[4] = {{0, 0, 0}, {0, h, 0}, {w, h, 0}, {w, 0, 0}};

    if (rot.x != 0.0) {
        D3DXMatrixRotationX(&mrx, float(rot.x));
        D3DXMatrixRotationY(&mry, float(rot.y));
        D3DXMatrixRotationZ(&mrz, float(double(spin) + rot.z));
        matrix4f m = mrx * mry * mrz;
        for (P &v3 : q) {
            D3DXVECTOR3 in{float(w * -0.5 + v3.x), float(h * -0.5 + v3.y), 0.0f};
            D3DXVECTOR4 o;
            D3DXVec3Transform(&o, &in, &m);
            v3.x = px + double(o.x) + width * 0.5 + offset.x;
            v3.y = py + double(o.y) + height * 0.5 + offset.y;
            v3.z = double(o.z) + z + offset.z;
        }
    } else if (pass == 1 && spin != 0.0f) {
        matrix4f m;
        D3DXMatrixRotationZ(&m, spin);
        for (P &v3 : q) {
            D3DXVECTOR3 in{float(w * -0.5 + v3.x), float(h * -0.5 + v3.y), 0.0f};
            D3DXVECTOR4 o;
            D3DXVec3Transform(&o, &in, &m);
            v3.x = px + double(o.x) + width * 0.5;
            v3.y = py + double(o.y) + height * 0.5;
            v3.z = double(o.z) + z;
        }
    } else {
        for (P &v3 : q) {
            v3.x += px;
            v3.y += py;
            v3.z = z;
        }
    }
    // Ripples: push vertices near each ripple centre back in z.
    for (P &v3 : q)
        for (const auto &rp : grid->ripples) {
            vec2d d(v3.x - rp.x, v3.y - rp.y);
            float t = 128.0f - float(d.length());
            if (t > 0.0f)
                v3.z += double(t * t) * 0.01 * rp.strength;
        }

    auto V = [&](const P &a, double uu, double vv) {
        NeonVertex &n = *out++;
        n.x = float(a.x);
        n.y = -float(a.y);
        n.z = float(a.z);
        n.u0 = n.u1 = n.u2 = float(uu);
        n.v0 = n.v1 = n.v2 = float(vv);
        n.colour = colour;
    };
    V(q[0], u, v);
    V(q[1], u, dv + v);
    V(q[2], du + u, dv + v);
    V(q[2], du + u, dv + v);
    V(q[3], du + u, v);
    V(q[0], u, v);
    return 6;
}
