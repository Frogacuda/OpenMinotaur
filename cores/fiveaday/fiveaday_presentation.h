#pragma once
#include "world.h"
#include <algorithm>
#include <vector>
namespace fiveaday {
// Read-only transition observations; counters belong to presentation, not saves.
class PresentationObserver {
  public:
    void reset() {
        *this = {};
    }
    void beforeTick(const World &world) {
        previousBoss_ = world.bossDefeated;
        collected_.clear();
        for (auto *entity = world.fruits; entity; entity = entity->next)
            if (auto *fruit = dynamic_cast<const Fruit *>(entity);
                fruit && fruit->minotaur && fruit->state == Fruit::State::Collected)
                collected_.push_back(fruit);
    }
    void afterTick(const World &world) {
        if (world.demo || world.paused)
            return;
        if (!previousBoss_ && world.bossDefeated)
            ++completed_;
        trail_ = 0;
        for (auto *entity = world.fruits; entity; entity = entity->next)
            if (auto *fruit = dynamic_cast<const Fruit *>(entity);
                fruit && !fruit->dead && fruit->state == Fruit::State::Collected) {
                ++trail_;
                if (fruit->minotaur &&
                    std::find(collected_.begin(), collected_.end(), fruit) == collected_.end())
                    ++rescued_;
            }
    }
    uint64_t completed() const {
        return completed_;
    }
    uint64_t rescued() const {
        return rescued_;
    }
    int trail() const {
        return trail_;
    }

  private:
    std::vector<const Fruit *> collected_;
    uint64_t completed_ = 0, rescued_ = 0;
    int trail_ = 0;
    bool previousBoss_ = false;
};
} // namespace fiveaday
