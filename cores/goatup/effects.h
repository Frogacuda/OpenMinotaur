#pragma once
#include "platform.h"
#include "neon/source_random.h"
#include <cstdint>

namespace goatup {
struct ClimbContext;
class Climber;
struct ActorDraw {
    Vec2 position{}, scale{1, 1};
    neon::colour colour;
    float angle = 0, radius = .02f;
    int frame = 0; // Original entity frame, before literal-page +2.
    bool particle = false;
};
// The initial background stars and the player's active psplash/trail families.
// They share the gameplay RNG and source list timing; they are not a cosmetic
// host overlay and cannot be advanced by an extra present/redraw.
class Effects {
  public:
    explicit Effects(neon::DarwinRandom &);
    ~Effects();
    Effects(const Effects &) = delete;
    Effects &operator=(const Effects &) = delete;
    void reset();
    void initializeStars();
    void initializeTitleStars(const std::array<uint32_t, 16> &collected, int unique,
                              const std::function<Vec2(int)> &spriteSize);
    void splash(int count, float speed, float spread, Vec2 at, uint32_t from, uint32_t to);
    void playerTrail(Vec2 at, int flightTicks);
    void loveHeart(float angle, double y, double xRange, double yRange, float cameraAngle);
    void floatingScore(int points, Vec2 at, double vx, double vy, float radius);
    void bridgeBit(Vec2 at);
    void summitBird(ClimbContext &, Climber &);
    void conveyorCap(Platform &, int frame, float angle, int tile);
    void tickParticles();
    void tickStars();
    void tickUi(bool paused, float cameraAngle, const neon::colour &cyclingColour,
                bool returning = false, bool gameOver = false, bool tablet = false);
    void drawStars(std::vector<ActorDraw> &) const;
    void drawParticles(std::vector<ActorDraw> &) const;
    void prepareUiDraw(std::vector<ActorDraw> &);
    int stars() const;
    int particles() const;
    int trails() const;
    int hearts() const;

  private:
    struct Star;
    struct Particle;
    struct UiActor;
    Star *stars_ = nullptr;
    const std::array<uint32_t, 16> *titleCollection_ = nullptr;
    int titleUnique_ = 0;
    std::function<Vec2(int)> titleSpriteSize_;
    Particle *particles_ = nullptr;
    UiActor *ui_ = nullptr;
    bool paused_ = false;
    bool returning_ = false, gameOver_ = false, tablet_ = false;
    float cameraAngle_ = 0;
    neon::colour cyclingColour_;
    neon::DarwinRandom &random_;
};
} // namespace goatup
