#pragma once
#include "neon/source_random.h"
#include <string>
namespace goatup2 {
enum class NameMode { Level, Bank };
// 0xa230/0xa080. Name generation is part of stage loading, not independent
// decoration: both the outer grammar and human-name tokens reseed global rand.
std::string generateOriginalName(neon::DarwinRandom &, uint32_t seed, NameMode);
} // namespace goatup2
