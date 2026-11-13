#pragma once
#include "generation.h"
#include <cstdint>

namespace goatup {
struct ClimbInput {
    // Canonical gravity points right; the original climb drive increases left.
    static double tiltAxisFromGravity(double x) {
        return -x;
    }
    enum class Method { None, RelativeTouch, DirectAxis, Accelerometer } method = Method::None;
    // RelativeTouch: original originX-currentX. DirectAxis: canonical native axis.
    double horizontal = 0;
    bool jump = false;
    bool dragHasHistory = false;
    double dragDelta = 0; // Original previousX-currentX from processJoyMove.
};
struct ClimbContext : GenerationState {
    explicit ClimbContext(TowerSurface &surface) : tower(surface) {}
    TowerSurface &tower;
    int ticks = 0, highestPlatform = 0, sessionHighestPlatform = 0, flightTicks = 0;
    int flightWarning = 0, flightFlash = 0;
    bool gameOver = false, lowGravity = false;
    bool demo = false; // Native non-playing/menu flag, not an autonomous controller.
    bool summitVisited = false, bridgeJustRemoved = false;
    bool diveRequested = false, tallyBegun = false, tallyComplete = false, summitBirdSent = false;
    float diveSpacing = .05f, targetDiveSpacing = .05f;
    std::function<void()> settleSummit, homecoming;
    std::function<void(int)> sound, stopSound;
    std::function<void(Platform &, int)> tread, smash;
    std::function<void(int, float, float, Vec2, uint32_t, uint32_t)> splash;
    std::function<void()> trailStar, contacts;
};
struct ClimbHistory {
    Vec2 position{}, scale{};
    float angle = 0;
    int frame = 0, platform = -1;
    float platformOffset = 0, standingY = 0;
    bool grounded = false;
};
// theGoat: ascent, summit settling and the descending homecoming route.
class Climber {
  public:
    Vec2 position{0, .35}, velocity{}, scale{1.5, 1};
    int platform = -1, lastPlatform = -1, jumpTicks = 0, kids = 0, rotationLock = 0;
    float heading = 0, lastDrive = 0, beltFactor = 1.2f, walked = 0, pregnancy = 0, standingY = 0,
          angle = 0;
    float radius = .03f;
    int frame = 1;
    bool jumped = false, touchRebaseRequested = false, specialLandingSound = false;
    Vec2 projectedScale{1.5, 1};
    neon::colour drawColour; // Source colour is sampled at player-run entry.
    std::array<ClimbHistory, 1024> history{};
    uint32_t historyCursor = 0;
    uint32_t historyCount = 0; // Host validity guard, not an original gameplay counter.
    Climber();
    void step(ClimbContext &, const ClimbInput &);
    void prepareDraw(bool diving = false); // Original draw mutates x, angle and horizontal scale.
};
} // namespace goatup
