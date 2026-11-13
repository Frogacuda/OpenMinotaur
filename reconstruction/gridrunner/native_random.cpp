#include "native_random.h"
#include <stdexcept>
namespace gr::reconstruction::ios {
double RandomStream::range(double low, double high) {
    const uint32_t value = next();
    if (value > 2147483647u)
        throw std::logic_error("iOS rand output outside original signed range");
    return (high - low) * (double(value) / 2147483647.0) + low;
}
uint32_t DarwinRandom::next() {
    // Algorithm reference (default, excluding USE_WEAK_SEEDING):
    // https://github.com/apple-oss-distributions/Libc/blob/main/stdlib/FreeBSD/rand.c
    const uint64_t value = state_ ? state_ : 123459876u;
    state_ = uint32_t((value * 16807u) % 2147483647u);
    return state_;
}
} // namespace gr::reconstruction::ios
