// Light Synth geometry contracts: finite surfaces, face orientation and UV mirroring.
#include "pc_cube.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Light Synth cube geometry contract failed");
}
} // namespace
int main() {
    try {
        neon::pc::ShapeSettings settings;
        settings.floats[2] = settings.floats[3] = settings.floats[5] = 1.f;
        settings.integers[8] = 1;
        settings.integers[10] = 2; // Four cells per axis.
        neon::pc::CubeField field{};
        constexpr int normalAxis[] = {2, 2, 0, 1, 0, 1};
        constexpr float normalSign[] = {1, -1, 1, 1, -1, -1};
        for (int mirror : {0, 1}) {
            settings.integers[14] = mirror;
            for (int face = 0; face < 6; ++face) {
                const auto mesh = neon::pc::cubeFace(settings, field, 0x12345678, face);
                require(mesh.size() == 4 * 4 * 6);
                for (const auto &vertex : mesh) {
                    const float position[] = {vertex.x, vertex.y, vertex.z};
                    require(position[normalAxis[face]] == normalSign[face]);
                    for (float component : position)
                        require(std::isfinite(component) && std::abs(component) <= 1.f);
                    require(vertex.rgba == 0x12345678 && vertex.u0 == vertex.u1 &&
                            vertex.v0 == vertex.v1);
                    require(vertex.u0 >= 0 && vertex.u0 <= 1 && vertex.v0 >= 0 && vertex.v0 <= 1);
                }
                require(mesh.front().u0 == 0 && mesh.front().v0 == 0);
                require(mesh[mesh.size() - 2].u0 == (mirror ? 0.f : 1.f));
                field.fill(.25f);
                const auto displaced = neon::pc::cubeFace(settings, field, 0, face);
                const auto &vertex = displaced.front();
                const float position[] = {vertex.x, vertex.y, vertex.z};
                require(position[normalAxis[face]] == normalSign[face] * 1.25f);
                field.fill(0);
            }
        }
        settings.floats[9] = .1f;
        for (float value : neon::pc::cubeField(settings))
            require(std::isfinite(value));
        bool rejected = false;
        try {
            neon::pc::cubeFace(settings, field, 0, 6);
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        require(rejected);
        std::cout << "Light Synth cube surfaces, displacement and mirrored UV behavior passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
