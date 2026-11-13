// level_data.cpp - binds the generated original data (level_data.inc) to level_data.h.
#include "level_data.h"
#include <cstring>

namespace deflex::level {

#include "level_data.inc"

namespace {
void put32(std::array<uint8_t, kRecordSize> &b, int off, uint32_t v) {
    for (int i = 0; i < 4; i++)
        b[size_t(off + i)] = uint8_t(v >> (8 * i));
}
void putText(std::array<uint8_t, kRecordSize> &b, int off, const char *s, int cap) {
    if (!s)
        return;
    const size_t n = std::strlen(s);
    std::memcpy(&b[size_t(off)], s, n < size_t(cap) ? n : size_t(cap));
}
} // namespace

std::array<uint8_t, kRecordSize> recordBytes(const LevelRecordData &l) {
    std::array<uint8_t, kRecordSize> b{};
    for (int m = 0; m < kMessageSlots; m++) {
        const Message &g = l.messages[m];
        putText(b, m * 0x94, g.text, 0x80);
        const int o = m * 0x94 + 0x80;
        put32(b, o, uint32_t(g.row));
        put32(b, o + 4, uint32_t(g.delayFrames));
        put32(b, o + 8, uint32_t(g.colourSequence));
        put32(b, o + 12, g.phaseRateBits);
        put32(b, o + 16, g.charStepBits);
    }
    put32(b, 0x5c8, uint32_t(l.columns));
    put32(b, 0x5cc, uint32_t(l.rows));
    put32(b, 0x5d0, l.flags);
    put32(b, 0x5d4, l.cellColour);
    for (int r = 0; r < kMapRows; r++)
        std::memcpy(&b[size_t(0x6a0 + kMapStride * r)], l.map[r], kMapStride);
    for (int i = 0; i < l.sparseCount; i++)
        put32(b, l.sparse[i].offset, l.sparse[i].value);
    return b;
}

} // namespace deflex::level
