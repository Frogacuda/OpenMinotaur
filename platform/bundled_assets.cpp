#include "bundled_assets.h"
#include "game_catalog.h"
#include "miniz.h"
#include <SDL3/SDL.h>
#include <mutex>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif !defined(SDL_PLATFORM_ANDROID)
extern "C" const unsigned char mino_game_assets_begin[], mino_game_assets_end[];
#endif

namespace mino {
namespace {
struct Archive {
    mz_zip_archive zip{};
    void *owned = nullptr;
    bool ready = false;
    std::mutex mutex;
    Archive() {
        const void *data = nullptr;
        size_t size = 0;
#ifdef _WIN32
        const auto module = GetModuleHandleW(nullptr);
        const auto resource = FindResourceW(module, MAKEINTRESOURCEW(148), MAKEINTRESOURCEW(10));
        if (resource) {
            size = SizeofResource(module, resource);
            data = LockResource(LoadResource(module, resource));
        }
#elif defined(SDL_PLATFORM_ANDROID)
        owned = SDL_LoadFile("game-assets.zip", &size);
        data = owned;
#else
        data = mino_game_assets_begin;
        size = size_t(reinterpret_cast<uintptr_t>(mino_game_assets_end) -
                      reinterpret_cast<uintptr_t>(mino_game_assets_begin));
#endif
        ready = data && mz_zip_reader_init_mem(&zip, data, size, 0);
    }
    ~Archive() {
        if (ready)
            mz_zip_reader_end(&zip);
        SDL_free(owned);
    }
};
Archive &archive() {
    static Archive value;
    return value;
}
// Runtime names are flat original resources, not filesystem/archive paths.
bool valid(const std::string &value) {
    return !value.empty() && value != "." && value != ".." &&
           value.find_first_of("/\\:") == std::string::npos;
}
} // namespace
bool bundledGameAvailable(const std::string &id) {
    if (!valid(id))
        return false;
    auto &a = archive();
    std::lock_guard<std::mutex> lock(a.mutex);
    return a.ready && mz_zip_reader_locate_file(&a.zip, (id + "/bundled.ini").c_str(), nullptr,
                                                MZ_ZIP_FLAG_CASE_SENSITIVE) >= 0;
}
bool allGamesBundled() {
    for (const auto &game : kGameCatalog)
        if (!bundledGameAvailable(game.id))
            return false;
    return true;
}
bool readBundledAsset(const std::string &id, const std::string &name, std::vector<uint8_t> &out) {
    out.clear();
    if (!valid(id) || !valid(name))
        return false;
    auto &a = archive();
    std::lock_guard<std::mutex> lock(a.mutex);
    if (!a.ready)
        return false;
    size_t size = 0;
    void *bytes = mz_zip_reader_extract_file_to_heap(&a.zip, (id + "/" + name).c_str(), &size,
                                                     MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (!bytes)
        return false;
    out.assign(static_cast<uint8_t *>(bytes), static_cast<uint8_t *>(bytes) + size);
    mz_free(bytes);
    return true;
}
} // namespace mino
