#pragma once
#include "ball_mesh.h"
namespace deflex {
struct BallTouch {
    bool down = false;
    neon::vec2d position;
};
// The original per-level best-move records, persisted by the game owner.
struct BallRecords {
    virtual ~BallRecords() = default;
    virtual int bestMoves() const = 0;
    virtual void setBestMoves(int moves) = 0;
    virtual bool firstFourInOneMove() const = 0;
};
struct MainGameBall : Entity {
    using Entity::draw;
    using Entity::run;

    BallFlight flight;
    BallMesh mesh;
    std::array<CellCoords, 3> touchCells{{{-1, -1}, {-1, -1}, {-1, -1}}};
    void init(ActorContext &context, uint32_t particleColour, bool demo = false);
    void run(ActorContext &context, BallFlightContext &movement, BallRecords &records,
             const std::array<BallTouch, 3> &touches);
    void draw(ActorContext &context, ActorRenderer &renderer, neon::vec2d view,
              uint32_t particleColour, bool paused, bool demo);
};
} // namespace deflex
