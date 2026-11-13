// Baked payloads are stored under assets/baked and embedded by the build.
#include "ui_assets.h"
namespace mino::ui {
namespace {
const unsigned char horizontal[] = {
#include "baked_assets/platform/wallpaper_2/horizontal.inc"
};
const unsigned char vertical[] = {
#include "baked_assets/platform/wallpaper_2/vertical.inc"
};
} // namespace
Image wallpaper2(bool portrait) {
    return portrait ? Image{vertical, sizeof vertical} : Image{horizontal, sizeof horizontal};
}
} // namespace mino::ui
