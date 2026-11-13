// Optional authoring/study catalogue; the title loads its saved scene directly.
#include "effect_runtime.h"
namespace neon::effects {
const std::vector<PresetInfo> &Runtime::presets() {
    static const std::vector<PresetInfo> list = {{0, "Recovered r2/c8 procedural output"},
                                                 {1, "Recovered r6/c15 cube"},
                                                 {2, "Jukebox r6/c11 composed background"},
                                                 {3, "Unverified r4/c3 shader study"},
                                                 {4, "Unverified r5/c13 planet study"},
                                                 {5, "Jukebox r1/c6 tunnel"},
                                                 {6, "Jukebox r1/c4 tunnel"},
                                                 {7, "Jukebox r1/c8 sprite flight"},
                                                 {8, "Jukebox r4/c10 particle planet"},
                                                 {9, "Jukebox r2/c13 light tunnel"},
                                                 {10, "Jukebox r6/c1 ribbed tunnel"},
                                                 {11, "Jukebox r1/c10 textured tunnel"},
                                                 {12, "Jukebox r5/c8 displaced tunnel"},
                                                 {13, "Jukebox r1/c0 woven field"},
                                                 {14, "Jukebox r3/c1 layered field"},
                                                 {15, "Jukebox r5/c0 round light field"},
                                                 {16, "Minotaur Project title flight"},
                                                 {17, "Vector-textured light tunnel"},
                                                 {100, "Lisspoint / original fire source"},
                                                 {101, "Lisspoint / colour field"},
                                                 {102, "Lisspoint / blue field"},
                                                 {103, "Galactica / colour field"},
                                                 {104, "Experimental geometry assembly"},
                                                 {18, "Build from components"},
                                                 {19, "Jukebox r1/c12 flower rings"},
                                                 {20, "Jukebox r2/c14 displaced planet"},
                                                 {21, "Generated shapes / recovered families"}};
    return list;
}
} // namespace neon::effects
