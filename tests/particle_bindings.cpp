// Actual early adapters versus saved reference methods, including emitted vertices.
#if defined(PARTICLE_RESCUE)
#define NEON_GAME minorescue
#include "cores/minorescue/engine/game.h"
#include "cores/minorescue/engine/neon_synth.h"
#include "cores/minorescue/engine/neon_resources.h"
#else
#define NEON_GAME minotron
#include "cores/minotron/engine/game.h"
#include "cores/minotron/engine/game_entities.h"
#include "cores/minotron/engine/neon_synth.h"
#include "engine/bindings/common/neon_resources.h"
#endif
#include "neon/particles.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace NEON_GAME::Neon_Internals;
#if defined(PARTICLE_RESCUE)
#include "fixtures/particles-reference/minorescue.h"
#else
#include "fixtures/particles-reference/minotron.h"
#endif
#ifdef PARTICLE_RESCUE
#include "fixtures/mechanisms-reference/minorescue-blowup.h"
#else
#include "fixtures/mechanisms-reference/minotron-blowup.h"
#endif
#ifdef PARTICLE_RESCUE
#include "fixtures/mechanisms-reference/minorescue-burst.h"
#include "fixtures/mechanisms-reference/minorescue-burst-source.h"
#else
constexpr double kTwoPi = 6.2831853072;
#include "fixtures/mechanisms-reference/minotron-burst.h"
#include "fixtures/mechanisms-reference/minotron-burst-source.h"
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
using Bytes = std::array<unsigned char, sizeof(_particle)>;
Bytes bytes(const _particle &p) {
    Bytes b;
    std::memcpy(b.data(), &p, b.size());
    return b;
}
void compareChanges(const Bytes &a0, const Bytes &a1, const Bytes &b0, const Bytes &b1) {
    for (size_t i = 0; i < a0.size(); ++i) {
        bool a = a0[i] != a1[i], b = b0[i] != b1[i];
        CHECK(a == b);
        if (a)
            CHECK(a1[i] == b1[i]);
    }
}
void sameColour(const colour &a, const colour &b) {
    CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
    for (int i = 0; i < 3; ++i)
        CHECK(a.phase[i] == b.phase[i] && a.speed[i] == b.speed[i] && a.amp[i] == b.amp[i] &&
              a.base[i] == b.base[i]);
}
int main() {
    try {
        NeonResManager resources;
        CHECK(resources.load(
            "<NeonResources><Literals><Literal>x.png</Literal></Literals>"
            "<Spritemaps><Spritemap entries='4'><Spentry>0,7,13,29,41</Spentry><Spentry>0,17,23,19,31</Spentry><Spentry>0,27,33,9,21</Spentry><Spentry>0,37,43,7,11</Spentry></Spritemap></Spritemaps>"
            "<SpritePages><Spritepage>512,512</Spritepage></SpritePages></NeonResources>"));
        auto session = std::make_unique<RenderSession>();
        auto scope = session->bind();
        session->attachResources(resources);
        auto as = std::make_unique<NeonStack>();
        auto bs = std::make_unique<NeonStack>();
        auto a = std::make_unique<_particle>();
        auto b = std::make_unique<LegacyParticle>();
        int comparisons = 0;
        for (int fd : {-2, -1, 0, 1, 3, 8, 14, 99})
            for (int td : {-1, 0, 4, 13})
                for (float life :
                     {-0.f, 0.f, std::numeric_limits<float>::denorm_min(), .01f, .5f, 1.f, 1.1f})
                    for (float fade : {0.f, .01f, .03f, -.02f})
                        for (int age : {0, 31, std::numeric_limits<int>::max()}) {
                            *a = _particle();
                            a->life = life;
                            a->fade = fade;
                            a->fromDynamic = fd;
                            a->toDynamic = td;
                            a->age = age;
                            a->pos = {.49, -.51};
                            a->vel = {.025, -.015};
                            a->drag = {.97, .98};
                            a->noWrap = (age & 1) != 0;
                            a->wrapped = true;
                            a->sizeStart = .3f;
                            a->sizeEnd = 2.7f;
                            a->size = .025f;
                            a->scaleX = 1.25;
                            a->scaleY = .75;
                            a->rot = .375f;
                            a->spriteIndex = age & 3;
                            a->from = {.125, .25, .5, .75};
                            a->to = {.875, .625, .375, .125};
                            a->from.phase[1] = 3.25f;
                            a->from.speed[2] = .125f;
                            static_cast<_particle &>(*b) = *a;
                            for (int frame = 0; frame < 4; ++frame) {
                                auto a0 = bytes(*a), b0 = bytes(*b);
                                neonSrand(917);
                                int expected = neonRand();
                                neonSrand(917);
                                a->run();
                                b->run();
                                CHECK(neonRand() == expected);
                                compareChanges(a0, bytes(*a), b0, bytes(*b));
                                sprite sa, sb;
                                sa.flags = sb.flags = SPR_CENTRE_X | SPR_CENTRE_Y | SPR_ROTATE;
                                sa.batchStack = as.get();
                                sa.batchPtr = as->verts;
                                sa.batchCount = 0;
                                sb.batchStack = bs.get();
                                sb.batchPtr = bs->verts;
                                sb.batchCount = 0;
                                a0 = bytes(*a);
                                b0 = bytes(*b);
                                neonSrand(917);
                                expected = neonRand();
                                neonSrand(917);
                                CHECK(a->draw(&sa, {640., 480.}) == 6 &&
                                      b->draw(&sb, {640., 480.}) == 6);
                                CHECK(bytes(*a) == a0 && bytes(*b) == b0);
                                CHECK(neonRand() == expected);
                                CHECK(sa.batchCount == 6 && sb.batchCount == 6);
                                CHECK(std::memcmp(as->verts, bs->verts, 6 * sizeof(NeonVertex)) ==
                                      0);
                                CHECK(sa.frame == sb.frame && sa.angle == sb.angle &&
                                      sa.width == sb.width && sa.height == sb.height &&
                                      sa.zOffset == sb.zOffset);
                                CHECK(sa.pos.x == sb.pos.x && sa.pos.y == sb.pos.y);
                                sameColour(sa.col, sb.col);
                                ++comparisons;
                            }
                        }
#ifdef PARTICLE_RESCUE
        using BirthGame = spaceWars;
#else
        using BirthGame = theGame;
#endif
        int births = 0;
        for (int count : {-2, 0, 1, 17})
            for (unsigned seed : {0u, 17u, 0xffffffffu}) {
                auto ag = std::make_unique<BirthGame>(), bg = std::make_unique<BirthGame>();
                entity source;
                LegacyEntity reference;
                source.game = ag.get();
                source.col = {.2, .3, .4, .5};
                source.col.phase[1] = 3.25f;
                source.rot = .375f;
                source.size = .017f;
                source.pos = {.1, .2};
                source.vel = {.003, -.004};
                static_cast<entity &>(reference) = source;
                reference.game = bg.get();
                neonSrand(917);
                source.blowUp(seed, count);
                int an = neonRand();
                neonSrand(917);
                reference.oldBlowUp(seed, count);
                int bn = neonRand();
                CHECK(an == bn && source.dead && reference.dead);
                auto *nextA = static_cast<explosion *>(ag->particles);
                auto *nextB = static_cast<explosion *>(bg->particles);
                CHECK(nextA && nextB && nextA->game == ag.get() && nextB->game == bg.get() &&
                      nextA->next == nullptr && nextB->next == nullptr);
                CHECK(nextA->count == nextB->count && nextA->count == count &&
                      nextA->seed == nextB->seed && nextA->seed == seed);
                sameColour(nextA->from, nextB->from);
                sameColour(nextA->to, nextB->to);
                CHECK(nextA->rot == nextB->rot && nextA->size == nextB->size &&
                      nextA->pos.x == nextB->pos.x && nextA->pos.y == nextB->pos.y &&
                      nextA->vel.x == nextB->vel.x && nextA->vel.y == nextB->vel.y);
                CHECK(nextA->sizeStart == nextB->sizeStart && nextA->sizeEnd == nextB->sizeEnd &&
                      nextA->life == nextB->life && nextA->fade == nextB->fade);
                CHECK(nextA->from.a == 1 && nextA->to.a == .5 &&
                      nextA->vel.x == source.vel.x * .1 && nextA->vel.y == source.vel.y * .1);
                ag->particles = bg->particles = nullptr;
                delete nextA;
                delete nextB;
                ++births;
            }
        std::cout << "Explosion births: " << births
                  << " actual construction/adoption and RNG comparisons passed\n";
        int bursts = 0, oldDifferences = 0;
        auto cs = std::make_unique<NeonStack>();
        for (int count : {-1, 0, 1, 7})
            for (float radius : {.1f, 1.f, 16.f})
                for (float life : {0.f, .37f, 1.f}) {
                    explosion actual;
                    SourceBurst expected;
                    LegacyBurst old;
                    actual.count = count;
                    actual.radius = radius;
                    actual.life = life;
                    actual.seed = 1917;
                    actual.size = .017f;
                    actual.sizeStart = .37f;
                    actual.sizeEnd = 2.7f;
                    actual.pos = {.1, .2};
                    actual.rot = .375f;
                    static_cast<explosion &>(expected) = actual;
                    static_cast<explosion &>(old) = actual;
                    sprite sa, sb, sc;
                    sa.flags = sb.flags = sc.flags = 3;
                    sa.batchStack = as.get();
                    sb.batchStack = bs.get();
                    sc.batchStack = cs.get();
                    sa.batchPtr = as->verts;
                    sb.batchPtr = bs->verts;
                    sc.batchPtr = cs->verts;
                    neonSrand(917);
                    int av = actual.draw(&sa, {641.25, 479.75}), an = neonRand();
                    neonSrand(917);
                    int bv = expected.sourceDraw(&sb, {641.25, 479.75}), bn = neonRand();
                    neonSrand(917);
                    int cv = old.oldDraw(&sc, {641.25, 479.75}), cn = neonRand();
                    CHECK(av == bv && av == cv && an == bn && an == cn &&
                          sa.batchCount == sb.batchCount);
                    CHECK(sa.width == sb.width && sa.height == sb.height && sa.pos.x == sb.pos.x &&
                          sa.pos.y == sb.pos.y);
                    sameColour(sa.col, sb.col);
                    CHECK(std::memcmp(as->verts, bs->verts,
                                      size_t(sa.batchCount) * sizeof(NeonVertex)) == 0);
                    if (sa.width != sc.width || sa.height != sc.height || sa.pos.x != sc.pos.x ||
                        sa.pos.y != sc.pos.y)
                        ++oldDifferences;
                    ++bursts;
                }
        CHECK(oldDifferences > 0);
        std::cout << "Seeded bursts: " << bursts << " source-expression comparisons, "
                  << oldDifferences << " old-port numeric corrections; RNG unchanged\n";
        // Independent original strict-negative retirement rule and no resurrection.
        float life = 0;
        bool dead = false;
        neon::particles::fadeLife(life, 0, dead);
        CHECK(life == 0 && !dead);
        neon::particles::fadeLife(life, .01f, dead);
        CHECK(life == 0 && dead);
        life = 1;
        neon::particles::fadeLife(life, 0, dead);
        CHECK(dead && life == 1);
        std::cout
            << "Particle adapter: " << comparisons
            << " full update/draw comparisons; vertices, colour phases, RNG and retirement passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
