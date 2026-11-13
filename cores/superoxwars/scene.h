#pragma once
#include "terrain.h"
namespace superoxwars {
class Scene {
  public:
    World &world;
    Terrain terrain;
    WaveController waves;
    float speed = 1;
    bool boundary = false, bossPhase = false;
    struct Bonus {
        int remaining = 0, initial = 0, steps = 0, step = 0, finalScore = 0;
        std::string heading, detail;
    };
    Bonus blueBonus, redBonus;
    int bonusState = 0, bonusTimer = 0;
    std::array<uint32_t, 1024> seeds{};
    static constexpr int SupportedAreas = 10;
    float flowerPhase = 0, shotChanceStep = 60.f / 254.f;
    explicit Scene(World &w) : world(w), terrain(w), waves(w) {}
    void start(int level = 0);
    void tick();
    void beginTick();
    void finishTick();
    void create(char, int, int);
    void endRun();
    void beginBonus();
    void tallyBonus();
    void nextLevel();
};
} // namespace superoxwars
