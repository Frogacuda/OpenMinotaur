// Embedded loading artwork from Default~ipad.png.
#include "loading_art.h"
namespace goatup2 {
namespace {
const uint8_t Artwork[] = {
#include "baked_assets/cores/goatup2/loading_art/Artwork.inc"
};
} // namespace
void loadingArt(std::vector<uint8_t> &out) {
    out.assign(std::begin(Artwork), std::end(Artwork));
}
} // namespace goatup2
