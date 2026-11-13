#pragma once
#include "neon/effect_dna.h"
namespace frozen {
struct LegacyModule {
    struct Param {
        double value = 0;
        int ival = 0;
    } params[64];
    int type = 0;
    void setBasics() {}
};
inline LegacyModule *createModule(int type) {
    auto *m = new LegacyModule();
    m->type = type;
    return m;
}
struct LegacyCell {
    std::vector<int> S, P;
    std::vector<float> F;
    int sc = 0, pc = 0, fc = 0;
    bool hasDNA = false;
    int s(int i) const {
        return i >= 0 && i < int(S.size()) ? S[size_t(i)] : -0x7fffffff;
    }
    LegacyModule *loadModule();
    int loadWaypoint(neon::WaypointData *waypoint);
};
bool loadGrid(const std::vector<uint8_t> &file, std::vector<LegacyCell> &cells, int maxCells = 256);
} // namespace frozen
