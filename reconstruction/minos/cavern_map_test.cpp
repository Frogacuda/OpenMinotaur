// Compare cavern generation with original-execution fixtures. Regenerate inputs
// from each fixture header before comparing lines and generation state.
#include "cavern_map.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

using namespace minos::cavern;

namespace {
struct Fnv {
    uint64_t h = 0xcbf29ce484222325ull;
    void bytes(const void *p, size_t n) {
        auto *b = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < n; i++) {
            h ^= b[i];
            h *= 0x100000001b3ull;
        }
    }
    template <class T> void value(T v) {
        bytes(&v, sizeof v);
    } // little-endian host assumed
};
struct Launch {
    char kind;
    Vec2 pos;
    int column, row;
};
struct Recorder : MapHost {
    std::vector<Launch> launches;
    void launch(char kind, Vec2 pos, int column, int row) override {
        launches.push_back({kind, pos, column, row});
    }
};
} // namespace

static double bitsToDouble(const std::string &hex) {
    uint64_t u = std::stoull(hex, nullptr, 16);
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}
static uint64_t doubleBits(double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    return u;
}

// Targeted witness: algoCavern1-5 on a synthetic profile, translate launch positions.
static int stress(const char *path) {
    std::ifstream in(path);
    std::string text;
    WallProfile profile;
    int checked = 0, failed = 0;
    CavernDefinition synthetic = kCavernDefinitions[1];
    int lastZone = -1;
    StripMap *map = nullptr;
    std::vector<StripMap> maps;
    FlashChannels flash;
    int32_t word9dc = 0;
    Recorder host;
    while (std::getline(in, text)) {
        if (text.empty() || text[0] == '#')
            continue;
        std::istringstream s(text);
        std::string kind;
        s >> kind;
        if (kind == "profile") {
            int k;
            s >> k;
            auto &track = k == 0   ? profile.leftPhase
                          : k == 1 ? profile.rightPhase
                          : k == 2 ? profile.leftDepth
                                   : profile.rightDepth;
            for (int i = 0; i < 64; i++) {
                std::string h;
                s >> h;
                uint32_t u = uint32_t(std::stoul(h, nullptr, 16));
                std::memcpy(&track[size_t(i)], &u, 4);
            }
        } else if (kind == "algo") {
            int zone, y;
            std::string h;
            s >> zone >> y >> h;
            if (zone != lastZone) {
                for (int &z : synthetic.zones)
                    z = -1;
                synthetic.zones[0] = synthetic.zones[1] = zone;
                synthetic.zoneLength = 12;
                maps.clear();
                maps.emplace_back();
                map = &maps.back();
                map->setCavern(&synthetic, 12);
                map->bind(&host, &profile, &flash, &word9dc);
                lastZone = zone;
            }
            host.launches.clear();
            map->getLine(0, y, 0);
            Fnv f;
            for (int i = 0; i < 21; i++)
                f.value(int32_t(map->tiles[size_t(i)]));
            for (const Launch &l : host.launches) {
                f.value(uint8_t(l.kind));
                f.value(l.pos.x);
                f.value(l.pos.y);
                f.value(int32_t(l.column));
                f.value(int32_t(l.row));
            }
            checked++;
            if (f.h != std::stoull(h, nullptr, 16)) {
                failed++;
                std::fprintf(stderr, "algo zone %d line %d mismatch\n", zone, y);
            }
        } else if (kind == "pos") {
            StripMap m;
            m.bind(&host, &profile, &flash, &word9dc);
            s >> m.column >> m.row;
            for (int i = 0; i < 10; i++) {
                std::string kv;
                s >> kv;
                const int off = std::stoi(kv.substr(0, kv.find(':')), nullptr, 16);
                const double v = bitsToDouble(kv.substr(kv.find(':') + 1));
                switch (off) {
                case 0x59c:
                    m.scroll.x = v;
                    break;
                case 0x5a4:
                    m.scroll.y = v;
                    break;
                case 0x5ac:
                    m.origin.x = v;
                    break;
                case 0x5b4:
                    m.origin.y = v;
                    break;
                case 0x5bc:
                    m.fraction.x = v;
                    break;
                case 0x5c4:
                    m.fraction.y = v;
                    break;
                case 0x5cc:
                    m.cellSize.x = v;
                    break;
                case 0x5d4:
                    m.cellSize.y = v;
                    break;
                case 0x5dc:
                    m.scale.x = v;
                    break;
                case 0x5e4:
                    m.scale.y = v;
                    break;
                }
            }
            std::string hx, hy;
            s >> hx >> hy;
            host.launches.clear();
            m.translate('M');
            checked++;
            if (host.launches.size() != 1 ||
                doubleBits(host.launches[0].pos.x) != std::stoull(hx, nullptr, 16) ||
                doubleBits(host.launches[0].pos.y) != std::stoull(hy, nullptr, 16)) {
                failed++;
                if (failed < 4)
                    std::fprintf(stderr, "position mismatch col %d row %d\n", m.column, m.row);
            }
        }
    }
    std::printf("cavern stress: %d checked, %d mismatched\n", checked, failed);
    return failed == 0 && checked > 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 3 && std::strcmp(argv[1], "--stress") == 0)
        return stress(argv[2]);
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cavern-lines-ios.txt | --stress cavern-stress-ios.txt\n",
                     argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    std::map<std::pair<int, int>, uint64_t> expected;
    std::string text;
    while (std::getline(in, text)) {
        if (text.empty() || text[0] == '#')
            continue;
        std::istringstream s(text);
        int c, y;
        std::string h;
        s >> c >> y >> h;
        expected[{c, y}] = std::stoull(h, nullptr, 16);
    }
    if (expected.empty()) {
        std::fprintf(stderr, "no witness rows\n");
        return 2;
    }
    int checked = 0, failed = 0;
    for (int c = 0; c < kCavernCount; c++) {
        const CavernDefinition &def = kCavernDefinitions[c];
        StripMap map;
        WallProfile profile;
        FlashChannels flash;
        int32_t word9dc = 0;
        Recorder host;
        map.setCavern(&def, def.zoneLength);
        map.installFlashTables();
        buildWallProfile(def, map.sectorLength(), profile);
        map.bind(&host, &profile, &flash, &word9dc);
        int sectors = 0;
        while (def.zones[sectors] >= 0)
            sectors++;
        int firstFailure = 0;
        for (int y = -4; y < sectors * map.sectorLength() + 30; y++) {
            host.launches.clear();
            map.getLine(0, y, 0);
            Fnv f;
            for (int i = 0; i < 21; i++)
                f.value(int32_t(map.tiles[size_t(i)]));
            for (const Launch &l : host.launches) {
                f.value(uint8_t(l.kind));
                f.value(l.pos.x);
                f.value(l.pos.y);
                f.value(int32_t(l.column));
                f.value(int32_t(l.row));
            }
            auto it = expected.find({c, y});
            if (it == expected.end()) {
                std::fprintf(stderr, "cavern %d line %d: no witness\n", c, y);
                failed++;
                continue;
            }
            checked++;
            if (it->second != f.h) {
                if (firstFailure++ < 3) {
                    std::fprintf(stderr, "cavern %d line %d mismatch; codes:", c, y);
                    for (int i = 0; i < 21; i++)
                        std::fprintf(stderr, " %x", unsigned(map.tiles[size_t(i)]));
                    for (const Launch &l : host.launches)
                        std::fprintf(stderr, " [%d %d,%d]", l.kind, l.column, l.row);
                    std::fprintf(stderr, "\n");
                }
                failed++;
            }
        }
    }
    std::printf("cavern lines: %d checked, %d mismatched (%zu witness rows)\n", checked, failed,
                expected.size());
    return failed == 0 && checked == int(expected.size()) ? 0 : 1;
}
