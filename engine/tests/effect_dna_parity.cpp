#include "legacy_effect_dna.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <limits>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "Failed: " #x " at " << __LINE__ << "\n";                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
bool same(const neon::WaypointData &a, const neon::WaypointData &b) {
    if (a.count != b.count || a.index != b.index || a.value != b.value || a.mode != b.mode ||
        a.inputFlags != b.inputFlags)
        return false;
    for (int i = 0; i < 3; i++)
        if (a.inputSrc[i] != b.inputSrc[i])
            return false;
    for (int i = 0; i < 6; i++)
        if (a.wrap[i] != b.wrap[i])
            return false;
    for (int i = 0; i < 5; i++)
        if (a.extra[i] != b.extra[i])
            return false;
    return true;
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    CHECK(input.good());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    std::vector<neon::EffectDna> engine;
    std::vector<frozen::LegacyCell> legacy;
    CHECK(neon::readEffectGrid(bytes, engine));
    CHECK(frozen::loadGrid(bytes, legacy));
    CHECK(engine.size() == legacy.size());
    int cells = 0, stacks = 0, modules = 0, waypoints = 0;
    for (size_t cell = 0; cell < engine.size(); cell++) {
        auto &a = engine[cell];
        auto &b = legacy[cell];
        CHECK(a.hasDNA == b.hasDNA);
        CHECK(a.S == b.S && a.P == b.P && a.F == b.F);
        if (!a.hasDNA)
            continue;
        cells++;
        int rev = 0;
        if ((uint32_t(a.s(0)) & 0xffff0000u) == 0xbaaa0000u) {
            rev = a.s(a.sc++) & 0xffff;
            b.sc++;
        }
        if (rev > 0) {
            a.sc++;
            b.sc++;
        }
        while (a.s(a.sc) == 3) {
            a.sc++;
            b.sc++;
            stacks++;
            while (a.s(a.sc) == 1) {
                neon::ModuleData record;
                CHECK(a.readModule(record));
                std::unique_ptr<frozen::LegacyModule> old(b.loadModule());
                CHECK(old);
                CHECK(record.type == old->type);
                for (int i = 0; i < 64; i++) {
                    CHECK(double(record.values[size_t(i)]) == old->params[i].value);
                    CHECK(record.integers[size_t(i)] == old->params[i].ival);
                }
                CHECK(a.sc == b.sc && a.pc == b.pc && a.fc == b.fc);
                modules++;
            }
            while (a.s(a.sc + 3) == 2) {
                a.sc += 3;
                b.sc += 3;
                neon::WaypointData current, old;
                CHECK(a.readWaypoint(current));
                b.loadWaypoint(&old);
                CHECK(same(current, old));
                CHECK(a.sc == b.sc && a.pc == b.pc && a.fc == b.fc);
                waypoints++;
            }
            if (rev > 0) {
                a.sc++;
                b.sc++;
            }
            CHECK(a.s(a.sc) == -1);
            a.sc++;
            b.sc++;
        }
    }
    CHECK(cells > 0 && modules > 0 && waypoints > 0);
    // Deliberately malformed/truncated inputs: reject lengths without allocating
    // a bogus stream; missing record payload retains the legacy zero-fill rule.
    std::vector<neon::EffectDna> invalid;
    CHECK(!neon::readEffectGrid({}, invalid));
    CHECK(!neon::readEffectGrid(bytes, invalid, -1));
    auto bad = bytes;
    bad[0] = bad[1] = bad[2] = 0xff;
    bad[3] = 0x7f;
    CHECK(!neon::readEffectGrid(bad, invalid));
    neon::EffectDna sparse;
    sparse.S = {1, 500, 2, 65};
    neon::ModuleData record;
    CHECK(sparse.readModule(record));
    CHECK(record.type == 500);
    for (auto x : record.values)
        CHECK(x == 0);
    for (auto x : record.integers)
        CHECK(x == 0);
    neon::WaypointData waypoint;
    CHECK(sparse.readWaypoint(waypoint));
    CHECK(waypoint.count == 0);
    CHECK(!sparse.readModule(record));
    std::cout << argv[1] << ": " << cells << " cells, " << stacks << " stacks, " << modules
              << " modules, " << waypoints << " waypoints match the parser reference\n";
}
