#include "neon/resource_catalog.h"
#include "legacy_resources.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

// Compare every public field, including empty slots and order, not just counts.
template <class Catalog> std::string snapshot(const Catalog &c) {
    std::ostringstream s;
    for (const auto &x : c.literals())
        s << "L" << x.size() << ":" << x << "\n";
    for (const auto &x : c.fonts())
        s << "F" << x.name.size() << ":" << x.name << ":" << x.size << ":" << x.bold << ":"
          << x.italic << "\n";
    for (const auto &m : c.spritemaps()) {
        s << "M" << m.size() << "\n";
        for (const auto &x : m)
            s << x.page << "," << x.x << "," << x.y << "," << x.w << "," << x.h << "\n";
    }
    for (const auto &x : c.pages())
        s << "P" << x.w << "," << x.h << "\n";
    for (const auto &x : c.samples())
        s << "S" << x.file.size() << ":" << x.file << ":" << x.maxInstances << "\n";
    return s.str();
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
        return 2;
    std::string xml((std::istreambuf_iterator<char>(input)), {});
    neon::ResourceCatalog engine;
    reference::Neon_Internals::NeonResManager legacy;
    std::string a, b;
    if (!engine.load(xml, &a) || !legacy.load(xml, &b) || snapshot(engine) != snapshot(legacy)) {
        std::cerr << "Manifest mismatch: " << argv[1] << " " << a << " " << b << "\n";
        return 1;
    }
    const char *sparse =
        "<NeonResources><Literals><Literal>first</Literal><Literal index='3'>fourth</Literal>"
        "<Literal index='0'>fifth</Literal></Literals><Fonts><Font index='2' size='18' bold='1'>font</Font></Fonts>"
        "<Spritemaps><Spritemap index='1' entries='2'><Spentry>2,123-62,-3+2,8+4,20-7</Spentry>"
        "<Spentry>1,2,3,4,5</Spentry></Spritemap></Spritemaps><SpritePages><Spritepage>512,256</Spritepage>"
        "</SpritePages><Sounds><Sample maxinstance='3'>  sample.wav  </Sample></Sounds></NeonResources>";
    if (!engine.load(sparse, &a) || !legacy.load(sparse, &b) ||
        snapshot(engine) != snapshot(legacy))
        return 1;
    const auto *entry = engine.sprite(0, 1);
    if (engine.literals().size() != 5 || engine.literals()[1] != "" ||
        engine.literals()[3] != "fourth" || engine.literals()[4] != "fifth" ||
        engine.fonts().size() != 3 || engine.fonts()[2].size != 18 || !engine.fonts()[2].bold ||
        !entry || entry->page != 2 || entry->x != 61 || entry->y != -1 || entry->w != 12 ||
        entry->h != 13 || engine.pages()[0].w != 512 || engine.pages()[0].h != 256 ||
        engine.samples()[0].file != "sample.wav" || engine.samples()[0].maxInstances != 3)
        return 1;
    // Loading is replacement, including failure; preserve that observable contract.
    if (engine.load("<invalid/>", &a) || legacy.load("<invalid/>", &b) || a != b ||
        snapshot(engine) != snapshot(legacy))
        return 1;
    std::cout << "Resource parity: " << argv[1] << "\n";
}
