// Baked payloads are stored under assets/baked and embedded by the build.
#include "royale_pipeline.h"
namespace mino {
namespace {
const unsigned char data[] = {
#include "baked_assets/platform/consumer_assets/data.inc"
};
} // namespace
const unsigned char *consumerArchive(std::size_t &size) {
    size = sizeof data;
    return data;
}
} // namespace mino
