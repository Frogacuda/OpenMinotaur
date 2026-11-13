#include "native_entity.h"
#include "neon/math.h"
#include <cmath>
#include <iostream>
int main() {
    gr::reconstruction::ios::Vec2 source{-0.0, 1};
    source.rotateBy(0);
    neon::vec2d shared{-0.0, 1};
    shared.rotateBy(0);
    if (!std::signbit(source.x) || std::signbit(shared.x))
        return 1;
    std::cout << "Confirmed: iOS zero-angle early return preserves signed zero; shared port math "
                 "does not\n";
}
