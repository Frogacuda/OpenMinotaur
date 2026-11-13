#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace neon::pc {
// A borrowed bundle reader; paths are logical resource names, never extracted.
using AssetReader = std::function<bool(const std::filesystem::path &, std::vector<uint8_t> &)>;
inline std::istringstream assetStream(const std::filesystem::path &path,
                                      const AssetReader &reader = {}) {
    std::vector<uint8_t> bytes;
    bool ok = false;
    if (reader)
        ok = reader(path, bytes);
    else {
        std::ifstream file(path, std::ios::binary);
        ok = bool(file);
        if (ok)
            bytes.assign(std::istreambuf_iterator<char>(file), {});
    }
    std::istringstream stream(std::string(bytes.begin(), bytes.end()),
                              std::ios::in | std::ios::binary);
    if (!ok)
        stream.setstate(std::ios::failbit);
    return stream;
}
inline std::string assetText(const std::filesystem::path &path, const AssetReader &reader = {}) {
    auto stream = assetStream(path, reader);
    if (!stream)
        throw std::runtime_error("Missing effect asset: " + path.string());
    return stream.str();
}
inline bool assetExists(const std::filesystem::path &path, const AssetReader &reader = {}) {
    if (!reader)
        return std::filesystem::exists(path);
    std::vector<uint8_t> bytes;
    return reader(path, bytes);
}
} // namespace neon::pc
