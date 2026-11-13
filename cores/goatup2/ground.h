#pragma once
#include "medium.h"
#include "player_history.h"
#include "terrain.h"

namespace goatup2 {
struct GroundScoreView {
    int32_t &score;
    int32_t &stageTotal;
    int32_t &highScore;
    int32_t &recordCounter; // game +0xce8: incremented on the first record crossing
    bool &newRecordLatch;
    const int32_t &multiplier;
};
struct GroundRewardActorView {
    const neon::vec2d &worldPosition;
    const neon::vec2d &stagePosition;
    const neon::vec2d &drawScale;
    const float &gravity;
    float &hunger;
};
// Overrides for original grassParticle / nomParticle, not splash/goatPoop.
struct GroundParticleBirth {
    enum class Kind { Grass, Nom };
    Kind kind;
    neon::vec2d worldPosition, stagePosition, velocity, savedVelocity, savedDrawScale;
    float gravity, radius, fadePerUpdate, angle, spin;
    double horizontalDrawScale;
    int32_t sprite;
    float opacity = 1;
    uint32_t tileInteractionMask = 0x440000U;
    int32_t ledgeResponse = 0;
    std::optional<neon::PaletteChannels> colourOverride;
};
class GroundOutput {
  public:
    virtual ~GroundOutput() = default;
    // Must construct/apply overrides/prepend immediately to +0x3bc.
    virtual void adoptGroundParticle(const GroundParticleBirth &) = 0;
    virtual void playSample(int32_t sample, uint32_t gain, int32_t sourceOption) = 0;
};
void collectGroundReward(GroundRewardActorView actor, GroundScoreView score,
                         ParticleSoundSettings sound, neon::DarwinRandom &random,
                         GroundOutput &output);

struct GroundActorView {
    GroundRewardActorView reward;
    neon::vec2d &stagePosition;
    bool &restingContact;
    const int32_t &supportColumn;
    const int32_t &supportRow;
    const float &zoom;
    float &sinkOffset;
    double &horizontalImpulse;
    int32_t &forcedJumpTicks;
    float &forcedJumpSpeed;
    GoatCheckpointHistory &checkpoints;
};
// 0x5a280; stored support coordinates, not an extra coordinate query.
// Supports direct reads within working/adjacent-backup physical storage.
void interactWithGround(GroundActorView actor, MutableTerrainGridView grid,
                        const TerrainLayout &layout, uint32_t clock, bool &exitContact,
                        MediumSharedState &shared, GroundScoreView score,
                        neon::DarwinRandom &random, GroundOutput &output);
} // namespace goatup2
