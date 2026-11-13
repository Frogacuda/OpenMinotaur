#pragma once
#include "console.h"
#include "world.h"
namespace deflex {
class History {
    struct CharacterBall : Entity {
        using Entity::run;

        neon::vec2d logical;
        CellCoords previous;
        std::array<CellCoords, 4> touchCells{{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}};
        int left, right, top, bottom, mode;
        bool turned = false;
        CharacterBall(History &, int);
        void run(History &);
    };
    std::unique_ptr<CharacterBall> ball_;
    std::list<std::unique_ptr<Entity>> targets_;
    float unusedPickupBonus_ = 0; // Source +348: +5 diamond/+20 sprite, capped at 99; no consumer.
    void spawnBall(int);
    void clearActors();
    void spawnTargets();
    void runTargets();
    void spray(int, neon::vec2d, float, uint32_t);
    bool lineNotes(int x, int y, int count, bool horizontal, bool blend = false);
    void startScript(int);

  public:
    World world;
    Console console;
    int phase = 0;
    History(Progress &, const std::vector<neon::vec2d> &, uint32_t seed);
    void tick(bool held, bool pressed, bool ok, bool paused = false);
    void drawGrid(GridRenderer &, ActorRenderer &, neon::vec2d, bool paused);
    void drawActors(ActorRenderer &, neon::vec2d, bool paused);
    bool trial() const {
        return phase == 5 || phase == 8 || phase == 11;
    }
};
} // namespace deflex
