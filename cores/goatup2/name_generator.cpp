#include "name_generator.h"
#include "frontend_tables.h"
#include <stdexcept>
#include <string_view>

namespace goatup2 {
namespace {
template <size_t N>
std::string_view pick(neon::DarwinRandom &random, const char *const (&words)[N]) {
    // 0xa5ce explicitly converts the signed rand result to binary64. There is
    // no intermediate float or integer-modulo selection here.
    const int index = int(double(N) * (double(random.next()) / 2147483647.));
    if (index < 0 || size_t(index) >= N)
        throw std::out_of_range("original name word selection");
    return words[size_t(index)];
}
std::string person(neon::DarwinRandom &random, uint32_t seed) {
    using namespace original;
    if (seed)
        random.seed(seed); // r1 retains the current grammar seed at both calls
    std::string value(pick(random, FirstNames));
    value += ' ';
    if (double(random.next()) / 2147483647. < .2) {
        const bool full = double(random.next()) / 2147483647. < .05;
        const auto middle = pick(random, MiddleNames);
        if (full)
            value += middle;
        else {
            value += middle.front();
            value += '.';
        }
        value += ' ';
    }
    value += pick(random, LastNames);
    return value;
}
std::string_view word(neon::DarwinRandom &random, int token) {
    using namespace original;
    switch (token) {
    case 1:
        return pick(random, NameWords1);
    case 2:
        return pick(random, NameWords2);
    case 3:
        return pick(random, NameWords3);
    case 4:
        return pick(random, NameWords4);
    case 5:
        return pick(random, NameWords5);
    case 6:
        return pick(random, NameWords6);
    case 7:
        return pick(random, NameWords7);
    case 8:
        return pick(random, NameWords8);
    case 9:
        return pick(random, NameWords9);
    case 10:
        return pick(random, NameWords10);
    case 11:
        return pick(random, NameWords11);
    case 12:
        return pick(random, NameWords12);
    case 13:
        return pick(random, NameWords13);
    case 14:
        return pick(random, NameWords14);
    case 15:
        return pick(random, NameWords15);
    default:
        throw std::out_of_range("unknown original name grammar token");
    }
}
} // namespace
std::string generateOriginalName(neon::DarwinRandom &random, uint32_t seed, NameMode mode) {
    using namespace original;
    for (;; seed += 0x0f105513U) {
        if (!seed)
            return "* NAMELESS THING *"; // leaves the caller's rand state alone
        random.seed(seed);
        const int *grammar = LevelNameGrammar[random.next() & 15U];
        if (mode == NameMode::Bank)
            grammar = NameGrammar[random.next() & 7U]; // first choice still consumes rand
        std::string value;
        for (int i = 0; i < 16 && grammar[i] != 0; ++i) {
            const int token = grammar[i];
            if (!value.empty())
                value += ' ';
            if (token == -1 || token == -2) {
                value += person(random, seed);
                if (token == -2)
                    value += "'s";
            } else
                value += word(random, token);
        }
        if (value.empty())
            throw std::logic_error("empty original name grammar");
        // Source counts the trailing separator, then replaces it with NUL.
        if (value.size() + 1 < 38)
            return value;
    }
}
} // namespace goatup2
