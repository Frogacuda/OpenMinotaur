#include "native_star.h"
namespace gr::reconstruction::ios {
void Star::initialize(RandomStream &random) {
    constexpr uint32_t colours[] = {0xffff00ff, 0xff0000ff, 0x0000ffff, 0xff00ffff}; // iOS 0x66710
    tint.unpack(colours[random.next() & 3]);
    position = {random.range(-.5, .5), random.range(-.6, .6)};
    sprite = 2;
    radius = .005f;
    age = int32_t(random.next() & 31);
}
void Star::run() {
    position.y += .004;
    if (position.y > .6)
        position.y = -.6;
    age = add32(age, 1);
}
void Starfield::reset(RandomStream &random) {
    // addEntity prepends each of the 100 new stars. Preserve that draw/update
    // order as well as the colour,x,y,age random schedule for every constructor.
    for (size_t i = stars_.size(); i > 0; --i) {
        stars_[i - 1] = Star{};
        stars_[i - 1].initialize(random);
    }
}
} // namespace gr::reconstruction::ios
