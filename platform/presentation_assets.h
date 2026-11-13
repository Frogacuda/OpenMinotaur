#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace mino {
const unsigned char *presentationArchive(std::size_t &size);
bool readPresentationAsset(const std::string &name, std::vector<uint8_t> &bytes);
} // namespace mino
