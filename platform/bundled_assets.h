// A resource archive embedded in desktop applications and packaged inside APKs.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace mino {
bool bundledGameAvailable(const std::string &id);
bool allGamesBundled();
bool readBundledAsset(const std::string &id, const std::string &name, std::vector<uint8_t> &out);
} // namespace mino
