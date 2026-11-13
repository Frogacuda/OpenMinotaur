#include "shape_component.h"
#include <cmath>
namespace neon::effects {
pc::ShapeSettings shapeComponentSettings(const Component &c, double seconds) {
    pc::ShapeSettings s{};
    auto &i = s.integers;
    auto &f = s.floats;
    auto &v = c.values;
    i[0] = int(std::lround(v[ShapeOrderA])) - 1;
    i[1] = int(std::lround(v[ShapeWaveCountA])) - 1;
    i[2] = int(std::lround(v[ShapeOrderB])) - 1;
    i[3] = int(std::lround(v[ShapeWaveCountB])) - 1;
    for (int k = 0; k < 4; k++) {
        i[k + 4] = int(std::lround(v[ShapePalette0 + k]));
        i[k + 8] = int(std::lround(v[ShapeAlpha0 + k]));
    }
    i[12] = int(std::lround((v[ShapeSignedScale] + 1) * 127.5f));
    i[13] = c.profile | (c.profileB << 5) | (v[ShapeMirrorA] >= .5f ? 0x800 : 0) |
            (v[ShapeMirrorB] >= .5f ? 0x1000 : 0);
    i[14] = 5;
    i[15] = 1;
    float phase = c.animated ? float(std::fmod(seconds * v[Speed], 1.)) : 0;
    f[0] = v[ShapePhaseA] + phase;
    f[1] = v[ShapeWavePhaseA] + phase;
    f[2] = v[ShapePhaseB] - phase;
    f[3] = v[ShapeWavePhaseB] + phase;
    f[4] = v[ShapeRadiusA] / 400;
    f[5] = v[ShapeRippleA] / 400;
    f[6] = v[ShapeRadiusB] / 400;
    f[7] = v[ShapeRippleB] / 400;
    f[8] = v[ShapeScaleAX];
    f[9] = v[ShapeScaleAY];
    f[10] = v[ShapeScaleBX];
    f[13] = v[ShapeScaleBY];
    f[11] = v[ShapeOffsetAX] / 400;
    f[12] = v[ShapeOffsetAY] / 400;
    f[14] = v[ShapeOffsetBX] / 400;
    f[15] = v[ShapeOffsetBY] / 400;
    return s;
}
} // namespace neon::effects
