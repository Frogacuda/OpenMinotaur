// Source-shaped sprite emission. No GL, platform, game headers or owned state.
// Adapters supply their existing value types, resource catalog, RNG and output
// callbacks. Templates preserve those types without copying or truncating full
// sprite/colour records. The operation order and float boundaries are deliberate.
#pragma once
#include "neon/foundation_rules.h"
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace neon::sprites {
inline constexpr int kFlushAt = 0xde8;
inline constexpr int kNoTexture = -12345;
// Original Minotron resets the stack count after purge; Rescue leaves it for
// its caller to reset. This is an independent behavior, not an engine version.
enum class StackPurgeRule { ResetCount, PreserveCount };
enum SpriteFlags : unsigned {
    SPR_CENTRE_X = 0x1,
    SPR_CENTRE_Y = 0x2,
    SPR_ROT_CW = 0x4,  // rotate the corner order one way
    SPR_ROT_CCW = 0x8, // ... and the other way
    SPR_FLIP_X = 0x10,
    SPR_FLIP_Y = 0x20,
    SPR_ROTATE = 0x40, // apply `angle` (always set by the draw calls)
    SPR_AREA_RT = 0x80,
    SPR_KEEP_ASPECT = 0x100, // height is a multiple of width * page aspect
    SPR_REGION = 0x400,      // draw the sub-rectangle region{X,Y,W,H} of the frame
    SPR_CORNERS = 0x800,     // explicit corners (from `corners`), no pos offset
    // Minos (Caverns of Minos) setupSimple only: replace the frame UVs by the rectangle
    // the sprite's `explicitUv` points at. Opt-in per sprite type (see ExplicitUV).
    SPR_EXPLICIT_UV = 0x1000,
};
// SPR_EXPLICIT_UV block, in the original's order {u, v, du, dv} (Minos sprite +0xdc points
// at one; stm::draw sets it for every map tile). A sprite type opts in by having a member
// `const ExplicitUV* explicitUv` (or another pointer to a type with u/v/du/dv); types
// without the member do not read the flag or change texture-coordinate behavior.
struct ExplicitUV {
    double u = 0, v = 0, du = 0, dv = 0;
};
namespace detail {
template <class S, class = void> struct HasExplicitUv : std::false_type {};
template <class S>
struct HasExplicitUv<S, std::void_t<decltype(std::declval<S &>().explicitUv)>> : std::true_type {};
} // namespace detail
struct FrameUV {
    double u, v, du, dv;
    int page;
};
template <class Resources>
bool frameUV(Resources *resources, int frame, FrameUV &out, SpriteHeightRule heightRule) {
    if (!resources)
        return false;
    if (frame > resources->spriteCount() - 1)
        return false;
    if (frame < 0) {
        out = {0.0, 0.0, 1.0, 1.0, -1};
        return true;
    }
    const auto *e = resources->sprite(frame);
    const auto *pg = e ? resources->page(e->page) : nullptr;
    if (!e || !pg || pg->w == 0 || pg->h == 0)
        return false;
    double pw = pg->w, ph = pg->h;
    out.u = (e->x + 0.0) / pw;
    out.v = (e->y + 0.0) / ph;
    out.du = (e->w + 0.0) / pw;
    out.dv = neon::normalizedSpriteHeight(e->h, ph, heightRule);
    out.page = e->page;
    return true;
}

template <class Vertex, class Point>
void emit(Vertex *&out, const Point &pt, float z, float u, float v, uint32_t c) {
    auto &n = *out++;
    n.x = float(pt.x);
    n.y = -float(pt.y);
    n.z = z;
    n.u0 = n.u1 = n.u2 = u;
    n.v0 = n.v1 = n.v2 = v;
    n.colour = c;
}

template <class Vertex, class Point>
void emitQuad(Vertex *&out, const Point &A, const Point &B, const Point &C, const Point &D, float z,
              float u0, float v0, float u1, float v1, uint32_t c) {
    emit(out, A, z, u0, v0, c);
    emit(out, B, z, u0, v1, c);
    emit(out, C, z, u1, v1, c);
    emit(out, C, z, u1, v1, c);
    emit(out, D, z, u1, v0, c);
    emit(out, A, z, u0, v0, c);
}

// The original immediate path mutates flags, aspect height, shatter gaps and
// alpha, and consumes three random draws per tile when tilesX >= 2. In the
// original draw loop corner-order flags also assign row=3/0. Keep that behavior;
// do not combine CCW with multi-row tiling in new callers (it cannot terminate).
template <class Sprite, class Resources, class Vertex, class Random, class SetTexture, class Purge>
void draw(Sprite &s, Resources *resources, Vertex *&cursor, int &count, Random &&random,
          SetTexture &&setTexture, Purge &&purge) {
    using Point = std::decay_t<decltype(s.pos)>;
    double spread = double(float(s.shatter * 32.0f));
    s.flags |= SPR_ROTATE;
    s.gapX = spread;
    s.gapY = spread;
    FrameUV uv;
    if (!frameUV(resources, s.frame, uv, neon::SpriteHeightRule::Exact))
        return;

    double w = s.width, h = s.height, gx = spread;
    if (s.flags & SPR_KEEP_ASPECT) {
        h = s.height * s.width * (uv.dv / uv.du);
        gx = s.gapX;
        s.height = h;
    }
    double totalW = w + double(s.tilesX - 1) * gx;
    double totalH = h + double(s.tilesY - 1) * s.gapY;
    double tileW = w / double(s.tilesX), tileH = h / double(s.tilesY);
    double du = uv.du / double(s.tilesX), dv = uv.dv / double(s.tilesY);
    double u0 = uv.u, v0 = uv.v;
    if (s.flags & SPR_FLIP_X) {
        du = -du;
        u0 += uv.du;
    }
    if (s.flags & SPR_FLIP_Y) {
        dv = -dv;
        v0 += uv.dv;
    }

    if (s.frame < 0) {
        if (s.frame == -999) {
            if (s.literalOverride != -1)
                purge(); // (texture override not used by the port)
        } else {
            setTexture(-1);
        }
    } else {
        setTexture(uv.page);
    }

    double x0c = totalW * -0.5 + 0.0, y0c = totalH * -0.5;
    for (int row = 0; row < s.tilesY; row++) {
        for (int c = 0; c < s.tilesX; c++) {
            double rot;
            if (s.tilesX < 2) {
                rot = s.angle;
                s.gapX = s.gapY = spread;
            } else {
                double r1 = random(-16.0, 16.0);
                s.gapX = spread + r1 * double(s.shatter);
                double r2 = random(-16.0, 16.0);
                s.gapY = spread + r2 * double(s.shatter) + double(float(s.shatter * 40.0f));
                double r3 = random(-3.141592654, 3.141592654);
                s.col.a = double(s.shatter);
                rot = s.angle + r3 * double(s.shatter);
            }
            float frot = float(rot);
            double u = u0 + double(c) * du, v = v0 + double(row) * dv;
            Point A((s.flags & SPR_CENTRE_X) ? x0c : 0.0,
                    (s.flags & SPR_CENTRE_Y) ? y0c + 0.0 : 0.0);
            A.x += double(c) * (tileW + s.gapX);
            A.y += double(row) * (tileH + s.gapY);
            Point B(A.x, A.y + tileH);
            Point C(B.x + tileW, B.y);
            Point D(A.x + tileW, A.y);
            if (s.flags & SPR_ROT_CW) {
                Point t = A;
                A = B;
                B = C;
                C = D;
                D = t;
                row = 3;
            }
            if (s.flags & SPR_ROT_CCW) {
                Point t = D;
                D = C;
                C = B;
                B = A;
                A = t;
                row = 0;
            }
            for (Point *q : {&A, &B, &C, &D}) {
                if (s.flags & SPR_ROTATE)
                    q->rotateBy(double(frot));
                *q = *q + s.pos;
            }
            emitQuad(cursor, A, B, C, D, float(s.z), float(u), float(v), float(u + du),
                     float(v + dv), s.col.packARGB());
            count += 6;
            if (count > kFlushAt)
                purge();
        }
    }
    if (s.frame == -999 && s.literalOverride != -1)
        purge();
}

template <class Sprite, class Resources, class Vertex, class Point, class SetTexture, class Purge>
void drawRegion(Sprite &s, Resources *resources, Vertex *&cursor, int &count, Point uv0, Point uv1,
                SetTexture &&setTexture, Purge &&purge) {
    FrameUV uv;
    if (!frameUV(resources, s.frame, uv, neon::SpriteHeightRule::Exact))
        return;
    if (s.flags & SPR_FLIP_X)
        std::swap(uv0.x, uv1.x);
    if (s.flags & SPR_FLIP_Y)
        std::swap(uv0.y, uv1.y);
    setTexture(s.frame < 0 ? -1 : uv.page);
    Point A((s.flags & SPR_CENTRE_X) ? s.width * -0.5 + 0.0 : 0.0,
            (s.flags & SPR_CENTRE_Y) ? s.height * -0.5 + 0.0 : 0.0);
    Point B(A.x, A.y + s.height), C(A.x + s.width, A.y + s.height), D(A.x + s.width, A.y);
    A = A + s.pos;
    B = B + s.pos;
    C = C + s.pos;
    D = D + s.pos;
    emitQuad(cursor, A, B, C, D, float(s.z), float(uv0.x), float(uv0.y), float(uv1.x), float(uv1.y),
             s.col.packARGB());
    count += 6;
    if (count > kFlushAt)
        purge();
}

// The stack path has its own UV-height rule, includes zOffset and supports
// supplied corners. Supplied corners are rotated but receive no position offset.
template <class Sprite, class Resources, class Vertex>
void setupSimple(Sprite &s, Resources *resources, Vertex *out, SpriteHeightRule heightRule) {
    using Point = std::decay_t<decltype(s.pos)>;
    unsigned f = s.flags;
    s.flags = f | SPR_ROTATE;
    FrameUV uv;
    if (!frameUV(resources, s.frame, uv, heightRule))
        return;
    double u = uv.u, v = uv.v, du = uv.du, dv = uv.dv;
    double pw = 0, ph = 0;
    if (s.frame >= 0) {
        const auto *e = resources->sprite(s.frame);
        pw = e->w;
        ph = e->h;
    }
    if (f & SPR_KEEP_ASPECT)
        s.height = s.height * s.width * (dv / du);
    if (f & SPR_FLIP_X) {
        u += du;
        du = -du;
    }
    if (f & SPR_FLIP_Y) {
        v += dv;
        dv = -dv;
    }
    float rot = float(s.angle);
    if (f & SPR_REGION) {
        u += (du / pw) * s.regionX;
        v += (dv / ph) * s.regionY;
        du *= s.regionW / pw;
        dv *= s.regionH / ph;
    }
    // Minos 0x10f38: after the aspect/flip/region adjustments and before the corners, the
    // explicit rectangle replaces all four values (so it overrides flips and region; the
    // aspect height above still uses the frame's UVs). Only setupSimple has this test.
    if constexpr (detail::HasExplicitUv<Sprite>::value) {
        if ((f & SPR_EXPLICIT_UV) && s.explicitUv) {
            u = s.explicitUv->u;
            v = s.explicitUv->v;
            du = s.explicitUv->du;
            dv = s.explicitUv->dv;
        }
    }
    Point A, B, C, D;
    if (f & SPR_CORNERS) {
        A = {s.corners[0], s.corners[1]};
        B = {s.corners[2], s.corners[3]};
        C = {s.corners[4], s.corners[5]};
        D = {s.corners[6], s.corners[7]};
    } else {
        A = {(f & SPR_CENTRE_X) ? s.width * -0.5 + 0.0 : 0.0,
             (f & SPR_CENTRE_Y) ? s.height * -0.5 + 0.0 : 0.0};
        B = {A.x, A.y + s.height};
        C = {A.x + s.width, A.y + s.height};
        D = {A.x + s.width, A.y};
    }
    if (f & SPR_ROT_CW) {
        Point t = A;
        A = B;
        B = C;
        C = D;
        D = t;
    }
    if (f & SPR_ROT_CCW) {
        Point t = D;
        D = C;
        C = B;
        B = A;
        A = t;
    }
    A.rotateBy(double(rot));
    unsigned g = s.flags;
    if (!(g & SPR_CORNERS))
        A = A + s.pos;
    for (Point *q : {&B, &C, &D}) {
        if (s.flags & SPR_ROTATE)
            q->rotateBy(double(rot));
        if (!(s.flags & SPR_CORNERS))
            *q = *q + s.pos;
    }
    float zz = float(double(s.zOffset) + s.z);
    Vertex *o = out;
    emitQuad(o, A, B, C, D, zz, float(u), float(v), float(u + du), float(v + dv), s.col.packARGB());
}

// Batch views borrow their storage and renderer. A missing context leaves the
// batch untouched, as in the adapters. Texture binding remains a host operation.
template <class Vertex, class Render>
void purge(Vertex *&cursor, Vertex *base, int &count, int &sourceTexture, Render *render) {
    if (count < 1 || !base || !render)
        return;
    render->UpdateBufferFromMem(base, count);
    render->primitive = static_cast<decltype(render->primitive)>(4); // original triangle list
    render->Render(count);
    count = 0;
    cursor = base;
    sourceTexture = kNoTexture;
}
template <class Sprite> void submitStack(Sprite &s) {
    s.batchStack->render.SetVertexBufferWO(s.batchStack->vb0);
    s.batchStack->render.UpdateBufferFromMem(s.batchStack->verts, s.batchCount);
    s.batchStack->render.Render(s.batchCount);
}
template <class Sprite> void setupChecking(Sprite &s, int flushAfter) {
    if (s.batchCount > flushAfter) {
        submitStack(s);
        s.batchCount = 0;
        s.batchPtr = s.batchStack->verts;
    }
    s.setupSimple(s.batchPtr);
    s.batchPtr += 6;
    s.batchCount += 6;
}
template <class Sprite> void setupPurge(Sprite &s, StackPurgeRule rule) {
    if (s.batchCount > 0)
        submitStack(s);
    if (rule == StackPurgeRule::ResetCount)
        s.batchCount = 0;
    // Neither original rewinds batchPtr here.
}
} // namespace neon::sprites
