// Regression probe of real adapters, deliberately separate from source fixtures.
#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef TRACE_MINOTRON
#define NEON_GAME minotron
#include "cores/minotron/engine/neon_sprite.h"
#include "cores/minotron/engine/neon_synth.h"
#include "engine/bindings/common/neon_resources.h"
#elif defined(TRACE_RESCUE)
#define NEON_GAME minorescue
#include "cores/minorescue/engine/neon_sprite.h"
#include "cores/minorescue/engine/neon_synth.h"
#include "cores/minorescue/engine/neon_resources.h"
#else
#define NEON_GAME gridrunner
#include "engine/bindings/late/neon_sprite.h"
#include "engine/bindings/late/neon_synth.h"
#include "engine/bindings/common/neon_resources.h"
#endif
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
using namespace NEON_GAME::Neon_Internals;

template <class T> void hashValue(uint64_t &h, const T &value) {
    const auto *bytes = reinterpret_cast<const unsigned char *>(&value);
    for (size_t i = 0; i < sizeof(value); ++i) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
}
int main(int argc, char **argv) {
    NeonResManager resources;
    if (!resources.load(
            "<NeonResources><Literals><Literal>test.png</Literal></Literals>"
            "<Spritemaps><Spritemap entries='1'><Spentry>0,7,13,29,41</Spentry></Spritemap></Spritemaps>"
            "<SpritePages><Spritepage>128,256</Spritepage></SpritePages></NeonResources>"))
        return 1;
    auto session = std::make_unique<RenderSession>();
    ScopedRenderSession binding(*session);
    session->resources = &resources;
    auto *&cursor = session->cursor;
    auto *base = session->base;
    auto &count = session->count;
#ifdef TRACE_RESCUE
    if (argc > 1 && std::strcmp(argv[1], "--rules") == 0) {
        sprite s;
        assert(s.z == 100. && kLitBase == 1);
        s.width = 8;
        s.height = 6;
        s.col.phase[1] = 3.25f;
        s.col.white(2.);
        assert(s.col.a == 2. && s.col.phase[1] == 3.25f);
        NeonVertex v[6]{};
        s.setupSimple(v);
        assert(v[0].z == 100.f && v[0].u0 == float(7. / 128.));
        assert(v[1].v0 == float((13. + 41.) / 256.)); // original exact height, no .07
        assert(v[1].v0 != float((13. + 41. + .07) / 256.));
        assert((v[0].colour >> 24) == 254); // original byte packing wraps, not clamps
        std::cout
            << "Rescue adapter: original exact UV, default depth/page base, unclamped alpha and full colour state passed\n";
        return 0;
    }
#endif
    if (argc > 1) {
        auto stack = std::make_unique<NeonStack>();
        sprite s;
        s.batchStack = stack.get();
#if defined(TRACE_MINOTRON) || defined(TRACE_RESCUE)
        static_assert(NeonStack::kMaxVerts == 8192 && NeonStack::kSpriteFlushAfter == 0x1fc4);
#else
        static_assert(NeonStack::kMaxVerts == 32768 && NeonStack::kSpriteFlushAfter == 0x7fc4);
        s.batchCount = 8136;
        s.batchPtr = stack->verts + 8136;
        s.setupChecking();
        assert(s.batchCount == 8142 && s.batchPtr == stack->verts + 8142);
#endif
        const int limit = NeonStack::kSpriteFlushAfter;
        s.batchCount = limit - 2;
        s.batchPtr = stack->verts + limit - 2;
        s.setupChecking();
        assert(s.batchCount == limit + 4);
        s.setupChecking();
        assert(s.batchCount == 6 && s.batchPtr == stack->verts + 6);
        s.setupPurge();
#ifdef TRACE_RESCUE
        assert(s.batchCount == 6 && s.batchPtr == stack->verts + 6);
        s.setupPurge();
        assert(s.batchCount == 6); // repeated purge submits again
#else
        assert(s.batchCount == 0 && s.batchPtr == stack->verts + 6);
#endif
        std::cout << "Actual adapter stack capacity and original flush boundary passed\n";
        return 0;
    }
    const double corners[] = {-1.25, 3.5, 2, 33.75, 16.5, 37.25, 18.125, -2};
    for (int path = 0; path < 3; ++path)
        for (unsigned flags = 0; flags < 4096; ++flags) {
            sprite s;
            s.frame = 0;
            s.flags = flags;
            s.width = 23.75;
            s.height = 1.125;
            s.pos = {71.25, -9.5};
            s.z = 610.123456789;
            s.zOffset = .375f;
            s.angle = .123456789;
            s.col = {.125, .5, .875, .75};
            s.col.phase[1] = 3.25f;
            s.col.speed[2] = .625f;
            s.regionX = 3;
            s.regionY = 7;
            s.regionW = 11;
            s.regionH = 17;
            s.corners = corners;
            s.shatter = .375f;
            s.tilesX = 3;
            s.tilesY = (flags & 12) ? 1 : 2;
            if ((flags & 3) == 3)
                s.frame = -1;
            if (flags == 4095)
                s.frame = 99;
            neonSrand(19);
            cursor = base;
            count = 0;
            std::memset(base, 0xa5, 3600 * sizeof(NeonVertex));
            if (path == 0) {
                s.setupSimple(base);
                count = 6;
            } else if (path == 1)
                s.draw();
            else
                s.drawRegion({.15, .25}, {.85, .75});
            uint64_t hash = 14695981039346656037ull;
            hashValue(hash, count);
            for (int i = 0; i < count; ++i)
                hashValue(hash, base[i]);
            hashValue(hash, s.flags);
            hashValue(hash, s.height);
            hashValue(hash, s.gapX);
            hashValue(hash, s.gapY);
            hashValue(hash, s.col);
            auto next = neonRand();
            hashValue(hash, next);
            std::cout << path << ' ' << flags << ' ' << std::hex << hash << std::dec << '\n';
        }
}
