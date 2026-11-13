// Source-rule integration checks. These are not original-execution fixtures.
#include "cell_grid_addressing.h"
#include <cmath>
#include <cstdio>

// Saved reference body for ripple arithmetic preservation.
void legacyDisplace(const deflex::CellGrid &grid, neon::vec3d &position) {
    for (const auto &source : grid.displacement) {
        const neon::vec2d delta{position.x - source.centre.x, position.y - source.centre.y};
        const float remaining = 128.0f - float(delta.length());
        if (remaining > 0.0f) {
            const float square = remaining * remaining;
            position.z = position.z + double(square) * 0.01 * source.strength;
        }
    }
}

int main() {
    using namespace deflex;
    int failures = 0;
    auto check = [&](bool ok, const char *rule) {
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "%s\n", rule);
        }
    };
    neon::colour palette[10];
    palette[0].red(1);
    palette[1].green(1);
    CellGrid geometry;
    for (int i = 0; i < 5; ++i) {
        geometry.displacement[i].centre = {i * 13.25, -i * 9.5};
        geometry.displacement[i].strength = (i - 2) * .125;
    }
    for (double x : {-256., -128., -127.999999, 0., .125, 127.999999, 128., 256.})
        for (double y : {-128., 0., 128.}) {
            neon::vec3d actual{x, y, 610.}, expected = actual;
            geometry.doDisplace(actual);
            legacyDisplace(geometry, expected);
            check(actual.x == expected.x && actual.y == expected.y && actual.z == expected.z,
                  "displacement float-square operation order");
        }
    GridCell c;
    c.bg.unpackRGBA(0x000000ff);
    c.bgOn = 1;
    c.setBCC(0.25f, 0xff0000ff);
    check(c.bg.packRGBA() == 0x000000ff && c.bgFrom.packRGBA() == 0xff0000ff &&
              c.bgTo.packRGBA() == 0x000000ff,
          "setBCC retains restore colour; visible change waits for run");
    c.run(palette);
    check(c.bg.packRGBA() == 0xbf0000ff && c.bgT == 0.25f,
          "background flash first frame interpolates toward restore");
    c.setBCC(0.25f, 0x00ff00ff);
    check(c.bgTo.packRGBA() == 0x000000ff, "retarget keeps first restore colour");
    for (int i = 0; i < 4; ++i)
        c.run(palette);
    check(c.bg.packRGBA() == 0x000000ff && c.fadeFlags == 3,
          "exact-one boundary retains source flags");
    c.bgT = 0.9f;
    c.run(palette);
    check(c.bgT == 1 && c.fadeFlags == 0, "overshoot clamps and clears fade flags");
    c.bg.unpackRGBA(0x000000ff);
    c.fadeFlags = 0;
    c.blendBCC(0.1f, 0xff0000ff, 0.5f);
    check(c.bg.packRGBA() == 0x7f0000ff && c.bgTo.packRGBA() == 0x000000ff,
          "blend quantizes and takes effect immediately");

    c = GridCell{};
    c.ch = 101;
    c.fgOn = 1;
    c.fgState = 4;
    c.dingDeflector();
    check(c.scale == 0.8f && c.fgT == 0 && c.fgSpeed == 0.04f && c.fgState == 4,
          "deflector ding preserves caller animation state");
    c.run(palette);
    check(c.col.a < 1 && c.col.a > 0.9, "ding starts foreground recovery fade");
    for (int i = 0; i < 4; ++i)
        c.dingDeflector();
    check(c.ch == 0x20 && !c.fgOn, "fifth ding consumes the deflector");

    int sequence[] = {0, 1, -1};
    c = GridCell{};
    c.col.blue(1);
    c.setFSequence(1, sequence);
    c.run(palette);
    check(c.col.g == 1 && c.fgT == 1, "foreground sequence advances before lookup");
    c.run(palette);
    check(c.col.r == 1 && c.fgT == 0, "foreground loop sentinel");
    int terminal[] = {0, -2};
    c.setFSequence(1, terminal);
    c.run(palette);
    check(c.fgSeq == nullptr && c.fgState == -1 && c.col.b == 1,
          "sequence replacement retains first restore colour");
    c = GridCell{};
    c.fgT = 0.75f;
    c.setBSequence(1, sequence);
    c.run(palette);
    c.run(palette);
    check(c.fgT == 0 && c.bgT == 2, "background loop resets foreground phase, as original");

    neon::DarwinRandom random(123);
    c = GridCell{};
    c.fgOn = c.bgOn = 1;
    c.smash(random);
    check(random.draws() == 6 && c.scale == 16 && c.decay == 0.98f && c.fgState == 2,
          "shared smash preserves six random draws and setup");
    for (int i = 0; i < 400; ++i)
        c.run(palette);
    check(!c.fgOn && !c.bgOn, "tumble retires both glyph and background");
    c = GridCell{};
    c.ch = 102;
    c.fgState = 4;
    for (int i = 0; i < 20; ++i)
        c.run(palette);
    check(c.fgAngle == 1.5707964f, "second deflector rotation caps at original float literal");
    c.ch = 101;
    for (int i = 0; i < 20; ++i)
        c.run(palette);
    check(c.fgAngle == 0, "first deflector rotation returns to zero");

    CellGrid g;
    g.setSize(8, 8);
    for (int y = 0; y <= g.rows; ++y)
        for (int x = 0; x <= g.cols; ++x)
            g.cell(y, x).bg.unpackRGBA(0x000000ff);
    const auto centre = g.getPos(3, 3);
    g.colOffset = 2;
    g.rowOffset = 1;
    g.blendRadius(0.125f, centre, 0.1f, 0xff0000ff, 1);
    check(g.cell(3, 3).bg.packRGBA() == 0xff0000ff && g.cell(3, 4).bg.packRGBA() == 0x000000ff,
          "radius centre full, edge excluded, direct physical cells ignore scroll");
    g.radiusBlendDisabled = true;
    g.blendRadius(1, centre, 0.1f, 0x00ff00ff, 1);
    check(g.cell(3, 3).bg.packRGBA() == 0xff0000ff, "radius disable flag");
    g.radiusBlendDisabled = false;
    g.blendRadius(0, centre, 0.1f, 0x00ff00ff, 1);
    check(g.cell(3, 3).bg.packRGBA() == 0xff0000ff, "zero radius leaves cells unchanged");
    g.blendRadius(-0.125f, g.getPos(5, 5), 0.1f, 0x00ff00ff, 1);
    check(g.cell(5, 5).bg.packRGBA() == 0x00ff00ff, "negative radius uses magnitude");
    g.messageLines[1] = {1, 4};
    g.messageLines[2] = {1, 0};
    g.fadeLines(1, 2);
    check(g.messageLines[1].state == 2 && g.messageLines[2].state == 1,
          "fade only active message lines");
    g.fadeLines(2, 1);
    check(g.messageLines[1].state == 2, "reversed line range no-op");
    g.displacement[0] = {{0, 0}, 1};
    g.displacement[1] = {{0, 0}, -0.5};
    neon::vec3d v{0, 0, 610};
    g.doDisplace(v);
    check(std::fabs(v.z - 691.92) < 1e-10, "displacements accumulate in source order");
    v = {128, 0, 610};
    g.doDisplace(v);
    check(v.z == 610, "displacement boundary excluded");
    std::printf("Deflex cell effects: %d failures (source-rule integration, not original replay)\n",
                failures);
    return failures ? 1 : 0;
}
