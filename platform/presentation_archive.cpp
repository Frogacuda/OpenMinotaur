// Baked payloads are stored under assets/baked and embedded by the build.
#include "presentation_assets.h"
namespace mino {
namespace {
const unsigned char bytes[] = {
#include "baked_assets/platform/presentation_archive/bytes.inc"
};
} // namespace
const unsigned char *presentationArchive(std::size_t &size) {
    size = sizeof bytes;
    return bytes;
}
} // namespace mino
