#pragma once
#include "world.h"
#include "waves.h"
namespace superoxwars {
// STM's circular terrain cache. Row scripts select ASCII patterns, which create
// both encoded font cells and game actors. Published cell positions belong to
// presentation preparation, as in 41534/5301c.
class Terrain {
  public:
    explicit Terrain(World &w) : world(w) {}
    World &world;
    int cursor = 0, worldRow = 0, script = 0;
    bool mirrored = false;
    int actorGlyph = 0, actorTint = -1;
    std::array<uint32_t, 1024> rowSeeds{};
    std::array<Colour, 32> colours{};
    std::function<void(char, int, int)> createActor;
    std::function<void()> crossed;
    void init(int area, bool preserveCache = false);
    bool scroll(float delta);
    void generate(int physicalRow);
    void prepare(std::vector<ActorDraw> &);
};
} // namespace superoxwars
