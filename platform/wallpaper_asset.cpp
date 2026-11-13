// Built-in original PNG pairs, split to bound compiler memory.
#include "ui_assets.h"
namespace mino::ui {
Image wallpaper0(bool);
Image wallpaper1(bool);
Image wallpaper2(bool);
Image wallpaper3(bool);
Image wallpaper(int index, bool portrait) {
    switch (index) {
    case 1:
        return wallpaper1(portrait);
    case 2:
        return wallpaper2(portrait);
    case 3:
        return wallpaper3(portrait);
    default:
        return wallpaper0(portrait);
    }
}
Image defaultWallpaper(bool portrait) {
    return wallpaper(0, portrait);
}
} // namespace mino::ui
