#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <deque>
#include <stdexcept>
namespace neon::pc {
// Ordered atlas ranges share one batch. Insertions consume emission slots while
// leaving both ordinary pool cursors and the rare cadence unchanged.
struct SpriteCycle {
    int baseFirst = 0, baseCount = 1, rareFirst = 1, rareCount = 0, rareEvery = 10;
};
struct SpriteParticleStyle {
    float speed = 1;
    int colourSource = -1;
};
struct SpriteEmission {
    int frame = 0;
    SpriteParticleStyle style;
};
class SpriteEmissionSequence {
  public:
    void configure(SpriteCycle cycle) {
        if (cycle.baseFirst < 0 || cycle.baseCount < 1 || cycle.baseFirst + cycle.baseCount > 256 ||
            cycle.rareFirst < 0 || cycle.rareCount < 0 || cycle.rareFirst + cycle.rareCount > 256 ||
            cycle.rareEvery < 1 || cycle.rareEvery > 1024)
            throw std::runtime_error("Invalid sprite emission cycle");
        cycle_ = cycle;
    }
    void reset() {
        base_ = rare_ = ordinary_ = 0;
        pending_.clear();
        queued_ = 0;
    }
    bool insert(int frame, unsigned count, float speed = 1, int colourSource = -1) {
        if (frame < 0 || frame >= 256 || count == 0 || count > 4096 - queued_ ||
            !std::isfinite(speed) || speed < .25f || speed > 8 || colourSource < -1 ||
            colourSource > 255)
            return false;
        pending_.push_back({frame, count, {speed, colourSource}});
        queued_ += count;
        return true;
    }
    SpriteEmission nextEmission(bool allowInsert = true) {
        if (allowInsert && !pending_.empty()) {
            auto &run = pending_.front();
            SpriteEmission result{run.frame, run.style};
            --queued_;
            if (--run.count == 0)
                pending_.pop_front();
            return result;
        }
        bool rare = ++ordinary_ % unsigned(cycle_.rareEvery) == 0 && cycle_.rareCount > 0;
        if (rare)
            return {cycle_.rareFirst + int(rare_++ % unsigned(cycle_.rareCount)), {}};
        return {cycle_.baseFirst + int(base_++ % unsigned(cycle_.baseCount)), {}};
    }
    int next(bool allowInsert = true) {
        return nextEmission(allowInsert).frame;
    }

  private:
    SpriteCycle cycle_;
    uint64_t base_ = 0, rare_ = 0, ordinary_ = 0;
    unsigned queued_ = 0;
    struct Run {
        int frame;
        unsigned count;
        SpriteParticleStyle style;
    };
    std::deque<Run> pending_;
};
} // namespace neon::pc
