// Raw iOS words, SHA256 c17852a2803d533fc73c03511c22946d4991f57328a73f21b5c592ac4334c394.
// Values retain the original binary32 encodings.
// Enemy columns: sprite, animation, colour, X motion, Y motion, scale,
// motion flags, behavior, buoyancy, hop period, damping, stretch, health,
// steering range (thousandths), then two unused words.
#pragma once
#include <array>
#include <cstdint>
namespace goatup2::actor_data {
inline constexpr std::array<int32_t, 16> Device0 = {27, 0, -1, 0, 0, 0, 0, 0,
                                                    0,  0, 0,  0, 0, 0, 0, 0}; // 0x7de18
inline constexpr std::array<int32_t, 16> Device1 = {-27, 0, -1, 0, 0, 0, 0, 0,
                                                    0,   0, 0,  0, 0, 0, 0, 0}; // 0x7de58
inline constexpr std::array<int32_t, 16> Device2 = {128, 0, -9371649, 0, 0, 0, 0, 0,
                                                    0,   0, 0,        0, 0, 0, 0, 0}; // 0x7de98
inline constexpr std::array<int32_t, 16> Device3 = {128, 0, 11010047, 0, 0, 0, 0, 0,
                                                    0,   0, 0,        0, 0, 0, 0, 0}; // 0x7ded8
inline constexpr std::array<int32_t, 16> Device4 = {181, 0, -1, 0, 0, 0, 0, 0,
                                                    0,   0, 0,  0, 0, 0, 0, 0}; // 0x7df18
inline constexpr std::array<int32_t, 16> Device5 = {181, 0, -1, 0, 0, 0, 0, 0,
                                                    0,   0, 0,  0, 0, 0, 0, 0}; // 0x7df58
inline constexpr std::array<int32_t, 16> Patroller0 = {45, 0,   -1,  1000, 0, 256, 19, 0, 100,
                                                       60, 990, 256, 0,    0, 0,   0}; // 0x7d118
inline constexpr std::array<int32_t, 16> Patroller1 = {
    46, 3, -65281, 250, 400, 256, 19, 0, 10, 60, 990, 256, 0, 0, 0, 0}; // 0x7d158
inline constexpr std::array<int32_t, 16> Patroller2 = {
    111, 3, -16711681, 700, 0, 256, 19, 0, -60, 60, 995, 256, 0, 0, 0, 0}; // 0x7d198
inline constexpr std::array<int32_t, 16> Patroller3 = {
    114, 3, -4194049, 500, 256, 256, 51, 0, -100, 120, 990, 256, 0, 0, 0, 0}; // 0x7d1d8
inline constexpr std::array<int32_t, 16> Patroller4 = {
    130, 4, -4194049, 300, 0, 400, 19, 0, -80, 20, 990, 500, 0, 0, 0, 0}; // 0x7d218
inline constexpr std::array<int32_t, 16> Patroller5 = {131, 4,   -1,  200, 400, 400, 51, 0, 100,
                                                       140, 980, 400, 0,   0,   0,   0}; // 0x7d258
inline constexpr std::array<int32_t, 16> Patroller6 = {137, 7,  3,   800, 0, 300, 19, 0,
                                                       80,  80, 980, 300, 0, 0,   0,  0}; // 0x7d298
inline constexpr std::array<int32_t, 16> Patroller7 = {141, 3,   -1,  200, 0, 406, 19, 0, 50,
                                                       200, 990, 400, 0,   0, 0,   0}; // 0x7d2d8
inline constexpr std::array<int32_t, 16> Patroller8 = {
    130, 4, -4194049, 100, 0, 400, 19, 0, -80, 20, 990, 600, 0, 0, 0, 0}; // 0x7d318
inline constexpr std::array<int32_t, 16> Patroller9 = {
    93, 9, -4194049, 500, 0, 400, 18, 0, -80, 20, 990, 400, 1000, 0, 0, 0}; // 0x7d358
inline constexpr std::array<int32_t, 16> Patroller10 = {
    46, 3, 16777215, 200, 0, 256, 19, 0, 10, 60, 990, 256, 0, 0, 0, 0}; // 0x7d398
inline constexpr std::array<int32_t, 16> Patroller11 = {
    114, 3, -4128769, 450, 0, 256, 51, 0, -100, 120, 990, 256, 0, 0, 0, 0}; // 0x7d3d8
inline constexpr std::array<int32_t, 16> Patroller12 = {182, 4,   -1,  200, 256, 256, 51, 0, -100,
                                                        20,  900, 256, 0,   0,   0,   0}; // 0x7d418
inline constexpr std::array<int32_t, 16> Patroller13 = {
    93, 9, -32513, 300, 0, 400, 18, 0, -80, 20, 990, 400, 1000, 0, 0, 0}; // 0x7d458
inline constexpr std::array<int32_t, 16> HorizontalFloater0 = {
    84, 1, 16711935, 200, 0, 420, 3, 2, 0, 60, 950, 350, 0, 0, 0, 0}; // 0x7d498
inline constexpr std::array<int32_t, 16> HorizontalFloater1 = {
    92, 4, -1, 300, 0, 512, 3, 0, -990, 1200, 999, 512, 0, 0, 0, 0}; // 0x7d4d8
inline constexpr std::array<int32_t, 16> HorizontalFloater2 = {
    176, 1, -1, 150, 0, 420, 3, 0, 0, 60, 950, 350, 0, 0, 0, 0}; // 0x7d518
inline constexpr std::array<int32_t, 16> HorizontalFloater3 = {
    176, 1, -8323073, 130, 0, 420, 3, 0, 0, 60, 950, 350, 1000, 0, 0, 0}; // 0x7d558
inline constexpr std::array<int32_t, 16> VerticalFloater0 = {
    86, 3, -16711681, 0, 100, 500, 3, 0, -100, 0, 1000, 256, 0, 0, 0, 0}; // 0x7d598
inline constexpr std::array<int32_t, 16> VerticalFloater1 = {89, 3, 2,   0, 130, 256, 3, 0, 0,
                                                             0,  0, 256, 0, 0,   0,   0}; // 0x7d5d8
inline constexpr std::array<int32_t, 16> VerticalFloater2 = {92, 4, -1,  0, 200, 500, 3, 0, 0,
                                                             0,  0, 500, 0, 0,   0,   0}; // 0x7d618
inline constexpr std::array<int32_t, 16> VerticalFloater3 = {94, 0,   -1,  0, 0, 768, 8, 0, -200,
                                                             0,  995, 768, 0, 0, 0,   0}; // 0x7d658
inline constexpr std::array<int32_t, 16> VerticalFloater4 = {
    144, 4, 0, 0, 80, 450, 3, 8, -200, 30, 990, 450, 0, 0, 0, 0}; // 0x7d698
inline constexpr std::array<int32_t, 16> VerticalFloater5 = {
    86, 3, -65281, 0, 170, 712, 3, 0, 100, 0, 1000, 512, 0, 0, 0, 0}; // 0x7d6d8
inline constexpr std::array<int32_t, 16> VerticalFloater6 = {89, 3, 3,   0, 160, 400, 3, 0, 0,
                                                             0,  0, 400, 0, 0,   0,   0}; // 0x7d718
inline constexpr std::array<int32_t, 16> VerticalFloater7 = {
    164, 0, 3, 0, 160, 700, 0, 14, 0, 0, 0, 700, 0, 0, 0, 0}; // 0x7d758
inline constexpr std::array<int32_t, 16> VerticalFloater8 = {
    176, 1, -1, 0, 100, 356, 3, 0, 0, 60, 950, 356, 0, 0, 0, 0}; // 0x7d798
inline constexpr std::array<int32_t, 16> VerticalFloater9 = {
    176, 1, -16711681, 0, 120, 356, 3, 0, 0, 60, 950, 356, 1000, 0, 0, 0}; // 0x7d7d8
inline constexpr std::array<int32_t, 16> RoamingFloater0 = {
    93, 5, -1, 50, 50, 968, 0, 0, -100, 50, 997, 768, 0, 0, 0, 0}; // 0x7d818
inline constexpr std::array<int32_t, 16> RoamingFloater1 = {
    92, 5, -1, 100, 150, 512, 0, 0, 100, 100, 995, 512, 0, 0, 0, 0}; // 0x7d858
inline constexpr std::array<int32_t, 16> RoamingFloater2 = {
    94, 0, -1, 100, 0, 768, 8, 0, 600, 0, 1000, 500, 0, 0, 0, 0}; // 0x7d898
inline constexpr std::array<int32_t, 16> RoamingFloater3 = {
    134, 0, 2, 0, 100, 300, 192, 5, 100, 20, 980, 300, 0, 0, 0, 0}; // 0x7d8d8
inline constexpr std::array<int32_t, 16> RoamingFloater4 = {
    135, 0, 1, 0, 200, 250, 192, 6, 200, 10, 950, 250, 0, 0, 0, 0}; // 0x7d918
inline constexpr std::array<int32_t, 16> RoamingFloater5 = {
    136, 0, 8454143, 80, 80, 512, 0, 7, 1010, 0, 1000, 512, 0, 0, 0, 0}; // 0x7d958
inline constexpr std::array<int32_t, 16> RoamingFloater6 = {
    167, 8, -1, 0, 50, 500, 0, 15, 100, 20, 980, 300, 1000, 2000, 0, 0}; // 0x7d998
inline constexpr std::array<int32_t, 16> RoamingFloater7 = {
    174, 0, -1, 0, 70, 500, 64, 15, 100, 20, 980, 300, 1000, 1000, 0, 0}; // 0x7d9d8
inline constexpr std::array<int32_t, 16> RoamingFloater8 = {
    130, 0, -1, 0, 90, 500, 0, 15, 100, 20, 980, 300, 10, 900, 0, 0}; // 0x7da18
inline constexpr std::array<int32_t, 16> RoamingFloater9 = {
    131, 0, -1, 0, 130, 500, 0, 15, 100, 20, 980, 300, 10, 800, 0, 0}; // 0x7da58
inline constexpr std::array<int32_t, 16> RoamingFloater10 = {
    167, 8, -65281, 0, 80, 500, 0, 15, 100, 20, 980, 320, 1000, 400, 0, 0}; // 0x7da98
inline constexpr std::array<int32_t, 16> RoamingFloater11 = {
    167, 8, 16777215, 0, 100, 550, 0, 15, 100, 20, 980, 350, 0, 200, 0, 0}; // 0x7dad8
inline constexpr std::array<int32_t, 16> Machine0 = {95, 2, -1,  0,   0, 768, 0, 1,
                                                     50, 0, 990, 768, 0, 0,   0, 0}; // 0x7db18
inline constexpr std::array<int32_t, 16> Machine1 = {105, -1, -1, 0,   0, 256, 0, 3,
                                                     0,   0,  0,  256, 0, 0,   0, 0}; // 0x7db58
inline constexpr std::array<int32_t, 16> Machine2 = {109, -1, -1,  0,    0, 1024, 0, 10,
                                                     -30, 0,  990, 1024, 0, 0,    0, 0}; // 0x7db98
inline constexpr std::array<int32_t, 16> Machine3 = {132, -1, -1, 0,   0, 400, 0, 4,
                                                     0,   0,  0,  400, 0, 0,   0, 0}; // 0x7dbd8
inline constexpr std::array<int32_t, 16> Machine4 = {156, -1, -1, 0,   0, 300, 0, 9,
                                                     0,   0,  0,  300, 0, 0,   0, 0}; // 0x7dc18
inline constexpr std::array<int32_t, 16> Machine5 = {156, -1, -1, 100, 0, 400, 0, 9,
                                                     0,   0,  0,  400, 0, 0,   0, 0}; // 0x7dc58
inline constexpr std::array<int32_t, 16> Machine6 = {157, -1, -1, 0,   0, 750, 0, 11,
                                                     0,   0,  0,  350, 0, 0,   0, 0}; // 0x7dc98
inline constexpr std::array<int32_t, 16> Machine7 = {157, -1, -1, 150, 0, 750, 0, 11,
                                                     0,   0,  0,  350, 0, 0,   0, 0}; // 0x7dcd8
inline constexpr std::array<int32_t, 16> Machine8 = {158, -1, -1,   0,   0, 450, 0, 12,
                                                     100, 0,  1000, 300, 0, 0,   0, 0}; // 0x7dd18
inline constexpr std::array<int32_t, 16> Machine9 = {161, -1, -1,   0,   200, 700, 0, 13,
                                                     100, 0,  1000, 512, 0,   0,   0, 0}; // 0x7dd58
inline constexpr std::array<int32_t, 16> Machine10 = {161, -1, -1,   200, 0, 700, 0, 13,
                                                      100, 0,  1000, 512, 0, 0,   0, 0}; // 0x7dd98
inline constexpr std::array<int32_t, 16> Machine11 = {109, -1, -1,  0,    0, 1024, 0, 16,
                                                      -30, 0,  990, 1024, 0, 0,    0, 0}; // 0x7ddd8
inline constexpr std::array<uint32_t, 4> SplitColours = {0xffff00ffU, 0x80ffffU, 0x40ff40ffU,
                                                         0xff8000ffU}; // 0x77730
inline const std::array<int32_t, 16> *find(int kind, int variant) {
    switch (kind) {
    case 4:
        switch (variant) {
        case 0:
            return &Patroller0;
        case 1:
            return &Patroller1;
        case 2:
            return &Patroller2;
        case 3:
            return &Patroller3;
        case 4:
            return &Patroller4;
        case 5:
            return &Patroller5;
        case 6:
            return &Patroller6;
        case 7:
            return &Patroller7;
        case 8:
            return &Patroller8;
        case 9:
            return &Patroller9;
        case 10:
            return &Patroller10;
        case 11:
            return &Patroller11;
        case 12:
            return &Patroller12;
        case 13:
            return &Patroller13;
        }
        break;
    case 5:
        switch (variant) {
        case 0:
            return &HorizontalFloater0;
        case 1:
            return &HorizontalFloater1;
        case 2:
            return &HorizontalFloater2;
        case 3:
            return &HorizontalFloater3;
        }
        break;
    case 6:
        switch (variant) {
        case 0:
            return &VerticalFloater0;
        case 1:
            return &VerticalFloater1;
        case 2:
            return &VerticalFloater2;
        case 3:
            return &VerticalFloater3;
        case 4:
            return &VerticalFloater4;
        case 5:
            return &VerticalFloater5;
        case 6:
            return &VerticalFloater6;
        case 7:
            return &VerticalFloater7;
        case 8:
            return &VerticalFloater8;
        case 9:
            return &VerticalFloater9;
        }
        break;
    case 7:
        switch (variant) {
        case 0:
            return &RoamingFloater0;
        case 1:
            return &RoamingFloater1;
        case 2:
            return &RoamingFloater2;
        case 3:
            return &RoamingFloater3;
        case 4:
            return &RoamingFloater4;
        case 5:
            return &RoamingFloater5;
        case 6:
            return &RoamingFloater6;
        case 7:
            return &RoamingFloater7;
        case 8:
            return &RoamingFloater8;
        case 9:
            return &RoamingFloater9;
        case 10:
            return &RoamingFloater10;
        case 11:
            return &RoamingFloater11;
        }
        break;
    case 8:
        switch (variant) {
        case 0:
            return &Machine0;
        case 1:
            return &Machine1;
        case 2:
            return &Machine2;
        case 3:
            return &Machine3;
        case 4:
            return &Machine4;
        case 5:
            return &Machine5;
        case 6:
            return &Machine6;
        case 7:
            return &Machine7;
        case 8:
            return &Machine8;
        case 9:
            return &Machine9;
        case 10:
            return &Machine10;
        case 11:
            return &Machine11;
        }
        break;
    }
    return nullptr;
}
} // namespace goatup2::actor_data
