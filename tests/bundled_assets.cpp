// Resource integration: compare every embedded file to the source bundle.
#include "bundled_assets.h"
#include "game_catalog.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    try {
        if (argc != 2 && argc != 3)
            throw std::runtime_error(
                "Pass source assets/games directory and optional single game ID");
        const std::string selected = argc == 3 ? argv[2] : "";
        if (selected.empty() && !mino::allGamesBundled())
            throw std::runtime_error("Incomplete embedded resource archive");
        const std::filesystem::path root = argv[1];
        int files = 0, games = 0;
        for (const auto &game : mino::kGameCatalog) {
            if (!selected.empty() && selected != game.id) {
                if (mino::bundledGameAvailable(game.id))
                    throw std::runtime_error("Unexpected game in standalone archive");
                continue;
            }
            if (!mino::bundledGameAvailable(game.id))
                throw std::runtime_error(game.id);
            ++games;
            for (const auto &file : std::filesystem::directory_iterator(root / game.id)) {
                std::ifstream input(file.path(), std::ios::binary);
                std::vector<uint8_t> expected((std::istreambuf_iterator<char>(input)), {}), actual;
                const auto name = file.path().filename().string();
                if (!mino::readBundledAsset(game.id, name, actual) || actual != expected)
                    throw std::runtime_error(std::string(game.id) + "/" + name);
                ++files;
            }
        }
        if (!games)
            throw std::runtime_error("Unknown game selection");
        std::vector<uint8_t> missing{1};
        const std::string probe = selected.empty() ? "minos" : selected;
        if (mino::readBundledAsset(probe, "missing.png", missing) || !missing.empty() ||
            mino::readBundledAsset(probe, "../bundled.ini", missing) ||
            mino::bundledGameAvailable("missing-game"))
            throw std::runtime_error("Missing/path handling");
        std::cout << "Embedded asset checks: " << games << " game(s), " << files
                  << " resources byte-identical; no loose game files or extraction required\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
