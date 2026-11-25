#include "image_assets.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <stb_image_write.h>
#include <stdexcept>
namespace neon::effects {
namespace {
constexpr size_t maxPixels = 4 * 1024 * 1024, maxEncoded = 20 * 1024 * 1024;
thread_local std::filesystem::path sceneBase;
void need(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
uint64_t hash(const void *ptr, size_t size, uint64_t value = 1469598103934665603ull) {
    auto bytes = (const uint8_t *)ptr;
    for (size_t i = 0; i < size; ++i)
        value = (value ^ bytes[i]) * 1099511628211ull;
    return value;
}
std::string hex(uint64_t value) {
    std::ostringstream s;
    s << std::hex << std::setw(16) << std::setfill('0') << value;
    return s.str();
}
std::string identity() {
    std::random_device random;
    return hex((uint64_t(random()) << 32) ^ random());
}
bool safe(const std::string &s) {
    return !s.empty() && s.size() <= 128 && s.find_first_of("\r\n") == std::string::npos;
}
std::vector<uint8_t> readBytes(const std::filesystem::path &file, size_t limit) {
    auto n = std::filesystem::file_size(file);
    need(n > 0 && n <= limit, "Image file exceeds the supported byte limit");
    std::ifstream in(file, std::ios::binary);
    std::vector<uint8_t> bytes(size_t(n), 0);
    need(bool(in.read((char *)bytes.data(), std::streamsize(n))), "Cannot read image");
    return bytes;
}
std::vector<uint8_t> decode(const std::vector<uint8_t> &bytes, int &w, int &h, int limit) {
    int channels;
    need(stbi_info_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels) != 0,
         "Invalid PNG/JPEG image");
    need(w > 0 && h > 0 && uint64_t(w) * uint64_t(h) <= uint64_t(limit),
         "Image dimensions exceed the import budget (16 megapixels)");
    auto pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
    need(pixels != nullptr, "Cannot decode image");
    std::vector<uint8_t> result(pixels, pixels + size_t(w) * h * 4);
    stbi_image_free(pixels);
    return result;
}
void append(void *context, void *bytes, int size) {
    auto &out = *(std::vector<uint8_t> *)context;
    out.insert(out.end(), (uint8_t *)bytes, (uint8_t *)bytes + size);
}
void validatePng(const ImageAsset &a) {
    need(a.png.size() >= 8 && a.png.size() <= maxEncoded && a.png[0] == 137 && a.png[1] == 'P' &&
             a.png[2] == 'N' && a.png[3] == 'G',
         "Imported asset is not a normalized PNG");
    int w, h, c;
    need(stbi_info_from_memory(a.png.data(), int(a.png.size()), &w, &h, &c) && w == a.width &&
             h == a.height,
         "Image metadata does not match its pixels");
}
} // namespace
uint64_t ImageAsset::fingerprint() const {
    auto value = contentHash ? contentHash : hash(png.data(), png.size());
    for (int v : {width, height, int(linear), int(mipmaps), wrapU, wrapV})
        value = hash(&v, sizeof(v), value);
    return value;
}
size_t ImageAsset::gpuBytes() const {
    size_t bytes = 0;
    int w = width, h = height;
    do {
        bytes += size_t(w) * h * 4;
        if (!mipmaps || (w == 1 && h == 1))
            break;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    } while (true);
    return bytes;
}
std::vector<uint8_t> ImageAsset::pixels() const {
    int w, h;
    return decode(png, w, h, 1024 * 1024);
}
const ImageAsset *ImageAssets::find(int selector) const {
    for (auto &item : items)
        if (item.selector == selector)
            return &item;
    return nullptr;
}
ImageAsset &ImageAssets::at(int selector) {
    for (auto &item : items)
        if (item.selector == selector)
            return item;
    throw std::runtime_error("Imported texture is missing");
}
const ImageAsset &ImageAssets::at(int selector) const {
    if (auto item = find(selector))
        return *item;
    throw std::runtime_error("Imported texture is missing");
}
void ImageAssets::validate() const {
    need(items.size() <= 64, "A scene supports at most 64 imported images");
    std::set<int> ids;
    std::set<std::string> names;
    size_t pixels = 0, bytes = 0;
    for (auto &a : items) {
        need(importedImageSelector(a.selector) && ids.insert(a.selector).second,
             "Invalid imported image selector");
        need(a.identity.size() == 16 &&
                 a.identity.find_first_not_of("0123456789abcdef") == std::string::npos &&
                 names.insert(a.identity).second && safe(a.name),
             "Invalid imported image identity/name");
        need(a.width > 0 && a.height > 0 && a.width <= 1024 && a.height <= 1024 && a.wrapU >= 0 &&
                 a.wrapU <= 2 && a.wrapV >= 0 && a.wrapV <= 2,
             "Imported image exceeds 1024 pixels or uses invalid sampling");
        pixels += size_t(a.width) * a.height;
        bytes += a.png.size();
        validatePng(a);
    }
    need(pixels <= maxPixels && bytes <= maxEncoded,
         "Imported image catalog exceeds 16 MiB of base RGBA pixels / 20 MiB "
         "encoded data");
}
uint64_t ImageAssets::fingerprint() const {
    uint64_t value = 0;
    for (auto &a : items)
        value ^= a.fingerprint() + uint64_t(a.selector) * 1099511628211ull;
    return value;
}
void ImageAssets::prepare() {
    validate();
    for (auto &asset : items)
        asset.contentHash = hash(asset.png.data(), asset.png.size());
}
int ImageAssets::addPixels(const std::string &name, int w, int h, const std::vector<uint8_t> &rgba,
                           int replace) {
    need(w > 0 && h > 0 && w <= 1024 && h <= 1024 && rgba.size() == size_t(w) * h * 4,
         "Imported pixels exceed 1024 or have an invalid size");
    ImageAssets pending = *this;
    ImageAsset a;
    if (replace)
        a = pending.at(replace);
    else {
        a.identity = identity();
        a.selector = firstImageSelector;
        while (pending.find(a.selector))
            ++a.selector;
    }
    a.width = w;
    a.height = h;
    if (!replace)
        a.name = name.substr(0, 128);
    a.png.clear();
    need(stbi_write_png_to_func(append, &a.png, w, h, 4, rgba.data(), w * 4) != 0,
         "Cannot encode imported image");
    a.contentHash = hash(a.png.data(), a.png.size());
    int id = a.selector;
    if (replace)
        pending.at(replace) = std::move(a);
    else
        pending.items.push_back(std::move(a));
    pending.validate();
    *this = std::move(pending);
    return id;
}
int ImageAssets::importFile(const std::filesystem::path &file, int edge, int replace) {
    need(edge >= 1 && edge <= 1024, "Import maximum must be between 1 and 1024");
    auto bytes = readBytes(file, 16 * 1024 * 1024);
    bool png = bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' &&
               bytes[3] == 'G';
    bool jpeg = bytes.size() >= 3 && bytes[0] == 255 && bytes[1] == 216 && bytes[2] == 255;
    need(png || jpeg, "Only PNG and JPEG images can be imported");
    int w, h;
    auto rgba = decode(bytes, w, h, 16 * 1024 * 1024);
    float scale = std::min(1.f, float(edge) / std::max(w, h));
    int nw = std::max(1, int(std::lround(w * scale))),
        nh = std::max(1, int(std::lround(h * scale)));
    if (nw != w || nh != h) {
        std::vector<uint8_t> resized(size_t(nw) * nh * 4);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x) {
                float sx = (x + .5f) * w / nw - .5f, sy = (y + .5f) * h / nh - .5f;
                int ix = int(std::floor(sx)), iy = int(std::floor(sy));
                float fx = sx - ix, fy = sy - iy;
                float sum[4]{};
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        auto p = &rgba[(size_t(std::clamp(iy + dy, 0, h - 1)) * w +
                                        std::clamp(ix + dx, 0, w - 1)) *
                                       4];
                        float weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                        sum[3] += p[3] * weight;
                        for (int c = 0; c < 3; ++c)
                            sum[c] += p[c] * p[3] / 255.f * weight;
                    }
                auto out = &resized[(size_t(y) * nw + x) * 4];
                out[3] = uint8_t(std::clamp(std::lround(sum[3]), 0l, 255l));
                for (int c = 0; c < 3; ++c)
                    out[c] = sum[3] > 0
                                 ? uint8_t(std::clamp(std::lround(sum[c] * 255 / sum[3]), 0l, 255l))
                                 : 0;
            }
        rgba = std::move(resized);
    }
    return addPixels(file.stem().u8string(), nw, nh, rgba, replace);
}
std::map<int, int> ImageAssets::merge(const ImageAssets &source) {
    ImageAssets pending = *this;
    std::map<int, int> remap;
    for (auto a : source.items) {
        auto same = std::find_if(pending.items.begin(), pending.items.end(), [&](auto &b) {
            return a.identity == b.identity && a.fingerprint() == b.fingerprint() && a.png == b.png;
        });
        if (same != pending.items.end()) {
            remap[a.selector] = same->selector;
            continue;
        }
        int old = a.selector;
        a.selector = firstImageSelector;
        while (pending.find(a.selector))
            ++a.selector;
        if (std::any_of(pending.items.begin(), pending.items.end(),
                        [&](auto &b) { return b.identity == a.identity; }))
            a.identity = identity();
        remap[old] = a.selector;
        pending.items.push_back(std::move(a));
    }
    pending.validate();
    *this = std::move(pending);
    return remap;
}
void ImageAssets::write(std::ostream &out, const std::filesystem::path &folder) const {
    validate();
    out << "IMAGEASSETS 1 " << items.size() << '\n';
    for (auto &a : items) {
        out << a.selector << ' ' << std::quoted(a.identity) << ' ' << std::quoted(a.name) << ' '
            << a.width << ' ' << a.height << ' ' << a.linear << ' ' << a.mipmaps << ' ' << a.wrapU
            << ' ' << a.wrapV << ' ';
        if (folder.empty()) {
            out << "inline " << a.png.size() << '\n';
            static const char digits[] = "0123456789abcdef";
            for (uint8_t v : a.png)
                out << digits[v >> 4] << digits[v & 15];
            out << '\n';
        } else {
            std::filesystem::create_directories(folder);
            auto filename = a.identity + "-" + hex(hash(a.png.data(), a.png.size())) + ".png";
            auto path = folder / filename;
            if (std::filesystem::exists(path))
                need(readBytes(path, maxEncoded) == a.png, "Asset filename collision");
            else {
                std::ofstream image(path, std::ios::binary);
                need(bool(image.write((const char *)a.png.data(), std::streamsize(a.png.size()))),
                     "Cannot write scene image");
            }
            out << "file " << std::quoted((folder.filename() / filename).generic_u8string())
                << '\n';
        }
    }
    need(bool(out), "Cannot save image catalog");
}
ImageAssets ImageAssets::read(std::istream &in, const std::filesystem::path &base) {
    ImageAssets result;
    std::string magic;
    int version, count;
    need(bool(in >> magic >> version >> count) && magic == "IMAGEASSETS" && version == 1 &&
             count >= 0 && count <= 64,
         "Invalid image catalog");
    size_t bytes = 0;
    for (int i = 0; i < count; ++i) {
        ImageAsset a;
        int linear, mips;
        std::string storage;
        need(bool(in >> a.selector >> std::quoted(a.identity) >> std::quoted(a.name) >> a.width >>
                  a.height >> linear >> mips >> a.wrapU >> a.wrapV >> storage) &&
                 linear >= 0 && linear <= 1 && mips >= 0 && mips <= 1,
             "Invalid image record");
        a.linear = linear != 0;
        a.mipmaps = mips != 0;
        if (storage == "file") {
            std::string path;
            need(bool(in >> std::quoted(path)), "Invalid image path");
            std::filesystem::path relative = std::filesystem::u8path(path);
            auto parent = relative.parent_path().generic_u8string();
            need(!base.empty() && !relative.is_absolute() && !relative.has_root_name() &&
                     parent.size() > 7 && parent.find('/') == std::string::npos &&
                     parent.find('\\') == std::string::npos && parent != ".." &&
                     parent.substr(parent.size() - 7) == ".assets" &&
                     relative.filename().u8string().find_first_not_of("0123456789abcdef-.png") ==
                         std::string::npos,
                 "Image paths must stay inside a neighboring .assets folder");
            a.png = readBytes(base / relative, maxEncoded);
        } else {
            need(storage == "inline", "Unknown image storage");
            size_t size;
            need(bool(in >> size) && size <= maxEncoded && bytes + size <= maxEncoded,
                 "Encoded image budget exceeded");
            in >> std::ws;
            a.png.resize(size);
            auto digit = [](int c) {
                return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            };
            for (auto &v : a.png) {
                int hi = digit(in.get()), lo = digit(in.get());
                need(hi >= 0 && lo >= 0, "Invalid embedded PNG data");
                v = uint8_t((hi << 4) | lo);
            }
        }
        a.contentHash = hash(a.png.data(), a.png.size());
        bytes += a.png.size();
        need(bytes <= maxEncoded, "Image catalog byte budget exceeded");
        result.items.push_back(std::move(a));
    }
    result.validate();
    for (auto &a : result.items)
        a.pixels();
    return result;
}
void ImageAssets::setFileBase(const std::filesystem::path &path) {
    sceneBase = path;
}
const std::filesystem::path &ImageAssets::fileBase() {
    return sceneBase;
}
} // namespace neon::effects
