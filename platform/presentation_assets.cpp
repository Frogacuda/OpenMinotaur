#include "presentation_assets.h"
#include "miniz.h"
#include <mutex>
namespace mino {
bool readPresentationAsset(const std::string &name, std::vector<uint8_t> &bytes) {
    struct Bundle {
        mz_zip_archive zip{};
        std::mutex mutex;
        bool ready = false;
        Bundle() {
            std::size_t size = 0;
            auto *data = presentationArchive(size);
            ready = mz_zip_reader_init_mem(&zip, data, size, 0) != 0;
        }
        ~Bundle() {
            if (ready)
                mz_zip_reader_end(&zip);
        }
    };
    static Bundle bundle;
    std::lock_guard<std::mutex> lock(bundle.mutex);
    if (!bundle.ready)
        return false;
    std::size_t size = 0;
    auto *data = static_cast<uint8_t *>(
        mz_zip_reader_extract_file_to_heap(&bundle.zip, name.c_str(), &size, 0));
    if (!data)
        return false;
    bytes.assign(data, data + size);
    mz_free(data);
    return true;
}
} // namespace mino
