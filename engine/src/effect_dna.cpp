#include "neon/effect_dna.h"
#include <cstring>
#include <utility>

namespace neon {
bool EffectDna::readModule(ModuleData &module) {
    if (s(sc) != 1)
        return false;
    sc++;
    module.type = s(sc++);
    for (auto &value : module.values) {
        value = fc < int(F.size()) ? F[size_t(fc)] : 0;
        fc++;
    }
    for (auto &value : module.integers) {
        value = pc < int(P.size()) ? P[size_t(pc)] : 0;
        pc++;
    }
    return true;
}
bool EffectDna::readWaypoint(WaypointData &waypoint) {
    if (s(sc) != 2)
        return false;
    sc++;
    waypoint.count = s(sc++);
    if (waypoint.count < 0 || waypoint.count > 64)
        waypoint.count = 0;
    waypoint.index.resize(size_t(waypoint.count));
    waypoint.value.resize(size_t(waypoint.count));
    for (auto &value : waypoint.index) {
        value = pc < int(P.size()) ? P[size_t(pc)] : 0;
        pc++;
    }
    for (auto &value : waypoint.value) {
        value = fc < int(F.size()) ? F[size_t(fc)] : 0;
        fc++;
    }
    int fields[16];
    for (auto &value : fields) {
        value = pc < int(P.size()) ? P[size_t(pc)] : 0;
        pc++;
    }
    waypoint.mode = fields[0];
    for (int i = 0; i < 3; i++)
        waypoint.inputSrc[i] = fields[i + 1];
    waypoint.inputFlags = fields[4];
    for (int i = 0; i < 6; i++)
        waypoint.wrap[i] = fields[i + 5];
    for (int i = 0; i < 5; i++)
        waypoint.extra[i] = fields[i + 11];
    return true;
}
static uint32_t word(const std::vector<uint8_t> &bytes, size_t offset) {
    return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
           uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
}
bool readEffectGrid(const std::vector<uint8_t> &file, std::vector<EffectDna> &cells, int maxCells) {
    cells.clear();
    if (maxCells < 0)
        return false;
    cells.resize(size_t(maxCells));
    if (file.size() < 12)
        return false;
    const int32_t ns = int32_t(word(file, 0)), np = int32_t(word(file, 4)),
                  nf = int32_t(word(file, 8));
    if (ns < 1 || np < 1024 || nf < 0)
        return false;
    const uint64_t total = uint64_t(ns) + uint64_t(np) + uint64_t(nf);
    if (total > (file.size() - 12) / 4)
        return false;
    std::vector<int> structure(size_t(ns), 0), integers(size_t(np), 0);
    std::vector<float> floats(size_t(nf), 0);
    size_t offset = 12;
    for (auto &x : structure) {
        x = int32_t(word(file, offset));
        offset += 4;
    }
    for (auto &x : integers) {
        x = int32_t(word(file, offset));
        offset += 4;
    }
    for (auto &x : floats) {
        const auto bits = word(file, offset);
        std::memcpy(&x, &bits, 4);
        offset += 4;
    }
    size_t so = 1, po = 1024, fo = 0;
    for (int i = 0; i < structure[0] && i < 256; i++) {
        const int cell = integers[size_t(i)], slen = integers[size_t(256 + i)],
                  plen = integers[size_t(512 + i)], flen = integers[size_t(768 + i)];
        if (slen < 0 || plen < 0 || flen < 0 || size_t(slen) > structure.size() - so ||
            size_t(plen) > integers.size() - po || size_t(flen) > floats.size() - fo)
            return false;
        if (cell >= 0 && cell < maxCells) {
            auto &target = cells[size_t(cell)];
            target.S.assign(structure.begin() + so, structure.begin() + so + slen);
            target.P.assign(integers.begin() + po, integers.begin() + po + plen);
            target.F.assign(floats.begin() + fo, floats.begin() + fo + flen);
            target.hasDNA = true;
        }
        so += size_t(slen);
        po += size_t(plen);
        fo += size_t(flen);
    }
    return true;
}
} // namespace neon
