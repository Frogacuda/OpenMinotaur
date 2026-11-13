// Source-rule integration of board setup and controls; no original replay claim.
#include "level_setup.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace deflex;
struct BirthLog : LevelBirths {
    std::string order;
    std::vector<TargetBirth> targets;
    std::vector<DeflectorButton> buttons;
    int hazards = 0, weights = 0, bombs = 0;
    void deflector(const DeflectorButton &b) override {
        buttons.push_back(b);
        order += b.orientation ? 'R' : 'L';
    }
    void target(const TargetBirth &b) override {
        targets.push_back(b);
        order += 'T';
    }
    void hazard(neon::vec2d, int) override {
        ++hazards;
        order += 'H';
    }
    void weight(neon::vec2d) override {
        ++weights;
        order += 'W';
    }
    void bomb(neon::vec2d) override {
        ++bombs;
        order += 'B';
    }
};
struct ParticleLog : DeflectorServices {
    int calls = 0, count = 0;
    float scale = 0, decay = 0;
    void derezAllBalls(int n, float s, float d) override {
        ++calls;
        count = n;
        scale = s;
        decay = d;
    }
};
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", message);
        }
    };
    const auto &intro = level::kLevelRecords[0];
    LevelSetup first(intro);
    BirthLog log;
    buildLevelBoard(first, intro, log);
    check(log.targets.size() == 40 && first.targetsRemaining == 40 && first.targetsCreated == 40,
          "intro's five rows of four targets, mirrored");
    check(log.buttons.size() == 2 && log.buttons[0].orientation == 1 &&
              log.buttons[1].orientation == 0,
          "right control constructed before left (newest-first owner runs left first)");
    check(log.targets[0].position.x == first.grid.getPos(1, 1).x &&
              log.targets[1].position.x == first.grid.getPos(19, 1).x,
          "mirror uses cols-1-x, with source then mirror creation");
    check(first.ballCell.x == 10 && first.ballCell.y == 5, "star marker selects ball's cell");
    check(first.definition.real(0x638) == first.grid.getPos(3, 2).y &&
              first.definition.real(0x658) == first.grid.getPos(4, 3).y,
          "dash markers update row anchor slots from one");
    check(first.grid.cell(0, 0).ch == 0 && first.grid.cell(0, 0).bgOn == 100 &&
              first.grid.cell(0, 0).attr == 1,
          "background-only base cell setup");
    for (const auto &record : level::kLevelRecords) {
        LevelSetup s(record);
        BirthLog b;
        buildLevelBoard(s, record, b);
        check(s.grid.cols == record.columns && s.grid.rows == record.rows,
              "all shipped board shapes");
        check(s.targetsCreated == int(b.targets.size()) && s.targetsRemaining == s.targetsCreated,
              "all shipped target births accounted for");
        check(s.ballCell.x >= 0 && s.ballCell.x < s.grid.cols && s.ballCell.y >= 0 &&
                  s.ballCell.y < s.grid.rows,
              "shipped ball start is inside board");
    }
    auto custom = intro;
    std::memset(custom.map, 0, sizeof custom.map);
    std::strcpy(custom.map[0], ".a0%<>#@+12B*");
    custom.flags = 0x10;
    LevelSetup s(custom);
    BirthLog events;
    buildLevelBoard(s, custom, events);
    check(events.order == "RLTTHWB", "source-order setup events");
    check(events.targets.size() == 2 && events.targets[0].group == 1 &&
              events.targets[1].group == 0,
          "lowercase marker consumed by target; percent puts target over next cell");
    check(s.grid.cell(0, 4).ch == 101 && s.grid.cell(0, 5).ch == 102 &&
              s.grid.cell(0, 4).fgState == 4 && s.grid.cell(0, 4).bgIcon == -1,
          "pre-placed deflectors use proper animation and bare-background flag");
    check(s.grid.cell(0, 6).ch == 104 && s.grid.cell(0, 7).ch == 105 &&
              s.grid.cell(0, 8).ch == 106 && s.grid.cell(0, 8).bgIcon == 5,
          "wall and boost cell variants");
    check(s.deflectorsCreated == 0 && s.hazardsCreated == 1 && events.weights == 1 &&
              events.bombs == 1,
          "initial pieces do not count as player placements");
    BirthLog demo;
    buildLevelBoard(s, custom, demo, true);
    check(demo.order.empty(), "demo omits controls and map, not grid setup");

    auto &wall = s.grid.cell(0, 1);
    wall.ch = 104;
    wall.fgOn = 1;
    wall.durability = 2;
    int explosions = 0;
    bool sawOriginal = false;
    const auto explode = [&](neon::vec2d) {
        ++explosions;
        sawOriginal = wall.ch == 104;
    };
    blowCell(s, 1, 0, explode);
    check(!explosions && wall.ch == 104 && wall.durability == 1,
          "first hit weakens a two-hit wall");
    blowCell(s, 1, 0, explode);
    check(explosions == 1 && sawOriginal && wall.ch == 0 && !wall.fgOn &&
              wall.bgFrom.packRGBA() == 0xff8000ff,
          "last hit explodes before restoring background and scheduling flash");
    wall.ch = 105;
    blowCell(s, 1, 0, explode);
    check(explosions == 1 && wall.durability == 0, "solid wall ignores blasts");
    blowCell(s, -1, 0, explode);
    check(explosions == 1, "out-of-grid blast is ignored");
    wall.ch = 0;
    wall.durability = 0;
    blowCell(s, 1, 0, explode);
    check(explosions == 2, "empty cell explosion is retained, not cosmetically suppressed");

    DeflectorInput input;
    ParticleLog particles;
    input.particleCost = 10;
    DeflectorButton button(0);
    for (int i = 0; i < 44; ++i)
        button.run(input, particles);
    check(button.phase == 1 && !input.pending, "source entrance finishes before activation");
    neon::input::ControllerState pad;
    pad.connected = true;
    pad.buttons = neon::input::South;
    input.captureControllers(&pad, 1);
    button.run(input, particles);
    check(input.pending == 0 && particles.calls == 1 && particles.count == 10 &&
              particles.scale == 0.1f && particles.decay == 0.975f,
          "native action queues one orientation and original particle cost");
    input.pending.reset();
    for (int i = 0; i < 8; ++i)
        button.run(input, particles);
    check(!input.pending && particles.calls == 1, "held button never repeats placements");
    input.nativeHeld[0] = false;
    button.run(input, particles);
    input.touches[3] = {true, button.position};
    button.run(input, particles);
    check(input.pending == 0 && particles.calls == 2,
          "fourth touch uses same action as native controller");
    input.nativeHeld[1] = true;
    DeflectorButton right(1);
    right.phase = 1;
    right.run(input, particles);
    check(input.pending == 0 && particles.calls == 2,
          "second button cannot overwrite occupied queue");
    input.levelFinished = true;
    button.run(input, particles);
    check(button.dead, "buttons retire on level completion");
    pad.connected = false;
    input.captureControllers(&pad, 1);
    check(!input.nativeHeld[0] && !input.nativeHeld[1] && input.touches[3].down,
          "disconnect releases native input without changing physical touch slots");
    std::printf(
        "Deflex setup/controls: %d failures; 52 boards exercised (source-rule integration)\n",
        failures);
    return failures ? 1 : 0;
}
