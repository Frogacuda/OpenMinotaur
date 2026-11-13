// Compare cavern draw commands with captured original-execution quad hashes.
// Fixture inputs reconstruct the map profile, viewport and scripted RNG state.
#include "cavern_draw.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

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
struct NoLaunch : MapHost {
    void launch(char, Vec2, int, int) override {}
};
double bitsToDouble(const std::string &hex) {
    uint64_t u = std::stoull(hex, nullptr, 16);
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s cavern-draw-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::string text;
    int checked = 0, failed = 0, lastCavern = -1, cameraFrames = 0;
    long quads = 0, sequenceQuads = 0, overlayQuads = 0;
    std::unique_ptr<StripMap> map;
    std::unique_ptr<WallProfile> profile;
    FlashChannels flash;
    int32_t word9dc = 0;
    NoLaunch host;
    while (std::getline(in, text)) {
        if (text.empty() || text[0] == '#')
            continue;
        std::istringstream s(text);
        std::string kind;
        int cavern, top = 0, lit, watched;
        s >> kind >> cavern;
        if (kind == "frame")
            s >> top;
        s >> lit >> watched;
        double v[8];
        for (double &d : v) {
            std::string h;
            s >> h;
            d = bitsToDouble(h);
        }
        int wantSector = 0;
        std::string wantFraction;
        if (kind == "camera")
            s >> wantSector >> wantFraction;
        int count;
        std::string hash;
        s >> count >> hash;
        if (cavern != lastCavern) {
            const CavernDefinition &cv = kCavernDefinitions[cavern];
            map = std::make_unique<StripMap>();
            profile = std::make_unique<WallProfile>();
            flash = FlashChannels{};
            word9dc = 0;
            map->setCavern(&cv, cv.zoneLength);
            map->installFlashTables();
            map->bind(&host, profile.get(), &flash, &word9dc);
            buildWallProfile(cv, map->sectorLength(), *profile);
            lastCavern = cavern;
        }
        for (int i = 0; i < 4; i++) {
            flash.lit[i] = uint8_t((lit >> i) & 1);
            flash.watched[i] = int8_t(((watched >> i) & 1) ? -1 : 0);
        }
        map->origin = {v[2], v[3]};
        map->scale = {v[4], v[5]};
        bool cameraOk = true;
        if (kind == "camera") {
            // theGame::_bgdraw, play state: camera -> fraction, visible lines, sector.
            const int sector = prepareVisibleLines(*map, {v[0], v[1]});
            char frac[33];
            const unsigned char *b = reinterpret_cast<const unsigned char *>(&map->fraction.x);
            const unsigned char *c = reinterpret_cast<const unsigned char *>(&map->fraction.y);
            for (int i = 0; i < 8; i++)
                std::snprintf(frac + 2 * i, 3, "%02x", b[i]);
            for (int i = 0; i < 8; i++)
                std::snprintf(frac + 16 + 2 * i, 3, "%02x", c[i]);
            cameraOk = sector == wantSector && wantFraction == frac;
            if (!cameraOk && std::getenv("MINOS_DRAW_DEBUG"))
                std::fprintf(stderr, "sector %d/%d fraction %s/%s\n", sector, wantSector, frac,
                             wantFraction.c_str());
            cameraFrames++;
        } else {
            map->fraction = {v[0], v[1]};
            for (int i = 0; i <= map->rows; i++)
                map->getLine(0, top + i, (map->columns + 1) * i);
        }
        const MapDrawPass pass = drawMap(*map, {v[6], v[7]});
        Fnv f;
        for (const TileQuad &q : pass.quads) {
            f.value(q.position.x);
            f.value(q.position.y);
            f.value(pass.size.x);
            f.value(pass.size.y);
            f.value(int32_t(MapDrawPass::kFrame));
            f.value(uint32_t(0x1000));
            f.value(q.uv.u);
            f.value(q.uv.v);
            f.value(q.uv.du);
            f.value(q.uv.dv);
            if (q.colour.fromSequence) {
                f.value(uint8_t('R'));
                f.value(q.colour.rgba);
                sequenceQuads++;
            } else {
                f.value(uint8_t('P'));
                f.value(int32_t(q.colour.paletteIndex));
            }
        }
        quads += long(pass.quads.size());
        for (int i = 0; i < (map->columns + 1) * (map->rows + 1); i++)
            if (overlayTileOf(map->tiles[size_t(i)]))
                overlayQuads++;
        checked++;
        if (!cameraOk || int(pass.quads.size()) != count || f.h != std::stoull(hash, nullptr, 16)) {
            failed++;
            std::fprintf(stderr, "%s cavern %d top %d: %zu quads (witness %d)%s%s\n", kind.c_str(),
                         cavern, top, pass.quads.size(), count,
                         f.h != std::stoull(hash, nullptr, 16) ? ", hash mismatch" : "",
                         cameraOk ? "" : ", camera sector/fraction mismatch");
        }
    }
    std::printf("cavern draw: %d frames checked (%d through _bgdraw), %d mismatched (%ld quads, "
                "%ld sequence-coloured, %ld overlay)\n",
                checked, cameraFrames, failed, quads, sequenceQuads, overlayQuads);
    return failed || checked == 0 ? 1 : 0;
}
