#include "reconstruction/deflex/grid_view.h"
#include "reconstruction/minos/enemies.h"
#include "cores/goatup/effects.h"
#include "cores/superoxwars/effects.h"
#include "neon/score_notices.h"
#include "cores/gridrunner/original_messages.h"
#include "fixtures/text-reference/legacy-gridrunner-messages.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <limits>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace deflex_ref {
using namespace deflex;
#include "fixtures/text-reference/legacy-deflex.h"
#include "fixtures/text-reference/legacy-deflex-draw.h"
} // namespace deflex_ref
namespace minos_ref {
using namespace minos;
#include "fixtures/text-reference/legacy-minos-score.h"
} // namespace minos_ref
namespace sow_ref {
#include "fixtures/text-reference/legacy-sow-score.h"
}
void sameColour(const neon::colour &a, const neon::colour &b) {
    CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
    for (int i = 0; i < 3; ++i)
        CHECK(a.phase[i] == b.phase[i] && a.speed[i] == b.speed[i] && a.amp[i] == b.amp[i] &&
              a.base[i] == b.base[i]);
}
struct Quads : deflex::GridRenderer {
    std::vector<std::array<deflex::GridVertex, 6>> records;
    void quad(const std::array<deflex::GridVertex, 6> &q, bool additive, int texture) override {
        CHECK(!additive && texture == 1);
        records.push_back(q);
    }
};
struct Sprites : minos::SpriteRenderer {
    std::vector<minos::SpriteFields> records;
    int drawEntity(minos::Entity &, minos::Vec2) override {
        return 0;
    }
    int drawSprite(const minos::SpriteFields &s) override {
        records.push_back(s);
        return 6;
    }
};
int main() {
    try {
        int updateCases = 0, layoutCases = 0, noticeCases = 0;
        for (int state : {-1, 0, 1, 2, 3})
            for (int hold : {-1, 0, 1, 2, 9})
                for (float progress : {0.f, .01f, .5f, .99f, 1.f})
                    for (float rate : {0.f, .01f, .03f}) {
                        deflex::ProportionalLine a;
                        deflex_ref::LegacyDeflexLine b;
                        a.state = state;
                        a.hold = hold;
                        a.progress = progress;
                        a.inRate = a.outRate = rate;
                        a.alpha = .375f;
                        a.text = "ALPHA";
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
                        static_cast<deflex::ProportionalLine &>(b) = a;
                        for (int i = 0; i < 8; ++i) {
                            a.update();
                            b.oldUpdate();
                            CHECK(a.text == b.text && a.state == b.state && a.hold == b.hold &&
                                  a.progress == b.progress && a.alpha == b.alpha &&
                                  a.phase == b.phase && a.currentSpacing == b.currentSpacing);
                            CHECK(a.pos.x == b.pos.x && a.pos.y == b.pos.y &&
                                  a.currentScale.x == b.currentScale.x &&
                                  a.currentScale.y == b.currentScale.y);
                            ++updateCases;
                        }
                    }
        deflex::ProportionalLine held;
        held.state = 1;
        held.alpha = .375f;
        held.update();
        CHECK(held.alpha == .375f); // actual Deflex source difference
        deflex::CellGrid cells;
        deflex::GridView view(cells);
        for (const std::string text : {std::string{}, std::string("ABC abc 09"),
                                       std::string("A\x80Z"), std::string("\1\x1f ")})
            for (int align : {0, 1, 3})
                for (double sx : {-.3, 0., 1.25}) {
                    Quads a, b;
                    view.text(a, text, {.13, -.27}, {sx, 2.7}, 0x8f00ff7f, align);
                    deflex::ProportionalLine line;
                    line.text = text;
                    line.pos = {.13, -.27};
                    line.scale = {sx, 2.7};
                    line.state = 1;
                    line.alpha = 1;
                    line.sequence = -1;
                    line.rgba = 0x8f00ff7f;
                    line.align = align;
                    deflex_ref::oldLine(b, line);
                    CHECK(a.records.size() == b.records.size());
                    for (size_t i = 0; i < a.records.size(); ++i)
                        CHECK(std::memcmp(a.records[i].data(), b.records[i].data(),
                                          sizeof(a.records[i])) == 0);
                    ++layoutCases;
                }
        minos::PlayState game;
        for (bool paused : {false, true})
            for (float alpha : {0.f, .01f, .375f, 1.f})
                for (float fade : {0.f, -.01f, .03f})
                    for (int count : {1, 3, 10}) {
                        minos::FloatingScore a;
                        minos_ref::LegacyMinosScore b;
                        a.g = &game;
                        game.paused = paused;
                        a.alpha = alpha;
                        a.fade = fade;
                        a.first = 15 - count;
                        a.pos = {.13, -.27};
                        a.vel = {.001, -.003};
                        a.size = .017f;
                        a.scale = {1.25, -.75};
                        a.rot = .3f;
                        for (int i = 0; i < 16; ++i)
                            a.digits[i] = i % 10;
                        static_cast<minos::FloatingScore &>(b) = a;
                        for (int frame = 0; frame < 5; ++frame) {
                            a.run();
                            b.oldRun();
                            CHECK(a.dead == b.dead && a.alpha == b.alpha && a.pos.x == b.pos.x &&
                                  a.pos.y == b.pos.y);
                            sameColour(a.col, b.col);
                            Sprites ar, br;
                            CHECK(a.draw(ar, {641.25, 479.75}) == b.oldDraw(br, {641.25, 479.75}));
                            CHECK(ar.records.size() == br.records.size());
                            for (size_t i = 0; i < ar.records.size(); ++i) {
                                const auto &x = ar.records[i];
                                const auto &y = br.records[i];
                                CHECK(x.frame == y.frame && x.pos.x == y.pos.x &&
                                      x.pos.y == y.pos.y && x.size.x == y.size.x &&
                                      x.size.y == y.size.y && x.angle == y.angle &&
                                      x.orFlags == y.orFlags);
                                sameColour(x.col, y.col);
                            }
                            ++noticeCases;
                        }
                    }
        superoxwars::World sw(17);
        for (float opacity : {0.f, .01f, .375f, 1.f})
            for (int side : {-1, 0, 1}) {
                superoxwars::FloatingScore a(sw, 123, {.1, .2}, side, {.001, -.003});
                sow_ref::LegacySowScore b(sw, 123, {.1, .2}, side, {.001, -.003});
                a.opacity = b.opacity = opacity;
                for (int i = 0; i < 6; ++i) {
                    a.run();
                    b.oldRun();
                    CHECK(a.opacity == b.opacity && a.dead == b.dead &&
                          a.position.x == b.position.x && a.position.y == b.position.y);
                    CHECK(a.colour.r == b.colour.r && a.colour.g == b.colour.g &&
                          a.colour.b == b.colour.b && a.colour.a == b.colour.a);
                    ++noticeCases;
                }
            }
        // Public GoatUp source owner against its saved score birth/update/prepare model.
        for (int value : {-1, 0, 1, 123, 123456789}) {
            neon::DarwinRandom random(17);
            goatup::Effects actual(random);
            actual.floatingScore(value, {.13, -.27}, .001f, -.003f, .017f);
            std::vector<int> digits;
            if (value > 0) {
                for (int n = value; n > 0; n /= 10)
                    digits.insert(digits.begin(), n % 10);
            } else
                digits.push_back(0);
            goatup::ActorDraw draw;
            draw.position = {.13, -.27};
            draw.radius = .017f;
            draw.colour.white(1);
            neon::vec2d velocity{double(.001f), double(-.003f)};
            float alpha = 1;
            bool dead = false;
            for (int frame = 0; frame < 140; ++frame) {
                const bool paused = frame % 11 == 0;
                neon::colour colour{.125, .25, .5, .75};
                colour.phase[1] = 3.25f;
                if (!dead && !paused) {
                    draw.position += velocity;
                    draw.colour = colour;
                    draw.colour.a = double(alpha);
                    alpha = float(alpha + float(-.01));
                    if (alpha < 0)
                        dead = true;
                }
                actual.tickUi(paused, 0, colour);
                std::vector<goatup::ActorDraw> records;
                actual.prepareUiDraw(records);
                CHECK(records.size() == (dead ? 0 : digits.size()));
                const auto size = draw.scale * double(draw.radius), spacing = size * 1.2;
                neon::vec2d at{draw.position.x + spacing.x * double(float(digits.size())) * -.5 +
                                   spacing.x * .5,
                               draw.position.y + spacing.y * -.5};
                for (size_t i = 0; i < records.size(); ++i) {
                    const auto &d = records[i];
                    CHECK(d.position.x == at.x && d.position.y == at.y &&
                          d.frame == 32 + digits[i] && d.radius == draw.radius &&
                          d.scale.x == draw.scale.x && d.scale.y == draw.scale.y &&
                          d.angle == draw.angle && d.particle);
                    sameColour(d.colour, draw.colour);
                    at.x += spacing.x;
                }
                CHECK(random.draws() == 0);
                ++noticeCases;
            }
        }
        int messages = 0;
        for (int first : {0, 4, 10})
            for (int second : {0, 4, 10}) {
                gridrunner::GoalMessages a;
                gridrunner_ef107::GoalMessages b;
                a.show(first);
                b.show(first);
                for (int frame = 0; frame < 220; ++frame) {
                    if (frame == 10) {
                        a.show(second);
                        b.show(second);
                    }
                    const bool paused = frame % 13 == 0;
                    a.prepare(paused);
                    b.prepare(paused);
                    CHECK(a.glyphs().size() == b.glyphs().size());
                    for (size_t i = 0; i < a.glyphs().size(); ++i) {
                        const auto &x = a.glyphs()[i];
                        const auto &y = b.glyphs()[i];
                        CHECK(x.x == y.x && x.y == y.y && x.w == y.w && x.h == y.h &&
                              x.atlasX == y.atlasX && x.atlasY == y.atlasY && x.width == y.width &&
                              x.colour == y.colour);
                    }
                    for (size_t i = 0; i < 2; ++i) {
                        const auto &x = a.lines()[i];
                        const auto &y = b.lines()[i];
                        CHECK(x.text == y.text && x.y == y.y && x.sx == y.sx && x.sy == y.sy &&
                              x.amount == y.amount && x.alpha == y.alpha &&
                              x.colourPhase == y.colourPhase && x.state == y.state &&
                              x.hold == y.hold);
                    }
                    ++messages;
                }
            }
        std::cout << "Gridrunner prepared messages: " << messages
                  << " complete frame/glyph comparisons passed\n";
        // Source-order boundary: alpha is published before strict-negative retirement.
        neon::vec2d p{}, v{};
        float alpha = 0;
        bool dead = false;
        double published = -1;
        neon::notices::advanceFloating(p, v, alpha, -.01f, dead, [&](double a) { published = a; });
        CHECK(published == 0 && dead && alpha < 0);
        std::cout << "Native text/notices: " << updateCases << " Deflex updates, " << layoutCases
                  << " text quads, " << noticeCases << " full notice cases passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
