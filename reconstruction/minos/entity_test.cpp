// Compare base entity state and motion with original-execution fixtures.
#include "entity.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace minos;

namespace {
constexpr uint32_t kGameAddress = 0x20000000, kExploVtable = 0x1ed01c;
double bits(const std::string &h) {
    uint64_t u = std::stoull(h, nullptr, 16);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
uint64_t bitsOf(double d) {
    uint64_t u;
    std::memcpy(&u, &d, 8);
    return u;
}
std::string explo(const ExplosionBirth &x, const std::vector<uint8_t> &ref) {
    std::vector<uint8_t> b = ref;
    auto put = [&](int off, const void *v, size_t n) { std::memcpy(&b[size_t(off)], v, n); };
    put(0, &kExploVtable, 4);
    put(0x1c, &x.spriteIndex, 4);
    put(0x84, &x.rot, 4);
    put(0x8c, &x.pos.x, 8);
    put(0x94, &x.pos.y, 8);
    put(0xec, &x.baseScale.x, 8);
    put(0xf4, &x.baseScale.y, 8);
    put(0x168, &kGameAddress, 4);
    put(0x16c, &x.fromScale, 4);
    put(0x170, &x.toScale, 4);
    put(0x174, &x.progress, 4);
    put(0x178, &x.scaleRate, 4);
    std::string out = "0x2e0:";
    bool first = true;
    char t[32];
    for (size_t i = 0; i < b.size();) {
        if (b[i] == ref[i]) {
            i++;
            continue;
        }
        std::snprintf(t, sizeof t, "%s%zx=", first ? "" : "/", i);
        out += t;
        first = false;
        while (i < b.size() && b[i] != ref[i]) {
            std::snprintf(t, sizeof t, "%02x", b[i]);
            out += t;
            i++;
        }
    }
    return out;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s centity-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    auto map = std::make_unique<cavern::StripMap>();
    neon::DarwinRandom rand(777);
    std::vector<uint8_t> ref;
    int checked = 0, failed = 0, explos = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        if (line.rfind("ref ", 0) == 0) {
            const std::string h = line.substr(4);
            for (size_t i = 0; i + 1 < h.size(); i += 2)
                ref.push_back(uint8_t(std::stoul(h.substr(i, 2), nullptr, 16)));
            continue;
        }
        std::istringstream s(line);
        std::string xs, ys, dxs, dys, bar, pxs, pys, wantExplo;
        int col, row, hidden, burst, clear;
        uint32_t cell, randAfter, cellAfterWant;
        int deadWant, delayWant, drawnWant;
        std::string cellHex;
        s >> xs >> ys >> dxs >> dys >> col >> row >> hidden >> burst >> clear >> cell >>
            randAfter >> bar >> pxs >> pys >> deadWant >> cellHex >> delayWant >> drawnWant >>
            wantExplo;
        cellAfterWant = uint32_t(std::stoul(cellHex, nullptr, 16));
        MapEntity e;
        e.pos = {bits(xs), bits(ys)};
        e.column = col;
        e.row = row;
        e.hidden = hidden != 0;
        e.burstOnClear = int8_t(uint8_t(burst));
        MapEntityWorld w;
        w.scrollDelta = {bits(dxs), bits(dys)};
        w.clearAll = clear;
        w.map = map.get();
        w.rand = &rand;
        w.game = nullptr; // the explo's game pointer is compared as the witness address below
        const size_t cellIndex =
            row >= 0 ? size_t(row) * cavern::StripMap::kMapColumns + size_t(col) : 0;
        if (row >= 0)
            map->cells[cellIndex] = cell;
        const auto x = centityRun(e, w);
        const bool drawn = centityDrawVisible(e);
        const uint32_t cellAfter = row >= 0 ? map->cells[cellIndex] : 0;
        const std::string gotExplo = x ? explo(*x, ref) : "-";
        if (x)
            explos++;
        checked++;
        const bool ok = bitsOf(e.pos.x) == std::stoull(pxs, nullptr, 16) &&
                        bitsOf(e.pos.y) == std::stoull(pys, nullptr, 16) &&
                        int(e.dead) == deadWant && cellAfter == cellAfterWant &&
                        e.drawDelay == delayWant && int(drawn) == drawnWant &&
                        rand.state() == randAfter && gotExplo == wantExplo;
        if (!ok) {
            failed++;
            if (failed <= 5)
                std::fprintf(stderr,
                             "case %d: dead %d/%d cell %08x/%08x delay %d/%d drawn %d/%d rand "
                             "%u/%u\n  explo %s\n  want  %s\n",
                             checked, int(e.dead), deadWant, cellAfter, cellAfterWant, e.drawDelay,
                             delayWant, int(drawn), drawnWant, rand.state(), randAfter,
                             gotExplo.c_str(), wantExplo.c_str());
        }
    }
    std::printf("centity run/draw: %d cases checked (%d explos), %d mismatched\n", checked, explos,
                failed);
    return failed || checked == 0 ? 1 : 0;
}
