// Game selection is explicit, never inferred from a device orientation.
// The VIC engine/entry action will use this same runtime format contract.
#pragma once
#include "core_api.h"
namespace gridrunner {
enum class GameVersion { Enhanced, ClassicVic20 };
constexpr mino::NativeFormat nativeFormat(GameVersion version) {
    return version == GameVersion::ClassicVic20 ? mino::NativeFormat{640, 480, 4.f / 3, 4.f / 3}
                                                : mino::NativeFormat{480, 640, .75f, .75f};
}
} // namespace gridrunner
