// The collection's display order is independent of playable core order.
#pragma once
#include <array>

namespace mino {

struct CatalogGame {
    const char *id;
    const char *title;
    const char *atlas;
    const char *label1;
    const char *label2;
    const char *monogram; // Fallback when the game icon is unavailable.
};

inline constexpr std::array<CatalogGame, 9> kGameCatalog = {{
    {"minorescue", "Minotaur Rescue", "spacewars.png", "Minotaur", "Rescue", "MR"},
    {"minotron", "Minotron: 2112", "minotron.png", "Minotron:", "2112", "MT"},
    {"deflex", "Deflex", "deflex.png", "Deflex", "", "DF"},
    {"goatup", "GoatUp", "goat.png", "GoatUp", "", "GU"},
    {"minos", "Caverns of Minos", "minos.png", "Caverns of", "Minos", "CM"},
    {"gridrunner", "Gridrunner", "gridrunner.png", "Gridrunner", "", "GR"},
    {"fiveaday", "Five A Day", "tp.png", "Five A Day", "", "5D"},
    {"superoxwars", "Super Ox Wars", "sf.png", "Super Ox", "Wars", "OX"},
    {"goatup2", "GoatUp 2", "grunt.png", "GoatUp 2", "", "G2"},
}};

} // namespace mino
