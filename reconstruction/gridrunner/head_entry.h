// Narrow calibration slice of iOS head::run 0x4e9e8. Not a complete Head.
#pragma once
#include <cstdint>
namespace gr::reconstruction {
struct HeadEntry {
    int32_t age = 0, frame = 0, sprite = 20, baseSprite = 20, mode = 0, type = 2;
    double x = 0, y = 0, vx = 0, vy = 0, shipY = 0;
    float opacity = 0, angle = 0, speed = 0, timeScale = 1;
};
// This calibration excludes damage and contacts. Its fixture exporter verifies
// that restricted scene. Turns, age 30 contacts and slow time are excluded.
void stepHeadEntry(HeadEntry &head);
} // namespace gr::reconstruction
