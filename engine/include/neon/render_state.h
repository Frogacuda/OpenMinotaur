#pragma once
#include <array>
#include <cstddef>

namespace neon {
struct ShaderCycleState {
    float c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    void fill(bool cycle, int beat, float *constants, bool *enabled);
};

// Scratch pointers refer into this instance: copying/moving would invalidate
// them. The module owns the instance for at least the rendering callback's life.
template <class Vertex, std::size_t Capacity> struct SpriteRenderState {
    std::array<Vertex, Capacity> vertices{};
    Vertex *cursor = vertices.data();
    Vertex *base = vertices.data();
    int count = 0;
    int sourceTexture = -1;
    ShaderCycleState shaderCycles;
    SpriteRenderState() = default;
    SpriteRenderState(const SpriteRenderState &) = delete;
    SpriteRenderState &operator=(const SpriteRenderState &) = delete;
    void beginBatch() {
        base = vertices.data();
        cursor = base;
        count = 0;
    }
};
} // namespace neon
