// level_data.h - Deflex level records compiled into the original executable.
//
// Neon_Internals::levels[] lists 52 static 0xe9c-byte records (intro, pong, ...). Fields
// are named only where theGame::newLevel (0x65368) or the entity code establishes them;
// all other words are kept by offset (sparse) so the original record is reproduced exactly.
// Data extracted from the iOS ARMv7 image.
#pragma once
#include <array>
#include <cstdint>

namespace deflex::level {

constexpr int kRecordSize = 0xe9c;
constexpr int kLevelCount = 52;
constexpr int kMapStride = 40; // +0x6a0 map rows are 40 bytes; newLevel steps 0x28
constexpr int kMapRows = 40;
constexpr int kMessageSlots = 10; // ten text[128] + descriptor[20] entries

struct Message {                          // entry +0x000..+0x093, repeated ten times
    const char *text;                     // entry +0x000 char[128]
    int32_t row;                          // +0x80: transform slot; proportional line row+1
    int32_t delayFrames;                  // +0x84: negative disables this and later slots
    int32_t colourSequence;               // +0x88
    uint32_t phaseRateBits, charStepBits; // +0x8c/+0x90: binary32 colour progression
};

struct SparseWord {
    uint16_t offset;
    uint32_t value;
};

struct LevelRecordData {
    Message messages[kMessageSlots]; // +0x000..+0x5c7; first message is level name
    const char *name() const {
        return messages[0].text;
    }
    int32_t columns, rows;              // +0x5c8/+0x5cc cellGrid::setSize(columns, rows)
    uint32_t flags;                     // +0x5d0 (newLevel: bit 3 copied to theGame+0x8ec)
    uint32_t cellColour;                // +0x5d4 RGBA unpacked into every cell
    char map[kMapRows][kMapStride + 1]; // +0x6a0 character map, row r at +0x6a0 + 40*r
                                        // (cells may be NUL; arrays keep embedded NULs).
                                        // map[0][0] == 'M' selects mirrored placement (newLevel)
    int sparseCount;
    SparseWord sparse[80]; // every other non-zero word, by original offset
};

extern const LevelRecordData kLevelRecords[kLevelCount];

// Rebuilds the original record bytes using the layout above.
std::array<uint8_t, kRecordSize> recordBytes(const LevelRecordData &level);

} // namespace deflex::level
