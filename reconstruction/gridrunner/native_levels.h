// Recovered from GridrunnerAndroid.so; all scripts/speed/separation/turn tables
// verified byte for byte in the supplied iOS ARMv7 executable.
#pragma once
#include <cstdint>
namespace gr::reconstruction::ios::levelData {
inline constexpr double speeds[] = {0.0040, 0.0045, 0.0050, 0.0055, 0.0060, 0.0065, 0.0070, 0.0075,
                                    0.0080, 0.0085, 0.0090, 0.0095, 0.0100, 0.0105, 0.0110, 0.0115};
inline constexpr int separations[] = {10, 9, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4};
inline constexpr int turnTicks[] = {16, 15, 15, 14, 14, 13, 13, 13, 12, 11, 10, 10, 9, 9, 8, 8};
inline constexpr uint32_t headColours[] = {0xcfff05ffu, 0xff8f05ffu, 0xffe035ffu, 0xd74bffffu,
                                           0xc309ffffu, 0x404040ffu, 0xd929d3ffu, 0x2671ffffu,
                                           0x00ffffffu, 0xc1ff15ffu, 0xff8000ffu, 0xffc015ffu,
                                           0x4000c0ffu, 0x6010c0ffu, 0x0b80ffffu, 0x00ff00ffu};
inline constexpr uint32_t bodyColours[] = {0x16878fffu, 0x81ff25ffu, 0xff5a25ffu, 0xff6245ffu,
                                           0x100090ffu, 0x202020ffu, 0x1b5ed7ffu, 0x29ff29ffu,
                                           0xffff00ffu, 0xffcf00ffu, 0xffff05ffu, 0xff10ffffu,
                                           0xff00ffffu, 0xe020f0ffu, 0xff00ffffu, 0x06c1ffffu};
inline constexpr uint32_t gridColours[] = {0x7a0b08ffu, 0x520a7affu, 0x0a1a70ffu, 0x0a4a08ffu,
                                           0x101010ffu, 0x601030ffu, 0x000040ffu, 0x861b00ffu};
inline constexpr const char *waves[] = {
    "9999=S50=P10=I270=Z2=V7t:6(7t8l500w)7t4=V12lq",
    "9999=S50=P10=I210=Z3=V7t:5(7t10l500wV+1=V)2=V4t2l7t7(4l200wV+1=V)10wq",
    ("9999=S40=P10=I190=Z3=V7t1=X5=Y4=L:4(LlY+2=YL+4=L300w)4t5l7t27=X5=Y4=L3=V4(LlY+2=YL+4=LV+1="
     "V300w)4t5l10wq"),
    "120=Z5=V7t:9(7t12l200w4t5l300w)7t6=V24lq",
    "9999=S30=P10=I150=Z4=V7t:2(7(6t2l200w)7t5=V12lV+2=V4t3l)q",
    "0=F9999=S30=P10=I180=Z4=V:8(7t5l3t0l500w7t5l4t4l500w)q",
    "5=W3=S50=P8=I230=Z0=V7t:5(5t0l50w6t8l500w3l500w)7t2=V12lq",
    "1=V:3(26=Y4(8t2=X4l26=X4l500wY-4=YV+1=V)3t0l)10wq",
    "220=Z0=B:4=V0=F7t4=Y12(4l500wY+2=Y)3t4(2l100w)4=Y7t12(6l300wY+2=Y)4t6(3l50w)F+2=FV+2=V200wq",
    "8=S110=Z0=H3=V6t20l100w2=L7t0=H12(Ll800wL+2=L)10wq",
    "3=S10=P3=I370=Z1=V4=F8=L5=W:2(6(6t8l500w5t2l)500w6t3=V0=H0=B12l300wL+8=LW+5=W)q",
    "9999=S10=P3=I310=Z4=V7t0=H0=B:6(7t8l500w)7t0=H0=B6=V32l4t10(2l10w)q",
    "9999=S10=P3=I210=Z0=V7t5=L:7(3(Ll250w)L+1=LV+1=V)q",
    "9999=S10=P10=I200=Z0=V5=L:6(6t3(Ll250w)4t5lL+2=LV+1=V)q",
    "220=Z0=B4=V0=F2(7t4=Y5(4l500wY+2=Y)3t4(2l100w)4=Y7t6(4l300wY+2=Y)4t6(3l50w)F+2=FV+2=V200w)q",
    "9999=S10=P2=I140=Z4=V7t3=F:4(7t8l3(100w3t4l500w)F+1=F)7t4=V12lq",
    "3=S10=P4=I280=Z2=V25=Y7t:8(2=V7t12l5=V9t2l500w5t2(2l)Y-1=Y)7t4=V12lq",
    "5S:2(25=Y12(9t3lY-1=Y60w)400w3t3l4t4l12(7t3lY+1=Y60w))q",
    "4=S5=P10=I340=Z2=V:2=L12(3(6tLl)800wL+1=L)q",
    "4=S:100=Z10=P2=I3=F10=L10(6tLl500w3t2lL+5=L)10wq",
    "4=S:300=Z10=P5=I3=F3=L1=V10(6tLl300w3t0l100w6tLl300w4t4lV+1=VL+1=L)10wq",
    "1=S50=P10=I270=Z8=V7t10=L5=W:12(7tLl500wL+1=LW+1=W)7t4=V12lq",
    "9999=S50=P10=I270=Z2=V7t:6(7t8l500w4t3l100w)7t4=V12lq",
};
} // namespace gr::reconstruction::ios::levelData
