// cavern_map.h - the Caverns of Minos scrolling cave map (original class Neon_Internals::stm).
//
// The original never stores a cavern as a tile grid. Every frame theGame::_bgdraw asks the
// map for the visible lines (stm::getLine), which re-derives each line from the cavern's
// sector/zone list: either a row of a static zone template (optionally mirrored) or one of
// five procedural wall generators (stm::algoCavern1-5) driven by the per-line wall profile
// built by theGame::initPlanetMap. stm::translate turns each template character into a tile
// code, launching the entity a character stands for the first time its cell is generated.
// The only persistent per-cell state is a flag word per map cell.
//
// Source: iOS Minos_armv7 (SHA-256 1b73b707...6f0e7). Addresses are given per function.
// Names of offset-only original fields are inferred from their uses and marked as such.
#pragma once
#include "cavern_data.h"
#include <array>
#include <cstdint>
#include <vector>

namespace minos::cavern {

struct Vec2 {
    double x = 0, y = 0;
};

// theGame-owned per-line wall profile (theGame+0x203840, four float[1024] tracks).
// Built by theGame::initPlanetMap only for caverns with flag bit 2; otherwise the previous
// contents remain (the original never clears them).
struct WallProfile {
    static constexpr int kLines = 1024;
    std::array<float, kLines> leftPhase{};  // +0x0000 summed key-frame track 0
    std::array<float, kLines> rightPhase{}; // +0x1000 summed key-frame track 1
    std::array<float, kLines> leftDepth{};  // +0x2000 interpolated key-frame track 2
    std::array<float, kLines> rightDepth{}; // +0x3000 interpolated key-frame track 3
};
// theGame::initPlanetMap (0x592dc).
void buildWallProfile(const CavernDefinition &cavern, int sectorLength, WallProfile &profile);

// theGame-owned digit-tile flash channels (theGame+0x207908/0x20790c/0x207910, 4 each).
// Meanings inferred: `lit` selects the on tile; `watched` makes generation mark `seen`.
struct FlashChannels {
    uint8_t lit[4] = {};
    int8_t watched[4] = {};
    uint8_t seen[4] = {};
};

// Callback to theGame::launch(char, vec2d, int column, int row) (0x5ab00): entity creation
// is a separate component. The map passes the generating column/row cursor as the original.
class MapHost {
  public:
    virtual ~MapHost() = default;
    virtual void launch(char kind, Vec2 position, int column, int row) = 0;
};

// Tile codes stm::translate returns for fixed characters, named by source character. A code
// packs, from the low byte: atlas tile, overlay atlas tile, colour (see cavern_draw.h), and a
// high byte stm::draw ignores (0x80 in the flash digit tiles).
namespace tile {
constexpr int32_t kEmpty = 0; // ' ' and every entity character
constexpr int32_t kAt = 1;    // '@'
constexpr int32_t kUpperL = 0x0f, kUpperR = 0x10, kLowerL = 0x11, kLowerR = 0x12;
constexpr int32_t kUpperF = 0x13, kUpperC = 0x14, kDash = 0x18;
constexpr int32_t kCloseBracket = 0x1d, kOpenBracket = 0x1e; // ']' '['
constexpr int32_t kUpperT = 0x20015, kEquals = 0x20019;
constexpr int32_t kHashOrStar = 0x30017; // '#' and '*' while the cell is not flagged removed
constexpr int32_t kUnset = 0x22b;        // algoCavern sentinel: column not yet generated
} // namespace tile
// Cell flag bits (stm+0x2de4 words). Low byte: translate launches only while it is 0;
// theGame::launch sets bit 0 for every character it handles (cavern_launch.h). Other
// low-byte bits: no writer found in launch. 0x100: blocks the breakable tile;
// 0x200: breakable generated.
namespace cellflag {
constexpr uint32_t kOwnerMask = 0xff, kLaunched = 0x1, kRemoved = 0x100, kGenerated = 0x200;
}

class StripMap {
  public:
    static constexpr int kMapColumns = 128; // cell flag row stride
    static constexpr int kMapRows = 4096;   // 0x200000 bytes of cell flags
    static constexpr int kTileCapacity = (20 + 1) * (25 + 1);

    StripMap(); // stm::stm (0x5637c)

    // The globals Neon_Internals::zoneLength/sectorLength, set by theGame::newLevel
    // (zoneLength = cavern.zoneLength; sectorLength = zoneLength << 1). Kept per map here.
    void setCavern(const CavernDefinition *cavern, int zoneLength);
    // theGame::newLevel copies _flashModes/_flashOnChars/_flashOffChars into the map.
    void installFlashTables();
    // gameWord9dc: theGame+0x9dc, which getLine clears on every call (meaning not yet known).
    void bind(MapHost *host, const WallProfile *profile, FlashChannels *flash,
              int32_t *gameWord9dc) {
        host_ = host;
        profile_ = profile;
        flash_ = flash;
        gameWord9dc_ = gameWord9dc;
    }

    void getLine(int x, int line, int offset); // stm::getLine (0x78784)
    // theGame::launch sets bit 0 (cellflag::kLaunched) of the cell it launched from; with
    // it set, translate does not launch from that cell again when the line is regenerated.
    void markLaunched(int r, int c) {
        *cell(r, c) |= cellflag::kLaunched;
    }
    // centity::run clears the whole low byte when a map entity leaves the visible band.
    void clearLaunched(int r, int c) {
        *cell(r, c) &= ~cellflag::kOwnerMask;
    }
    int32_t translate(char c); // stm::translate (0x5bda4)

    // ---- original state (offsets in stm) ----
    int columns = 20; // +0x504
    int rows = 25;    // +0x508
    // +0x50c..0x511 algorithmic-zone characters, written by getLine before algoCavernN.
    char fillChar = 0, edgeChar = 0, firstEdgeChar = 0, lastEdgeChar = 0, featureChar = 0,
         coverChar = 0;
    std::array<int32_t, 112> columnOpen{}; // +0x514 per column: still open below (inferred)
    int lineInSector = 0;                  // +0x594
    int atlasColumns = 0x1c;               // +0x598 tiles per atlas row (stm::draw)
    Vec2 scroll;   // +0x59c / +0x5a4 camera cell position (theGame::_bgdraw)
    Vec2 origin;   // +0x5ac
    Vec2 fraction; // +0x5bc / +0x5c4 negative fractional scroll (theGame::_bgdraw)
    Vec2 cellSize{double(0.05f),
                  double(0.04f)}; // +0x5cc constructor stores binary32-widened values
    Vec2 scale{1.0, 1.0};         // +0x5dc
    // +0x5ec..0x610 tile atlas in texture pixels (stm::draw; constructor values): tile t is
    // at x = atlasX + marginX + pitchX * (t % atlasColumns), y = atlasY + marginY +
    // pitchY * (t / atlasColumns), size tileW x tileH, texture textureW x textureH.
    struct TileAtlas {
        int32_t atlasX = 0, atlasY = 0x1b2;         // +0x5ec, +0x5f0
        int32_t pitchX = 0x12, pitchY = 0x12;       // +0x5f4, +0x5f8
        int32_t marginX = 1, marginY = 1;           // +0x5fc, +0x600
        int32_t tileW = 0x10, tileH = 0x0f;         // +0x604, +0x608
        int32_t textureW = 0x200, textureH = 0x200; // +0x60c, +0x610
    } atlas;
    int32_t flashModes[16] = {};                // +0x614
    int32_t flashOnTiles[16] = {};              // +0x654
    int32_t flashOffTiles[16] = {};             // +0x694
    std::array<int32_t, kTileCapacity> tiles{}; // +0x6d4 generated tile codes, (columns+1) per line
    std::vector<uint32_t> cells;                // +0x2de4 [kMapRows][kMapColumns] cell flags
    int row = 0;                                // +0x202de4 generation cursor: map line
    int column = 0;                             // +0x202de8 generation cursor: column

    int zoneLength() const {
        return zoneLength_;
    }
    int sectorLength() const {
        return sectorLength_;
    }

  private:
    void algoCavern1(int offset); // 0x77690
    void algoCavern2(int offset); // 0x783a0
    void algoCavern3(int offset); // 0x77f50
    void algoCavern4(int offset); // 0x77b00
    void algoCavern5(int offset); // 0x5c37c
    void prefillUnset(int offset);
    char rowEdge() const;
    void placeFeature(int offset, int leftIndex, int rightIndex, bool moveCursor);
    void fillUnsetEdges(int offset);
    void coverOpenFloor(int offset, char cover);
    uint32_t *cell(int r, int c);
    Vec2 cellPosition() const;

    const CavernDefinition *cavern_ = nullptr;
    int zoneLength_ = 12, sectorLength_ = 24;
    MapHost *host_ = nullptr;
    const WallProfile *profile_ = nullptr;
    FlashChannels *flash_ = nullptr;
    int32_t *gameWord9dc_ = nullptr;
    uint32_t outOfMapCell_ = 0;
};

// Mirrors a template character for the mirrored zone variants and algorithmic features
// ("LlrR][<>(){}" <-> "RrlL[]><)(}{"); other characters are unchanged.
char mirrorTile(char c);

} // namespace minos::cavern
