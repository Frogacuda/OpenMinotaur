// Compare entity factory writes with original-execution fixtures across printable
// map characters and scripted positions. Constructor-equal stores are not visible
// in these captures; source review accounts for their initialization separately.
#include "cavern_launch.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace minos::cavern;

namespace {
constexpr uint32_t kGameAddress = 0x20000000; // where the witness placed theGame
struct ClassInfo {
    const char *name;
    int size;
    uint32_t vtable;
};
// Original allocation sizes and vtable address points (__ZTV... + 8) of Minos_armv7.
const std::map<EntityClass, ClassInfo> kClasses = {
    {EntityClass::ScrambleRocket, {"scrambleRocket", 0x19c, 0x1ed0bc}},
    {EntityClass::Faller, {"faller", 0x1a0, 0x1ed0dc}},
    {EntityClass::Turret, {"turret", 0x1a4, 0x1ed09c}},
    {EntityClass::Rocket, {"rocket", 0x1a8, 0x1ed07c}},
    {EntityClass::CavernGoal, {"cavernGoal", 0x184, 0x1ecfdc}},
    {EntityClass::WavingMinotaur, {"wavingMinotaur", 0x1c0, 0x1ecf1c}},
    {EntityClass::OneUp, {"oneUp", 0x184, 0x1ecfbc}},
    {EntityClass::MotherSheep, {"motherSheep", 0x1b8, 0x1ecf9c}},
    {EntityClass::Goat, {"goat", 400, 0x1ecf3c}},
    {EntityClass::Saucer, {"saucer", 0x198, 0x1ecf5c}},
};

struct Image {
    std::vector<uint8_t> b;
    template <class T> void put(int off, T v) {
        std::memcpy(&b[size_t(off)], &v, sizeof v);
    }
    int32_t i32(int off) const {
        int32_t v;
        std::memcpy(&v, &b[size_t(off)], 4);
        return v;
    }
};

std::string diff(const Image &cur, const std::vector<uint8_t> &ref, int n) {
    std::string out;
    char hex[3];
    for (int i = 0; i < n;) {
        if (cur.b[size_t(i)] == ref[size_t(i)]) {
            i++;
            continue;
        }
        int j = i;
        if (!out.empty())
            out += '/';
        char head[16];
        std::snprintf(head, sizeof head, "%x=", i);
        out += head;
        while (j < n && cur.b[size_t(j)] != ref[size_t(j)]) {
            std::snprintf(hex, 3, "%02x", cur.b[size_t(j)]);
            out += hex;
            j++;
        }
        i = j;
    }
    return out;
}

double bitsToDouble(const std::string &h) {
    uint64_t u = std::stoull(h, nullptr, 16);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cavern-launch-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::vector<uint8_t> ref;
    neon::DarwinRandom rand(12345);
    auto map = std::make_unique<StripMap>();
    int checked = 0, failed = 0, spawned = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream s(line);
        std::string tag;
        s >> tag;
        if (tag == "ref") {
            std::string h;
            s >> h;
            for (size_t i = 0; i + 1 < h.size(); i += 2)
                ref.push_back(uint8_t(std::stoul(h.substr(i, 2), nullptr, 16)));
            continue;
        }
        int ch = int(std::stoul(tag, nullptr, 16)), cav, col, row;
        std::string xs, ys, cellHex, want;
        s >> cav >> col >> row >> xs >> ys >> cellHex >> want;
        LaunchContext ctx;
        ctx.cavern = &kCavernDefinitions[cav];
        ctx.rand = &rand;
        ctx.spriteSize = [](int) { return Vec2{0.0625, 0.046875}; };
        const size_t cellIndex = size_t(row) * StripMap::kMapColumns + size_t(col);
        map->cells[cellIndex] = 0;
        const auto req =
            launch(*map, ctx, char(ch), {bitsToDouble(xs), bitsToDouble(ys)}, col, row);
        const uint32_t cell = map->cells[cellIndex];

        std::string got;
        if (req) {
            spawned++;
            const ClassInfo &info = kClasses.at(req->cls);
            Image o{ref};
            got = "new:" + std::to_string(info.size);
            o.put(0, info.vtable);
            o.put<uint8_t>(0x181, 1);
            o.put(0x8c, req->position.x);
            o.put(0x94, req->position.y);
            o.put(0x178, int32_t(req->column));
            o.put(0x17c, int32_t(req->row));
            if (req->spriteIndex)
                o.put(0x1c, *req->spriteIndex);
            if (req->rot)
                o.put(0x84, *req->rot);
            auto applyLater = [&] {
                if (req->spriteIndexAfterInit)
                    o.put(0x1c, *req->spriteIndexAfterInit);
                if (req->promoteSprite5To6 && o.i32(0x1c) == 5)
                    o.put<int32_t>(0x1c, 6);
                if (req->size)
                    o.put(0x134, *req->size);
                if (req->hidden)
                    o.put(0x180, *req->hidden);
                if (req->burstOnClear)
                    o.put(0x181, *req->burstOnClear);
                if (req->drawDelay)
                    o.put(0x20, *req->drawDelay);
                if (req->game)
                    o.put(0x168, kGameAddress);
                if (req->scaleX)
                    o.put(0xdc, *req->scaleX);
                if (req->scaleY)
                    o.put(0xe4, *req->scaleY);
                if (req->phase184)
                    o.put(0x184, *req->phase184);
                if (req->word188)
                    o.put(0x188, *req->word188);
                if (req->home1b0) {
                    o.put(0x1b0, req->home1b0->x);
                    o.put(0x1b8, req->home1b0->y);
                }
            };
            if (req->phase184)
                got += ";rand:" + std::to_string(rand.state());
            if (req->sizeQueryFrame)
                got += ";getSize:" + std::to_string(*req->sizeQueryFrame);
            if (req->callsInit) {
                got += ";init:" + std::string(info.name) + "," + diff(o, ref, info.size);
                if (req->cls == EntityClass::Rocket)
                    o.put<int32_t>(0x1c, 5); // witness stand-in
            }
            applyLater();
            got += ";add:" + std::string(req->list == EntityList::List2e4 ? "0x2e4" : "0x2e8") +
                   "," + diff(o, ref, info.size);
        } else {
            got = "-";
        }
        char cellText[16];
        std::snprintf(cellText, sizeof cellText, "%08x", cell);
        checked++;
        if (got != want || cellHex != cellText) {
            failed++;
            if (failed <= 5)
                std::fprintf(stderr, "char %02x cavern %d col %d:\n  want %s %s\n  got  %s %s\n",
                             ch, cav, col, cellHex.c_str(), want.c_str(), cellText, got.c_str());
        }
    }
    std::printf("cavern launch: %d cases checked (%d spawns), %d mismatched\n", checked, spawned,
                failed);
    return failed || checked == 0 ? 1 : 0;
}
