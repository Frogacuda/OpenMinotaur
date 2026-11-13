#include "neon/render_state.h"
#include <cmath>
namespace neon {
void ShaderCycleState::fill(bool cycle, int beat, float *c, bool *enabled) {

    for (int i = 6; i < 32; i++)
        enabled[i] = false;
    c[8] = 0.0f;
    c[9] = 0.0f;
    c[10] = 0.95f;
    if (beat != 0) {
        c1 += 0.17f;
        c2 += 0.091f;
        c3 += 0.07f;
        c4 += 0.121f;
        if (c1 > 1.0f)
            c1 = 0;
        if (c2 > 1.0f)
            c2 = 0;
        if (c3 > 1.0f)
            c3 = 0;
        if (c4 > 1.0f)
            c4 = 0;
    }
    if (!cycle) {
        for (int i = 11; i <= 19; i++)
            c[i] = 1.0f;
        for (int i = 0; i < 5; i++)
            enabled[i] = true;
        return;
    }
    float s1 = (std::sin(c1) + 1.0f) * 0.5f;
    float s2 = std::sin(c2) + 1.0f;
    c[15] = 0.85f;
    c[12] = s1 * 0.3f + 0.25f;
    c[13] = (1.0f - s2 * 0.5f) * 0.3f + 0.25f;
    c[14] = (1.0f - s1) * 0.3f + 0.5f;
    float s3 = (std::sin(c3) + 1.0f) * 0.5f;
    float s4 = (std::sin(c4) + 1.0f) * 0.5f;
    c[19] = 0.85f;
    c[11] = 0.0f;
    c[16] = (1.0f - s4) * 0.3f + 0.5f;
    c[17] = s3 * 0.3f + 0.25f;
    c[18] = s4 * 0.3f + 0.25f;
}

} // namespace neon
