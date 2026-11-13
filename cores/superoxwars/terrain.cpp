#include "terrain.h"
#include "original_tables.h"
#include "ground.h"
#include <algorithm>
#include <cstring>
namespace superoxwars {
void Terrain::init(int area, bool preserveCache) {
    script = area % 10;
    cursor = worldRow = 0;
    mirrored = (world.area / 10) & 1;
    if (!preserveCache) {
        world.tiles.cells = {};
        world.tiles.rowOffset = 0;
        world.tiles.scroll = {};
        world.tiles.origin = {};
        world.tiles.scale = {1.25, 1};
    }
    const int bank = area & 1;
    const auto *c = original::AreaColours.data() + script * 16;
    for (auto entry : std::array<std::array<int, 2>, 7>{{{bank + 1, 0},
                                                         {bank | 4, 1},
                                                         {bank | 6, 4},
                                                         {bank | 8, 5},
                                                         {bank | 10, 6},
                                                         {bank | 12, 7},
                                                         {bank | 14, 8}}})
        colours[size_t(entry[0])] = unpack(c[entry[1]]);
    for (int n = 0; n < world.level / 10; ++n)
        for (int index : {bank + 1, bank | 4}) {
            auto &col = colours[size_t(index)];
            col = {col.g, col.b, double(float(col.r)), col.a};
        }
    if (world.demo)
        for (int index : {bank + 1, bank | 4}) {
            auto &col = colours[size_t(index)];
            const double dim = double(.9f);
            col = {col.r * dim, col.g * dim, col.b * dim, col.a * dim};
        }
    for (auto &seed : rowSeeds)
        seed = world.random.next();
}
bool Terrain::scroll(float delta) {
    auto &t = world.tiles;
    t.scroll.y += double(delta);
    if (t.scroll.y >= 0 && t.scroll.y < 1)
        return false;
    if (t.scroll.y >= 1) {
        t.scroll.y -= 1;
        t.rowOffset = (t.rowOffset + TileScreen::BufferRows - 1) % TileScreen::BufferRows;
        ++worldRow;
        if (++cursor >= int(original::RowsLength[size_t(script)]))
            cursor = 0;
    } else {
        t.scroll.y += 1;
        t.rowOffset = (t.rowOffset + 1) % TileScreen::BufferRows;
        --worldRow;
        if (--cursor < 0)
            cursor = int(original::RowsLength[size_t(script)]) - 1;
    }
    generate(t.rowOffset);
    if (crossed)
        crossed();
    return true;
}
void Terrain::generate(int physicalRow) {
    auto &t = world.tiles;
    for (int x = 0; x <= TileScreen::Columns; ++x)
        t.poke(x, physicalRow, 0);
    const int row = original::Rows[size_t(script)][cursor];
    const char *pattern = original::RowPatterns[size_t(std::abs(row))];
    uint32_t bits = rowSeeds[size_t(worldRow & 1023)];
    const bool reversed = row < 0;
    const int bank = world.area & 1;
    const int base = bank + 1, edge = bank | 4;
    auto bulb = [&](int cell, bool green) {
        auto *p = new Crater(world, cell, green ? .0225f : .015f);
        p->frame = 2;
        p->phaseStep = green ? .03f : .015f;
        p->positionDelta.y = green ? (reversed ? -double(.025f) : double(.025f)) : .03125;
        p->cycle = green ? original::EdgeColours.data() : original::CraterColours.data();
        world.add(Domain::Decor, p);
    };
    for (int x = 0; x <= TileScreen::Columns; ++x) {
        char c = pattern[x];
        if (pattern[0] >= 'a' && pattern[0] <= 'z' && x == 0)
            c = pattern[1];
        int column = mirrored ? TileScreen::Columns - x : x;
        int glyph = 0, tint = base;
        if (c == '#' || (c >= '0' && c <= '9')) {
            if (pattern[0] == 'c') {
                glyph = 50;
                const int n = mirrored ? '6' - c : c - '0';
                const int banks[7] = {base,      edge,      bank | 6, bank | 8,
                                      bank | 10, bank | 12, bank | 14};
                // iOS 45d68 indexes a seven-entry stack table with '#'-'0'.
                // Unmirrored this aliases saved bank|6 (sp+74); mirrored it
                // reads the saved frame pointer (sp+f4), an address-dependent
                // colour. Use the stable paired-row colour for that marker.
                if (c == '#')
                    tint = bank | 6;
                else {
                    if (n < 0 || n >= 7) {
                        world.unsupported("Terrain colour-cell bank index");
                        return;
                    }
                    tint = banks[n];
                }
            } else if ((bits & 0xf0) < 0x10)
                glyph = int((bits & 0x7ff) >> 8) + 22;
            if (world.area == 5 && pattern[0] != 'c')
                glyph = 49;
        }
        switch (c) {
        case ' ':
            glyph = 0;
            tint = 0;
            break;
        case '~':
            mirrored = !mirrored;
            glyph = 0;
            tint = 0;
            break;
        case '.':
            glyph = int(bits & 3) + (x < 8 ? 1 : 5);
            tint = edge;
            break;
        case '<':
            glyph = mirrored ? (reversed ? 12 : 10) : (reversed ? 11 : 9);
            tint = edge;
            break;
        case '>':
            glyph = mirrored ? (reversed ? 11 : 9) : (reversed ? 12 : 10);
            tint = edge;
            break;
        case '[':
            glyph = int(bits & 1) + (mirrored ? 15 : 13);
            tint = edge;
            break;
        case ']':
            glyph = int(bits & 1) + (mirrored ? 13 : 15);
            tint = edge;
            break;
        case '^':
            glyph = int(bits & 1) + (reversed ? 18 : 20);
            tint = edge;
            break;
        case '$':
            glyph = 0;
            tint = column <= 8 ? (bank | 6) : column <= 12 ? base : (bank | 8);
            break;
        case '`':
            glyph = mirrored ? (reversed ? 9 : 11) : (reversed ? 10 : 12);
            tint = edge;
            break;
        case '|':
            glyph = mirrored ? (reversed ? 10 : 12) : (reversed ? 9 : 11);
            tint = edge;
            break;
        case '+':
            glyph = mirrored ? (reversed ? 38 : 40) : (reversed ? 39 : 41);
            break;
        case '=':
            glyph = (x > 0 && pattern[x - 1] == '=')
                        ? (reversed ? 42 : 45)
                        : (reversed ? (mirrored ? 39 : 38) : (mirrored ? 41 : 40));
            break;
        case ':':
            glyph = mirrored ? 44 : 43;
            break;
        case ';':
            glyph = mirrored ? 43 : 44;
            break;
        case '&':
            mirrored = !mirrored;
            glyph = (bits & 0xf0) < 0x10 ? int((bits & 0x7ff) >> 8) + 22 : 0;
            break;
        case '%':
            glyph = 46;
            tint = bank | 6;
            break;
        case '_':
            glyph = 46;
            tint = bank | 10;
            break;
        case '@':
            glyph = 46;
            tint = edge;
            break;
        case '*':
            actorGlyph = 46;
            actorTint = -1;
            if (createActor)
                createActor(c, physicalRow * TileScreen::Stride + column, -1);
            glyph = actorGlyph;
            tint = edge;
            break;
        case '?':
            actorGlyph = 0;
            actorTint = -1;
            if (createActor)
                createActor(c, physicalRow * TileScreen::Stride + column, -1);
            glyph = actorGlyph;
            break;
        case '(':
            glyph = mirrored ? 33 : 32;
            bulb(physicalRow * TileScreen::Stride + column - (mirrored ? 1 : 0), false);
            if (cursor & 1) {
                const int adjacent = column + (mirrored ? 1 : -1);
                if (adjacent >= 0 && adjacent <= TileScreen::Columns &&
                    t.peek(adjacent, physicalRow) == 0)
                    t.poke(adjacent, physicalRow, (mirrored ? 33 : 32) | (base << 8));
            }
            break;
        case ')':
            glyph = mirrored ? 32 : 33;
            if (cursor & 1) {
                const int adjacent = column + (mirrored ? -1 : 1);
                if (adjacent >= 0 && adjacent <= TileScreen::Columns &&
                    (!mirrored || t.peek(adjacent, physicalRow) == 0))
                    t.poke(adjacent, physicalRow, (mirrored ? 32 : 33) | (base << 8));
            }
            break;
        case '{':
            glyph = mirrored ? (reversed ? 35 : 37) : (reversed ? 34 : 36);
            bulb(physicalRow * TileScreen::Stride + column, true);
            break;
        case '}':
            glyph = mirrored ? (reversed ? 34 : 36) : (reversed ? 35 : 37);
            break;
        case '-':
            glyph = -1;
            break;
        default:
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                if (c >= 'a' && c <= 'z') {
                    if (!(cursor & 1)) {
                        glyph = -1;
                        break;
                    }
                    c = char(c - 'a' + 'A');
                }
                glyph = 0;
                const char next = x < TileScreen::Columns ? pattern[x + 1] : 0;
                const int variation = next >= '0' && next <= '9' ? next - '0' : -1;
                actorGlyph = 0;
                actorTint = -1;
                if (createActor)
                    createActor(c, physicalRow * TileScreen::Stride + column, variation);
                else
                    world.unsupported("Terrain actor " + std::string(1, c));
                glyph = actorGlyph;
                if (actorTint >= 0)
                    tint = actorTint;
            } else if (c != '#' && !(c >= '0' && c <= '9'))
                world.unsupported("Terrain glyph " + std::string(1, c));
        }
        if (glyph >= 0)
            t.poke(column, physicalRow, glyph | (tint << 8));
        const auto word = bits & 0xffff;
        bits = (word >> 1) | ((bits & 1) ? 0x8000 : 0);
    }
}
void Terrain::prepare(std::vector<ActorDraw> &out) {
    auto &t = world.tiles;
    for (int i = 0; i < 16; ++i)
        colours[size_t(i + 16)] = world.palette[size_t(i)];
    if (!world.demo && (world.level % 10 == 7 || world.level % 10 == 8))
        colours[1] = world.palette[0];
    const Vec2 cell{.05000000074505806 * t.scale.x, .0625 * t.scale.y};
    const double left =
        (cell.x * double(float(TileScreen::Columns)) * -.5 + t.origin.x * t.scale.x) - cell.x * .5 +
        t.scroll.x * cell.x;
    double y = (cell.y * double(float(TileScreen::Rows)) * -.5 + t.origin.y * t.scale.y) -
               cell.y * .5 + t.scroll.y * cell.y;
    for (int row = 0; row < TileScreen::BufferRows; ++row, y += cell.y) {
        const int physical = (t.rowOffset + row) % TileScreen::BufferRows;
        double x = left;
        for (int col = 0; col <= TileScreen::Columns; ++col, x += cell.x) {
            const size_t index = size_t(physical * TileScreen::Stride + col);
            t.positions[index] = {x, y};
            const uint32_t word = uint32_t(t.cells[index]);
            const int tint = int((word & 65535) >> 8);
            if (y >= cell.y + .5 || tint == 0)
                continue;
            const int glyph = word & 255;
            ActorDraw draw;
            draw.position = {x, y};
            draw.radius = .5f;
            draw.scale = cell;
            draw.colour = colours[size_t(tint)];
            draw.frame = 0;
            draw.explicitUv = true;
            draw.uv = {double(glyph % 28 * 18 + 1) / 512., double(glyph / 28 * 18 + 426) / 512.};
            draw.uvSize = {16. / 512., 16. / 512.};
            out.push_back(draw);
        }
    }
}
} // namespace superoxwars
