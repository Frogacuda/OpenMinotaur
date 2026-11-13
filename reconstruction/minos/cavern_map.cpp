// cavern_map.cpp - stm (cave map) line generation. See cavern_map.h for the overview.
#include "cavern_map.h"
#include <cassert>
#include <cmath>
#include <cstring>

namespace minos::cavern {
namespace {

// Row literals that getLine builds as local char arrays rather than taking from a
// C.NNN table (string literals in the original __cstring section).
constexpr const char *kBlankRow = "]                  [ ";
constexpr const char *kStartRow = "]        S         [ ";   // zone 1, first line
constexpr const char *kEndTopRow = "@TTTTTTTTTTTTTTTTTT@ ";  // past the last sector, first line
constexpr const char *kEndFillRow = "@BBBBBBBBBBBBBBBBBB@ "; // past the last sector, other lines
constexpr const char *kShaftTop = "@M M          MM   @ ";   // zone 2
constexpr const char *kShaftLip = "@TTTTT        TTTTT@ ";
constexpr const char *kShaftWall = "@BBBBB        BBBBB@ ";
constexpr const char *kShaftBottom = "@BBBBBDD    DDBBBBB@ ";
constexpr const char *kPitWall = "@BBBBBBB    BBBBBBB@ "; // zone 3
constexpr const char *kPitGoal = "@BBBBBBB G  BBBBBBB@ ";

const char *tableRow(int symbol, int index) {
    const RowTable t = rowTable(symbol);
    assert(t.rows && index >= 0 && index < t.count);
    return t.rows[index];
}

// How a sector line is taken from a C.NNN table. Original getLine cases 6, 13..96 index
// a table by lineInSector (table identity confirmed by tracing the
// frame copy read by each case in original execution).
struct TableZone {
    int16_t table;
    uint8_t columns;
    bool mirrored;
};
constexpr TableZone kNone{0, 0, false};
constexpr TableZone D(int t) {
    return {int16_t(t), 20, false};
}
constexpr TableZone D21(int t) {
    return {int16_t(t), 21, false};
}
constexpr TableZone M(int t) {
    return {int16_t(t), 20, true};
}
constexpr TableZone kTableZones[97] = {
    kNone,  kNone,  kNone,  kNone,  kNone,  kNone,    D21(284), kNone,  // 0-7
    kNone,  kNone,  kNone,  kNone,  kNone,  D21(280), D21(283), kNone,  // 8-15
    M(284), M(280), M(283), D(287), M(287), kNone,    D(261),   M(261), // 16-23
    D(262), M(262), D(263), M(263), D(260), M(260),   D(272),   D(275), // 24-31
    M(275), D(254), M(254), D(255), M(255), D(256),   M(256),   D(257), // 32-39
    M(257), D(258), M(258), D(259), D(267), M(267),   D(291),   D(292), // 40-47
    M(292), D(293), M(293), D(294), D(295), M(295),   D(296),   M(296), // 48-55
    kNone,  kNone,  kNone,  kNone,  kNone,  D(297),   kNone,    D(298), // 56-63
    D(299), M(299), D(300), M(300), D(301), D(289),   D21(285), D(268), // 64-71
    M(268), D(273), D(302), M(302), D(303), M(303),   D(304),   M(304), // 72-79
    D(305), D(306), M(306), D(307), M(307), D(308),   M(308),   D(309), // 80-87
    M(309), D(281), M(281), D(282), D(290), D(276),   D(277),   D(278), // 88-95
    D(274),                                                             // 96
};

float waveExtent(float phase, float depth) {
    // float sinf, widened; 0.5 + s*0.5 in binary64 (vmla, not fused); narrowed; times depth
    const float s = std::sin(phase);
    const double d = 0.5 + double(s) * 0.5;
    return float(d) * depth;
}
float waveExtentCentred(float phase, float depth) {
    // algoCavern5: depth*f + depth*0.5f in binary32 (NEON vmul/vadd)
    const float s = std::sin(phase);
    const float f = float(0.5 + double(s) * 0.5);
    return depth * f + depth * 0.5f;
}

} // namespace

char mirrorTile(char c) {
    static constexpr char from[] = "LlrR][<>(){}";
    static constexpr char to[] = "RrlL[]><)(}{";
    for (int i = 0; i < 12; i++)
        if (c == from[i])
            return to[i];
    return c;
}

// ------------------------------------------------------------------ profile --

void buildWallProfile(const CavernDefinition &cavern, int sectorLength, WallProfile &p) {
    if (!(cavern.flags & 4))
        return;
    auto key = [&](int track, int sector) {
        float f;
        uint32_t bits = cavern.profileKeys[track][sector];
        static_assert(sizeof f == sizeof bits);
        std::memcpy(&f, &bits, sizeof f);
        return double(f);
    };
    auto interpolate = [](double a, double b, double t) { // Neon_Internals::interpolate 0xdf40
        const double d = b - a;
        const double product = d * t; // vmla.f64: rounded product, then the add
        return a + product;
    };
    const float step = 1.0f / float(sectorLength);
    float left = 0, right = 0; // phase accumulators persist across sectors
    int out = 0;
    for (int k = 1; cavern.zones[k - 1] >= 0; k++) {
        float t = 0;
        for (int i = 0; i < sectorLength; i++, out++) {
            const double tt = double(t);
            t += step;
            left += float(interpolate(key(0, k - 1), key(0, k), tt));
            right += float(interpolate(key(1, k - 1), key(1, k), tt));
            const float depthL = float(interpolate(key(2, k - 1), key(2, k), tt));
            const float depthR = float(interpolate(key(3, k - 1), key(3, k), tt));
            assert(out < WallProfile::kLines);
            p.leftPhase[out] = left;
            p.rightPhase[out] = right;
            p.leftDepth[out] = depthL;
            p.rightDepth[out] = depthR;
        }
        if (k >= kMaxZones)
            break;
    }
}

// ------------------------------------------------------------------- StripMap --

StripMap::StripMap() : cells(size_t(kMapRows) * kMapColumns, 0u) {}

void StripMap::setCavern(const CavernDefinition *cavern, int zoneLength) {
    cavern_ = cavern;
    zoneLength_ = zoneLength;
    sectorLength_ = zoneLength << 1;
}

void StripMap::installFlashTables() {
    for (int i = 0; i < 16; i++) {
        flashModes[i] = kFlashModes[i];
        flashOnTiles[i] = kFlashOnTiles[i];
        flashOffTiles[i] = kFlashOffTiles[i];
    }
}

uint32_t *StripMap::cell(int r, int c) {
    // The original indexes stm+0x2de4 + (r*128 + c)*4 without bounds. Shipped caverns begin
    // with zone 5 (only ']' and '['), so lines above the map never read or write a flag
    // that matters; out-of-range cells use a scratch word instead of adjacent members.
    if (r < 0 || r >= kMapRows || c < 0 || c >= kMapColumns) {
        outOfMapCell_ = 0;
        return &outOfMapCell_;
    }
    return &cells[size_t(r) * kMapColumns + size_t(c)];
}

Vec2 StripMap::cellPosition() const {
    const int sy = int(scroll.y), sx = int(scroll.x); // truncation (fpscr mode 3)
    const Vec2 size{cellSize.x * scale.x, cellSize.y * scale.y};
    const Vec2 base{origin.x * scale.x, origin.y * scale.y};
    const float w = float(columns), c = float(column - sx);
    const float h = float(rows), r = float(row - sy);
    Vec2 p;
    p.y = base.y + size.y * double(h) * -0.5 + size.y * 0.5 + size.y * fraction.y +
          size.y * double(r);
    p.x = base.x + size.x * double(w) * -0.5 + size.x * 0.5 + size.x * fraction.x +
          size.x * double(c);
    return p;
}

int32_t StripMap::translate(char ch) {
    const int c = ch;
    if (uint8_t(c - '0') < 10) {
        const int digit = c - '0';
        const uint32_t mode = uint32_t(flashModes[digit]) & 0x7fff;
        if (mode < 4 && flash_) {
            const bool lit = flash_->lit[mode] != 0;
            if (flash_->watched[mode] != 0)
                flash_->seen[mode] = 1;
            if (lit)
                return flashOnTiles[digit];
        }
        return flashOffTiles[digit];
    }
    uint32_t *here = cell(row, column);
    const uint32_t flags = *here;
    switch (c) {
    case ' ':
        return tile::kEmpty;
    case '#':
        if (!(flags & cellflag::kRemoved)) {
            *here = flags | cellflag::kGenerated;
            return tile::kHashOrStar;
        }
        return tile::kEmpty;
    case '*':
        if ((flags & 0x1ff) == 0 && host_)
            host_->launch('*', cellPosition(), column, row);
        if (!(flags & cellflag::kRemoved)) {
            uint32_t *again = cell(row, column); // re-read: launch may have written the cell
            *again |= cellflag::kGenerated;
            return tile::kHashOrStar;
        }
        return tile::kEmpty;
    case '-':
        return tile::kDash;
    case '=':
        return tile::kEquals;
    case '@':
        return tile::kAt;
    case 'B':
        return int32_t((uint32_t((row & 0x3f) + 0x40) << 16) | 0x1f);
    case 'C':
        return tile::kUpperC;
    case 'F':
        return tile::kUpperF;
    case 'L':
        return tile::kUpperL;
    case 'R':
        return tile::kUpperR;
    case 'T':
        return tile::kUpperT;
    case '[':
        return tile::kOpenBracket;
    case ']':
        return tile::kCloseBracket;
    case 'l':
        return tile::kLowerL;
    case 'r':
        return tile::kLowerR;
    default:
        // Every other character (letters such as 'M', 'Y', 'G', 'S', '\0' and punctuation)
        // stands for an entity: launch it unless the cell already has an owner.
        if ((flags & cellflag::kOwnerMask) == 0 && host_)
            host_->launch(ch, cellPosition(), column, row);
        return tile::kEmpty;
    }
}

void StripMap::getLine(int x, int line, int offset) {
    (void)x; // passed by theGame::_bgdraw (horizontal cell) but unused by getLine
    const int sector = line / sectorLength_;
    const int zone = cavern_->zones[sector < 0 ? 0 : sector];
    lineInSector = line % sectorLength_;
    row = line;
    if (gameWord9dc_)
        *gameWord9dc_ = 0;

    auto emit = [&](const char *text, int count) {
        for (column = 0; column < count; column++)
            tiles[size_t(offset + column)] = translate(text[column]);
    };
    auto emitMirrored = [&](const char *text) {
        for (column = 0; column < columns; column++)
            tiles[size_t(offset + column)] = translate(mirrorTile(text[columns - 1 - column]));
    };
    const int full = columns + 1, narrow = columns;

    if (zone < 0) {
        emit(lineInSector != 0 ? kEndFillRow : kEndTopRow, full);
        return;
    }

    const int zoneParity = (line / zoneLength_) & 1;
    const int lineInZone = line % zoneLength_;
    const int ls = lineInSector;
    switch (zone) {
    case 0: // alternating ledges: 271 on even zones, 270 on odd; rows past 5 repeat row 5
    case 15: {
        const int table = (zoneParity == 0) == (zone == 0) ? 271 : 270;
        emit(tableRow(table, lineInZone < 5 ? lineInZone : 5), full);
        return;
    }
    case 1:
        emit(ls == 0 ? kStartRow : kBlankRow, full);
        return;
    case 2: {
        const char *text = ls == 1      ? kShaftLip
                           : ls == 0x17 ? kShaftBottom
                           : ls == 0    ? kShaftTop
                                        : kShaftWall;
        emit(text, full); // (0x17 is a literal, not sectorLength-1)
        return;
    }
    case 3:
        emit(ls == sectorLength_ - 1 ? kPitGoal : kPitWall, full);
        return;
    case 4:
    case 5:
        emit(kBlankRow, full);
        return;
    case 7:
        emit(ls < 7 ? tableRow(266, ls) : kBlankRow, full);
        return;
    case 8:
        emit(ls < 7 ? tableRow(269, ls) : kBlankRow, full);
        return;
    case 9:
        emit(tableRow(286, lineInZone), full);
        return;
    case 10:
        emit(tableRow(264, ls < 3 ? ls : 3), full);
        return;
    case 11:
        emit(tableRow(265, ls < 3 ? ls : 3), full);
        return;
    case 12: {
        const bool blank = ls > zoneLength_ ? lineInZone <= zoneLength_ - 8 : ls <= zoneLength_;
        emit(blank ? kBlankRow : tableRow(279, lineInZone - (zoneLength_ - 7)), full);
        return;
    }
    case 21:
        // The original's local copy of C.288 has 16 rows but is indexed by lineInSector.
        // With 24-line sectors, lines 16..23 read the next local array (zone 9's C.286 copy).
        // Potential original defect; reproduced as executed.
        emit(ls < 16 ? tableRow(288, ls) : tableRow(286, ls - 16), narrow);
        return;
    case 56:
        firstEdgeChar = 'T';
        edgeChar = '@';
        fillChar = 0;
        lastEdgeChar = 0;
        featureChar = 'B';
        algoCavern1(offset);
        return;
    case 57:
        firstEdgeChar = 'T';
        edgeChar = '@';
        fillChar = 0;
        lastEdgeChar = 0;
        featureChar = '*';
        algoCavern2(offset);
        return;
    case 58:
        firstEdgeChar = 'T';
        edgeChar = 'B';
        fillChar = 0;
        lastEdgeChar = 0;
        featureChar = '*';
        algoCavern3(offset);
        return;
    case 59:
        fillChar = 0;
        edgeChar = '@';
        firstEdgeChar = '*';
        lastEdgeChar = '*';
        featureChar = '*';
        algoCavern4(offset);
        return;
    case 60:
        fillChar = 0;
        firstEdgeChar = 0;
        lastEdgeChar = 0;
        edgeChar = '#';
        featureChar = '*';
        coverChar = 'A';
        algoCavern5(offset);
        return;
    case 62:
        fillChar = 0;
        firstEdgeChar = 0;
        lastEdgeChar = 0;
        edgeChar = '1';
        featureChar = '*';
        coverChar = 'Y';
        algoCavern5(offset);
        return;
    default:
        break;
    }
    if (zone > 96)
        return; // outside the original jump table: line left untouched
    const TableZone &z = kTableZones[zone];
    assert(z.table != 0);
    const char *text = tableRow(z.table, ls);
    if (z.mirrored)
        emitMirrored(text);
    else
        emit(text, z.columns == 21 ? full : narrow);
}

// ------------------------------------------------------- algorithmic zones --

char StripMap::rowEdge() const {
    char edge = edgeChar;
    if (firstEdgeChar > 0 && lineInSector == 0)
        edge = firstEdgeChar;
    if (lastEdgeChar > 0 && lineInSector == sectorLength_ - 1)
        edge = lastEdgeChar;
    return edge;
}

void StripMap::prefillUnset(int offset) {
    for (column = 0; column < columns; column++) {
        tiles[size_t(offset + column)] = tile::kUnset;
        columnOpen[size_t(column)] = 1;
    }
}

void StripMap::placeFeature(int offset, int leftIndex, int rightIndex, bool moveCursor) {
    // A feature character just inside each wall, the right one mirrored. algoCavern1 leaves
    // the generation cursor where the previous loop ended; algoCavern2-4 point it at each cell.
    if (featureChar <= 0)
        return;
    if (moveCursor)
        column = leftIndex;
    const int32_t left = translate(featureChar);
    if (moveCursor)
        column = rightIndex;
    tiles[size_t(offset + leftIndex)] = left;
    tiles[size_t(offset + rightIndex)] = translate(mirrorTile(featureChar));
}

void StripMap::fillUnsetEdges(int offset) {
    const char edge = rowEdge();
    for (column = 0; column < columns;) {
        const size_t at = size_t(offset + column);
        if (tiles[at] != tile::kUnset) {
            column++;
            continue;
        }
        const int32_t code = translate(edge);
        column++;
        tiles[at] = code;
    }
}

void StripMap::coverOpenFloor(int offset, char cover) {
    for (column = 0; column < columns;) {
        const size_t at = size_t(offset + column);
        if (tiles[at] != 0 || columnOpen[size_t(column)] == 0) {
            column++;
            continue;
        }
        const int32_t code = translate(cover);
        column++;
        tiles[at] = code;
    }
}

void StripMap::algoCavern1(int offset) {
    prefillUnset(offset);
    const WallProfile &p = *profile_;
    const int r = row;
    const int left0 = int(waveExtent(p.leftPhase[size_t(r)], p.leftDepth[size_t(r)])) + 1;
    const int right0 = 19 - int(waveExtent(p.rightPhase[size_t(r)], p.rightDepth[size_t(r)]));
    const int left1 = int(waveExtent(p.leftPhase[size_t(r + 1)], p.leftDepth[size_t(r + 1)])) + 1;
    const int right1 =
        19 - int(waveExtent(p.rightPhase[size_t(r + 1)], p.rightDepth[size_t(r + 1)]));
    for (column = left0; column < right0; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = left1; column < right1; column++)
        columnOpen[size_t(column)] = 0;
    placeFeature(offset, left0, right0 - 1, false);
    fillUnsetEdges(offset);
    coverOpenFloor(offset, 'Y');
}

void StripMap::algoCavern2(int offset) {
    prefillUnset(offset);
    const WallProfile &p = *profile_;
    const int r = row;
    const int left0 = int(p.leftDepth[size_t(r)]) + 1;
    const int right0 = 19 - int(p.rightDepth[size_t(r)]);
    const int left1 = int(p.leftDepth[size_t(r + 1)]) + 1;
    const int right1 = 19 - int(p.rightDepth[size_t(r + 1)]);
    for (column = left0; column < right0; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = left1; column < right1; column++)
        columnOpen[size_t(column)] = 0;
    placeFeature(offset, left0, right0 - 1, true);
    fillUnsetEdges(offset);
    if (lineInSector <= sectorLength_ - 1)
        coverOpenFloor(offset, 'Y');
}

void StripMap::algoCavern3(int offset) {
    prefillUnset(offset);
    const WallProfile &p = *profile_;
    const int r = row;
    const int left0 = int(waveExtent(p.leftPhase[size_t(r)], p.leftDepth[size_t(r)])) + 1;
    const int right0 = 19 - int(p.rightDepth[size_t(r)]);
    const int left1 = int(waveExtent(p.leftPhase[size_t(r + 1)], p.leftDepth[size_t(r + 1)])) + 1;
    const int right1 = 19 - int(p.rightDepth[size_t(r + 1)]);
    for (column = left0; column < right0; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = left1; column < right1; column++)
        columnOpen[size_t(column)] = 0;
    placeFeature(offset, left0, right0 - 1, true);
    fillUnsetEdges(offset);
    if (lineInSector <= sectorLength_ - 1)
        coverOpenFloor(offset, 'Y');
}

void StripMap::algoCavern4(int offset) {
    prefillUnset(offset);
    const WallProfile &p = *profile_;
    const int r = row;
    const int left0 = int(p.leftDepth[size_t(r)]) + 1;
    const int right0 = 19 - int(waveExtent(p.rightPhase[size_t(r)], p.rightDepth[size_t(r)]));
    const int left1 = int(p.leftDepth[size_t(r + 1)]) + 1;
    const int right1 =
        19 - int(waveExtent(p.rightPhase[size_t(r + 1)], p.rightDepth[size_t(r + 1)]));
    for (column = left0; column < right0; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = left1; column < right1; column++)
        columnOpen[size_t(column)] = 0;
    placeFeature(offset, left0, right0 - 1, true);
    fillUnsetEdges(offset);
    if (lineInSector <= sectorLength_ - 1)
        coverOpenFloor(offset, 'Y');
}

void StripMap::algoCavern5(int offset) {
    // Walls grow inward from both edges; the edge columns are always ']' and '['.
    prefillUnset(offset);
    const WallProfile &p = *profile_;
    const int r = row;
    const int left0 = int(waveExtentCentred(p.leftPhase[size_t(r)], p.leftDepth[size_t(r)]));
    const int left1 =
        int(waveExtentCentred(p.leftPhase[size_t(r + 1)], p.leftDepth[size_t(r + 1)]));
    for (column = 0; column < left0; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = 0; column < left1; column++)
        columnOpen[size_t(column)] = 0;
    const int right0 =
        19 - int(waveExtentCentred(p.rightPhase[size_t(r)], p.rightDepth[size_t(r)]));
    const int right1 =
        19 - int(waveExtentCentred(p.rightPhase[size_t(r + 1)], p.rightDepth[size_t(r + 1)]));
    for (column = right0; column < 19; column++)
        tiles[size_t(offset + column)] = translate(fillChar);
    for (column = right1; column < 19; column++)
        columnOpen[size_t(column)] = 0;
    column = 0;
    const int32_t leftWall = translate(']');
    columnOpen[size_t(column)] = 1;
    column = 19;
    tiles[size_t(offset)] = leftWall;
    const int32_t rightWall = translate('[');
    columnOpen[size_t(column)] = 1;
    column = 0;
    tiles[size_t(offset + 19)] = rightWall;
    const char edge = rowEdge();
    for (column = 0; column < columns;) {
        const size_t at = size_t(offset + column);
        if (tiles[at] != tile::kUnset) {
            column++;
            continue;
        }
        tiles[at] = translate(edge);
        if (tiles[size_t(offset + column)] < 1)
            columnOpen[size_t(column)] = 0;
        column++;
    }
    for (column = 0; column < columns;) {
        const size_t at = size_t(offset + column);
        if (tiles[at] == 0 && columnOpen[size_t(column)] != 0) {
            tiles[at] = translate(coverChar);
            column++;
            continue;
        }
        column++;
    }
}

} // namespace minos::cavern
