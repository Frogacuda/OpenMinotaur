#pragma once
#include "kids.h"
#include "neon/game_presentation.h"
#include <string>

namespace goatup {
// Original campaign lists, terrain interactions and summit/return state.
class World {
  public:
    neon::DarwinRandom random;
    TowerSurface tower;
    ClimbContext game;
    Climber goat;
    Effects effects;
    DenizenContext actors;
    KidState kidState;
    Denizens denizens;
    Kids kids;
    PlatformGenerator generator;
    bool paused = false;
    int zoneTicks = 0, captionTicks = 0, summitTicks = 0, goalTicks = 0, goal = -1;
    float feedbackPhase = 0;
    neon::FeedbackState feedback;
    std::string caption, summitCaption;
    struct HudRow {
        int x, row;
        std::string text;
        neon::colour colour;
    };
    std::vector<HudRow> hudRows; // Retained text printed before actor updates.
    bool hudGameOver = false;
    World(CollectionRecord &, uint32_t seed, bool initializeStars = true);
    void tick(const ClimbInput &);
    void prepareBackground(std::vector<ActorDraw> &);
    void prepareForeground(std::vector<ActorDraw> &);

  private:
    bool touching_ = false;
    int kidReleaseTicks_ = 0;
    float bridgeFeedback_ = 0;
    bool checkBridge(Platform &);
    void smash(Platform &, int);
    void settleSummit();
    void updatePowerups();
    void updateBridgeFeedback();
    void prepareHud();
};
} // namespace goatup
