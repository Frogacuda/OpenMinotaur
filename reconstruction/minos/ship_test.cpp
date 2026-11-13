// Compare ship behavior with original-execution fixtures under scripted inputs.
// The fixture backend selects the math-service implementation used by the capture.
#include "ship.h"
#include "cavern_data.h"
#include "cavern_draw.h"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace minos;
using namespace minos::cavern;

namespace {
bool gDump = false;
struct NoLaunch : MapHost {
    void launch(char, Vec2, int, int) override {}
};
struct Game : PlayState {
    std::vector<std::pair<int, int>> samples;
    void playSample(int id, int vol) override {
        samples.push_back({id, vol});
    }
};
double hd(const std::string &s) {
    uint64_t u = std::stoull(s, nullptr, 16);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
uint64_t du(double d) {
    uint64_t u;
    std::memcpy(&u, &d, 8);
    return u;
}
uint32_t fu(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
uint64_t fnv(const void *p, size_t n, uint64_t h) {
    auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
std::string listHash(Entity *head) {
    uint64_t h = 0xcbf29ce484222325ull;
    int n = 0;
    for (Entity *e = head; e; e = e->next, n++) {
        char c = '?';
        uint8_t extra[4] = {};
        size_t en = 0;
        if (auto *s = dynamic_cast<Sensor *>(e)) {
            c = 'S';
            extra[0] = s->touching;
            extra[1] = s->hazard;
            en = 2;
        } else if (auto *g = dynamic_cast<Gun *>(e)) {
            c = 'G';
            std::memcpy(extra, &g->left, 4);
            en = 4;
        } else if (auto *t = dynamic_cast<ThrustParticle *>(e)) {
            c = 'T';
            std::memcpy(extra, &t->life, 4);
            en = 4;
        } else if (auto *b = dynamic_cast<Bspark *>(e)) {
            c = 'B';
            std::memcpy(extra, &b->age, 4);
            en = 4;
        } else if (auto *x = dynamic_cast<Explo *>(e)) {
            c = 'E';
            std::memcpy(extra, &x->t, 4);
            en = 4;
        } else if (auto *s2 = dynamic_cast<Shot *>(e)) {
            c = 'H';
            std::memcpy(extra, &s2->grow, 4);
            en = 4;
        }
        double v[4] = {e->pos.x, e->pos.y, e->vel.x, e->vel.y};
        h = fnv(&c, 1, h);
        h = fnv(v, 32, h);
        h = fnv(&e->rot, 4, h);
        h = fnv(&e->scale.x, 8, h);
        h = fnv(extra, en, h);
        if (gDump) {
            std::fprintf(stderr, "ent %02x", uint8_t(c));
            auto hex = [](const void *p, size_t n) {
                for (size_t i = 0; i < n; i++)
                    std::fprintf(stderr, "%02x", static_cast<const uint8_t *>(p)[i]);
            };
            hex(v, 32);
            hex(&e->rot, 4);
            hex(&e->scale.x, 8);
            hex(extra, en);
            std::fprintf(stderr, "\n");
        }
    }
    char buf[40];
    std::snprintf(buf, sizeof buf, "%d:%016" PRIx64, n, h);
    return buf;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s fixtures/ship-ios.txt\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::ofstream actual;
    if (const char *path = std::getenv("MINOS_ACTUAL")) {
        actual.open(path);
        if (!actual) {
            std::fprintf(stderr, "cannot write diagnostic %s\n", path);
            return 2;
        }
        actual << "# PORT observations for diagnosis, not an independent reference or replay "
                  "fixture\n";
    }
    std::unique_ptr<StripMap> map;
    std::unique_ptr<WallProfile> profile;
    FlashChannels flash;
    int32_t w9dc = 0;
    NoLaunch host;
    std::unique_ptr<Game> G;
    Ship *ship = nullptr;
    neon::DarwinRandom rng;
    std::string line, inLine;
    int frames = 0, failed = 0, scenarios = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream s(line);
        std::string kind;
        s >> kind;
        if (actual && kind == "scenario")
            actual << line << '\n';
        if (kind == "scenario") {
            int cav, n, seed;
            std::string sy, fuel = "3f800000", shield = "3f800000";
            s >> cav >> n >> sy >> seed >> fuel >> shield;
            const CavernDefinition &cv = kCavernDefinitions[cav];
            map = std::make_unique<StripMap>();
            profile = std::make_unique<WallProfile>();
            flash = FlashChannels{};
            map->setCavern(&cv, cv.zoneLength);
            map->installFlashTables();
            map->bind(&host, profile.get(), &flash, &w9dc);
            buildWallProfile(cv, map->sectorLength(), *profile);
            int nsect = 0;
            while (nsect < kMaxZones && cv.zones[nsect] >= 0)
                nsect++;
            G = std::make_unique<Game>();
            rng.seed(uint32_t(seed));
            G->rand = &rng;
            G->map = map.get();
            G->scrollLimit = double(map->sectorLength() * nsect - 24);
            G->screenY = hd(sy);
            G->lives = 3;
            G->volume = 8;
            {
                uint32_t u = uint32_t(std::stoul(fuel, nullptr, 16));
                std::memcpy(&G->fuel, &u, 4);
                u = uint32_t(std::stoul(shield, nullptr, 16));
                std::memcpy(&G->shield, &u, 4);
            }
            ship = new Ship;
            ship->init(*G);
            G->shipList = ship;
            prepareVisibleLines(*map, {0.0, G->cameraY});
            scenarios++;
            continue;
        }
        if (kind == "in") {
            int fr, t0, t1, btn;
            std::string p[5];
            s >> fr >> t0 >> t1 >> p[0] >> p[1] >> p[2] >> p[3] >> p[4] >> btn;
            G->touchDown[0] = t0;
            G->touchDown[1] = t1;
            G->touchPos[0][0] = hd(p[0]);
            G->touchPos[0][1] = hd(p[1]);
            G->touchPos[1][0] = hd(p[2]);
            G->touchPos[1][1] = hd(p[3]);
            G->analog = hd(p[4]);
            G->prefsButtons = uint32_t(btn);
            G->frameCount = fr;
            G->samples.clear();
            ship->run();
            G->runShots();
            G->runDebris();
            prepareVisibleLines(*map, {0.0, G->cameraY});
            continue;
        }
        if (kind != "out")
            continue;
        frames++;
        char buf[1024];
        std::string sens;
        for (int i = 0; i < 5; i++) {
            Sensor *sn = ship->sensor(i);
            char b[32];
            std::snprintf(b, sizeof b, "%s%d%d%x", i ? "/" : "", sn->touching, sn->hazard,
                          sn->touching ? uint32_t(sn->impact) : 0u);
            sens += b;
        }
        std::string smp;
        for (auto &x : G->samples) {
            char b[24];
            std::snprintf(b, sizeof b, "%s%x:%x", smp.empty() ? "" : ",", x.first, x.second);
            smp += b;
        }
        if (smp.empty())
            smp = "-";
        int fr = 0;
        s >> fr;
        static const int dumpScenario =
            std::getenv("MINOS_SHIP_DUMP_S") ? std::atoi(std::getenv("MINOS_SHIP_DUMP_S")) : -1;
        static const int dumpFrame =
            std::getenv("MINOS_SHIP_DUMP") ? std::atoi(std::getenv("MINOS_SHIP_DUMP")) : -1;
        gDump = scenarios == dumpScenario && fr == dumpFrame;
        std::snprintf(
            buf, sizeof buf,
            "%d %08x %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
            " %016" PRIx64 " %08x %08x %d %d %d %d %d %d %d %d %d %08x %08x %016" PRIx64
            " %016" PRIx64 " %d %d %d %d %u %s %s %s %s",
            fr, fu(ship->rot), du(ship->pos.x), du(ship->pos.y), du(ship->vel.x), du(ship->vel.y),
            du(ship->mapPos.x), du(ship->mapPos.y), fu(ship->thrust), fu(ship->thrustTarget),
            ship->isLanded, ship->dying, ship->type, ship->impact, ship->sinceContact,
            ship->rotateHeld, ship->thrustHeld, ship->deathTimer, ship->launchHold, fu(G->fuel),
            fu(G->shield), du(G->cameraY), du(G->scrollDelta.y), G->lives, G->landed,
            G->respawnLanded, G->landingBonus, rng.state(), sens.c_str(), smp.c_str(),
            listHash(G->debris).c_str(), listHash(G->shots).c_str());
        const std::string want = line.substr(4), got = buf;
        if (actual)
            actual << "out " << got << '\n';
        if (want != got) {
            if (failed < 3 || std::getenv("MINOS_TRACE_FAILURES")) {
                std::istringstream a(want), b(got);
                std::string x, y;
                int k = 0;
                while (a >> x && b >> y) {
                    if (x != y) {
                        std::fprintf(stderr, "scenario %d frame %d field %d: want %s got %s\n",
                                     scenarios, fr, k, x.c_str(), y.c_str());
                        break;
                    }
                    k++;
                }
            }
            failed++;
        }
    }
    std::printf("ship family: %d scenarios, %d frames checked, %d mismatched\n", scenarios, frames,
                failed);
    return failed ? 1 : 0;
}
