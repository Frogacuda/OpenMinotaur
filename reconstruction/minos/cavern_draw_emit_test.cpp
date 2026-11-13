// cavern_draw_emit_test.cpp - port integration check (not fidelity evidence): a MapDrawPass
// emitted through the engine's setupChecking/setupSimple with SPR_EXPLICIT_UV produces one
// 6-vertex quad per TileQuad, with the quad's UV rectangle (not the frame's), its position
// and the size; the flag is cleared afterwards. Built in the main build only
// (mino_minos_emit_checks; needs engine/include).
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cavern_draw_emit.h"
#include "neon/math.h"
#include "neon/sprite_draw.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

using namespace minos::cavern;
namespace S = neon::sprites;

namespace {
struct Vertex {
    float x, y, z, u0, v0, u1, v1, u2, v2;
    uint32_t colour;
};
struct Catalog {
    struct Entry {
        int page, x, y, w, h;
    } entry{0, 0, 0, 512, 512};
    struct Page {
        int w, h;
    } dims{512, 512};
    int spriteCount() const {
        return 3;
    }
    const Entry *sprite(int) const {
        return &entry;
    }
    const Page *page(int) const {
        return &dims;
    }
};
struct Renderer {
    int primitive = 0, uploads = 0;
    void UpdateBufferFromMem(Vertex *, int) {
        uploads++;
    }
    void Render(int) {}
    void SetVertexBufferWO(int) {}
};
struct Stack {
    Renderer render;
    int vb0 = 0;
    Vertex verts[0x8000]{};
};
struct TestSprite {
    neon::vec2d pos{0, 0};
    double z = 610, angle = 0, width = 0, height = 0;
    float zOffset = 0, shatter = 0;
    double regionX = 0, regionY = 0, regionW = 0, regionH = 0;
    int frame = 0;
    unsigned flags = 0;
    int tilesX = 1, tilesY = 1;
    double gapX = 0, gapY = 0;
    neon::colour col{1, 1, 1, 1};
    int literalOverride = -1;
    const double *corners = nullptr;
    const S::ExplicitUV *explicitUv = nullptr; // Minos opt-in member
    int batchCount = 0;
    Vertex *batchPtr = nullptr;
    Stack *batchStack = nullptr;
    Catalog *catalog = nullptr;
    void setupSimple(Vertex *out) {
        S::setupSimple(*this, catalog, out, neon::SpriteHeightRule::AddSevenHundredths);
    }
    void setupChecking() {
        S::setupChecking(*this, 0x7fc4);
    }
};
} // namespace

int main() {
    MapDrawPass pass;
    pass.size = {0.05, 0.04};
    pass.quads.push_back({{0.1, 0.2}, {1.0 / 512, 435.0 / 512, 16.0 / 512, 15.0 / 512}, {}});
    pass.quads.push_back(
        {{0.15, 0.2}, {19.0 / 512, 453.0 / 512, 16.0 / 512, 15.0 / 512}, {true, 0, 0xff911affu}});
    Catalog catalog;
    auto stack = std::make_unique<Stack>();
    TestSprite s;
    s.catalog = &catalog;
    s.batchStack = stack.get();
    s.batchPtr = stack->verts;
    int colours = 0;
    emitMapDrawPass(pass, s, [&](TestSprite &, const QuadColour &) { colours++; });
    assert(colours == 2 && s.batchCount == 12 && s.frame == 2 && !(s.flags & S::SPR_EXPLICIT_UV) &&
           !s.explicitUv);
    for (size_t i = 0; i < pass.quads.size(); i++) {
        const Vertex *v = stack->verts + 6 * i;
        const TileQuad &q = pass.quads[i];
        assert(v[0].u0 == float(q.uv.u) && v[0].v0 == float(q.uv.v));
        assert(v[2].u0 == float(q.uv.u + q.uv.du) && v[2].v0 == float(q.uv.v + q.uv.dv));
        assert(v[0].x == float(q.position.x) && v[2].x == float(q.position.x + pass.size.x));
    }
    std::printf("map draw emission through engine SPR_EXPLICIT_UV: %zu quads ok\n",
                pass.quads.size());
}
