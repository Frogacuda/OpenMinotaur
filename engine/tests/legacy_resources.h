// neon_resources.h - NeonResManager: the game's resource table, read from
// NeonResources.xml in the imported bundle.
//
// The original used Llamasoft's own wide-char XML walker with node open/close
// callbacks; we parse with pugixml and fill the same tables:
//   Literals   - texture files by index (Minotron: index 0 = minotron.png)
//   Fonts      - name/size/bold/italic (unused by the game itself)
//   Spritemaps - per map, entries of {page, x, y, w, h} in texture pixels
//   SpritePages - page sizes (UV = pixel / page size)
//   Sounds     - sample files and their voice limits
// UiTheme.xml is not loaded: its data is only read by the light-synth editor UI,
// which the game never runs.
#pragma once

#include <string>
#include <vector>

namespace NEON_GAME {
namespace Neon_Internals {

struct SpriteEntry {
    int page = 0;     // literal (texture) index
    int x = 0, y = 0; // top-left in texture pixels
    int w = 0, h = 0;
};

struct SpritePage {
    int w = 0, h = 0; // page size the sprite map's coordinates are relative to
};

struct SampleEntry {
    std::string file;
    int maxInstances = 1;
};

struct FontEntry {
    std::string name;
    int size = 12;
    bool bold = false, italic = false;
};

class NeonResManager {
  public:
    // Parses NeonResources.xml text. Returns false if the document is unusable.
    bool load(const std::string &xmlText, std::string *error = nullptr);

    const std::vector<std::string> &literals() const {
        return literals_;
    }
    const std::vector<FontEntry> &fonts() const {
        return fonts_;
    }
    const std::vector<std::vector<SpriteEntry>> &spritemaps() const {
        return spritemaps_;
    }
    const std::vector<SpritePage> &pages() const {
        return pages_;
    }
    const std::vector<SampleEntry> &samples() const {
        return samples_;
    }
    int spriteCount(int map = 0) const {
        return map >= 0 && map < int(spritemaps_.size()) ? int(spritemaps_[size_t(map)].size()) : 0;
    }
    const SpritePage *page(int i) const {
        return i >= 0 && i < int(pages_.size()) ? &pages_[size_t(i)] : nullptr;
    }

    // Entry `index` of sprite map `map` (map 0 is the game's sprite sheet), or null.
    const SpriteEntry *sprite(int index, int map = 0) const;

  private:
    std::vector<std::string> literals_;
    std::vector<FontEntry> fonts_;
    std::vector<std::vector<SpriteEntry>> spritemaps_;
    std::vector<SpritePage> pages_;
    std::vector<SampleEntry> samples_;
};

} // namespace Neon_Internals
} // namespace NEON_GAME
