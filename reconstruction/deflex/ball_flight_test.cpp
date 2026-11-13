// Source-rule integration of the movement/contact block. Event sinks record
// calls; this does not pretend to implement the remaining game effects/entities.
#include "ball_flight.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace deflex;
struct EmptyBirths : LevelBirths {
    void deflector(const DeflectorButton &) override {}
    void target(const TargetBirth &) override {}
    void hazard(neon::vec2d, int) override {}
    void weight(neon::vec2d) override {}
    void bomb(neon::vec2d) override {}
};
struct Events : BallFlightEvents {
    std::string order;
    std::vector<CellCoords> blasts;
    std::vector<int> notes;
    int sparkCount = 0, samples = 0;
    float targetRadius = 0, hazardRadius = 0;
    void note(int n) override {
        order += 'N';
        notes.push_back(n);
    }
    void trail(int n, const BallFlight &) override {
        order += 'T';
        if (n != 1)
            throw 1;
    }
    void spark(int n, const BallFlight &) override {
        order += 'S';
        sparkCount = n;
    }
    void blow(int x, int y) override {
        order += 'B';
        blasts.push_back({x, y});
    }
    void sample(int) override {
        order += 'V';
        ++samples;
    }
    void contactTargets(BallFlight &b) override {
        order += 'C';
        targetRadius = b.radius;
    }
    void contactHazards(BallFlight &b) override {
        order += 'H';
        hazardRadius = b.radius;
    }
};
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *rule) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", rule);
        }
    };
    auto record = level::kLevelRecords[0];
    record.columns = record.rows = 8;
    record.flags = 0;
    std::memset(record.map, 0, sizeof record.map);
    std::strcpy(record.map[3], "...*");
    LevelSetup setup(record);
    EmptyBirths births;
    buildLevelBoard(setup, record, births);
    // Exact binary speed exercises the source closed centre-hit boundary.
    // A decimal step can land one ULP beyond that boundary; do not add epsilon.
    setup.definition.setReal(0x5ec, 0);
    setup.definition.setReal(0x5f4, 0.00390625);
    DeflectorInput input;
    neon::DarwinRandom random(11);
    Events events;
    BallFlightContext context{setup, input, random, events};
    BallFlight ball;
    ball.init(setup);
    auto centre = ball.logical;
    ball.advance(ball.predict(setup.grid, false), context);
    check(ball.position.y == centre.y + 0.00390625 && ball.radius == 0.03125f &&
              events.order == "TCH",
          "ordinary flight copies logical position then contacts targets before hazards");
    check(events.targetRadius == 0.05f && events.hazardRadius == 0.05f,
          "contact radius enlarged only during both contact calls");

    for (int axis = 0; axis < 2; ++axis)
        for (int glyph = 101; glyph <= 102; ++glyph)
            for (int sign : {-1, 1}) {
                events = Events{};
                ball.init(setup);
                ball.velocity = axis == 0 ? neon::vec2d{0, 0.00390625 * sign}
                                          : neon::vec2d{0.00390625 * sign, 0};
                auto &tile = setup.grid.cell(3, 3);
                tile = GridCell{};
                placeDeflector(setup.grid, 3, 3, glyph == 102, false);
                ball.advance(ball.predict(setup.grid, false), context);
                const int wantedSign = glyph == 101 ? -sign : sign;
                check((axis == 0 ? ball.velocity.x : ball.velocity.y) ==
                              double(ball.turnSpeed) * wantedSign &&
                          (axis == 0 ? ball.velocity.y : ball.velocity.x) == 0,
                      "both deflectors, both approach axes and signs use original turn table");
                check(ball.turnedInCell && tile.scale == 0.8f && events.sparkCount == 40,
                      "turn latches and wears deflector once");
                const auto draws = random.draws();
                ball.advance(ball.predict(setup.grid, false), context);
                check(tile.scale == 0.8f && random.draws() == draws,
                      "latched same-cell turn does not repeat");
            }
    events = Events{};
    ball.init(setup);
    setup.grid.cell(3, 3) = GridCell{};
    setup.grid.cell(3, 3).ch = 0;
    input.pending = 1;
    ball.advance(ball.predict(setup.grid, true), context);
    check(!input.pending && setup.grid.cell(3, 3).ch == 102 && setup.deflectorsCreated == 1 &&
              ball.velocity.x > 0,
          "queued button places and turns immediately in the current cell");
    ball.init(setup);
    input.pending = 0;
    ball.advance(ball.predict(setup.grid, true), context);
    check(!input.pending && setup.deflectorsCreated == 1,
          "occupied glyph rejects pending placement");

    events = Events{};
    ball.init(setup);
    ball.logical = {-0.49, 0};
    ball.position = ball.logical;
    ball.velocity = {-0.02, 0};
    const auto predicted = ball.predict(setup.grid, false);
    ball.advance(predicted, context);
    check(ball.velocity.x == 0.02 && ball.logical.x == -0.49,
          "edge reflection cancels this step rather than overshooting back across it");

    for (int glyph : {104, 105, 106}) {
        events = Events{};
        ball.init(setup);
        auto &tile = setup.grid.cell(3, 3);
        tile = GridCell{};
        tile.ch = glyph;
        ball.advance(ball.predict(setup.grid, false), context);
        if (glyph == 105)
            check(events.blasts.empty() && tile.bgState == 3 &&
                      tile.bgFrom.packRGBA() == 0x40008010,
                  "solid wall flashes without a blast");
        else {
            check(events.blasts.size() == 9 && events.blasts[0].x == 2 && events.blasts[0].y == 2 &&
                      events.blasts[1].x == 2 && events.blasts[1].y == 3 &&
                      events.blasts[8].x == 4 && events.blasts[8].y == 4,
                  "blast traverses columns outside rows in source order");
            if (glyph == 104)
                check(ball.velocity.y == -0.00390625, "destructible wall reflects travel");
            else
                check(ball.velocity.y == 0.00390625 * 1.03 &&
                          ball.turnSpeed == float(double(float(0.00390625)) * 1.03) &&
                          events.sparkCount == 140,
                      "boost applies double gain with float turn-speed store");
        }
    }
    for (bool vertical : {true, false}) {
        events = Events{};
        context.voicePlayed = false;
        ball.init(setup);
        setup.grid.cell(3, 3).ch = 3;
        ball.velocity = vertical ? neon::vec2d{0, 0.00390625} : neon::vec2d{0.00390625, 0};
        const auto p = ball.predict(setup.grid, false);
        check((vertical ? p.next.y - ball.logical.y : p.next.x - ball.logical.x) > 0.0003906 &&
                  (vertical ? p.next.y - ball.logical.y : p.next.x - ball.logical.x) < 0.0003907,
              "destroyed floor slows prediction to one tenth");
        ball.advance(p, context);
        check(events.order == (vertical ? "TVSCH" : "TSVCH"),
              "vertical voice-before-spark versus horizontal spark-before-voice order");
    }
    events = Events{};
    ball.init(setup);
    setup.flags = 0x10;
    setup.grid.cell(3, 3) = GridCell{};
    setup.grid.cell(3, 3).ch = 0;
    const auto draws = random.draws();
    ball.advance(ball.predict(setup.grid, false), context);
    check(setup.grid.cell(3, 3).ch == 3 && random.draws() == draws + 6 && ball.turnedInCell,
          "floor destruction uses shared six-draw smash and latches");
    std::printf("Deflex ball flight: %d failures (movement/contact source-rule integration)\n",
                failures);
    return failures ? 1 : 0;
}
