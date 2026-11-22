#pragma once
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>
namespace neon::effects {
constexpr int firstImageSelector = 1000, lastImageSelector = 9999;
inline bool importedImageSelector(int id) {
    return id >= firstImageSelector && id <= lastImageSelector;
}
struct ImageAsset {
    int selector = 0, width = 0, height = 0;
    uint64_t contentHash = 0;
    std::string identity, name;
    bool linear = true, mipmaps = true;
    int wrapU = 0, wrapV = 0; // repeat, mirrored repeat, clamp
    std::vector<uint8_t> png;
    uint64_t fingerprint() const;
    size_t gpuBytes() const;
    std::vector<uint8_t> pixels() const;
};
// Compressed, CPU-owned assets. GPU copies belong to each Runtime/context.
class ImageAssets {
  public:
    std::vector<ImageAsset> items;
    const ImageAsset *find(int) const;
    ImageAsset &at(int);
    const ImageAsset &at(int) const;
    int importFile(const std::filesystem::path &, int maximumEdge = 1024, int replace = 0);
    int addPixels(const std::string &, int, int, const std::vector<uint8_t> &, int replace = 0);
    std::map<int, int> merge(const ImageAssets &);
    void validate() const;
    void prepare();
    uint64_t fingerprint() const;
    void write(std::ostream &, const std::filesystem::path &assetFolder = {}) const;
    static ImageAssets read(std::istream &, const std::filesystem::path &base = {});
    static void setFileBase(const std::filesystem::path &);
    static const std::filesystem::path &fileBase();
};
} // namespace neon::effects
