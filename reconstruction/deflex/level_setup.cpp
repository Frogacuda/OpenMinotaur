#include "level_setup.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace deflex {
uint32_t LevelDefinition::word(int offset) const {
    const auto *b = bytes_.data() + offset;
    return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}
float LevelDefinition::scalar(int offset) const {
    const auto bits = word(offset);
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
double LevelDefinition::real(int offset) const {
    const uint64_t bits = uint64_t(word(offset)) | uint64_t(word(offset + 4)) << 32;
    double value;
    std::memcpy(&value, &bits, 8);
    return value;
}
void LevelDefinition::setReal(int offset, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    for (int i = 0; i < 8; ++i)
        bytes_[size_t(offset + i)] = uint8_t(bits >> (i * 8));
}
namespace {
uint32_t packedLerp(uint32_t a, uint32_t b, float t) {
    neon::colour left, right;
    left.unpackRGBA(a);
    right.unpackRGBA(b);
    return left.lerp(right, t).packRGBA();
}
uint32_t boardColour(const CellGrid &grid, int x, int y, const uint32_t *corners) {
    const auto position = grid.getPos(x, y);
    const float horizontal = std::min(1.0f, std::fabs(float(position.x + position.x)));
    const float vertical = std::min(1.0f, std::fabs(float(position.y + position.y)));
    const auto top = packedLerp(corners[1], corners[0], horizontal);
    const auto bottom = packedLerp(corners[3], corners[2], horizontal);
    return packedLerp(bottom, top, vertical);
}
} // namespace
void placeDeflector(CellGrid &grid, int x, int y, int orientation, bool bareBackground) {
    auto &c = grid.cell(y, x);
    c.fadeTo.white(1);
    c.fgFrom.white(1);
    c.col.white(1);
    c.fgT = 0;
    c.fgSpeed = 0.04f;
    c.scale = 1;
    c.rot = {0, 0, 0};
    c.offset = {0, 0, 0};
    c.ch = orientation ? 102 : 101;
    c.fgOn = 1;
    c.fgIcon = 1;
    if (bareBackground)
        c.bgIcon = -1;
    c.attr |= 2;
    c.fgAngle = orientation ? 1.5707964f : 0.0f;
    c.fgState = 4;
}
void buildLevelBoard(LevelSetup &state, const level::LevelRecordData &record, LevelBirths &births,
                     bool demo) {
    state.definition = LevelDefinition(record);
    auto &grid = state.grid;
    auto &d = state.definition;
    state.flags = record.flags;
    state.flippingDeflectors = (record.flags & 8) != 0;
    state.mirrored = record.map[0][0] == 'M';
    grid.setSize(record.columns, record.rows);
    for (int y = 0; y <= grid.rows; ++y)
        for (int x = 0; x <= grid.cols; ++x) {
            auto &c = grid.cell(y, x);
            c.fgOn = 0;
            c.ch = 0;
            c.attr |= 1;
            c.bg.unpackRGBA(record.cellColour);
            c.bgIcon = d.integer(0x5d8);
            c.bgOn = 100;
        }
    state.targetsRemaining = state.targetsCreated = state.hazardsCreated = state.deflectorsCreated =
        0;
    if (demo)
        return; // Original demo skips controls/map/UI, but still makes a ball later.
    births.deflector(DeflectorButton(1));
    births.deflector(DeflectorButton(0));
    int anchor = 1, group = 0;
    bool targetOnNextCell = false;
    uint32_t corners[4];
    for (int i = 0; i < 4; ++i)
        corners[i] = d.word(0x690 + i * 4);
    static constexpr uint32_t plusColours[4] = {0xff0000ff, 0xff2050ff, 0xff2050ff, 0xff0000ff};
    const auto target = [&](int x, int y, int marker) {
        TargetBirth b;
        b.position = grid.getPos(x, y);
        b.frame = d.integer(0xce0);
        b.colour = d.word(0xce4);
        b.size = d.scalar(0xce8);
        b.spriteScale = {d.real(0xcec), d.real(0xcf4)};
        b.group = marker;
        b.movementFlags = d.word(0xcfc);
        b.behaviourFlags = d.word(0xd00);
        ++state.targetsRemaining;
        births.target(b);
        ++state.targetsCreated;
    };
    for (int y = 0; y < grid.rows; ++y) {
        for (int x = 0; x < level::kMapStride && record.map[y][x]; ++x) {
            const char token = record.map[y][x];
            const int opposite = grid.cols - 1 - x;
            if (x > grid.cols)
                throw std::runtime_error("Deflex map exceeds its original grid");
            if (token >= 'a' && token <= 'z')
                group = token - 'a' + 1;
            const auto both = [&](const auto &operation) {
                operation(x);
                if (state.mirrored)
                    operation(opposite);
            };
            switch (token) {
            case '#':
            case '@':
            case '+': {
                const auto colour =
                    token == '@' ? d.word(0x5e8)
                                 : boardColour(grid, x, y, token == '+' ? plusColours : corners);
                const int icon = token == '+' ? 5 : d.integer(token == '#' ? 0x5e0 : 0x5e4);
                both([&](int column) {
                    auto &c = grid.cell(y, column);
                    c.durability = d.integer(0xd04);
                    c.ch = token == '#' ? 104 : token == '@' ? 105 : 106;
                    c.bg.unpackRGBA(colour);
                    c.bgIcon = icon;
                    c.attr |= 1;
                    c.fgAngle = 0;
                });
                break;
            }
            case '*':
                state.ballCell = {x, y};
                break;
            case '-':
                d.setReal(0x618 + anchor * 0x20, grid.getPos(x, y).y);
                ++anchor;
                break;
            case '0':
                target(x, y, group);
                if (state.mirrored)
                    target(opposite, y, 0);
                group = 0;
                break;
            case '1':
                both([&](int column) {
                    ++state.hazardsCreated;
                    births.hazard(grid.getPos(column, y), d.integer(0xd0c));
                });
                break;
            case '2':
                if (d.integer(0xd0c) == 0)
                    both([&](int column) { births.weight(grid.getPos(column, y)); });
                break;
            case '<':
            case '>': {
                const int orientation = token == '>';
                const auto colour = boardColour(grid, x, y, corners) ^ 0x0000ff00;
                const auto place = [&](int column, int direction) {
                    ++state.deflectorsCreated;
                    placeDeflector(grid, column, y, direction, (state.flags & 0x10) != 0);
                    auto &c = grid.cell(y, column);
                    c.fadeTo.unpackRGBA(colour);
                    c.fgFrom.unpackRGBA(colour);
                };
                place(x, orientation);
                if (state.mirrored)
                    place(opposite, !orientation);
                break;
            }
            case 'B':
                both([&](int column) { births.bomb(grid.getPos(column, y)); });
                break;
            case '%':
                targetOnNextCell = true;
                continue;
            default:
                break;
            }
            if (targetOnNextCell) {
                target(x, y, group);
                if (state.mirrored)
                    target(opposite, y, 0);
                group = 0;
                targetOnNextCell = false;
            }
        }
    }
    // newLevel resets this total after creating pre-placed deflectors. Runtime
    // uses it to count player placements, not the initial map's pieces.
    state.deflectorsCreated = 0;
}
void blowCell(LevelSetup &state, int x, int y, const std::function<void(neon::vec2d)> &explosion) {
    // theGame::blow 573cc. Explosions happen before the cell is restored, and
    // even an empty eligible cell reaches this path; do not suppress its RNG.
    auto &grid = state.grid;
    if (x < 0 || y < 0 || x > grid.cols || y > grid.rows)
        return;
    auto &cell = grid.cell(y, x);
    if (cell.ch == 105)
        return;
    cell.durability = int32_t(uint32_t(cell.durability) - 1u);
    if (cell.durability < 1) {
        explosion(grid.getPos(x, y));
        cell.bg.unpackRGBA(state.definition.word(0x5d4));
        cell.bgTo = cell.bg;
        cell.ch = 0;
        cell.fgOn = 0;
        cell.bgIcon = state.definition.integer(0x5d8);
    }
    cell.setBCC(0.1f, 0xff8000ff);
}
} // namespace deflex
