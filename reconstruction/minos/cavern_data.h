// cavern_data.h - Caverns of Minos level content compiled into the original executable.
//
// The original keeps each cavern as a static 0x6fc-byte record (Neon_Internals::cavern0..19
// and cavern00, listed in Neon_Internals::caverns[]). Field meanings are recorded only as far
// as the reconstructed code establishes them; the rest are kept as raw words with their
// original offsets. The records were extracted from the iOS ARMv7 image.
#pragma once
#include <cstdint>
#include <cstddef>

namespace minos::cavern {

constexpr int kMaxZones = 64;

struct CavernDefinition {
    char name[32]; // +0x000 shown by theGame::announceCavern/dynamicSelect ("a nice biccie")
    int32_t goalSpriteIndex;  // +0x020 theGame::launch 'G': cavernGoal sprite index (its size query
                              // uses index + 2)
    int32_t zones[kMaxZones]; // +0x024 sector zone types (stm::getLine switch), -1 terminated
    int32_t zoneArgs[kMaxZones]; // +0x124 per-sector word, -1/2/...; read outside the map generator
    int32_t zoneLength;       // +0x224 theGame::newLevel: zoneLength = this; sectorLength = 2*this
    int32_t parTimeBase;      // +0x228 newLevel scales this by ship kind before deriving parTime
    int32_t unresolved22c[3]; // +0x22c..0x234 no maintained reader; preserve extracted words
    int32_t starPrewarmScroll[3]; // +0x238..0x240 initStars' pre-run scroll distances
    struct FlashTiming {
        int32_t onFrames, cycles, offFrames;
    } flashTiming[4];          // +0x244..0x270
    int32_t bgeWavesByKind[7]; // +0x274..0x28c BGESetup: asteroid, invader, galag, grunt, slab,
                               // ball, tet
    int32_t unresolved290[9];  // +0x290..0x2b0 no maintained reader; preserve extracted words
    uint32_t palette[16]; // +0x2b4 RGBA words unpacked by theGame::newLevel into theGame colours
    int32_t flags;        // +0x2f4 bit 2: theGame::initPlanetMap builds the wall profile;
                          //        bit 6 (0x40): theGame::launch creates saucers for 's'
    int32_t word2f8;      // +0x2f8 unnamed
    // +0x2fc four per-sector key-frame tracks (float bits), interpolated per line by
    // theGame::initPlanetMap: 0/1 are summed into wall phase, 2/3 are wall amplitudes.
    uint32_t profileKeys[4][kMaxZones];
};
static_assert(sizeof(CavernDefinition) == 0x6fc, "original record size");
static_assert(offsetof(CavernDefinition, flashTiming) == 0x244 &&
                  offsetof(CavernDefinition, bgeWavesByKind) == 0x274 &&
                  offsetof(CavernDefinition, palette) == 0x2b4,
              "original parameter layout");

extern const CavernDefinition kCavernDefinitions[21];
constexpr int kCavernCount = 21;

// Digit tiles: _flashModes/_flashOnChars/_flashOffChars, copied into each stm by newLevel.
extern const int32_t kFlashModes[16];
extern const int32_t kFlashOnTiles[16];
extern const int32_t kFlashOffTiles[16];

// Neon_Internals::a8cols, the colour sequence stm::draw uses for colour bytes >= 64: word 0
// is the count getColFromSequence scales by (14), then 16 RGBA words (the symbol's extent),
// then the following image word (damRate[0]), which an overlay with colour 127 would read.
extern const uint32_t kA8ColourSequence[18];

// stm::getLine's static row tables, named after the compiler symbols
// getLine(int,int,int)::C.NNN. Each row is 20 map columns + 1 spare column.
struct RowTable {
    const char *const *rows;
    int count;
};
RowTable rowTable(int symbolNumber); // 254..309; {nullptr,0} for an unknown number

} // namespace minos::cavern
