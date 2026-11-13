#pragma once
#include <cstdint>
#include <cstring>
namespace neon::integers {
inline int32_t signedBits(uint32_t bits) {
    int32_t result;
    std::memcpy(&result, &bits, sizeof result);
    return result;
}
// Original ARM word arithmetic, without host signed-overflow undefined behavior.
inline int32_t add32(int32_t value, int32_t delta) {
    return signedBits(uint32_t(value) + uint32_t(delta));
}
inline int32_t subtract32(int32_t value, int32_t delta) {
    return signedBits(uint32_t(value) - uint32_t(delta));
}
} // namespace neon::integers
