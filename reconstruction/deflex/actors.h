// Deflex entity/target family. Drawing remains an ordered operation: target draw
// publishes its anchor as the position subsequently used by circle contacts.
#pragma once
#include "ball_flight.h"
#include "neon/entity_motion.h"
#include <vector>
#include <string>
#include <optional>

namespace deflex {
struct ActorContext;
struct ActorSprite {
    neon::vec2d position, size;
    neon::colour colour;
    float depth = 0;
    double angle = 0;
    int frame = 0;
    uint32_t orFlags = 0; // preserve incoming sprite flags, optionally OR
    bool region = false;
    neon::vec2d regionOrigin, regionSize;
    bool customCorners = false;
    std::array<double, 8> corners{};
};
struct ActorRenderer {
    virtual ~ActorRenderer() = default;
    virtual void emit(const ActorSprite &sprite) = 0;
};
struct Entity {
    Entity *prev = nullptr, *next = nullptr;
    bool dead = false, enabled = true, noWrap = false, wrapped = false;
    int32_t age = 0;
    int kind = 0, frame = 140, skipDrawFrames = 0, damageFlash = 0;
    uint32_t flags = 0;
    neon::vec2d pos, vel, drag{0.98, 0.98}, extent{1, 1}, scale{1, 1};
    neon::colour colour;
    float size = 0.02f, mass = 1, nearestDistance = 512, angle = 0, depth = 0;
    virtual ~Entity() = default;
    virtual void run(ActorContext &) {
        integrate();
    }
    virtual void hitBy(ActorContext &, BallFlight &) {} // original base entity operation
    virtual int draw(ActorContext &, ActorRenderer &renderer, neon::vec2d view) {
        return drawEntity(renderer, view);
    }
    void integrate() {
        neon::entities::integrate(pos, vel, drag, age, noWrap, wrapped, extent);
    }
    int drawEntity(ActorRenderer &renderer, neon::vec2d view);
};
// controlBlock::attract f0d0. Uses the actual newest-first source list. The
// subject's flags gate attraction; do not substitute the other entity's flags.
neon::vec2d attract(Entity &subject, Entity *list, float fixedMass);

struct ScoreNotice {
    std::string text;
    neon::vec2d position, scale{1.2, 2.4}, destination, destinationScale{4, 4};
    int row = 11, hold = 4, sequence = 3, state = 0;
    float rate = 1, fade = 0.5f;
};
// _particle constructor defaults plus the assignments made by hazard::run.
struct HazardParticleBirth {
    neon::vec2d position, velocity;
    neon::colour from{1, 0, 0, 1}, to{1, 0, 0, 0.2};
    float lifeRate = 0.01f, size = 0.02f;
    double endScale = 3;
    int frame = 30;
};
struct ActorServices {
    virtual ~ActorServices() = default;
    virtual void restoreBallParticles(BallFlight &ball, int count) = 0;
    virtual void sample(int id, int volume) = 0;
    virtual void note(int index) = 0;
    virtual void spark(int count, const Entity &entity) = 0;
    virtual void derez(const Entity &entity, int kind, float gain, int frames) = 0;
    virtual void plinger(int frames) = 0;
    virtual void message(const ScoreNotice &notice) = 0;
    virtual void bonusGlow(neon::vec2d position) = 0;
    virtual void achievement(int id) = 0;
    virtual void hazardParticle(const HazardParticleBirth &birth) = 0;
    virtual void explosion(neon::vec2d position) = 0;
};
struct ActorContext : GameplayFeedback {
    LevelSetup &level;
    neon::DarwinRandom &random;
    ActorServices &services;
    Entity *balls = nullptr, *targets = nullptr;
    // pongBall::run publishes its post-move state before the two bats run.
    // A value snapshot preserves that state on its final (retirement) tick too.
    struct PongMotion {
        neon::vec2d position, velocity;
    };
    std::optional<PongMotion> pongMotion;
    uint32_t frameCount = 0;
    bool levelFinished = false, levelSucceeded = false, targetHit = false, ravished = false;
    int32_t score = 0;
    int bestScore = 0, lastScore = 0, framesSinceTarget = 0, levelFrames = 0, levelIndex = 0;
    int volume = 8, bonusChain = 0;
    float pointScale = 1, ballFraction = 1, multiplier = 1, volumeScale = 1;
    float targetFlash = 0, numberFlash = 0, attractionGain = 0, secondaryGain = 0;
    std::array<neon::colour, 10> palette{};
    ActorContext(LevelSetup &board, neon::DarwinRandom &rng, ActorServices &sink)
        : level(board), random(rng), services(sink) {}
};

struct NumberedTarget : Entity {
    neon::vec2d anchor, numberOffset, numberScale{1, 1.3}, bodyScale, originalScale;
    CellCoords currentCell{};
    std::array<int, 16> digits{}; // inactive leading slots are host-safe zeros
    int firstDigit = 15, digitBase = 17, phase = 0, marker = 0;
    uint32_t movementFlags = 0, behaviourFlags = 0;
    float value = 999, valueDecay = 0.1f, award = 0, collectProgress = 0;
    float numberSize = 0, bodySize = 0, spinPhase = 0, spinLow = 0.1f, spinHigh = 0.5f;
    float proximity = 0, alphaProgress = 0, baseAlpha = 1;
    NumberedTarget(ActorContext &context, const TargetBirth &birth);
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context, BallFlight &ball) override;
    int draw(ActorContext &context, ActorRenderer &renderer, neon::vec2d view) override;

  private:
    void encode(float number);
    void drawNumber(ActorRenderer &renderer, neon::vec2d view);
};
struct Hazard : Entity {
    int variant = 0, motionMode = 0, damage = 5;
    uint32_t movementFlags = 11;
    float angleBias = 0, oscillation = 0;
    // Modes 4/variants 7..10 read +17c without an init store. Zero here is an
    // explicit host allocation policy, not a claim about the original heap.
    Hazard(ActorContext &context, neon::vec2d position, int variant);
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context);
    void hitBy(ActorContext &context, BallFlight &) override {
        hitBy(context);
    }
};
struct Weight : Entity {
    neon::vec2d restPosition;
    int phase = 0;
    explicit Weight(neon::vec2d position);
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context);
    void hitBy(ActorContext &context, BallFlight &) override {
        hitBy(context);
    }
};
struct Bomb : Entity {
    float oscillation = 0;
    Bomb(ActorContext &context, neon::vec2d position);
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context);
    void hitBy(ActorContext &context, BallFlight &) override {
        hitBy(context);
    }
};
struct PongBall : Entity {
    float speed = 0.01f;
    PongBall();
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context, BallFlight &) override;
};
struct PongBat : Entity {
    explicit PongBat(double x);
    void run(ActorContext &context) override;
    void hitBy(ActorContext &context, BallFlight &) override;
};
} // namespace deflex
