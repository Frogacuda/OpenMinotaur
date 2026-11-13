#pragma once
#include "native_entity.h"
#include "native_random.h"
namespace gr::reconstruction::ios {
class Star final : public Entity {
  public:
    void initialize(RandomStream &); // inline iOS theGame title reset 0x3f610
    void run() override;             // iOS 0x4a4b8
    bool visible() const {
        return (uint32_t(age) & 31) < 15;
    } // custom draw 0x4a500
};
class Starfield {
  public:
    explicit Starfield(RandomStream &stream) {
        reset(stream);
    }
    void reset(RandomStream &);
    void tick() {
        for (auto &s : stars_)
            s.run();
    }
    const std::array<Star, 100> &stars() const {
        return stars_;
    }
    std::array<Star, 100> &stars() {
        return stars_;
    }

  private:
    std::array<Star, 100> stars_{};
};
} // namespace gr::reconstruction::ios
