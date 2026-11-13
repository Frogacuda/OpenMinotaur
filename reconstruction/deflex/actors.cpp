#include "actors.h"
#include "neon/integer.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace deflex {
int Entity::drawEntity(ActorRenderer &renderer, neon::vec2d view) {
    if (frame < 0)
        return 0;
    ActorSprite sprite;
    sprite.colour = colour;
    if (damageFlash != 0) {
        damageFlash = neon::integers::add32(damageFlash, -1);
        const float fraction = float(damageFlash & 7) / 7.0f;
        sprite.colour.b = 0;
        sprite.colour.g = double(fraction);
        sprite.colour.r = double(fraction) * 0.5 + 0.5;
    }
    sprite.angle = double(angle);
    sprite.frame = frame + 2;
    sprite.depth = depth;
    const float diameter = size + size;
    sprite.size = {double(diameter) * view.x * scale.x, double(diameter) * view.y * scale.y};
    sprite.position = pos * view;
    if (skipDrawFrames > 0) {
        --skipDrawFrames;
        return 0;
    }
    renderer.emit(sprite);
    return 6;
}
neon::vec2d attract(Entity &subject, Entity *list, float fixedMass) {
    neon::vec2d result;
    float nearest = 512;
    const float gravity = subject.mass * 1.0e-6f;
    const float fixedForce = gravity * fixedMass;
    for (auto *other = list; other; other = other->next) {
        if (other == &subject || subject.flags)
            continue;
        auto direction = (other->pos - subject.pos) * 512.0;
        const double distance = std::sqrt(direction.y * direction.y + direction.x * direction.x);
        const float rounded = float(distance);
        if (rounded < nearest)
            nearest = rounded;
        direction = distance > 0 ? direction / distance : neon::vec2d{};
        const float force = fixedMass > 0 ? fixedForce : gravity * other->mass;
        const float square = rounded * rounded;
        const float acceleration = force / square;
        result.x += direction.x * double(acceleration);
        result.y += direction.y * double(acceleration);
    }
    subject.nearestDistance = nearest;
    return result;
}
namespace {
constexpr uint32_t colours[11] = {0xff0000ff, 0xff8000ff, 0xffff00ff, 0x80ff00ff,
                                  0x00ff00ff, 0x00ff80ff, 0x00ffffff, 0x0080ffff,
                                  0x0000ffff, 0x000080ff, 0x008040ff};
// _targets.76061 at 1d80d8, words up to the next original symbol.
constexpr std::array<int, 34> targetFrames = {78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67,
                                              66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55,
                                              54, 51, 50, 49, 46, 45, 44, 43, 42, 16};
void wrapOnce(double &value) {
    if (value < -.5)
        value += 1;
    if (value > .5)
        value -= 1;
}
void warnVoice(ActorContext &c) {
    if (!c.voicePlayed) {
        static constexpr int voices[] = {0, 1, 2, 3, 4, 4};
        const auto a = c.random.next(), b = c.random.next();
        c.services.sample(voices[(b & 1) + (a & 3)], c.volume);
        c.voicePlayed = true;
    }
}
// Identical damage presentation in hazard, weight, pongBall and pongBat.
// Queue damage in the caller before this feedback; sparks consume shared RNG
// before the once-per-tick warning sample chooses its two random indices.
void contactFeedback(ActorContext &c, const Entity &e) {
    if (!(c.frameCount & 15))
        c.level.grid.blendRadius(0.3f, e.pos, 0.05f, 0xaf40f0ff, 0.1f);
    if (!(c.frameCount & 7))
        c.services.spark(20, e);
    warnVoice(c);
}
} // namespace
NumberedTarget::NumberedTarget(ActorContext &c, const TargetBirth &birth) {
    pos = anchor = birth.position;
    frame = birth.frame;
    colour.unpackRGBA(birth.colour);
    size = birth.size;
    scale = birth.spriteScale;
    kind = 1;
    mass = c.level.definition.scalar(0xe14);
    drag = {double(c.level.definition.scalar(0xe18)), double(c.level.definition.scalar(0xe18))};
    encode(999);
    spinPhase = float(c.random.range(-3.141592654, 3.141592654));
    baseAlpha = float(colour.a);
    if (c.level.definition.integer(0xe5c) == 1) {
        colour.unpackRGBA(colours[c.random.next() % 11]);
        size = float(double(size) * c.random.range(0.4, 1.2));
        vel = {0, c.random.range(0.001, 0.002)};
        vel.rotateBy(c.random.range(-3.141592654, 3.141592654));
        for (int i = 0; i < c.level.definition.integer(0xe60); ++i) {
            pos -= vel;
            wrapOnce(pos.x);
            wrapOnce(pos.y);
        }
        spinLow = float(c.random.range(-0.1, 0.1));
        anchor = pos;
    } else if (c.level.definition.integer(0xe5c) == 2) {
        // Original shipped callers index this static sprite sequence by creation order.
        frame = targetFrames.at(size_t(c.level.targetsCreated));
    }
    bodySize = size;
    bodyScale = originalScale = scale;
    numberOffset = {0, double(size) * 2.5};
    numberSize = (size * 3.0f) / 5.0f;
    // The newLevel caller supplies these after numberedTarget::init returns.
    value = 999;
    marker = birth.group;
    movementFlags = birth.movementFlags;
    behaviourFlags = birth.behaviourFlags;
}
void NumberedTarget::encode(float number) {
    firstDigit = 15;
    int index = 15;
    for (int n = int(number); n > 0; n /= 10) {
        digits[size_t(index--)] = n % 10;
        firstDigit = index;
    }
    if (index == 15)
        firstDigit = 14;
}
void NumberedTarget::run(ActorContext &c) {
    encode(value);
    if (alphaProgress < 1) {
        colour.a = double(alphaProgress * baseAlpha);
        alphaProgress = float(double(alphaProgress) + 0.01);
        if (alphaProgress > 1)
            alphaProgress = 1;
    } else
        colour.a = double(baseAlpha);
    neon::vec3d displaced{pos.x * 512, pos.y * 512, 0};
    c.level.grid.doDisplace(displaced);
    depth = float(displaced.z);
    if (phase != 0) {
        if (phase != 1)
            return;
        anchor = anchor.lerp({0, 0.35}, collectProgress);
        collectProgress = float(double(collectProgress) + 0.005);
        if ((anchor - neon::vec2d{0, 0.35}).length() < 0.01)
            collectProgress = 1;
        else if (collectProgress < 1)
            return;
        const int32_t before = c.score;
        c.score = neon::integers::add32(c.score, int32_t(int(award)));
        const bool best = c.bestScore >= 1 && c.bestScore >= before && c.bestScore < c.score;
        const bool last = c.lastScore >= 1 && c.lastScore >= before && c.lastScore < c.score;
        if (last || best) {
            const bool occupied = c.level.grid.messageLines[11].count != 0;
            ScoreNotice notice;
            notice.text = last ? "LAST ATTEMPT BEATEN" : "LOCAL HIGH BEATEN";
            notice.row = occupied ? 12 : 11;
            notice.position = {c.level.definition.real(0x650),
                               c.level.definition.real(0x658) + (occupied ? .1 : 0)};
            notice.destination = {occupied ? double(-0.55f) : -0.5,
                                  occupied ? double(0.35f) : double(0.4f)};
            c.services.message(notice);
        }
        dead = true;
        return;
    }
    currentCell = c.level.grid.getCellCoords(pos);
    if ((c.level.flags & 1) && value > 10)
        value -= valueDecay;
    if (c.levelFinished) {
        phase = 2;
        c.services.derez(*this, 0, 1, 60);
        dead = true;
    }
    const auto *first = c.balls;
    const float distance = float((first->pos - anchor).length());
    proximity = std::clamp(float((0.5 - double(distance)) + (0.5 - double(distance))), 0.0f, 1.0f);
    if (movementFlags) {
        if (movementFlags & 4) {
            mass += c.attractionGain;
            auto force = attract(*this, c.balls, 1000000.0f);
            if (!(movementFlags & 1))
                force.x = 0;
            if (!(movementFlags & 2))
                force.y = 0;
            vel += force;
            mass -= c.attractionGain;
        }
        auto proposed = anchor + vel;
        const auto oldCell = c.level.grid.getCellCoords(anchor);
        currentCell = c.level.grid.getCellCoords(proposed);
        int collisionX = c.level.grid.peek(currentCell.x, oldCell.y),
            collisionY = c.level.grid.peek(oldCell.x, currentCell.y);
        if (movementFlags & 8)
            collisionX = collisionY = 0;
        neon::vec2d reflection{1, 1};
        if (movementFlags & 16)
            wrapOnce(proposed.x);
        if ((!(movementFlags & 16) && (proposed.x < -.45 || proposed.x > .45)) || collisionX != 0) {
            proposed.x = anchor.x;
            reflection.x = -1;
        }
        if (movementFlags & 32)
            wrapOnce(proposed.y);
        if ((!(movementFlags & 32) && (proposed.y < -.45 || proposed.y > .45)) || collisionY != 0) {
            proposed.y = anchor.y;
            reflection.y = -1;
        }
        anchor = proposed;
        vel = vel * reflection;
        vel = vel * drag;
    }
    angle = 0;
    if (behaviourFlags & 16)
        angle = float(std::atan2(-vel.y, vel.x) - 1.570796327);
    if (behaviourFlags & 2) {
        spinPhase += spinLow + (spinHigh - spinLow) * proximity;
        if (double(spinPhase) > 3.141592654)
            spinPhase = float(double(spinPhase) - 6.2831853072);
        if (double(spinPhase) < -3.141592654)
            spinPhase = float(double(spinPhase) + 6.2831853072);
        angle = float(double(angle) + double(std::sin(spinPhase)) * 0.1);
    }
    if ((behaviourFlags & 8) && double(std::fabs(float(pos.x - first->pos.x))) > 0.01)
        bodyScale.x = pos.x - first->pos.x < 0 ? originalScale.x : -originalScale.x;
    if (behaviourFlags & 4)
        bodyScale.x = vel.x <= 0 ? originalScale.x : -originalScale.x;
    if (behaviourFlags & 32) {
        spinPhase += spinLow;
        angle = spinPhase;
    }
}
void NumberedTarget::hitBy(ActorContext &c, BallFlight &ball) {
    if (phase >= 1)
        return;
    c.targetFlash = 1;
    if (marker >= 2) {
        ++c.pendingParticleLoss;
        if (!(c.frameCount & 15))
            c.level.grid.blendRadius(0.3f, pos, 0.05f, 0xaf40f0ff, 0.1f);
        if (!(c.frameCount & 7))
            c.services.spark(20, *this);
        warnVoice(c);
        return;
    }
    c.services.restoreBallParticles(ball, c.level.definition.integer(0x604));
    --c.level.targetsRemaining;
    c.targetHit = true;
    phase = 1;
    const int savedVolume = c.volume;
    c.volume = int(float(c.volume) * c.volumeScale);
    c.services.sample(int(c.random.next() % 7) + 6, c.volume);
    c.volume = savedVolume;
    collectProgress = 0;
    c.random.next(); // discarded original draw
    if (c.framesSinceTarget < 5 || marker == 1) {
        c.multiplier += 1;
        if (c.multiplier > 9)
            c.services.achievement(4);
        if (c.multiplier > 19)
            c.services.achievement(5);
        c.services.plinger(3);
        const std::string text = "x" + std::to_string(int(c.multiplier));
        ScoreNotice message;
        message.text = text;
        message.position = {0, 0};
        message.scale = {4.8, 5.8};
        message.row = 1;
        message.sequence = 5;
        message.state = 1;
        c.services.message(message);
        if (marker == 1)
            for (auto *e = c.targets; e; e = e->next)
                if (e->kind == 1)
                    --static_cast<NumberedTarget *>(e)->marker;
    }
    if (c.multiplier > 1) {
        ++c.bonusChain;
        if (c.bonusChain > 30)
            c.services.achievement(2);
        if (c.bonusChain > 50)
            c.services.achievement(3);
        c.services.bonusGlow(pos);
    }
    c.framesSinceTarget = 0;
    if (c.level.flags & 0x40) {
        const double gain = double(c.level.definition.scalar(0xe1c)) + 1.03;
        ball.turnSpeed = float(double(ball.turnSpeed) * gain);
        ball.velocity = ball.velocity * gain;
    }
    if (c.level.flags & 0x80)
        c.attractionGain += c.level.definition.scalar(0xe20);
    if (c.level.flags & 0x100)
        c.secondaryGain += c.level.definition.scalar(0xe24);
    award = value * c.pointScale;
    award *= c.ballFraction;
    award *= c.multiplier;
    c.level.grid.blendRadius(0.5f, pos, 0.016666668f, colours[c.random.next() % 7], 0.7f);
    c.services.spark(40, *this);
    c.reactionFrames = 20;
    if ((c.random.next() & 7) == 0)
        c.services.plinger(int(c.random.range(3, 7)));
    else {
        c.services.note(currentCell.x);
        c.services.note(currentCell.x + 2);
        c.services.note(currentCell.y);
        c.services.note(currentCell.y + 2);
    }
}
void NumberedTarget::drawNumber(ActorRenderer &renderer, neon::vec2d view) {
    const auto unit = scale * double(size), step = unit * 1.2;
    const float count = float(15 - firstDigit);
    auto point = pos;
    point.x += step.x * double(count) * -0.5 + step.x * 0.5;
    point.y += step.y * -0.5;
    for (int index = firstDigit + 1; index < 16; ++index) {
        ActorSprite sprite;
        sprite.position = point * view;
        sprite.size = unit * view;
        sprite.colour = colour;
        sprite.angle = double(angle);
        sprite.depth = depth;
        sprite.frame = digitBase + digits[size_t(index)] + 2;
        sprite.orFlags = 3;
        renderer.emit(sprite);
        point.x += step.x;
    }
}
int NumberedTarget::draw(ActorContext &c, ActorRenderer &renderer, neon::vec2d view) {
    const auto savedColour = colour;
    if ((behaviourFlags & 1) || phase == 1) {
        pos = anchor + numberOffset;
        size = numberSize;
        scale = numberScale;
        colour = c.palette[c.numberFlash <= 0 ? 4 : 0];
        const float saved = value;
        const float savedAngle = angle;
        if (phase == 0) {
            float gain = c.pointScale * c.multiplier;
            gain *= c.ballFraction;
            value = float(int(value * gain));
        } else
            value = award;
        encode(value);
        drawNumber(renderer, view);
        value = saved;
        angle = savedAngle;
    }
    pos = anchor;
    size = bodySize;
    scale = bodyScale;
    colour = savedColour;
    if (marker == 1) {
        colour.a *= 0.7;
        scale = scale * 2.0;
    }
    if (phase < 1)
        drawEntity(renderer, view);
    return 6;
}
Hazard::Hazard(ActorContext &c, neon::vec2d position, int type) : variant(type) {
    pos = position;
    frame = 51; // theGame::newLevel supplies this before init
    switch (variant) {
    case 0:
        angleBias = -0.7853981852531433f;
        colour.red(1);
        scale = scale * 2;
        break;
    case 1:
        motionMode = 1;
        damage = 20;
        mass = 0.8f;
        frame = 39;
        scale.x = 2;
        size = 0.03f;
        break;
    case 2:
        motionMode = 2;
        mass = 1.2f;
        frame = 64;
        colour.r = colour.g = colour.b = 0.5;
        damage = 1;
        scale.y = 1.4;
        size = 0.06f;
        drag = {0.92, 0.92};
        break;
    case 3:
        motionMode = 1;
        mass = 0.2f;
        frame = 70;
        colour.r = colour.g = colour.b = 0.5;
        damage = 10;
        scale.y = 1.2;
        size = 0.035f;
        drag = {0.92, 0.92};
        break;
    case 4:
        motionMode = 1;
        mass = 0.3f;
        frame = 75;
        colour.r = colour.g = colour.b = 0.3;
        damage = 5;
        scale.y = 1.2;
        size = 0.055f;
        drag = {0.975, 0.975};
        break;
    case 5:
        angleBias = -0.7853981852531433f;
        colour.red(1);
        movementFlags = 1;
        scale = scale * 1.5;
        drag = {0.96, 0.96};
        mass = 0.5f;
        break;
    case 6: {
        frame = 80;
        movementFlags = 42;
        mass = 0;
        damage = 50;
        motionMode = 3;
        scale = scale * 2;
        drag = {1, 1};
        // The three-entry original table is indexed by a one-based count.
        // NIFTY LIFTY 2 creates three lifts: its last lookup reads the adjacent
        // 0x33333333 word at 1d80a8. Preserve that observable colour explicitly.
        static constexpr std::array<uint32_t, 4> liftColours = {0xff0000ff, 0x00ff4000, 0xffff00ff,
                                                                0x33333333};
        colour.unpackRGBA(liftColours.at(size_t(c.level.hazardsCreated)));
        vel.y = double(float(c.level.hazardsCreated) + 1.0f) * 0.002;
        break;
    }
    case 7:
    case 8:
        motionMode = 4;
        damage = variant == 7 ? 2 : 5;
        mass = variant == 7 ? 0.0f : 2.0f;
        frame = 89;
        colour.r = 0.4;
        colour.g = 0.1;
        colour.b = 0.3;
        size = 0.03f;
        break;
    case 9:
        motionMode = 4;
        mass = 0;
        frame = 96;
        scale.y = 2;
        break;
    case 10:
        motionMode = 4;
        damage = 15;
        mass = 2;
        frame = 98;
        scale.y = 2;
        colour.r = colour.g = colour.b = 0.5;
        size = 0.03f;
        break;
    case 11:
        motionMode = 4;
        damage = 100;
        mass = 0;
        frame = 64;
        colour.r = colour.g = colour.b = 0.5;
        scale.y = 1.4;
        size = 0.06f;
        drag = {0.92, 0.92};
        break;
    }
}
void Hazard::run(ActorContext &c) {
    const auto oldCell = c.level.grid.getCellCoords(pos);
    mass += c.secondaryGain;
    neon::vec2d proposed = pos, reflection{1, 1};
    CellCoords nextCell = oldCell;
    int collisionX = 0, collisionY = 0;
    if (motionMode != 3) {
        auto force = attract(*this, c.balls, 1000000.0f);
        if (motionMode == 0) {
            if (!(movementFlags & 1))
                force.x = 0;
            if (!(movementFlags & 2))
                force.y = 0;
        }
        // Mode 2 explicitly rounds length to float before the threshold test.
        const double forceLength = motionMode == 2 ? double(float(force.length())) : force.length();
        if (forceLength < 0.02)
            vel += force;
    }
    if (motionMode == 0)
        angle = float(std::atan2(-vel.y, vel.x) - 1.570796327) + angleBias;
    // Mode 2's original branch never writes its next-cell stack pair before
    // the common note/flash tail (5d428). Retain this port's deterministic
    // actual-cell policy rather than turning stale stack bytes into RNG draws.
    // This differs from executions that consume unspecified stack contents.
    proposed = pos + vel;
    nextCell = c.level.grid.getCellCoords(proposed);
    if (motionMode == 1 || motionMode == 4 || (motionMode == 0 && !(movementFlags & 8))) {
        collisionX = c.level.grid.peek(nextCell.x, oldCell.y);
        collisionY = c.level.grid.peek(oldCell.x, nextCell.y);
    }
    if (motionMode == 2 || motionMode == 4) {
        const float distance = float((pos - c.balls->pos).length());
        const float closeness = distance > 0.5f ? 0.0f : 0.5f - distance;
        oscillation += closeness;
        angle = float(double(std::sin(oscillation)) * 0.2);
        if (motionMode == 4 && frame == 98) {
            const float dx = float(c.balls->pos.x - pos.x);
            if (dx > 0 && scale.x < 0)
                scale.x = -scale.x;
            if (dx < 0 && scale.x > 0)
                scale.x = -scale.x;
        }
        if ((motionMode == 2 || frame == 64) && double(closeness) > 0.3 && !(c.frameCount & 7)) {
            HazardParticleBirth birth;
            birth.position = pos;
            birth.velocity.x = c.random.range(-0.01, 0.01);
            birth.velocity.y = c.random.range(-0.01, 0.01);
            c.services.hazardParticle(birth);
        }
    }
    mass -= c.secondaryGain;
    bool edgeX = false, edgeY = false;
    if (movementFlags & 16)
        wrapOnce(proposed.x);
    else
        edgeX = proposed.x < -.49 || proposed.x > .49;
    if (movementFlags & 32)
        wrapOnce(proposed.y);
    else
        edgeY = proposed.y < -.49 || proposed.y > .49;
    if (edgeX || collisionX) {
        proposed.x = pos.x;
        nextCell.x = oldCell.x;
        reflection.x = -1;
    }
    if (edgeY || collisionY) {
        proposed.y = pos.y;
        nextCell.y = oldCell.y;
        reflection.y = -1;
    }
    if (nextCell.x != oldCell.x || nextCell.y != oldCell.y) {
        if (nextCell.x != oldCell.x)
            c.services.note(nextCell.x);
        if (nextCell.y != oldCell.y)
            c.services.note(nextCell.y);
        // rcols starts with its count; colours[] contains the following 11 entries.
        c.level.grid.blendRadius(0.3f, pos, 0.016666668f, colours[c.random.next() % 11], 0.3f);
    }
    vel = vel * reflection;
    vel = vel * drag;
    pos = proposed;
    if (c.levelFinished) {
        c.services.explosion(pos);
        dead = true;
    }
}
void Hazard::hitBy(ActorContext &c) {
    c.pendingParticleLoss = neon::integers::add32(c.pendingParticleLoss, damage);
    if (frame == 64)
        c.ravished = true;
    contactFeedback(c, *this);
}
Weight::Weight(neon::vec2d position) : restPosition(position) {
    pos = position;
    frame = 57;
    scale.x = 1.2;
    size = 0.03f;
}
void Weight::run(ActorContext &c) {
    if (phase == 1) {
        vel.y += 0.0006;
        pos.y += vel.y;
        if (pos.y > 0.49) {
            vel.y = -0.01;
            phase = 2;
        }
    } else if (phase == 2) {
        pos.y += vel.y;
        if (pos.y <= restPosition.y) {
            vel.y = 0;
            phase = 0;
        }
        // Deliberately still evaluated when the preceding branch sets phase 0.
        if (std::fabs(c.balls->pos.x - pos.x) < 0.1)
            phase = 1;
    } else if (phase == 0 && std::fabs(c.balls->pos.x - pos.x) < 0.1)
        phase = 1;
    if (c.levelFinished) {
        c.services.explosion(pos);
        dead = true;
    }
}
void Weight::hitBy(ActorContext &c) {
    c.pendingParticleLoss = neon::integers::add32(c.pendingParticleLoss, 25);
    contactFeedback(c, *this);
    vel.x = -vel.x;
    pos.x += vel.x;
}
Bomb::Bomb(ActorContext &c, neon::vec2d position) {
    pos = position;
    frame = 48;
    size = 0.03f;
    scale.y = 1.2;
    oscillation = float(c.random.range(-3.141592654, 3.141592654));
}
void Bomb::run(ActorContext &c) {
    oscillation = float(double(oscillation) + 0.1);
    if (double(oscillation) > 3.141592654)
        oscillation = float(double(oscillation) - 6.2831853072);
    angle = float(double(std::sin(oscillation)) * 0.1);
    if (c.levelFinished) {
        c.services.explosion(pos);
        dead = true;
    }
}
void Bomb::hitBy(ActorContext &c) {
    // Unlike the ball's neighbourhood blast, this visits rows before columns.
    for (int y = 0; y < c.level.grid.rows; ++y)
        for (int x = 0; x < c.level.grid.cols; ++x) {
            const int glyph = c.level.grid.peek(x, y);
            if (glyph == 101 || glyph == 102)
                blowCell(c.level, x, y, [&](neon::vec2d p) { c.services.explosion(p); });
        }
    c.services.explosion(pos);
    c.level.grid.blendRadius(0.6f, pos, 0.016666668f, 0xffc020ff, 1.0f);
    const int first = int(c.random.next() & 7) * 6 + 14;
    // Eight overlapping samples, despite the six-sample group stride.
    for (int i = 0; i < 8; ++i)
        c.services.sample(first + i, c.volume);
    dead = true;
}
PongBall::PongBall() {
    frame = 53;
    size = 0.02f;
    scale.y = 1.2;
    pos = {0, .1};
    vel = neon::vec2d{.05, .02}.normalize() * double(speed);
}
void PongBall::run(ActorContext &c) {
    if (c.targetHit) {
        c.targetHit = false;
        speed = float(double(speed) + .001);
    }
    pos += vel;
    if ((pos.x < -.4 && vel.x < 0) || (pos.x > .4 && vel.x > 0)) {
        neon::vec2d direction;
        if (std::fabs(c.balls->pos.x - pos.x) <= .2)
            direction = {-pos.x, c.random.range(-.8, .8)};
        else
            direction = c.balls->pos - pos;
        vel = direction.normalize() * double(speed);
    }
    if (pos.y < -.45 || pos.y > .45) {
        const double old = vel.y;
        vel.y = -old;
        pos.y -= old;
    }
    c.pongMotion = ActorContext::PongMotion{pos, vel};
    if (c.levelFinished) {
        c.services.explosion(pos);
        dead = true;
    }
}
void PongBall::hitBy(ActorContext &c, BallFlight &) {
    c.pendingParticleLoss = neon::integers::add32(c.pendingParticleLoss, 25);
    contactFeedback(c, *this);
    vel.x = -vel.x;
    pos.x += vel.x;
}
PongBat::PongBat(double x) {
    frame = 52;
    size = 0.02f;
    scale.y = 2.2;
    pos = {x, 0};
}
void PongBat::run(ActorContext &c) {
    if (c.pongMotion && ((pos.x < 0 && c.pongMotion->velocity.x < 0) ||
                         (pos.x > 0 && c.pongMotion->velocity.x > 0))) {
        const float follow = float(1.0 - std::fabs(pos.x - c.pongMotion->position.x));
        pos.y += (c.pongMotion->position.y - pos.y) * double(follow);
    }
    if (c.levelFinished) {
        c.services.explosion(pos);
        dead = true;
    }
}
void PongBat::hitBy(ActorContext &c, BallFlight &) {
    c.pendingParticleLoss = neon::integers::add32(c.pendingParticleLoss, 5);
    contactFeedback(c, *this);
}
} // namespace deflex
