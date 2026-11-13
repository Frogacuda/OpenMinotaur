// Portable source-level types, NOT packed guest-memory images.
#pragma once
#include "entity_lists.h"
#include "native_colour.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace gr::reconstruction::ios {
struct ShipWorld;
struct BulletWorld;
static_assert(sizeof(float) == 4 && sizeof(double) == 8 && std::numeric_limits<float>::is_iec559 &&
                  std::numeric_limits<double>::is_iec559,
              "Original Gridrunner numeric types require IEEE binary32/binary64");
int32_t add32(int32_t value, int32_t delta);
struct Vec2 {
    double x = 0, y = 0;
    void rotateBy(double angle); // iOS 0x9550
};

// Source Entity vtable: told, draw, run, wasShot, foom, hitBy, didHit.
// Only source-backed simulation fields/methods are implemented at this stage.
class Entity : public ListNode {
  public:
    // Original offsets are documented for evidence, not imposed on host ABI.
    bool flag05 = true, wrapped = false, noWrap = false;              // +5,+6,+7
    bool contact08 = false, contact09 = false, acceptsContact = true; // +8,+9,+a
    int32_t field0c = 0, age = 0, type = 0;                           // +c,+10,+14
    float depth = 0;
    int32_t sprite = 0x8c, drawDelay = 0; // +18,+1c,+20
    Rgba tint;                  // complete original +28..+77 colour, including cycler state
    int32_t collisionGroup = 0; // +78
    uint32_t collisionFlags = 0;
    float angle = 0, response = 1;                                   // +80,+84,+88
    Vec2 position, velocity;                                         // +8c,+9c
    Vec2 damping{.98, .98};                                          // +ac
    Vec2 wrapExtent{1, 1}, scale{1, 1};                              // +cc,+dc
    int32_t fieldFc = 0, field104 = 1, field108 = 1, flashTicks = 0; // +fc,+104,+108,+10c
    float mass = 1, radius = .02f;                                   // +12c,+134
    void run() override;             // iOS 0x2f720, distinct from Head/Ship updates
    virtual void told(int32_t) {}    // original base no-op 0x2f958
    virtual void hitBy(Entity &) {}  // original base no-op 0x2fae0
    virtual void didHit(Entity &) {} // original base no-op 0x2f95c
    // The base implementation expects the distinct generic bull damage field.
    // Gridrunner myBull has collisionFlags=0 and does not enter this path.
    virtual void wasShot(Entity &) {
        throw std::logic_error("Generic bull damage path not reconstructed");
    }
};

// The original class name is tao. It is the trail/arrival base of Ship and
// both moving laser emitters; it is not the snake history mechanism.
class Tao : public Entity {
  public:
    float arrival = 0, arrivalStep = .005f; // +170,+174
    std::array<Vec2, 512> trail{};          // +178
    uint32_t trailFrame = 0;                // +2178
    void initTrail();
    void run() override;
};
// Original Ship derives directly from Tao. The game binding is established
// separately from init/rerez, as with the original game/ac pointer assignment.
class Ship : public Tao {
  public:
    Ship();
    explicit Ship(ShipWorld &);
    ~Ship() override;
    enum class State : int32_t { Arriving = 0, Playing = 1, Dying = 2, Departing = 3, Hidden = 4 };
    State state = State::Arriving; // +217c
    int32_t fireTimer = 0, firePeriod = 20;
    float fireBoost = 0; // +2180,+2184,+2188
    void initialize(bool alternateLayout);
    void rerez(bool alternateLayout);
    void run() override;            // iOS 0x4a530; requires the original game binding
    void didHit(Entity &) override; // iOS 0x4b43c
    void foom();                    // iOS 0x4b448
  private:
    ShipWorld *world_ = nullptr;
    ShipWorld &sourceWorld();
    void fire(float rotation);
    void firePlaying();
    void moveAndTouch(Vec2 displacement);
};
} // namespace gr::reconstruction::ios
