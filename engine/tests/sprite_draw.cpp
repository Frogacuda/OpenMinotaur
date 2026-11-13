#ifdef NDEBUG
#undef NDEBUG
#endif
#include "neon/sprite_draw.h"
#include "neon/math.h"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace neon::sprites;
struct Vertex {
    float x, y, z, u0, v0, u1, v1, u2, v2;
    uint32_t colour;
};
static_assert(sizeof(Vertex) == 40);
struct Catalog {
    struct Entry {
        int page, x, y, w, h;
    } entry{0, 8, 16, 32, 64};
    struct Page {
        int w, h;
    } dimensions{128, 256};
    int spriteCount() const {
        return 1;
    }
    const Entry *sprite(int) const {
        return &entry;
    }
    const Page *page(int) const {
        return &dimensions;
    }
};
struct RecordingRenderer {
    std::vector<std::string> events;
    int primitive = 0;
    void UpdateBufferFromMem(Vertex *, int n) {
        events.push_back("upload:" + std::to_string(n));
    }
    void Render(int n) {
        events.push_back("draw:" + std::to_string(n));
    }
    void SetVertexBufferWO(int n) {
        events.push_back("buffer:" + std::to_string(n));
    }
};
struct Stack {
    RecordingRenderer render;
    int vb0 = 7;
    Vertex verts[32768]{};
};
struct Sprite {
    neon::vec2d pos{10, 20};
    double z = 610, angle = 0, width = 8, height = 6;
    float zOffset = .25f, shatter = 0;
    double regionX = 8, regionY = 16, regionW = 16, regionH = 32;
    int frame = 0;
    unsigned flags = 0;
    int tilesX = 1, tilesY = 1;
    double gapX = 0, gapY = 0;
    neon::colour col{.25, .5, .75, 1};
    int literalOverride = -1;
    const double *corners = nullptr;
    int batchCount = 0;
    Vertex *batchPtr = nullptr;
    Stack *batchStack = nullptr;
    Catalog *catalog = nullptr;
    void setupSimple(Vertex *out) {
        neon::sprites::setupSimple(*this, catalog, out, neon::SpriteHeightRule::Exact);
    }
};
// Minos sprite: opts in to SPR_EXPLICIT_UV through the explicitUv member.
struct ExplicitUvSprite : Sprite {
    const ExplicitUV *explicitUv = nullptr;
    void setupSimple(Vertex *out) {
        neon::sprites::setupSimple(*this, catalog, out, neon::SpriteHeightRule::Exact);
    }
};
static void explicitUvChecks(Catalog &catalog) {
    const ExplicitUV rect{.125, .25, .0625, .03125};
    // Replaces the frame UVs; flips and region are applied first and then overridden.
    for (unsigned extra : {0u, unsigned(SPR_FLIP_X | SPR_FLIP_Y), unsigned(SPR_REGION)}) {
        ExplicitUvSprite s;
        s.catalog = &catalog;
        s.explicitUv = &rect;
        s.flags = SPR_EXPLICIT_UV | extra;
        Vertex v[6]{};
        s.setupSimple(v);
        assert(v[0].u0 == float(.125) && v[0].v0 == float(.25) && v[1].v0 == float(.25 + .03125));
        assert(v[2].u0 == float(.125 + .0625) && v[4].v0 == float(.25));
        assert(s.flags == (SPR_EXPLICIT_UV | extra | SPR_ROTATE));
    }
    // Aspect height still uses the frame's UVs (64/256 over 32/128 -> 48 for 8 x 6).
    ExplicitUvSprite a;
    a.catalog = &catalog;
    a.explicitUv = &rect;
    a.flags = SPR_EXPLICIT_UV | SPR_KEEP_ASPECT;
    Vertex va[6]{};
    a.setupSimple(va);
    assert(a.height == 48 && va[0].u0 == float(.125));
    // Without the flag the member is ignored; the quad equals the plain sprite's.
    ExplicitUvSprite e;
    e.catalog = &catalog;
    e.explicitUv = &rect;
    Sprite p;
    p.catalog = &catalog;
    Vertex ve[6]{}, vp[6]{};
    e.setupSimple(ve);
    p.setupSimple(vp);
    assert(std::memcmp(ve, vp, sizeof ve) == 0);
    // Host safety default: flag without a rectangle falls back to the frame UVs.
    e.explicitUv = nullptr;
    e.flags = SPR_EXPLICIT_UV;
    e.setupSimple(ve);
    assert(ve[0].u0 == float(8. / 128.));
    // Sprite types without the member (every other title) never read the flag.
    p.flags = SPR_EXPLICIT_UV;
    Vertex vq[6]{};
    p.setupSimple(vq);
    assert(vq[0].u0 == float(8. / 128.));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream file(argv[1]);
    assert(file);
    Catalog catalog;
    const double corners[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::string line;
    int cases = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        Sprite s;
        s.catalog = &catalog;
        s.corners = corners;
        std::string name, order;
        double u0, v0, u1, v1;
        std::istringstream in(line);
        assert(in >> name >> s.flags >> order >> u0 >> v0 >> u1 >> v1);
        const auto originalFlags = s.flags;
        Vertex vertices[6]{};
        s.setupSimple(vertices);
        double h = (originalFlags & SPR_KEEP_ASPECT) ? 48 : 6;
        assert(s.height == h && s.flags == (originalFlags | SPR_ROTATE));
        const double x = (originalFlags & SPR_CENTRE_X) ? -4 : 0;
        const double y = (originalFlags & SPR_CENTRE_Y) ? h * -.5 : 0;
        double xy[4][2] = {{x, y}, {x, y + h}, {x + 8, y + h}, {x + 8, y}};
        if (originalFlags & SPR_CORNERS)
            for (int j = 0; j < 4; ++j) {
                xy[j][0] = corners[j * 2];
                xy[j][1] = corners[j * 2 + 1];
            }
        const int indices[] = {0, 1, 2, 2, 3, 0};
        const double us[] = {u0, u0, u1, u1, u1, u0}, vs[] = {v0, v1, v1, v1, v0, v0};
        for (int i = 0; i < 6; ++i) {
            int j = order[indices[i]] - '0';
            const auto &v = vertices[i];
            assert(v.x == float(xy[j][0] + ((originalFlags & SPR_CORNERS) ? 0 : 10)));
            assert(v.y == -float(xy[j][1] + ((originalFlags & SPR_CORNERS) ? 0 : 20)));
            assert(v.z == 610.25f && v.colour == s.col.packARGB());
            assert(v.u0 == float(us[i]) && v.u1 == v.u0 && v.u2 == v.u0);
            assert(v.v0 == float(vs[i]) && v.v1 == v.v0 && v.v2 == v.v0);
        }
        ++cases;
    }
    assert(cases == 16);
    explicitUvChecks(catalog);
    Vertex v[6]{};
    Sprite s;
    s.catalog = &catalog;
    setupSimple(s, &catalog, v, neon::SpriteHeightRule::AddSevenHundredths);
    assert(v[1].v0 == float(.0625 + (64. + .07) / 256.));
    auto canary = v[0];
    s.frame = 1;
    s.flags = 0;
    s.setupSimple(v);
    assert(std::memcmp(&canary, v, sizeof(Vertex)) == 0 && s.flags == SPR_ROTATE);
    s.frame = 0;
    s.angle = .123456789;
    s.setupSimple(v);
    // Original narrows angle to float before the double-precision rotation.
    neon::vec2d expected{0, 6};
    expected.rotateBy(double(float(s.angle)));
    expected += s.pos;
    assert(v[1].x == float(expected.x) && v[1].y == -float(expected.y));

    auto storage = std::make_unique<std::array<Vertex, 3600>>();
    auto *base = storage->data();
    auto *cursor = base;
    int count = 0, source = -1;
    RecordingRenderer renderer;
    auto flush = [&] { purge(cursor, base, count, source, &renderer); };
    auto texture = [&](int page) {
        flush();
        renderer.events.push_back("texture:" + std::to_string(page));
        source = page;
    };
    int randomCalls = 0;
    auto random = [&](double lo, double hi) {
        ++randomCalls;
        assert(lo == -16 && hi == 16);
        return 0.;
    };
    s = Sprite{};
    draw(s, &catalog, cursor, count, random, texture, flush);
    draw(s, &catalog, cursor, count, random, texture, flush); // same texture still purges
    s.frame = -1;
    drawRegion(s, &catalog, cursor, count, neon::vec2d{0, 0}, neon::vec2d{1, 1}, texture, flush);
    assert(randomCalls == 0 && count == 6 && cursor == base + 6);
    assert(
        (renderer.events == std::vector<std::string>{"texture:0", "upload:6", "draw:6", "texture:0",
                                                     "upload:6", "draw:6", "texture:-1"}));
    renderer.events.clear();
    cursor = base + 3552;
    count = 3552;
    s.frame = -999;
    draw(s, &catalog, cursor, count, random, texture, flush);
    assert(count == 3558 && renderer.events.empty());
    draw(s, &catalog, cursor, count, random, texture, flush);
    assert(count == 0 && cursor == base && source == kNoTexture);
    assert((renderer.events == std::vector<std::string>{"upload:3564", "draw:3564"}));
    cursor = base + 6;
    count = 6;
    source = 3;
    purge(cursor, base, count, source, static_cast<RecordingRenderer *>(nullptr));
    assert(cursor == base + 6 && count == 6 && source == 3);
    flush();
    renderer.events.clear();
    s.literalOverride = 2;
    draw(s, &catalog, cursor, count, random, texture, flush);
    assert((renderer.events == std::vector<std::string>{"upload:6", "draw:6"}));
    assert(cursor == base && count == 0);
    renderer.events.clear();
    s = Sprite{};
    s.tilesX = 2;
    s.tilesY = 2;
    s.shatter = .25f;
    s.col.phase[1] = 3.25f;
    s.col.speed[2] = .625f;
    randomCalls = 0;
    auto shatterRandom = [&](double lo, double hi) {
        int i = randomCalls++ % 3;
        if (i < 2) {
            assert(lo == -16 && hi == 16);
            return i == 0 ? -8. : 4.;
        }
        assert(lo == -3.141592654 && hi == 3.141592654);
        return .5;
    };
    draw(s, &catalog, cursor, count, shatterRandom, texture, flush);
    assert(randomCalls == 12 && count == 24 && s.gapX == 6 && s.gapY == 19 && s.col.a == .25);
    assert(s.col.phase[1] == 3.25f && s.col.speed[2] == .625f);
    // Test the original row assignment without entering its nonterminating
    // CCW/multi-row combination. CW skips rows 1..3 after its first tile.
    cursor = base;
    count = 0;
    s.tilesX = 1;
    s.tilesY = 4;
    s.flags = SPR_ROT_CW;
    draw(s, &catalog, cursor, count, shatterRandom, texture, flush);
    assert(count == 6);

    auto stack = std::make_unique<Stack>();
    s = Sprite{};
    s.catalog = &catalog;
    s.batchStack = stack.get();
    for (int limit : {0x1fc4, 0x7fc4}) {
        stack->render.events.clear();
        s.batchPtr = stack->verts + limit - 2;
        s.batchCount = limit - 2;
        setupChecking(s, limit);
        assert(s.batchCount == limit + 4 && stack->render.events.empty());
        setupChecking(s, limit);
        assert(s.batchCount == 6 && s.batchPtr == stack->verts + 6);
        assert((stack->render.events ==
                std::vector<std::string>{"buffer:7", "upload:" + std::to_string(limit + 4),
                                         "draw:" + std::to_string(limit + 4)}));
        setupPurge(s, StackPurgeRule::ResetCount);
        assert(s.batchCount == 0 && s.batchPtr == stack->verts + 6);
    }
    const auto events = stack->render.events.size();
    setupPurge(s, StackPurgeRule::ResetCount);
    assert(stack->render.events.size() == events);
    // Rescue original 0x11850 returns after Render without resetting count.
    s.batchCount = 6;
    s.batchPtr = stack->verts + 6;
    stack->render.events.clear();
    setupPurge(s, StackPurgeRule::PreserveCount);
    setupPurge(s, StackPurgeRule::PreserveCount);
    assert(s.batchCount == 6 && s.batchPtr == stack->verts + 6);
    assert((stack->render.events == std::vector<std::string>{"buffer:7", "upload:6", "draw:6",
                                                             "buffer:7", "upload:6", "draw:6"}));
    const auto submitted = stack->render.events.size();
    s.batchCount = -6;
    setupPurge(s, StackPurgeRule::PreserveCount);
    assert(s.batchCount == -6);
    setupPurge(s, StackPurgeRule::ResetCount);
    assert(s.batchCount == 0);
    assert(stack->render.events.size() == submitted);
    s.batchCount = 0;
    s.frame = 1;
    s.batchPtr = stack->verts;
    setupChecking(s, 0x1fc4);
    assert(s.batchCount == 6 &&
           s.batchPtr == stack->verts + 6); // lookup failure still advances the stack
    std::cout
        << cases
        << " source quad fixtures; explicit UV (Minos); numeric boundaries, shatter RNG and ordered batch contracts passed\n";
}
