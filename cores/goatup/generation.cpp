#include "generation.h"
#include "original_tables.h"
#include <algorithm>
#include <stdexcept>

namespace goatup {
int PlatformGenerator::slot(const Platform &p) const {
    return p.selectBirthSlot([&](double a, double b) { return random.range(a, b); });
}
void PlatformGenerator::birth(Platform &p, PlatformBirth::Kind kind, int frame, int tile, int bonus,
                              double speed, float verticalOffset) {
    PlatformBirth b{kind, &p, float(float(tile) / 40.f - .5f), frame, bonus, speed, verticalOffset};
    send(b);
}
void PlatformGenerator::send(const PlatformBirth &b) {
    if (!create)
        throw std::logic_error("GoatUp constructors not connected");
    create(b);
}
void PlatformGenerator::generate(Platform &p) {
    if (p.level < 1)
        throw std::logic_error("GoatUp platform levels must be positive");
    const float lap = float(state.laps);
    if (state.returning)
        state.returnSpeed = float(double(state.returnSpeed) * (double(lap) * .008 + 1.02));
    state.highestGenerated = std::max(state.highestGenerated, p.level);
    const int zone = (p.level >> 6) % 9;
    const bool changed = state.lastTheme != zone;
    if (changed) {
        if (!enterZone)
            throw std::logic_error("GoatUp zone notification is not connected");
        state.lastTheme = zone;
        enterZone();
        if (zone > 7)
            state.atTop = true;
        else if (!state.returning)
            state.targetClimbSpeed = float(double(float(float(zone) / 6.f)) * .001 + .003);
        else if (zone != 7)
            state.returnSpeed = float(double(float(float(6 - zone) / 6.f)) * .0005 + .003);
    }
    state.targetClimbSpeed = float(double(state.targetClimbSpeed) * (double(lap) * .008 + 1.015));
    p.layout(state.returning, &random, [&](int frame, float angle, int tile) {
        PlatformBirth cap{PlatformBirth::Kind::ConveyorCap, &p, angle, frame};
        cap.tile = tile;
        send(cap);
    });
    if ((p.level & 7) == 0) {
        // All these draws occur even though Green Grass normally selects a
        // Billy rather than a powerup. Removing apparently cosmetic choices
        // would change the later actor/bonus stream.
        const int at = slot(p);
        bool power = (random.next() & 3) == 0;
        int pickup = 0;
        if (power) {
            if (state.climbSpeed <= state.pickupSpeedThreshold || (random.next() & 3) != 0)
                pickup = int(random.next() & 1) + 83;
            else
                pickup = 82;
        }
        constexpr int zonePickup[] = {83, 83, 84, 82, 82, 82, 83, 83, 83};
        if ((random.next() & 1) != 0 && power)
            pickup = zonePickup[zone & 3];
        if (zone == 0)
            power = false; // Native override AFTER the above draws.
        if (changed) {
            power = true;
            pickup = 82;
        }
        if (!state.atTop) {
            if (power) {
                if (at < 0)
                    throw std::logic_error("Source pickup has no valid pair");
                p.spawnable[size_t(at >> 1)] = false;
                birth(p, PlatformBirth::Kind::Pickup, pickup, at);
            } else if (!state.pregnant) {
                if (at < 0)
                    throw std::logic_error("Source Billy has no valid pair");
                p.spawnable[size_t(at >> 1)] = false;
                birth(p, PlatformBirth::Kind::BillyGoat, 6, at);
            }
        }
    }
    if (!state.returning) {
        const bool sheep =
            zone == 0 &&
            (state.laps == 0 ? (((p.level >> 4) & 3) == 3 && (p.level & 7) == 5) : !(p.level & 1));
        if (sheep)
            for (int n = 0; n < (state.laps >> 1) + 1; ++n) {
                const int at = slot(p);
                if (at >= 0)
                    birth(p, PlatformBirth::Kind::Sheep, 596, at, -1,
                          random.range(.0005, .001) + .001);
            }
        if (zone == 1) {
            // A site draw occurs even on levels which create no 2049 actor.
            const int at = slot(p);
            if (at >= 0) {
                if ((p.level & 3) == 2 && (p.level & 4)) {
                    const float offset = float(random.range(-.02, -.08));
                    const int frame = int(random.next() & 3) + 89;
                    birth(p, PlatformBirth::Kind::Goody2049, frame, at, -1, 0, offset);
                } else if ((p.level & 3) == 1 || (p.level & 3) == 3) {
                    for (int n = 0; n < state.laps + 1; ++n) {
                        // The outer draw gates birth, but each baddy uses its
                        // own second site draw, without reserving either pair.
                        const int baddyAt = slot(p);
                        birth(p, PlatformBirth::Kind::Baddy2049, 85, baddyAt, -1, .001);
                    }
                }
            }
        }
        if (zone == 2 && (p.level & 2))
            for (int n = 0; n < state.laps + 1; ++n) {
                const int at = slot(p);
                double speed = .0025;
                if (random.next() & 1)
                    speed = -speed;
                birth(p, PlatformBirth::Kind::MinerWilly, 95, at, -1, speed);
            }
        if (zone == 3 && ((p.level >> 4) & 3) != 0)
            for (int n = 0; n < state.laps + 1; ++n) {
                const int at = (random.next() & 1) ? 2 : 38;
                double speed = .002 * double(float(float((p.level >> 4) & 3) * .125f + 1.f));
                if (random.next() & 1)
                    speed = -speed;
                birth(p, PlatformBirth::Kind::CcDude, 102, at, -1, speed);
            }
        if (zone == 4) {
            if (p.level & 3)
                for (int n = 0; n < state.laps + 1; ++n) {
                    const int at = slot(p);
                    if (at >= 0) {
                        const double speed = (random.next() & 1) ? -.0035 : .0035;
                        birth(p, PlatformBirth::Kind::CcCar, 179, at, -1, speed);
                    }
                }
            if (!(p.level & 7) && state.laps <= 0) {
                const int at = slot(p);
                if (at >= 0) {
                    PlatformBirth cat{PlatformBirth::Kind::CcCat, &p, float(float(at) / 40.f - .5f),
                                      180};
                    cat.facing = (random.next() & 1) ? -1.2 : 1.2;
                    send(cat);
                }
            }
        }
        if (zone == 5 && (p.level & 3))
            for (int n = 0; n < state.laps + 1 + (((p.level >> 4) & 3) >> 1); ++n) {
                const int at = slot(p);
                if (at >= 0) {
                    PlatformBirth pie{PlatformBirth::Kind::PinkiePie, &p,
                                      float(float(at) / 40.f - .5f), 187};
                    pie.facing = (random.next() & 1) ? -1 : 1;
                    send(pie);
                }
            }
        if (zone == 6)
            for (int n = 0; n < (state.laps + 1) * ((p.level >> 4) & 3); ++n) {
                const int at = slot(p);
                if (at >= 0) {
                    const double speed = (random.next() & 1) ? -.003 : .003;
                    birth(p, PlatformBirth::Kind::RollingSkull, 190, at, -1, speed);
                }
            }
        if (zone == 7)
            for (int n = 0; n < ((p.level >> 4) & 3) + 1; ++n) {
                const int at = slot(p);
                if (at >= 0)
                    birth(p, PlatformBirth::Kind::Egg, 201, at);
            }
    }
    // Bonuses do not reserve the chosen pair. Overlapping births are source
    // behavior; do not sample without replacement to make a tidier platform.
    if (zone <= 7)
        for (int n = 0; n < 3; ++n) {
            const int at = slot(p);
            if (at >= 0) {
                const int index = (p.level - 1) % 512;
                birth(p, PlatformBirth::Kind::Bonus, original::Bonuses[size_t(index)], at, index);
            }
        }
}
} // namespace goatup
