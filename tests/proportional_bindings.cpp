#include "neon/proportional_text.h"
#if defined(TEXT_MINOS)
#include "cores/minos/engine/neon_modules.h"
#include "cores/minos/engine/neon_synth.h"
#else
#if defined(TEXT_FIVE)
#include "cores/fiveaday/backend.h"
#else
#include "cores/superoxwars/backend.h"
#endif
#endif
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
using namespace NEON_GAME::Neon_Internals;
namespace frozen {
#if defined(TEXT_MINOS)
#include "cores/minos/engine/cellgrid_text_data.inc"
#elif defined(TEXT_FIVE)
#include "cores/fiveaday/font_data.inc"
#else
#include "cores/superoxwars/font_data.inc"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4458) // Frozen reference names are part of the control.
#endif
#include "fixtures/text-reference/legacy-proportional.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
} // namespace frozen
void samePoint(vec2d a, vec2d b) {
    CHECK(a.x == b.x && a.y == b.y);
}
void sameLine(const proportionalLine &a, const proportionalLine &b) {
    samePoint(a.pos, b.pos);
    samePoint(a.scale, b.scale);
    samePoint(a.to, b.to);
    samePoint(a.from, b.from);
    samePoint(a.away, b.away);
    samePoint(a.inScale, b.inScale);
    samePoint(a.outScale, b.outScale);
    samePoint(a.curScale, b.curScale);
    CHECK(a.t == b.t && a.inRate == b.inRate && a.outRate == b.outRate && a.inCurve == b.inCurve &&
          a.outCurve == b.outCurve && a.state == b.state && a.hold == b.hold && a.text == b.text);
    CHECK(a.spacing == b.spacing && a.curSpacing == b.curSpacing && a.seqLen == b.seqLen &&
          a.seq == b.seq && a.rgba == b.rgba && a.align == b.align && a.font == b.font &&
          a.phaseRate == b.phaseRate && a.charStep == b.charStep && a.alpha == b.alpha &&
          a.phase == b.phase);
}
void sameSprite(const sprite &a, const sprite &b) {
    samePoint(a.pos, b.pos);
    CHECK(a.width == b.width && a.height == b.height && a.z == b.z && a.batchCount == b.batchCount);
    CHECK(a.batchPtr - a.batchStack->verts == b.batchPtr - b.batchStack->verts);
    CHECK(std::memcmp(a.batchStack->verts, b.batchStack->verts, sizeof(a.batchStack->verts)) == 0);
}
int main() {
    try {
        auto grid = std::make_unique<cellGrid>();
        auto old = std::make_unique<frozen::LegacyGrid>();
        auto as = std::make_unique<NeonStack>(), bs = std::make_unique<NeonStack>();
        grid->stack = as.get();
        old->stack = bs.get();
        int updates = 0, draws = 0, quicks = 0;
        for (int state : {-1, 0, 1, 2, 3})
            for (int hold : {-1, 0, 1, 2, 9})
                for (float t : {0.f, .01f, .5f, .99f, 1.f})
                    for (float rate : {0.f, .01f, .03f}) {
                        proportionalLine a;
                        frozen::LegacyLine b;
                        a.text = "ALPHA";
                        a.state = state;
                        a.hold = hold;
                        a.t = t;
                        a.inRate = rate;
                        a.outRate = rate;
                        a.alpha = .375f;
                        a.pos = {.03, -.04};
                        a.from = {-.55, .4};
                        a.to = {.35, -.2};
                        a.away = {.17, -.3};
                        a.scale = {1.25, 2.4};
                        a.inScale = {4, .8};
                        a.outScale = {.5, 1.7};
                        a.inCurve = .8f;
                        a.outCurve = 4;
                        a.spacing = .7f;
                        a.phase = 3.25f;
                        a.phaseRate = .8f;
                        static_cast<proportionalLine &>(b) = a;
                        for (int frame = 0; frame < 8; ++frame) {
                            a.update();
                            b.frozenUpdate();
                            sameLine(a, b);
                            ++updates;
                        }
                    }
        const std::string words[] = {"", "Abc! 09", "\x01\x1f ", std::string("A\x80Z"),
                                     std::string(1500, 'W')};
        for (const auto &text : words)
            for (int font : {-1, 400})
                for (int align : {0, 1, 3})
                    for (int state : {0, 1, 2})
                        for (bool suppressed : {false, true})
                            for (int start : {0, 0x1fc6}) {
                                proportionalLine a;
                                frozen::LegacyLine b;
                                a.text = text.c_str();
                                a.state = state;
                                a.hold = 1;
                                a.t = .5f;
                                a.alpha = .37f;
                                a.font = font;
                                a.align = align;
                                a.scale = {1.3, 2.7};
                                a.curScale = {.7, 1.9};
                                a.pos = {-.13, .27};
                                a.spacing = .7f;
                                a.curSpacing = 1.7f;
                                a.seq = frozen::kColourSequences[3];
                                a.seqLen = frozen::kColourSequenceLengths[3];
                                a.phase = 3.25f;
                                a.charStep = .5f;
                                static_cast<proportionalLine &>(b) = a;
                                std::memset(as->verts, 0xa5, sizeof(as->verts));
                                std::memset(bs->verts, 0xa5, sizeof(bs->verts));
                                sprite sa, sb;
                                sa.batchStack = as.get();
                                sb.batchStack = bs.get();
                                sa.batchCount = sb.batchCount = start;
                                sa.batchPtr = as->verts + start;
                                sb.batchPtr = bs->verts + start;
                                sa.z = sb.z = 610.125;
                                grid->noProp = old->noProp = suppressed;
                                grid->propLine(&sa, &a);
                                old->oldProp(&sb, &b);
                                sameLine(a, b);
                                sameSprite(sa, sb);
                                ++draws;
                            }
        for (int seq : {-1, 0, 3, 6, 7})
            for (int hold : {-1, 0, 1, 4}) {
                proportionalLine a;
                frozen::LegacyLine b;
                grid->lines[0] = &a;
                old->lines[0] = &b;
                grid->QuickMsg("TEST", {.1, .2}, {1.2, 2.4}, hold, 0, seq, .3f, .5f);
                old->oldQuick("TEST", {.1, .2}, {1.2, 2.4}, hold, 0, seq, .3f, .5f);
                // Identical module data, separately compiled into the frozen table namespace.
                CHECK(a.seqLen == b.seqLen);
                for (int i = 0; i < a.seqLen && a.seq; ++i)
                    CHECK(a.seq[i] == b.seq[i]);
                b.seq = a.seq;
                sameLine(a, b);
                ++quicks;
                grid->rowsN = old->rowsN = 1;
                grid->clm();
                old->oldClear();
                sameLine(a, b);
                CHECK(!a.text);
                grid->lines[0] = old->lines[0] = nullptr;
            }
        grid->stack = old->stack = nullptr;
        // Independent source rules: signed-byte termination and raw-glyph UV/packing.
        const auto measure =
            neon::text::measureSignedText("AB\x80Z", [](unsigned c) { return c == 'A' ? 3 : 7; });
        CHECK(measure.total == 10 && measure.average == 5);
        sprite check;
        check.batchStack = as.get();
        check.batchPtr = as->verts;
        check.batchCount = 0;
        check.pos = {1, 2};
        check.width = 3;
        check.height = 4;
        check.z = 610.125;
        grid->noProp = false;
        grid->rawGlyph(&check, 4, 8, 16, 18, 0x11223344);
        CHECK(check.batchCount == 6 && as->verts[0].x == 1 && as->verts[0].y == -2 &&
              as->verts[0].z == float(610.125) && as->verts[0].colour == 0x44332211);
        CHECK(as->verts[0].u0 == float(4. / 512.) &&
              as->verts[1].v0 == float(8. / 512. + 18. / 512. * .95));
        check.batchCount = 0x1fc4;
        check.batchPtr = as->verts + 0x1fc4;
        grid->rawGlyph(&check, 4, 8, 16, 18, 0xffffffff);
        CHECK(check.batchCount == 0x1fca);
        proportionalLine entry;
        entry.state = 0;
        entry.t = 0;
        entry.inRate = .25f;
        entry.from = {.1, .2};
        entry.to = {.3, .4};
        entry.text = "A";
        entry.update();
        CHECK(entry.pos.x == .1 && entry.pos.y == .2 && entry.t == .25f && entry.alpha == .25f);
        // Source-generation distinction: opaque hold versus Deflex's preserved alpha.
        proportionalLine line;
        line.state = 1;
        line.alpha = .375f;
        line.update();
        CHECK(line.alpha == 1.f);
        std::cout << "Proportional text: " << updates << " updates, " << draws
                  << " full layout/vertex cases, " << quicks << " message replacements passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
