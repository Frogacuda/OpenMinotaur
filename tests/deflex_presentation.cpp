#include "../cores/deflex/presentation_observer.h"
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
    auto record = level::kLevelRecords[0];
    record.columns = record.rows = 8;
    record.flags = 0;
    std::memset(record.map, 0, sizeof record.map);
    std::strcpy(record.map[3], "...*");
    LevelSetup setup(record);
    EmptyBirths births;
    buildLevelBoard(setup, record, births);
    setup.definition.setReal(0x5ec, 0);
    setup.definition.setReal(0x5f4, 0.00390625);
    DeflectorInput input;
    neon::DarwinRandom random(11);
    Events events;
    BallFlightContext context{setup, input, random, events};
    for (bool flipping : {false, true})
        for (bool horizontal : {false, true})
            for (int glyph : {101, 102})
                for (int sign : {-1, 1}) {
                    setup.flippingDeflectors = flipping;
                    BallFlight ball;
                    ball.init(setup);
                    ball.velocity = horizontal ? neon::vec2d{.00390625 * sign, 0}
                                               : neon::vec2d{0, .00390625 * sign};
                    auto before = ball.velocity;
                    placeDeflector(setup.grid, 3, 3, glyph == 102, false);
                    ball.advance(ball.predict(setup.grid, false), context);
                    int expected = (horizontal ? glyph == 102 : glyph == 101) ? 1 : -1;
                    if (mino::deflex_presentation::reflectionTurn(before, ball, setup) != expected)
                        return 1;
                    ball.velocity = before * -1;
                    if (mino::deflex_presentation::reflectionTurn(before, ball, setup) != 0)
                        return 2;
                }
    std::puts(
        "PASS: Deflex host observer: both signs on both axes, both deflectors, flipping boards, and excluded 180-degree reversals.");
    return 0;
}
