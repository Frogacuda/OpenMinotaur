#include "denizens.h"
#include "neon/entity_list.h"
#include "neon/entity_motion.h"
#include <cmath>
#include <stdexcept>

namespace goatup {
namespace {
int sign(float v) {
    return v < 0 ? -1 : 1;
}
template <class T> void dispose(T *p) {
    delete p;
}
template <class F> F &required(F &callback) {
    if (!callback)
        throw std::logic_error("Unbound GoatUp denizen feedback");
    return callback;
}
} // namespace
void DenizenContext::play(int sample, int shift) {
    required(sound)(sample, shift);
}
void DenizenContext::award(int goal) {
    const uint32_t bit = uint32_t(1) << goal;
    if (game.demo || mode >= 2 || (achievements & bit))
        return;
    achievements |= bit;
    play(13, 13);
    required(achievement)(goal);
}
void DenizenContext::showScore(int points, Vec2 p, float vx, float vy, float radius) {
    if (!game.gameOver)
        required(popup)(points, p, vx, vy, radius);
}
struct Denizens::Actor {
    Actor *prev = nullptr;
    Actor *next = nullptr;
    bool dead = false;
    Denizens &owner;
    DenizenContext &c;
    PlatformBirth::Kind kind;
    Platform *platform;
    ActorDraw draw;
    Vec2 originalScale{1, 1}, velocity{};
    float surfaceAngle = 0, alpha = 1, floatOffset = 0;
    int drawSkip = 2, state = 0, bonusIndex = -1;
    bool floating = false;
    float heartTimer = 1, heartRate = .01f, bobPhase = 0, rockPhase = 0;
    bool knocked = false;
    int groundTile = -1;
    float gravity = .0005f, poisonGravity = .00025f, travelDistance = 0;
    int type = 0, lastTile = -1;
    bool touching = false;
    Vec2 angularPosition{};
    float eggEnergy = 0, eggDecay = 0, spin = 0;
    Actor(Denizens &o, const PlatformBirth &birth)
        : owner(o), c(o.context_), kind(birth.kind), platform(birth.platform),
          surfaceAngle(birth.angle), bonusIndex(birth.bonusIndex) {
        if (!platform && kind != PlatformBirth::Kind::Chick &&
            kind != PlatformBirth::Kind::CeDude && kind != PlatformBirth::Kind::FlyingCat &&
            kind != PlatformBirth::Kind::PlatformBit)
            throw std::logic_error("GoatUp denizen without platform");
        draw.frame = birth.frame;
        draw.colour.white(1);
        switch (kind) {
        case PlatformBirth::Kind::Bonus: {
            if (bonusIndex < 0 || bonusIndex >= 512)
                throw std::logic_error("GoatUp bonus index outside original table");
            draw.radius = .04f;
            const Vec2 size = required(c.spriteSize)(draw.frame + 2);
            if (size.x <= 0 || size.y <= 0)
                throw std::logic_error("GoatUp bonus has no original sprite size");
            if (size.x <= size.y)
                originalScale.x = size.x / size.y;
            else
                originalScale.y = size.y / size.x;
            break;
        }
        case PlatformBirth::Kind::BillyGoat:
            draw.radius = .034f;
            originalScale.x = 1.7;
            break;
        case PlatformBirth::Kind::Sheep:
            draw.radius = .055f;
            originalScale.x = 1.3;
            velocity.x = birth.horizontalSpeed;
            break;
        case PlatformBirth::Kind::Baddy2049:
            draw.radius = .035f;
            originalScale.x = 1.3;
            velocity.x = birth.horizontalSpeed;
            break;
        case PlatformBirth::Kind::Goody2049:
            draw.radius = .035f;
            floatOffset = birth.verticalOffset;
            break;
        case PlatformBirth::Kind::MinerWilly:
            draw.radius = .03f;
            originalScale.y = 1.6;
            velocity.x = birth.horizontalSpeed;
            break;
        case PlatformBirth::Kind::CcDude:
            draw.radius = .03f;
            originalScale.y = .8;
            velocity.x = birth.horizontalSpeed;
            gravity = .001f;
            poisonGravity = .0005f;
            break;
        case PlatformBirth::Kind::CcCar:
            draw.radius = .04f;
            originalScale.x = 1.4;
            velocity.x = birth.horizontalSpeed;
            type = 3;
            gravity = float(double(.001f) * .2);
            poisonGravity = float(double(.001f) * .2 * .8);
            break;
        case PlatformBirth::Kind::CcCat:
            draw.radius = .04f;
            originalScale.x = birth.facing;
            break;
        case PlatformBirth::Kind::PinkiePie:
            draw.radius = .04f;
            originalScale.x = birth.facing;
            gravity = .001f;
            poisonGravity = .0005f;
            break;
        case PlatformBirth::Kind::RollingSkull:
            draw.radius = .025f;
            originalScale.y = 1.2;
            velocity.x = birth.horizontalSpeed;
            gravity = float(double(.001f) * .6);
            poisonGravity = float(double(.001f) * .6 * .8);
            break;
        case PlatformBirth::Kind::Egg:
            draw.frame = 201;
            draw.radius = .02f;
            originalScale.x = (c.random.next() & 1) ? -1.5 : 1.5;
            eggEnergy = float(double(float(c.game.laps)) * -.5 + 3.14);
            eggDecay = float(c.random.range(.003, .04));
            gravity = .001f;
            poisonGravity = .0005f;
            break;
        case PlatformBirth::Kind::Chick:
            draw.frame = 202;
            draw.radius = .03f;
            originalScale.y = 1.6;
            velocity.x = (c.random.next() & 1) ? -.0045 : .0045;
            draw.position = birth.position;
            break;
        case PlatformBirth::Kind::CeDude:
            draw.frame = 200;
            draw.radius = .04f;
            originalScale.y = 1.3;
            platform = nullptr;
            velocity.x = (c.random.next() & 1) ? -.0025 : .0025;
            angularPosition.x = c.random.range(-.45, .45);
            surfaceAngle = float(angularPosition.x);
            draw.position.y = .49;
            gravity = float(double(.001f) * .2);
            poisonGravity = float(gravity * .5f);
            velocity.y = c.random.range(-.02, -.01);
            draw.position.y += velocity.y;
            break;
        case PlatformBirth::Kind::FlyingCat:
            platform = nullptr;
            type = 4;
            draw.radius = .08f;
            originalScale = {.6, .6};
            angularPosition = birth.position;
            velocity = birth.velocity;
            break;
        case PlatformBirth::Kind::PlatformBit:
            platform = nullptr;
            type = 5;
            draw.radius = .02f;
            draw.position = birth.position;
            velocity = birth.velocity;
            spin = float(c.random.range(-.2, .2));
            break;
        case PlatformBirth::Kind::ConveyorCap:
            throw std::logic_error("Conveyor cap belongs to UI list");
        case PlatformBirth::Kind::Pickup: {
            draw.radius = .05f;
            const Vec2 size = required(c.spriteSize)(draw.frame + 2);
            if (size.x <= 0 || size.y <= 0)
                throw std::logic_error("GoatUp pickup has no sprite size");
            if (size.x <= size.y)
                originalScale.x = size.x / size.y;
            else
                originalScale.y = size.y / size.x;
            break;
        }
        }
        draw.scale = originalScale;
        if (kind == PlatformBirth::Kind::FlyingCat)
            projectFlying();
        else if (kind != PlatformBirth::Kind::PlatformBit)
            project();
        // Billy alone restores 2 AFTER projection; the other constructors keep
        // the back-face write of 1. Platform bits use the base entity delay 0.
        if (kind == PlatformBirth::Kind::BillyGoat)
            drawSkip = 2;
        if (kind == PlatformBirth::Kind::PlatformBit)
            drawSkip = 0;
        if (kind == PlatformBirth::Kind::Goody2049)
            originalScale.x *= 1.8;
    }
    void projectFlying() {
        const float angle = float((angularPosition.x - double(c.game.tower.angle)) * 3.141592654),
                    cosine = std::cos(angle);
        draw.position = {double(std::sin(angle)) * .5, angularPosition.y};
        draw.scale = {double(cosine) * originalScale.x, originalScale.y};
        if (sign(float(draw.scale.x)) != sign(float(originalScale.x)))
            drawSkip = 1;
    }
    void project() {
        const float delta = float(surfaceAngle - c.game.tower.angle);
        const float angle = float(double(delta) * 3.141592654), cosine = std::cos(angle);
        draw.position.x = double(std::sin(angle)) * .5;
        // setPosition only reprojects X. Y stays at the last drawn scale until
        // the floating path explicitly copies the saved vector (e.g. an egg's
        // swelling saved Y must not change its resting height).
        draw.scale.x = double(cosine) * originalScale.x;
        if (sign(float(draw.scale.x)) != sign(float(originalScale.x)))
            drawSkip = 1;
        draw.colour.a = double(cosine) * .9 + .05;
        if (c.game.gameOver)
            draw.colour.a *= .4;
        if (floating)
            draw.scale = originalScale;
        if (!platform)
            return;
        draw.position.y = double(platform->y) + double(draw.radius) * draw.scale.y * -.5;
        if (!c.game.returning) {
            if (platform->offscreenTicks == 0 && draw.position.y >= .5)
                dead = true;
            if (draw.position.y > .6)
                dead = true;
        } else if (draw.position.y < -.55)
            dead = true;
    }
    bool solidAt(float angle) const {
        const float column = float(angle + .5f);
        return column >= 0 && column < 1 && platform->tiles[size_t(int(float(column * 40.f)))] != 0;
    }
    void runBilly() {
        // setPosition has already run. Movement changes the angle for NEXT run,
        // not this frame's contact position.
        if ((platform->offscreenTicks == 0 && draw.position.y > .5) || draw.position.y > .6)
            dead = true;
        const Vec2 delta = c.goat.position - draw.position;
        const float distance = float(std::sqrt(delta.x * delta.x + delta.y * delta.y));
        heartRate = float(double(float(1.f - distance)) * .09);
        heartTimer = float(heartTimer - heartRate);
        if (heartTimer < 0) {
            heartTimer = 1;
            owner.heart(surfaceAngle, draw.position.y, .002, .002);
        }
        double candidate;
        if (surfaceAngle <= c.goat.heading) {
            originalScale.x = -1.7;
            candidate = velocity.x + .0003;
        } else {
            originalScale.x = 1.7;
            candidate = velocity.x - .0003;
        }
        if (std::abs(float(candidate)) < .003)
            velocity.x = double(float(candidate));
        const float nextAngle = float(double(surfaceAngle) + velocity.x);
        if (solidAt(nextAngle))
            surfaceAngle = nextAngle;
    }
    void runBonus() {
        if (draw.frame == 196)
            draw.colour = c.cyclingBonusColour;
        if (state == 0)
            drawSkip = 1;
        else if (state == 1) {
            if (floatOffset < 0) {
                velocity.y += .0005;
                floatOffset = float(velocity.y + double(floatOffset));
                draw.position.y += double(floatOffset);
            } else
                state = 2;
            draw.angle = float(velocity.y * 50.);
            if (velocity.x >= 0)
                draw.angle = -draw.angle;
        } else if (state == 3) {
            velocity.y += .001;
            draw.angle = float(double(draw.angle) + .1);
            alpha = float(double(alpha) * .96);
            draw.colour.a = double(alpha);
            floatOffset = float(double(floatOffset) + velocity.y);
            draw.position.y += double(floatOffset);
            // The native literals are 1.02 / 1.03: this sprite grows, not shrinks.
            originalScale = originalScale * (c.tablet ? 1.03 : 1.02);
            if (draw.position.y > .6 || double(alpha) < .05)
                dead = true;
        }
    }
    void runSheep() {
        if (state < 2) {
            float nextAngle = float(double(surfaceAngle) + velocity.x);
            if (!solidAt(nextAngle)) {
                velocity.x = -velocity.x;
                nextAngle = float(velocity.x + double(surfaceAngle));
            }
            surfaceAngle = nextAngle;
            originalScale.x = velocity.x <= 0 ? -1.3 : 1.3;
        }
        if (state == 2) {
            velocity.y += .001;
            draw.angle = float(double(draw.angle) - .1);
            alpha = float(double(alpha) * .98);
            draw.colour.a = double(alpha);
            floatOffset = float(double(floatOffset) + velocity.y);
            draw.position.y += double(floatOffset);
            originalScale = originalScale * (c.tablet ? 1.04 : 1.02);
            if (draw.position.y > .6 || double(alpha) < .05)
                dead = true;
        }
    }
    void runBaddy() {
        const bool protectedPlayer = c.hazardGrace != 0 && c.hazardFlash == 0;
        draw.colour = protectedPlayer ? c.vulnerableColour : c.cyclingBonusColour;
        project();
        if (state < 2) {
            if (state != 1)
                draw.frame = ((c.game.ticks >> 5) & 1) + 85;
            if (protectedPlayer)
                draw.frame += 2;
            float nextAngle = float(double(surfaceAngle) + velocity.x);
            if (!solidAt(nextAngle)) {
                velocity.x = -velocity.x;
                nextAngle = float(velocity.x + double(surfaceAngle));
            }
            surfaceAngle = nextAngle;
            originalScale.x = velocity.x <= 0 ? 1.3 : -1.3;
        }
        if (state == 2) {
            velocity.y += .001;
            draw.angle = float(double(draw.angle) - .1);
            alpha = float(double(alpha) * .98);
            draw.colour.a = double(alpha);
            floatOffset = float(double(floatOffset) + velocity.y);
            draw.position.y += double(floatOffset);
            originalScale = originalScale * (c.tablet ? 1.04 : 1.02);
            if (draw.position.y > .6 || double(alpha) < .05)
                dead = true;
        }
    }
    void runGoody() {
        draw.position.y += double(floatOffset);
        if (state != 0) {
            velocity.y += .001;
            draw.angle = float(double(draw.angle) - .1);
            alpha = float(double(alpha) * .98);
            draw.colour.a = double(alpha);
            floatOffset = float(double(floatOffset) + velocity.y);
            draw.position.y += double(floatOffset);
            originalScale = originalScale * (c.tablet ? 1.04 : 1.02);
            if (draw.position.y > .6 || double(alpha) < .05)
                dead = true;
        }
    }
    void runPickup() {
        if (state == 1) {
            velocity.y += .001;
            draw.angle = float(double(draw.angle) + .1);
            alpha = float(double(alpha) * .98);
            draw.colour.a = double(alpha);
            floatOffset = float(double(floatOffset) + velocity.y);
            draw.position.y += double(floatOffset);
            originalScale = originalScale * (c.tablet ? 1.04 : 1.03);
            if (draw.position.y > .6 || double(alpha) < .05)
                dead = true;
        } else {
            bobPhase = float(double(bobPhase) + .2);
            rockPhase = float(double(rockPhase) + .13);
            const float wave = float(std::sin(bobPhase) * .5f);
            draw.angle = float(float(std::sin(rockPhase) * .5f) * .5f);
            draw.position.y = (draw.position.y + (double(wave) + .5) * -.02) - .03;
        }
    }
    void runTraveller() {
        groundTile = -1;
        float nextAngle = float(double(surfaceAngle) + velocity.x);
        if (nextAngle < -.5f || nextAngle >= .5f) {
            velocity.x = -velocity.x;
            nextAngle = float(double(nextAngle) + velocity.x);
        }
        surfaceAngle = nextAngle;
        if (knocked) {
            floating = true;
            velocity.y += double(gravity);
            draw.position.y += velocity.y;
            draw.angle = float(double(draw.angle) - .1);
            alpha = float(double(alpha) * .98);
            draw.colour.a = double(alpha);
            project();
            return;
        }
        project();
        if (!platform) {
            velocity.y += double(c.poison != 0 ? poisonGravity : gravity);
            if (velocity.y > .049)
                velocity.y = .049;
            draw.position.y += velocity.y;
            const float foot = float(draw.position.y + double(float(draw.radius * .5f)));
            if (velocity.y > 0)
                for (auto &p : c.game.tower.platforms)
                    if (p.active) {
                        const float bottom = float(p.y + float(p.thickness * .5f));
                        if (p.y < foot && foot < bottom) {
                            const int tile = int((double(surfaceAngle) + .5) * 40.);
                            if (p.tiles[size_t(tile)] < 1)
                                return;
                            platform = &p;
                            velocity.y = 0;
                            return;
                        }
                    }
        } else {
            groundTile = int((double(surfaceAngle) + .5) * 40.);
            if (groundTile > 39)
                groundTile = -1;
            if (!solidAt(surfaceAngle)) {
                platform = nullptr;
                groundTile = -1;
            }
        }
    }
    void runMiner() {
        const float prior = surfaceAngle;
        const bool grounded = platform != nullptr;
        draw.colour = c.cyclingBonusColour;
        runTraveller();
        if (platform)
            draw.position.y -= .005;
        travelDistance = float(travelDistance + std::abs(float(surfaceAngle - prior)));
        originalScale.x = velocity.x <= 0 ? -1 : 1;
        constexpr int frames[] = {95, 96, 97, 96};
        draw.frame = frames[int(float(travelDistance * 50.f)) & 3];
        if (grounded && !platform)
            velocity.y = -.005;
    }
    void runBird(bool wanderingDude) {
        // chick 56a84 and cedude 5692c differ in frames and edge-jump velocity.
        const float prior = surfaceAngle;
        const bool grounded = platform != nullptr;
        if (c.game.summitVisited) {
            c.effects.splash(10, -.019f, 3.8f, draw.position, 0xff8000ffu, 0x80000fffu);
            dead = true;
        }
        runTraveller();
        if (platform)
            draw.position.y -= .005;
        travelDistance = float(travelDistance + std::abs(float(surfaceAngle - prior)));
        originalScale.x = velocity.x <= 0 ? 1 : -1;
        draw.frame = (int(float(travelDistance * 50.f)) & 1) + (wanderingDude ? 199 : 202);
        if (grounded && !platform)
            velocity.y = wanderingDude ? -.009 : -.005;
    }
    void runCar() {
        const bool grounded = platform != nullptr;
        runTraveller();
        if (!knocked) {
            draw.angle = float(velocity.y * 50.);
            if (velocity.x >= 0)
                draw.angle = -draw.angle;
        }
        originalScale.x = velocity.x > 0 ? 1.4 : -1.4;
        if (platform) {
            const float column = float(surfaceAngle + .5f);
            if (column >= 0 && column < 1) {
                auto &tile = platform->tiles[size_t(int(float(column * 40.f)))];
                if (tile < 179)
                    tile = 178;
                else {
                    c.effects.splash(30, -.019f, 3.8f, draw.position, 0xffff00ffu, 0xff00ffffu);
                    dead = true;
                }
            }
        }
        if (grounded && !platform)
            velocity.y = -((draw.position.y + .5) * .01 + .005);
    }
    void runCcDude() {
        const float prior = surfaceAngle;
        const bool grounded = platform != nullptr;
        bool hole = false;
        if (platform) {
            const float column = float(surfaceAngle + .5f);
            hole =
                column < 0 || column >= 1 || platform->tiles[size_t(int(float(column * 40.f)))] < 1;
        }
        if (lastTile == 100 && hole && c.game.bridgeJustRemoved) {
            if (!c.game.gameOver) {
                const int points = (c.goat.kids + 1) * (c.game.laps + 1) * 1000;
                c.showScore(points, draw.position, float(draw.position.x * -.01),
                            float(draw.position.y * .001 + .003), .06f);
                c.score += points;
            }
            c.award(6);
        }
        if (lastTile == 100) {
            const double old = velocity.x;
            velocity.x *= .5;
            runTraveller();
            velocity.x = double(float(old));
        } else
            runTraveller();
        if (knocked)
            return;
        lastTile = -1;
        const float column = float(surfaceAngle + .5f);
        if (column >= 0 && column < 1 && platform) {
            const int tile = int(float(column * 40.f));
            lastTile = platform->tiles[size_t(tile)];
            if ((lastTile == 101 && draw.position.y > -.47) ||
                (lastTile == 105 && draw.position.y > -.4)) {
                const int changed = lastTile == 101 ? 106 : 107;
                platform->tiles[size_t(tile)] = changed;
                platform->tiles[size_t((tile & 1) ? tile - 1 : tile + 1)] = changed;
                required(c.checkBridge)(*platform);
                c.game.bridgeJustRemoved = false;
                lastTile = 0;
            }
        }
        travelDistance = float(travelDistance + std::abs(float(surfaceAngle - prior)));
        constexpr int frames[] = {102, 103, 104, 103};
        originalScale.x = velocity.x <= 0 ? -1 : 1;
        draw.frame = frames[int(float(travelDistance * 40.f)) & 3];
        if (grounded && !platform)
            velocity.y = -.005;
    }
    void runPinkie() {
        if (platform && draw.position.y > -.5) {
            if (groundTile >= 0)
                velocity.x = double(float(platform->belt[size_t(groundTile)] * c.goat.beltFactor));
        } else
            velocity.x *= .95;
        runTraveller();
    }
    void runSkull() {
        runTraveller();
        if (knocked)
            return;
        draw.angle = float(double(draw.angle) + velocity.x * -30.);
        draw.colour = c.hazardGrace == 0 ? c.protectionColour : c.vulnerableColour;
    }
    void runEgg() {
        if (c.game.summitVisited) {
            c.effects.splash(10, -.019f, 3.8f, draw.position, 0xff8000ffu, 0x80000fffu);
            dead = true;
        }
        runTraveller();
        if (double(eggEnergy) < .8) {
            draw.angle = float(double(std::sin(float(eggEnergy * 60.f))) * .3);
            draw.colour = c.vulnerableColour;
            originalScale = originalScale * 1.001;
        }
        if (draw.position.y > -.45)
            eggEnergy = float(eggEnergy - eggDecay);
        if (eggEnergy < 0) {
            c.effects.splash(30, -.029f, .8f, draw.position, 0xffffffffu, 0xffff00ffu);
            PlatformBirth chick{PlatformBirth::Kind::Chick, platform, surfaceAngle, 202};
            chick.position = draw.position;
            owner.create(chick);
            dead = true;
        }
    }
    void runFlying() {
        angularPosition += velocity;
        if (angularPosition.x < -.5 || angularPosition.x >= .5) {
            angularPosition.x -= velocity.x;
            velocity.x = -velocity.x;
        }
        projectFlying();
        owner.touchActor(*this);
        if (draw.position.y < -.6)
            dead = true;
        originalScale.x = velocity.x <= 0 ? -1 : 1;
    }
    void runBit() {
        draw.position += velocity;
        velocity.y += .0008;
        draw.angle = float(draw.angle + spin);
        if (draw.position.y > .6)
            dead = true;
        owner.touchActor(*this);
    }
    void run() {
        // baddy copies its current palette BEFORE the common projection alpha.
        if (kind == PlatformBirth::Kind::Baddy2049) {
            runBaddy();
            return;
        }
        if (kind == PlatformBirth::Kind::MinerWilly) {
            runMiner();
            return;
        }
        if (kind == PlatformBirth::Kind::CcDude) {
            runCcDude();
            return;
        }
        if (kind == PlatformBirth::Kind::CcCar) {
            runCar();
            return;
        }
        if (kind == PlatformBirth::Kind::PinkiePie) {
            runPinkie();
            return;
        }
        if (kind == PlatformBirth::Kind::RollingSkull) {
            runSkull();
            return;
        }
        if (kind == PlatformBirth::Kind::Egg) {
            runEgg();
            return;
        }
        if (kind == PlatformBirth::Kind::Chick || kind == PlatformBirth::Kind::CeDude) {
            runBird(kind == PlatformBirth::Kind::CeDude);
            return;
        }
        if (kind == PlatformBirth::Kind::FlyingCat) {
            runFlying();
            return;
        }
        if (kind == PlatformBirth::Kind::PlatformBit) {
            runBit();
            return;
        }
        project();
        switch (kind) {
        case PlatformBirth::Kind::BillyGoat:
            runBilly();
            break;
        case PlatformBirth::Kind::Bonus:
            runBonus();
            break;
        case PlatformBirth::Kind::Sheep:
            runSheep();
            break;
        case PlatformBirth::Kind::Goody2049:
            runGoody();
            break;
        case PlatformBirth::Kind::Pickup:
            runPickup();
            break;
        case PlatformBirth::Kind::CcCat:
            break;
        default:
            throw std::logic_error("Unimplemented GoatUp actor update");
        }
    }
    void hitBilly(int hitKind) {
        if (hitKind != 1)
            return;
        dead = true;
        c.game.pregnant = true;
        if (!c.game.demo) {
            c.play(15);
            c.play(20);
        }
        ++c.runBillyGoats;
        ++c.records.billyGoats;
        if (c.records.billyGoats == 100)
            c.award(5);
        if (c.game.pickupSpeedThreshold < c.game.climbSpeed) {
            c.game.climbSpeed = float(double(c.game.climbSpeed) * .8);
            c.game.pickupSpeedThreshold = float(double(c.game.pickupSpeedThreshold) + .00015);
        }
        for (int n = 0; n < 20; ++n)
            owner.heart(surfaceAngle, draw.position.y, .006, .002);
        const int points = (c.goat.kids + 1) * 100;
        if (!c.game.gameOver) {
            // This actor constructs its score notice directly with a double velocity.
            required(c.popup)(points, draw.position, 0, -.001, .05f);
            c.score += points;
        }
    }
    void hitBonus() {
        if (state == 0) {
            if (draw.position.y > -.45 && draw.position.y < .5) {
                state = 1;
                velocity.y = -.01;
                floatOffset = -.01f;
                if (!c.game.demo)
                    c.play(17);
            }
            return;
        }
        if (state == 1) {
            if (platform->offscreenTicks > 0)
                dead = true;
            if (velocity.y <= -.005)
                return;
        } else if (state != 2)
            return;
        state = 3;
        floating = true;
        velocity.y = -.02;
        const uint32_t mask = uint32_t(1) << (bonusIndex & 31);
        const size_t word = size_t((bonusIndex >> 5) & 15);
        if (!(c.runBonuses[word] & mask)) {
            c.runBonuses[word] |= mask;
            ++c.uniqueRunBonuses;
        }
        if (!(c.records.bonuses[word] & mask)) {
            c.records.bonuses[word] |= mask;
            ++c.records.uniqueBonuses;
            if (c.records.uniqueBonuses == 100)
                c.award(7);
            if (c.records.uniqueBonuses == 250)
                c.award(8);
            if (c.records.uniqueBonuses == 500)
                c.award(9);
        }
        if (!c.game.demo)
            c.play(19, 10);
        // The random draw precedes the special-frame OR, even for 586..589.
        if (c.poison > 0 || ((c.random.next() & 7) == 0) || unsigned(draw.frame - 586) < 4)
            required(c.bonusCaption)(draw.frame);
        int points;
        if (!c.game.returning)
            points = (c.goat.kids + 1) * platform->level;
        else {
            points = (513 - platform->level) * 10;
            c.game.returnSpeed = float(double(c.game.returnSpeed) * 1.001);
        }
        points += c.game.laps * 512;
        if (!c.game.gameOver) {
            // This actor constructs its score notice directly with a double velocity.
            required(c.popup)(points, draw.position, 0, -.001, .03f);
            c.score += points;
        }
        c.game.climbSpeed = float(double(c.game.climbSpeed) * 1.01);
        if (c.game.pregnant) {
            c.goat.pregnancy = float(double(c.goat.pregnancy) + .2);
            if (c.goat.pregnancy >= 4)
                c.goat.pregnancy = 3.99f;
        }
    }
    void hitSheep(int hitKind, Vec2 sourcePosition) {
        if (state == 2)
            return;
        if (hitKind == 1 && c.hazardGrace == 0) {
            c.goat.rotationLock = c.goat.kids ? 40 + c.game.laps * 20 : 90;
            c.goat.platform = -1;
            c.goat.velocity.x *= c.random.range(-1.3, -.8);
            c.goat.velocity.y = c.random.range(-.03, -.01);
            if (c.pendingKidLoss == 0)
                c.pendingKidLoss = c.goat.kids;
            if (!c.game.demo)
                c.play(43, 14);
        } else {
            if (!c.game.demo) {
                c.play(18);
                c.award(4);
            }
            c.effects.splash(30, -.019f, 3.8f, draw.position, 0xffffffu, 0xffffu);
            // Unlike the inline bonus/Billy births, theGame::showScore gates
            // both the popup and credit on gameOver, and credits internally.
            if (!c.game.gameOver) {
                const int points = (c.rewardKids + 1) * (c.game.laps + 1) * 1000;
                c.showScore(points, sourcePosition, float(draw.position.x * -.01),
                            float(draw.position.y * .002 + .003), .05f);
                c.score += points;
            }
        }
        const auto r1 = c.random.next(), r2 = c.random.next();
        constexpr int noises[] = {0, 1, 2, 3, 4, 4};
        c.play(noises[(r1 & 3) + (r2 & 1)]);
        c.flash = 1;
        state = 2;
        floating = true;
        velocity.y = -.02;
    }
    void hitBaddy(int hitKind, Vec2 p) {
        if (state == 2)
            return;
        if (hitKind == 1 && c.hazardGrace == 0) {
            c.goat.rotationLock = c.goat.kids ? 40 + c.game.laps * 20 : 90;
            c.goat.platform = -1;
            c.goat.velocity.x *= c.random.range(-1.3, -.8);
            c.goat.velocity.y = c.random.range(-.03, -.01);
            if (c.pendingKidLoss == 0)
                c.pendingKidLoss = c.goat.kids;
            if (!c.game.demo)
                c.play(43, 14);
        } else {
            if (!c.game.demo)
                c.play(23);
            c.effects.splash(30, -.019f, 3.8f, draw.position, 0xffffffu, 0xff00ffffu);
            if (!c.game.gameOver) {
                const int points = (c.rewardKids + 1) * (c.game.laps + 1) * 100;
                c.showScore(points, p, float(draw.position.x * -.01),
                            float(draw.position.y * .002 + .003), .05f);
                c.score += points;
            }
        }
        state = 2;
        floating = true;
        velocity.y = -.02;
    }
    void hitGoody() {
        if (state != 0)
            return;
        velocity.y = -.02;
        state = 1;
        floating = true;
        c.hazardFlash = 0;
        if (c.hazardGrace < 350)
            c.hazardGrace = 350;
        c.hazardWarning = 262;
        if (!c.game.demo)
            c.play(24);
    }
    void hitPickup(int hitKind) {
        if (state == 1 || hitKind == 5)
            return;
        if (draw.frame == 83) {
            c.game.flightFlash = 0;
            c.game.flightTicks = 1200;
            c.game.flightWarning = 900;
            const auto noise = c.random.next();
            if (!c.game.demo)
                c.play(int((noise & 1023) % 6) + 6, 13);
            required(c.status)(1, 240);
        } else if (draw.frame == 84) {
            if (!c.game.demo)
                c.play(44);
            c.poison = 1;
            c.hazardFlash = 0;
            if (c.hazardGrace < 500)
                c.hazardGrace = 500;
            c.hazardWarning = 375;
            required(c.status)(2, 500);
        } else if (draw.frame == 82) {
            c.game.climbSpeed = float(c.game.climbSpeed * .5f);
            c.game.pickupSpeedThreshold = float(double(c.game.pickupSpeedThreshold) + .0003);
            required(c.status)(0, 240);
            const auto noise = c.random.next();
            if (!c.game.demo)
                c.play(int((noise & 1023) % 6) + 6, 13);
        }
        velocity.y = -.02;
        state = 1;
        floating = true;
    }
    void hitMiner(int hitKind, Vec2 p) {
        if (knocked)
            return;
        if (hitKind == 1 && c.hazardGrace == 0) {
            c.goat.rotationLock = c.goat.kids ? 40 + c.game.laps * 20 : 90;
            c.goat.platform = -1;
            c.goat.velocity.x *= c.random.range(-1.3, -.8);
            c.goat.velocity.y = c.random.range(-.03, -.01);
            if (c.pendingKidLoss == 0)
                c.pendingKidLoss = c.goat.kids;
            if (!c.game.demo)
                c.play(43, 14);
        } else {
            c.effects.splash(30, -.019f, 3.8f, draw.position, 0xffffffu, 0xff00ffffu);
            if (!c.game.gameOver) {
                const int points = (c.rewardKids + 1) * (c.game.laps + 1) * 100;
                c.showScore(points, p, float(draw.position.x * -.01),
                            float(draw.position.y * .002 + .003), .05f);
                c.score += points;
            }
        }
        if (!c.game.demo)
            c.play(26);
        velocity.y = -.01;
        platform = nullptr;
        knocked = true;
    }
    void recoil() {
        c.goat.rotationLock = c.goat.kids ? 40 + c.game.laps * 20 : 90;
        c.goat.platform = -1;
        c.goat.velocity.x *= c.random.range(-1.3, -.8);
        c.goat.velocity.y = c.random.range(-.03, -.01);
        if (c.pendingKidLoss == 0)
            c.pendingKidLoss = c.goat.kids;
        if (!c.game.demo)
            c.play(43, 14);
    }
    void reward(Vec2 p, int points, uint32_t from, uint32_t to, double vertical) {
        c.effects.splash(30, -.019f, 3.8f, draw.position, from, to);
        if (!c.game.gameOver) {
            c.showScore(points, p, float(draw.position.x * -.01),
                        float(draw.position.y * vertical + .003), .05f);
            c.score += points;
        }
    }
    void hitLater(int sourceType, Vec2 p) {
        if (knocked)
            return;
        const bool damaging = sourceType == 1 && c.hazardGrace == 0;
        const int normal = (c.rewardKids + 1) * (c.game.laps + 1);
        switch (kind) {
        case PlatformBirth::Kind::CcCar:
            if (damaging)
                recoil();
            else
                reward(p, normal * 100, 0xffff00ffu, 0xff00ffffu, .002);
            if (!c.game.demo)
                c.play(18);
            velocity.y = -.01;
            break;
        case PlatformBirth::Kind::CcDude:
            if (damaging)
                recoil();
            reward(p, normal * 10, 0xffffffu, 0xffffu, .001);
            velocity.y = -.01;
            break;
        case PlatformBirth::Kind::PinkiePie: {
            if (damaging)
                recoil();
            else
                reward(p, normal * 100, 0xff8080ffu, 0x80080fffu, .004);
            const auto first = c.random.next(), second = c.random.next();
            constexpr int noises[] = {0, 1, 2, 3, 4, 4};
            c.play(noises[(first & 3) + (second & 1)]);
            c.flash = 1;
            velocity.y -= .02;
            break;
        }
        case PlatformBirth::Kind::RollingSkull:
            if (damaging)
                recoil();
            else
                reward(p, (sourceType == 1 ? c.rewardKids + 1 : normal) * 100, 0xffff00ffu,
                       0xff00ffffu, .001);
            velocity.y = -.01;
            break;
        case PlatformBirth::Kind::Chick:
        case PlatformBirth::Kind::CeDude:
            if (!c.game.demo)
                c.play(18);
            if (damaging)
                recoil();
            else
                reward(p, normal * (kind == PlatformBirth::Kind::CeDude ? 500 : 100), 0xffffffu,
                       0xff00ffffu, .002);
            velocity.y = kind == PlatformBirth::Kind::CeDude ? -.02 : -.01;
            break;
        case PlatformBirth::Kind::Egg:
            c.hazardFlash = 0;
            if (c.hazardGrace < 200)
                c.hazardGrace = 200;
            c.hazardWarning = 150;
            reward(p, normal * 100, 0xff8080ffu, 0x80080fffu, .004);
            if (!c.game.demo)
                c.play(40);
            velocity.y -= .02;
            break;
        default:
            throw std::logic_error("Not a later traveller contact");
        }
        platform = nullptr;
        knocked = true;
    }
    void hitCat() {
        if (c.hazardGrace < 440)
            c.hazardGrace = 440;
        c.hazardWarning = 330;
        c.hazardFlash = 0;
        PlatformBirth flying{PlatformBirth::Kind::FlyingCat, nullptr, 0, 181};
        flying.position = {double(surfaceAngle), draw.position.y};
        flying.velocity = {originalScale.x * -.002, -.002};
        owner.create(flying);
        dead = true;
        if (!c.game.demo)
            c.play(32);
    }
    void hit(int hitKind, Vec2 p) {
        switch (kind) {
        case PlatformBirth::Kind::BillyGoat:
            hitBilly(hitKind);
            break;
        case PlatformBirth::Kind::Bonus:
            hitBonus();
            break;
        case PlatformBirth::Kind::Sheep:
            hitSheep(hitKind, p);
            break;
        case PlatformBirth::Kind::Baddy2049:
            hitBaddy(hitKind, p);
            break;
        case PlatformBirth::Kind::Goody2049:
            hitGoody();
            break;
        case PlatformBirth::Kind::Pickup:
            hitPickup(hitKind);
            break;
        case PlatformBirth::Kind::MinerWilly:
            hitMiner(hitKind, p);
            break;
        case PlatformBirth::Kind::CcCat:
            hitCat();
            break;
        case PlatformBirth::Kind::CcDude:
        case PlatformBirth::Kind::CcCar:
        case PlatformBirth::Kind::PinkiePie:
        case PlatformBirth::Kind::RollingSkull:
        case PlatformBirth::Kind::Egg:
        case PlatformBirth::Kind::Chick:
        case PlatformBirth::Kind::CeDude:
            hitLater(hitKind, p);
            break;
        default:
            break;
        }
    }
};
Denizens::Denizens(DenizenContext &context) : context_(context) {}
Denizens::~Denizens() {
    reset();
}
void Denizens::reset() {
    neon::entities::destroyList(actors_, dispose<Actor>);
    neon::entities::destroyList(bits_, dispose<Actor>);
}
void Denizens::create(const PlatformBirth &birth) {
    auto node = std::make_unique<Actor>(*this, birth);
    auto *&head = birth.kind == PlatformBirth::Kind::FlyingCat ||
                          birth.kind == PlatformBirth::Kind::PlatformBit
                      ? bits_
                      : actors_;
    neon::entities::prepend(head, *node);
    node.release();
}
void Denizens::heart(float angle, double y, double xRange, double yRange) {
    context_.effects.loveHeart(angle, y, xRange, yRange, context_.game.tower.angle);
}
void Denizens::tick() {
    neon::entities::runAndCull(actors_, dispose<Actor>);
}
void Denizens::homecoming() {
    // theGoat::run removes native type 19: bonus::init assigns that type.
    // cedude inherits entity type 0 and is not the target of this sweep.
    for (auto *a = actors_; a; a = a->next)
        if (a->kind == PlatformBirth::Kind::Bonus)
            a->dead = true;
}
void Denizens::tickBits() {
    neon::entities::runAndCull(bits_, dispose<Actor>);
}
void Denizens::drawBits(std::vector<ActorDraw> &out) {
    for (auto *a = bits_; a; a = a->next) {
        if (a->drawSkip > 0) {
            --a->drawSkip;
            continue;
        }
        out.push_back(a->draw);
    }
}
void Denizens::prepareDraw(std::vector<ActorDraw> &out) {
    for (auto *a = actors_; a; a = a->next) {
        if (a->kind == PlatformBirth::Kind::Bonus && context_.paused && a->state == 0) {
            a->drawSkip = 1;
            continue;
        }
        if (a->drawSkip > 0) {
            --a->drawSkip;
            continue;
        }
        out.push_back(a->draw);
    }
}
bool Denizens::touch(Vec2 position, float radius, int type, bool &touching, int drawSkip,
                     bool dead) {
    return touchImpl(position, radius, type, touching, drawSkip, dead, nullptr);
}
void Denizens::touchActor(Actor &source) {
    touchImpl(source.draw.position, source.draw.radius, source.type, source.touching,
              source.drawSkip, source.dead, &source);
}
bool Denizens::touchImpl(Vec2 position, float radius, int type, bool &touching, int drawSkip,
                         bool dead, Actor *source) {
    // controlBlock::touch -> collide(false), 0xf0b0 / 0xedcc. These caller
    // classes have no projectile flags or collision groups. Reuse the shared
    // distance rounding; retain the source traversal/latch without creating
    // synthetic full physics entities to call the bounce-capable binding.
    if (drawSkip || dead)
        return false;
    bool blocked = false, hit = false;
    for (auto *target = actors_; target; target = target->next) {
        if (target->dead || target == source)
            continue;
        const float radii = float(radius + target->draw.radius);
        const Vec2 delta = position - target->draw.position;
        const float distance = neon::entities::roundedDistance(delta.y, delta.x);
        if (radii <= distance) {
            touching = false;
            continue;
        }
        if (touching || target->drawSkip != 0) {
            blocked = true;
            continue;
        }
        if (!blocked) {
            target->hit(type, position);
            hit = true;
        }
    }
    return hit;
}
void Denizens::hit(int ordinal, int type, Vec2 p) {
    auto *a = actors_;
    while (a && ordinal-- > 0)
        a = a->next;
    if (!a)
        throw std::out_of_range("GoatUp contact ordinal");
    a->hit(type, p);
}
std::vector<Denizens::Contact> Denizens::contacts() const {
    std::vector<Contact> out;
    int ordinal = 0;
    for (auto *a = actors_; a; a = a->next)
        out.push_back({ordinal++, a->draw.position, a->draw.radius, a->drawSkip, a->dead});
    return out;
}
int Denizens::count() const {
    int n = 0;
    for (auto *a = actors_; a; a = a->next)
        ++n;
    return n;
}
} // namespace goatup
