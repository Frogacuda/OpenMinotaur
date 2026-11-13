#include "head_entry.h"
#ifdef GRR_NATIVE_HEAD
#include "native_head.h"
#endif
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace gr::reconstruction;
#ifdef GRR_NATIVE_HEAD
struct ReplayWorld : ios::HeadWorld {
    ios::Vec2 ship;
    int touches = 0, sweeps = 0;
    ReplayWorld() {
        shipPosition = &ship;
    }
    void touchEnemyList(ios::Head &) override {
        touches++;
    }
    void checkSweep(ios::Head &) override {
        sweeps++;
    }
};
struct NativeReplay {
    ReplayWorld world;
    ios::Head head{world};
    void start(const HeadEntry &e) {
        head.age = e.age;
        head.frame = uint32_t(e.frame);
        head.sprite = e.sprite;
        head.baseSprite = e.baseSprite;
        head.type = e.type;
        head.mode = e.mode;
        head.position = {e.x, e.y};
        head.velocity = {e.vx, e.vy};
        head.opacity = e.opacity;
        head.movementAngle = e.angle;
        head.speed = e.speed;
        head.radius = head.baseRadius = .03f;
        world.ac.timeScale = e.timeScale;
    }
    HeadEntry step(const HeadEntry &input) {
        world.ship = {0, input.shipY};
        head.run();
        HeadEntry e = input;
        e.age = head.age;
        e.frame = int32_t(head.frame);
        e.sprite = head.sprite;
        e.x = head.position.x;
        e.y = head.position.y;
        e.vx = head.velocity.x;
        e.vy = head.velocity.y;
        e.opacity = head.opacity;
        e.angle = head.movementAngle;
        e.speed = head.speed;
        e.mode = head.mode;
        e.type = head.type;
        return e;
    }
};
#endif
template <class T> T fromBits(const std::string &s) {
    const uint64_t bits = std::stoull(s, nullptr, 16);
    T value;
    if constexpr (sizeof(T) == 4) {
        const uint32_t b = uint32_t(bits);
        std::memcpy(&value, &b, 4);
    } else
        std::memcpy(&value, &bits, 8);
    return value;
}
template <class T> bool same(T a, T b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}
void equal(const HeadEntry &a, const HeadEntry &b) {
#define CHECK(field)                                                                               \
    if (!same(a.field, b.field))                                                                   \
    throw std::runtime_error("First divergence at age " + std::to_string(b.age) + ": " #field)
    CHECK(age);
    CHECK(frame);
    CHECK(sprite);
    CHECK(x);
    CHECK(y);
    CHECK(vx);
    CHECK(vy);
    CHECK(opacity);
    CHECK(angle);
    CHECK(speed);
    CHECK(timeScale);
#undef CHECK
}
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Expected observed original fixture");
        std::ifstream input(argv[1]);
        if (!input)
            throw std::runtime_error("Cannot read fixture");
        std::string line;
        HeadEntry current;
        int rows = 0;
#ifdef GRR_NATIVE_HEAD
        NativeReplay native;
#endif
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            HeadEntry expected;
            std::string x, y, vx, vy, alpha, angle, speed, scale, shipY;
            std::istringstream fields(line);
            if (!(fields >> expected.age >> expected.frame >> expected.sprite >> x >> y >> vx >>
                  vy >> alpha >> angle >> speed >> scale >> shipY))
                throw std::runtime_error("Malformed fixture");
            expected.x = fromBits<double>(x);
            expected.y = fromBits<double>(y);
            expected.vx = fromBits<double>(vx);
            expected.vy = fromBits<double>(vy);
            expected.opacity = fromBits<float>(alpha);
            expected.angle = fromBits<float>(angle);
            expected.speed = fromBits<float>(speed);
            expected.timeScale = fromBits<float>(scale);
            expected.shipY = fromBits<double>(shipY);
            if (!rows) {
                current = expected;
#ifdef GRR_NATIVE_HEAD
                native.start(expected);
#endif
            } else {
#ifdef GRR_NATIVE_HEAD
                current = native.step(current);
#else
                stepHeadEntry(current);
#endif
                equal(current, expected);
                current.shipY = expected.shipY;
            }
            rows++;
        }
        if (rows < 2)
            throw std::runtime_error("Insufficient original evidence");
#ifdef GRR_NATIVE_HEAD
        if (native.world.touches != rows - 1 || native.world.sweeps != rows - 1)
            throw std::runtime_error("Missing original callback boundaries");
#endif
        std::cout << rows - 1
                  << " original-observed head-entry transitions matched exactly (limited captured "
                     "fields)\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
