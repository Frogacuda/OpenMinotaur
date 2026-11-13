#include "animation.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__func__) + ": " + #x);                           \
    } while (false)
using namespace goatup2;
namespace {
template <class B, class V> B bits(V v) {
    B b;
    static_assert(sizeof(B) == sizeof(V));
    std::memcpy(&b, &v, sizeof b);
    return b;
}
struct Actor {
    neon::vec2d travelled{0, 0}, weights{1, 0};
    int32_t freeze = 0, mode = 1, base = 100, sprite = 99;
    double scale = -2, saved = 2, local = .125;
    float heading = .125f;
    uint8_t frame = 0x40;
    void run() {
        updateMovementAnimation(
            {freeze, travelled, weights, mode, base, saved, local, frame, scale, heading, sprite});
    }
};
void fixture(const char *path) {
    std::ifstream in(path);
    CHECK(in.good());
    std::string token;
    unsigned count = 0;
    while (in >> token) {
        if (token[0] == '#') {
            std::string rest;
            std::getline(in, rest);
            continue;
        }
        Actor a;
        a.mode = std::stoi(token);
        std::string distance, heading, scale;
        int expected;
        in >> distance >> expected >> heading >> scale;
        CHECK(in.good());
        a.travelled.x = bits<double>(std::stoull(distance, nullptr, 16));
        a.run();
        CHECK(a.sprite == expected && bits<uint64_t>(a.scale) == std::stoull(scale, nullptr, 16));
        const auto expectedHeading = bits<float>(uint32_t(std::stoul(heading, nullptr, 16)));
        if (a.mode == 4)
            CHECK(std::abs(a.heading - expectedHeading) < 2e-7f); // host sinf tolerance
        else
            CHECK(a.heading == expectedHeading);
        CHECK(a.travelled.x == bits<double>(std::stoull(distance, nullptr, 16)) &&
              a.travelled.y == 0);
        ++count;
    }
    CHECK(count == 117);
}
void branches() {
    for (int mode :
         {std::numeric_limits<int32_t>::min(), -1, 0, 10, std::numeric_limits<int32_t>::max()}) {
        Actor a;
        a.mode = mode;
        a.run();
        CHECK(a.sprite == 99 && a.heading == .125f && a.scale == -2);
    }
    for (int mode = 1; mode <= 9; ++mode) {
        Actor a;
        a.mode = mode;
        a.freeze = 1;
        a.run();
        CHECK(a.sprite == 99 && a.heading == .125f && a.scale == -2);
    }
    Actor norm;
    norm.mode = 5;
    norm.travelled = {3, 4};
    norm.weights = {2, -2};
    norm.run();
    CHECK(norm.heading == 4 && norm.sprite == 100); // sqrt(36+64)*.4, not sum/axis-only travel
    Actor scale;
    scale.mode = 2;
    scale.frame = 0xbf;
    scale.saved = -3;
    scale.run();
    CHECK(scale.scale == 3 && scale.sprite == 99 && scale.heading == .125f);
    scale.frame = 0xff;
    scale.run();
    CHECK(scale.scale == -3);
    Actor overflow;
    overflow.base = std::numeric_limits<int32_t>::max();
    overflow.travelled.x = 1;
    overflow.run();
    CHECK(overflow.sprite == std::numeric_limits<int32_t>::min());
    Actor negativeFreeze;
    negativeFreeze.freeze = -1;
    negativeFreeze.run();
    CHECK(negativeFreeze.sprite == 100);
}
} // namespace
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        fixture(argv[1]);
        branches();
        std::cout << "GoatUp 2 movement-animation source-rule checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
