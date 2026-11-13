#include "climber.h"
#include "original_tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace goatup {
namespace {
template <class Fn, class... Args> void call(Fn &f, Args &&...args) {
    if (!f)
        throw std::logic_error("GoatUp climb dependency is not connected");
    f(std::forward<Args>(args)...);
}
int sign(float f) {
    return f < 0 ? -1 : 1;
}
} // namespace
Climber::Climber() {
    // Zeroed history is host-owned storage. Kid lookup accepts only entries
    // written through the source history ring, tracked by historyCount.
}
void Climber::step(ClimbContext &g, const ClimbInput &input) {
    if (g.gameOver)
        return;
    touchRebaseRequested = false;
    if (rotationLock)
        --rotationLock;
    bool heldJump = input.jump;
    int tile = int((double(heading) + .5) * 40.);
    if (tile < 0 || tile >= 40)
        throw std::logic_error("GoatUp heading outside source tile range");
    if (jumped && g.ticks > 60) {
        if (platform < 0)
            call(g.trailStar);
        if (g.diveRequested) {
            g.returning = true;
            g.summitVisited = false;
            if (velocity.y > 0) {
                rotationLock = 0;
                g.diveRequested = false;
                g.targetDiveSpacing = .07f;
                for (auto &p : g.tower.platforms)
                    p.reset();
            }
        }
        if (!g.summitVisited) {
            if (!g.returning) {
                g.tower.scroll = float(g.tower.scroll + g.climbSpeed);
                if (g.climbSpeed < g.targetClimbSpeed)
                    g.climbSpeed = float(double(g.climbSpeed) * 1.0008);
                if (g.targetClimbSpeed < g.climbSpeed)
                    g.climbSpeed = float(double(g.climbSpeed) * .999);
            } else {
                if (!g.diveRequested && double(g.tower.scroll) > -.2) {
                    g.tower.scroll = float(g.tower.scroll - g.returnSpeed);
                    if (g.tower.scroll < -.2f)
                        g.tower.scroll = -.2f;
                }
                if (g.tower.scroll < 1)
                    g.targetDiveSpacing = .05f;
                if (g.diveSpacing < g.targetDiveSpacing)
                    g.diveSpacing = float(double(g.diveSpacing) * 1.001);
                if (g.targetDiveSpacing < g.diveSpacing)
                    g.diveSpacing = float(double(g.diveSpacing) * .999);
            }
        } else {
            const float delta = float(.4 - position.y);
            if (std::abs(delta) > .1)
                g.tower.scroll = float(double(g.tower.scroll) + (delta > 0 ? .004 : -.004));
            else
                call(g.settleSummit);
            if (!g.diveRequested) {
                if (auto *p = g.tower.find(lastPlatform))
                    position.y = double(float(p->y + float(radius * -.5f)));
            }
            heldJump = false;
        }
    }
    g.tower.tileStep = g.diveSpacing;
    Platform *ground = nullptr;
    if (platform < 0) {
        float gravity = g.tower.fade == 0 ? .001f : .0016f;
        if (g.lowGravity)
            gravity = .0002f;
        if (!heldJump)
            jumpTicks = 0;
        else if (jumpTicks >= 1 || g.flightTicks != 0) {
            --jumpTicks;
            velocity.y -= .0014;
            gravity = .001f;
        }
        velocity.y += double(gravity);
        if (velocity.y > .024)
            velocity.y = .024;
        position.y += velocity.y;
        bool hitCeiling = false;
        if (!g.returning && position.y < -.48) {
            position.y = -.48;
            if (velocity.y < 0) {
                velocity.y = 0;
                hitCeiling = true;
            }
        }
        const float foot = float(position.y + double(float(radius * .5f)));
        if (!hitCeiling && velocity.y > 0) {
            for (auto &p : g.tower.platforms)
                if (p.active) {
                    const float bottom = float(float(p.y + float(p.thickness * .5f)) +
                                               (g.returning ? p.thickness : 0.f));
                    if (p.y < foot && foot < bottom) {
                        if (double(bottom) > -.4 && !rotationLock && p.tiles[size_t(tile)] > 0 &&
                            !(p.tiles[size_t(tile)] == 204 && g.diveRequested)) {
                            platform = lastPlatform = p.level;
                            g.lowGravity = false;
                            ground = &p;
                            call(g.splash, 15, -.009f, 1.8f, position, 0x00ff40ffu, 0x008000ffu);
                            velocity.y = 0;
                            if (specialLandingSound && !g.demo)
                                call(g.sound, 37);
                        }
                        break; // First geometric candidate wins, even if its cell is a gap.
                    }
                }
        } else if (!hitCeiling && g.tower.smashableVisible) {
            // Upward head check uses the goat centre, not the foot test above.
            const float centre = float(position.y);
            for (auto &p : g.tower.platforms)
                if (p.active) {
                    const float height = float(p.thickness * p.thicknessScale);
                    const float top = float(float(p.y + float(height * -.5f)) + p.displacement);
                    const float bottom = float(top + height);
                    if (top < centre && centre<bottom &&double(bottom)> - .5) {
                        if (p.smashable)
                            call(g.smash, p, tile);
                        break;
                    }
                }
        }
    }
    if (platform >= 0) {
        if (platform == 1 && g.returning) {
            jumped = false;
            g.returning = false;
            g.summitBirdSent = false;
            g.tallyBegun = false;
            g.tallyComplete = true;
            g.atTop = false;
            g.flightTicks = g.flightWarning = g.flightFlash = 0;
            g.tower.fade = 0;
            ++g.laps;
            g.climbSpeed = g.targetClimbSpeed = .003f;
            g.lastTheme = -1;
            for (auto &p : g.tower.platforms) {
                p.reset();
                p.y = -.8f;
            }
            if (!g.demo)
                call(g.sound, 48);
            call(g.homecoming);
        }
        if (auto *support = g.tower.find(platform)) {
            ground = support;
            tile = int((double(heading) + .5) * 40.);
            if (ground->tiles[size_t(tile)] < 1) {
                platform = -1;
                jumped = true;
            } else {
                call(g.tread, *ground, tile);
                g.highestPlatform = std::max(g.highestPlatform, ground->level);
                g.sessionHighestPlatform = std::max(g.sessionHighestPlatform, ground->level);
                const float y = float(ground->y + float(radius * -.5f));
                position.y = double(y);
                standingY = y;
            }
        } else
            platform = -1;
        if (position.y < -.5 && g.returning) {
            for (int sample : {41, 38, 31, 32, 33, 27})
                call(g.stopSound, sample);
            g.gameOver = true;
            g.ticks = 0;
            if (!g.demo)
                call(g.sound, 42);
        }
        if (heldJump) {
            if (!ground)
                throw std::logic_error("Source jump without its supporting platform");
            velocity.y = -.005;
            platform = -1;
            jumped = true;
            static constexpr int sounds[] = {14, 21, 25, 29, 14, 35, 36, 39, 14};
            call(g.sound, sounds[size_t(ground->theme)]);
            if (ground->theme == 6)
                specialLandingSound = true;
            jumpTicks = 25 - int(pregnancy);
            if (g.tower.fade != 0)
                jumpTicks = 35 - int(pregnancy);
        }
    }
    if (position.y > .55) {
        if (g.flightTicks < 1) {
            for (int sample : {41, 38, 31, 32, 33, 27})
                call(g.stopSound, sample);
            g.gameOver = true;
            g.ticks = 0;
            if (!g.demo)
                call(g.sound, 42);
        } else {
            velocity.y = -.025;
            platform = -1;
            position.y -= .025;
        }
    }
    if (input.method != ClimbInput::Method::None &&
        (input.method != ClimbInput::Method::Accelerometer ||
         std::abs(float(input.horizontal * .5)) > .01)) {
        // Direct modern controllers use the source tilt response as an intent,
        // with no manufactured touch IDs, positions, or touch state mutations.
        const bool tilt = input.method == ClimbInput::Method::Accelerometer;
        const bool direct = tilt || input.method == ClimbInput::Method::DirectAxis;
        const float halfTilt = float(input.horizontal * .5);
        const double displacement = tilt     ? double(halfTilt)
                                    : direct ? -double(halfTilt)
                                             : input.horizontal;
        float drive = float(displacement * .19);
        if (!direct && input.dragHasHistory && sign(float(input.dragDelta)) != sign(drive) &&
            std::abs(input.dragDelta) > .006) {
            drive = -drive;
            touchRebaseRequested = true;
        }
        if (!direct && sign(drive) != sign(float(velocity.x)))
            drive = float(drive + drive);
        velocity.x += double(drive) * -.1;
        if (velocity.x > .01)
            velocity.x = .01;
        if (velocity.x < -.01)
            velocity.x = -.01;
        lastDrive = -drive;
    }
    const float prior = heading;
    heading = float(double(heading) + velocity.x);
    if (ground) {
        const float movement = float(ground->belt[size_t(tile)] * beltFactor);
        heading = float(heading + movement);
        walked = float(walked + std::fabs(movement));
    }
    const bool relative = input.method == ClimbInput::Method::RelativeTouch;
    if (heading < -.5f && velocity.x < 0) {
        heading = prior;
        velocity.x *= -.6;
        touchRebaseRequested = relative;
    }
    if (heading > .5f && velocity.x > 0) {
        heading = prior;
        velocity.x *= -.6;
        touchRebaseRequested = relative;
    }
    walked = float(walked + std::fabs(float(prior - heading)));
    velocity.x *= .85;
    scale.x = lastDrive < 0 ? 1.5 : -1.5;
    g.tower.angle = float(heading * .5f);
    const int stage = int(pregnancy);
    if (stage < 0 || stage > 3)
        throw std::logic_error("GoatUp pregnancy outside original frame tables");
    frame = platform >= 0 && (int(float(walked * 20.f)) & 1) ? original::ClosedFrames[size_t(stage)]
                                                             : original::OpenFrames[size_t(stage)];
    call(g.contacts);
    auto &h = history[historyCursor];
    h.platform = lastPlatform;
    h.grounded = platform >= 0;
    if (auto *p = g.tower.find(lastPlatform))
        h.platformOffset = float(position.y - double(p->y));
    h.standingY = standingY;
    h.position = position;
    h.scale = scale;
    h.angle = angle;
    h.frame = frame;
    historyCursor = (historyCursor + 1) & 1023;
    if (historyCount < 1024)
        ++historyCount;
}
void Climber::prepareDraw(bool diving) {
    const float radians = float(double(heading) * .5 * 3.141592654);
    const float cosine = std::cos(radians), sine = std::sin(radians);
    if (rotationLock == 0 && (!diving || velocity.y >= 0)) {
        angle = float(velocity.y * 50.);
        if (lastDrive >= 0)
            angle = -angle;
    } else
        angle = float(double(angle) + .4);
    position.x = double(sine) * .5;
    // Entity draw uses scale.x*cosine temporarily and restores the source scale.
    // Consumers apply this cosine to the prepared sprite, not persistent scale.
    projectedScale = {scale.x * double(cosine), scale.y};
}
} // namespace goatup
