#pragma once
#include <filesystem>
#include <string>
namespace gridrunner {
// Copy compatible preview-profile saves only when the destination is absent.
// Restrict migration to known filenames to avoid importing incompatible scores
// or progress. Keep the source files available for recovery.
inline void migratePreviewProfile(const std::string &saveDir) {
    namespace fs = std::filesystem;
    const fs::path destination = fs::path(saveDir).lexically_normal();
    const fs::path source =
        (destination.filename().empty() ? destination.parent_path() : destination).parent_path() /
        "gridrunner-native";
    constexpr const char *oldNames[] = {"native-preview-highscores.txt",
                                        "native-preview-checkpoints.txt",
                                        "native-preview-controls.txt"};
    constexpr const char *newNames[] = {"highscores.txt", "checkpoints.txt", "controls.txt"};
    for (int i = 0; i < 3; ++i) {
        std::error_code error;
        const auto target = destination / newNames[i];
        if (fs::exists(target, error))
            continue;
        fs::copy_file(source / oldNames[i], target, fs::copy_options::none, error);
        // The checkpoint reader can recover a last-known-good backup.
        if (i == 1) {
            error.clear();
            fs::copy_file(source / (std::string(oldNames[i]) + ".bak"),
                          destination / (std::string(newNames[i]) + ".bak"), fs::copy_options::none,
                          error);
        }
    }
}
} // namespace gridrunner
