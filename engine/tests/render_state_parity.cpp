#include "neon/render_state.h"
#include <iostream>

void legacyShaderConstants(int, bool, float, int, int, int, float, float, float *, float *, float *,
                           bool *);
int main() {
    neon::ShaderCycleState state;
    for (int frame = 0; frame < 1500; frame++) {
        float expected[32], actual[32];
        bool expectedEnabled[32], actualEnabled[32];
        for (int i = 0; i < 32; i++) {
            expected[i] = actual[i] = float(i + frame) * 0.01f;
            expectedEnabled[i] = actualEnabled[i] = (i % 2) != 0;
        }
        const bool cycle = (frame % 3) != 0;
        const int beat = (frame % 7) == 0 ? 0 : frame;
        legacyShaderConstants(0, cycle, 0, beat, 480, 320, 1, 1, nullptr, nullptr, expected,
                              expectedEnabled);
        state.fill(cycle, beat, actual, actualEnabled);
        for (int i = 0; i < 32; i++)
            if (expected[i] != actual[i] || expectedEnabled[i] != actualEnabled[i]) {
                std::cerr << "Shader output changed at frame " << frame << ", slot " << i << "\n";
                return 1;
            }
        // Change the history owner halfway through the trace.
        if (frame == 749) {
            neon::ShaderCycleState freshSession;
            freshSession = state;
            state = freshSession;
        }
    }
    struct Vertex {
        int value = 0;
    };
    neon::SpriteRenderState<Vertex, 16> first, second;
    first.vertices[0].value = 23;
    first.cursor += 6;
    first.count = 6;
    first.sourceTexture = 4;
    if (second.vertices[0].value != 0 || second.count != 0 ||
        second.cursor != second.vertices.data() || second.sourceTexture != -1)
        return 1;
    first.beginBatch();
    if (first.cursor != first.vertices.data() || first.base != first.cursor || first.count != 0 ||
        first.sourceTexture != 4)
        return 1;
    std::cout << "Shader parity: 1500 calls; independent batch ownership verified\n";
}
