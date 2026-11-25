#pragma once
#include "core_api.h"
#include "neon/resource_catalog.h"
#include "original_tables.h"
#include "records.h"
#include "world.h"
#include <algorithm>
#include <cmath>

namespace fiveaday {
inline constexpr auto WidgetFruitTotals = [] {
    std::array<int, original::Levels.size()> totals{};
    for (size_t stage = 0; stage < totals.size(); ++stage) {
        const char *script = original::Levels[stage];
        while (*script != ',')
            ++script;
        for (++script; *script != ':'; ++script) {
            // Each of these commands spawns a group of five ordinary fruit.
            if (*script == 'S' || *script == 'L' || *script == 'T')
                totals[stage] += 5;
        }
    }
    return totals;
}();

inline mino::PresentationState widgetSnapshot(const World &world, const Records &records,
                                              const neon::ResourceCatalog &resources,
                                              uint64_t session, bool paused) {
    if (world.demo || world.gameOver)
        return {};
    const size_t stage = size_t(std::max(world.level + 1, 0));
    const size_t day = stage / 5;
    const auto *best =
        day < records.days.size() && records.days[day].lives > 0 ? &records.days[day] : nullptr;

    int trail = 0;
    for (auto *entity = world.fruits; entity; entity = entity->next) {
        const auto *fruit = dynamic_cast<const Fruit *>(entity);
        if (fruit && !fruit->dead && !fruit->minotaur && fruit->state == Fruit::State::Collected)
            ++trail;
    }
    bool forming = false;
    if (!world.bossDefeated) {
        for (auto *entity = world.enemies; entity; entity = entity->next) {
            const auto *attacker = dynamic_cast<const Attacker *>(entity);
            if (attacker && attacker->formationMember && !attacker->dead && !attacker->deathTicks) {
                forming = true;
                break;
            }
        }
    }
    mino::PresentationState state{
        true,
        paused,
        session,
        {{"highScore", int64_t(records.highScore)},
         {"dayBest", best ? int64_t(best->score) : -1},
         {"dayLives", best ? int64_t(best->lives) : -1},
         {"trail", int64_t(trail)},
         {"trailMax", int64_t(WidgetFruitTotals[stage % WidgetFruitTotals.size()])},
         {"lifetimeMinotaurs", int64_t(records.minotaurs) + world.powerFruitTotal},
         {"lifetimeFruit", int64_t(records.fruit) + world.ordinaryFruitTotal},
         {"firepower", std::clamp(double(world.ship.weaponEnergy), 0., 1.)},
         {"formationActive", forming},
         {"formationKills", int64_t(world.formationKills)},
         {"nextBonus", int64_t(world.formationKillPoints())},
         {"doublePoints", world.formationBonusMultiplier == 2},
         {"scorePenalty",
          int64_t(std::lround(100 * (1 - std::clamp(double(world.scoreMultiplier), 0., 1.))))},
         {"minotaursLost", int64_t(world.minotaursLost)},
         {"damageTaken", int64_t(world.damageTaken)}},
        {},
        {}};

    // The HUD list puts the newest fruit first. Read only fruit that has landed.
    std::array<const neon::SpriteEntry *, 5> fruitSet{};
    size_t count = 0;
    const size_t landed = size_t(std::clamp(world.fruitCount, 0, int(fruitSet.size())));
    for (auto *entity = world.hud; entity && count < landed; entity = entity->next) {
        const auto *fruit = dynamic_cast<const ProgressFruit *>(entity);
        if (fruit && !fruit->dead && (fruit->state == 1 || fruit->state == 2)) {
            const auto *sprite = resources.sprite(fruit->frame + 2);
            if (sprite && sprite->page == 0)
                fruitSet[count++] = sprite;
        }
    }
    state.values["fruitSetCount"] = int64_t(count);
    for (size_t slot = 0; slot < count; ++slot) {
        const auto &sprite = *fruitSet[count - slot - 1];
        const auto key = "fruit" + std::to_string(slot);
        state.values[key + "X"] = int64_t(sprite.x);
        state.values[key + "Y"] = int64_t(sprite.y);
        state.values[key + "W"] = int64_t(sprite.w);
        state.values[key + "H"] = int64_t(sprite.h);
    }
    return state;
}
} // namespace fiveaday
