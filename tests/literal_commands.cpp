#include "neon/sprite_commands.h"
#if defined(COMMAND_GOAT)
#include "cores/goatup/backend.h"
#define OLD old_goatup
#elif defined(COMMAND_FIVE)
#include "cores/fiveaday/backend.h"
#define OLD old_fiveaday
#else
#include "cores/superoxwars/backend.h"
#define OLD old_superoxwars
#endif
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
using namespace NEON_GAME;
using namespace NEON_GAME::Neon_Internals;
using Point = vec2d;
namespace frozen {
#include "fixtures/mechanisms-reference/literal_commands.h"
}
void same(const sprite &a, const sprite &b) {
    CHECK(a.flags == b.flags && a.frame == b.frame && a.width == b.width && a.height == b.height &&
          a.angle == b.angle && a.zOffset == b.zOffset);
    CHECK(a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.batchCount == b.batchCount);
    CHECK(a.batchPtr - a.batchStack->verts == b.batchPtr - b.batchStack->verts);
    CHECK(std::memcmp(&a.col, &b.col, sizeof(a.col)) == 0);
#if !defined(COMMAND_GOAT) && !defined(COMMAND_FIVE)
    CHECK(a.explicitUv == nullptr && b.explicitUv == nullptr);
#endif
    for (int i = 0; i < 3; ++i)
        CHECK(a.batchStack->render.srcLiteral[i] == b.batchStack->render.srcLiteral[i]);
    CHECK(std::memcmp(a.batchStack->verts, b.batchStack->verts, sizeof(a.batchStack->verts)) == 0);
}
// An independent ordered-output model for the page protocol. Production adapters
// below are also called directly with real sprites; this models only batching.
struct Event {
    int kind, a, b, c;
    bool operator==(const Event &o) const {
        return kind == o.kind && a == o.a && b == o.b && c == o.c;
    }
};
struct TraceRender {
    std::vector<Event> *out;
    void SetSourceFromLiterals(int a, int b, int c) {
        out->push_back({1, a, b, c});
    }
};
struct TraceStack {
    int verts[1]{};
    TraceRender render;
};
struct TraceSprite {
    TraceStack *batchStack;
    int *batchPtr = nullptr;
    std::vector<Event> *out;
    void setupPurge() {
        out->push_back({0, 0, 0, 0});
    }
};
struct TraceResources {
    struct Entry {
        int page;
    };
    Entry pages[3]{{0}, {1}, {2}};
    const Entry *sprite(int i) {
        return i >= 0 && i < 3 ? &pages[i] : nullptr;
    }
};
void protocol() {
    TraceResources r;
    for (int count : {0, 1, 2, 3, 15})
        for (int pattern = 0; pattern < 4; ++pattern) {
            std::vector<int> commands;
            for (int i = 0; i < count; ++i)
                commands.push_back(pattern == 0   ? 0
                                   : pattern == 1 ? 2
                                   : pattern == 2 ? i % 3
                                                  : (i == count - 1 ? 99 : 1));
            std::vector<Event> a, b;
            TraceStack stack;
            stack.render.out = &a;
            TraceSprite target{&stack, nullptr, &a};
            bool threw = false;
            try {
                neon::sprites::executeLiteralCommands(
                    target, r, commands, [](int n) { return n; },
                    [&](auto &, int n, int) { a.push_back({2, n, 0, 0}); },
                    [](int) { throw std::logic_error("missing"); });
            } catch (const std::logic_error &) {
                threw = true;
            }
            int page = -1;
            bool expectedThrow = false;
            for (int n : commands) {
                if (n == 99) {
                    expectedThrow = true;
                    break;
                }
                if (n != page) {
                    b.push_back({0, 0, 0, 0});
                    b.push_back({1, n, n, n});
                    page = n;
                }
                b.push_back({2, n, 0, 0});
            }
            if (!expectedThrow && page != -1) {
                b.push_back({0, 0, 0, 0});
                b.push_back({1, 0, 0, 0});
            }
            CHECK(a == b && threw == expectedThrow);
        }
}
int main() {
    try {
        protocol();
        Backend backend;
        auto &resources =
            const_cast<NeonResManager &>(static_cast<const NeonResManager &>(backend.resources()));
        CHECK(resources.load(
            "<NeonResources><Literals><Literal>a.png</Literal><Literal>b.png</Literal><Literal>c.png</Literal></Literals><Spritemaps><Spritemap entries='5'><Spentry>0,1,2,4,8</Spentry><Spentry>0,3,4,7,11</Spentry><Spentry>1,5,6,13,17</Spentry><Spentry>2,7,8,19,23</Spentry><Spentry>0,9,10,29,31</Spentry></Spritemap></Spritemaps><SpritePages><Spritepage>64,128</Spritepage><Spritepage>256,512</Spritepage><Spritepage>128,256</Spritepage></SpritePages></NeonResources>"));
        auto session = std::make_unique<RenderSession>();
        auto scope = session->bind();
        session->attachResources(resources);
        auto aa = std::make_unique<NeonStack>(), bb = std::make_unique<NeonStack>();
        int comparisons = 0;
        for (int count : {0, 1, 3, 9})
            for (int pattern = 0; pattern < 4; ++pattern)
                for (int start : {0, 6, NeonStack::kSpriteFlushAfter + 4})
                    for (int variant = 0; variant < 4; ++variant) {
                        std::vector<ActorDraw> actors;
                        for (int i = 0; i < count; ++i) {
                            ActorDraw a;
                            a.frame = pattern == 0   ? 0
                                      : pattern == 1 ? i % 3
                                      : pattern == 2 ? 2
                                                     : (i == count - 1 ? 90 : 0);
                            a.position = {double(i) * .01 - .3, .17};
                            a.scale = {variant & 1 ? -1.1 : 1.25, variant & 2 ? 0 : .75};
                            a.radius = .017f;
                            a.angle = .4f;
                            a.colour = {.125, .25, .5, .75};
#if defined(COMMAND_GOAT)
                            a.particle = (i & 1) != 0;
                            a.colour.phase[1] = 3.25f;
#elif !defined(COMMAND_FIVE)
                            a.pivot = {.03, -.05};
                            a.z = .375f;
                            a.explicitUv = (i & 1) != 0;
                            a.uv = {.17, .23};
                            a.uvSize = {.31, -.43};
#endif
                            actors.push_back(a);
                        }
                        std::memset(aa->verts, 0xa5, sizeof(aa->verts));
                        std::memset(bb->verts, 0xa5, sizeof(bb->verts));
                        sprite a, b;
                        a.batchStack = aa.get();
                        b.batchStack = bb.get();
                        a.batchCount = b.batchCount = start;
                        a.batchPtr = aa->verts + start;
                        b.batchPtr = bb->verts + start;
                        a.zOffset = b.zOffset = .125f;
                        a.col.phase[1] = b.col.phase[1] = 2.5f;
                        bool at = false, bt = false;
                        try {
                            backend.drawActors(a, 641.25, 479.75, actors);
                        } catch (const std::logic_error &) {
                            at = true;
                        }
                        try {
                            frozen::OLD(b, resources, 641.25, 479.75, actors);
                        } catch (const std::logic_error &) {
                            bt = true;
                        }
                        CHECK(at == bt);
                        same(a, b);
                        ++comparisons;
                    }
        std::cout << "Literal commands: " << comparisons
                  << " actual-backend comparisons plus ordered page/exception protocol passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
