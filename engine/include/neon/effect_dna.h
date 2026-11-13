#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace neon {
// Serialized values only: no GL objects, concrete modules or game ownership.
struct WaypointData {
    int count = 0;
    std::vector<int> index;
    std::vector<float> value;
    int mode = 0;
    int inputSrc[3] = {};
    int inputFlags = 0;
    int wrap[6] = {};
    int extra[5] = {};
};
struct ModuleData {
    int type = 0;
    std::array<float, 64> values{};
    std::array<int, 64> integers{};
};
struct EffectDna {
    std::vector<int> S, P;
    std::vector<float> F;
    int sc = 0, pc = 0, fc = 0;
    int rev = 0;
    bool hasDNA = false, built = false;
    std::string error;
    void effectSaveReset() {
        sc = pc = fc = 0;
    }
    int s(int i) const {
        return i >= 0 && std::size_t(i) < S.size() ? S[std::size_t(i)] : -0x7fffffff;
    }
    // Retains the reconstructed reader's zero-fill and invalid-count behavior.
    bool readModule(ModuleData &module);
    bool readWaypoint(WaypointData &waypoint);
};
// Little-endian grid container. Original cell indices and stream order survive.
bool readEffectGrid(const std::vector<uint8_t> &file, std::vector<EffectDna> &cells,
                    int maxCells = 256);
} // namespace neon
