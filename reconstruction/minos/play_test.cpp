// Compare play-state transitions, entity output and HUD writes with captured
// original execution under scripted input, RNG and service responses.
#include "game_states.h"
#include "bge.h"
#include "cavern_launch.h"
#include "neon/entity_list.h"
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
constexpr Vec2 kSize{0.0625, 0.046875}; // the witness's sprite::getSize
struct Game : PlayState {
    std::vector<std::pair<int, int>> samples;
    std::string marks;
    void playSample(int id, int vol) override {
        samples.push_back({id, vol});
    }
    Entity *keep = nullptr; // the ship: freed by reset on the device, still read by the record
    void dispose(Entity *e) override {
        if (e != keep && !dynamic_cast<Sensor *>(e))
            delete e;
    } // sensors: read via the ship too
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
std::string colHex(const neon::colour &c) {
    char b[80];
    uint64_t u[4];
    double d[4] = {c.r, c.g, c.b, c.a};
    std::memcpy(u, d, 32);
    std::snprintf(b, sizeof b, "%016" PRIx64 "%016" PRIx64 "%016" PRIx64 "%016" PRIx64, u[0], u[1],
                  u[2], u[3]);
    return b;
}
// The cellGrid side of theGame's HUD, logged as the witness logs its intercepts.
struct TestHud : Hud {
    std::vector<std::string> log;
    neon::colour cur{0, 0, 0, 0}, palette[11];
    double shake[2] = {0, 0};
    uint8_t flags[2] = {0, 0};
    uint8_t cursor = 0, labelMode = 0;
    int32_t label = 0;                           // +0x297f, +0x297c, +0x2a84
    int32_t lineFont[4] = {}, lineState[4] = {}; // QuickMsg lines 0..3 +0xb8 / +0x98
    float rowDisplacement[129] = {};
    int rowWrites = 0;
    void setRowDisplacement(int row, float cells) override {
        rowDisplacement[row] = cells;
        ++rowWrites;
    }
    int touchLabel = 0; // the witness's scripted lpeek result
    TestHud() {
        for (auto &c : palette)
            c = neon::colour(0, 0, 0, 0);
    }
    void put(std::string s) { // the witness escapes newlines
        for (size_t k; (k = s.find('\n')) != std::string::npos;)
            s.replace(k, 1, "\\n");
        log.push_back(s);
    }
    std::string lab() const {
        return labelMode ? " l" + std::to_string(label) : std::string();
    }
    static std::string h64(double d) {
        char b[20];
        std::snprintf(b, sizeof b, "%016" PRIx64, du(d));
        return b;
    }
    static std::string h32(uint32_t u) {
        char b[12];
        std::snprintf(b, sizeof b, "%08x", u);
        return b;
    }
    void setPrintColour(const neon::colour &c) override {
        cur = c;
    }
    void printAt(int x, int y, const char *s) override {
        put("P " + std::to_string(x) + " " + std::to_string(y) + " " + (s ? s : "(null)") + " " +
            colHex(cur) + lab());
    }
    void goA8(int c, int r, int bx, int by) override {
        put("G " + std::to_string(c) + " " + std::to_string(r) + " " + std::to_string(bx) + " " +
            std::to_string(by));
    }
    void hlin(int bg, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) override {
        put("H " + std::to_string(bg) + " " + h32(c0) + " " + h32(c1) + " " + std::to_string(x0) +
            " " + std::to_string(x1) + " " + std::to_string(y) + " " + h32(fu(alpha)) + lab());
    }
    void clm() override {
        put("M");
    }
    void setCursor(bool on) override {
        cursor = on;
    }
    void setLabelMode(bool on) override {
        labelMode = on;
    }
    void setLabel(int l) override {
        label = l;
    }
    void setLine(int row, int font, int state) override {
        if (row < 0 || row > 3)
            return;
        lineFont[row] = font;
        if (state >= 0)
            lineState[row] = state;
    }
    int labelAt(double, double) override {
        return touchLabel;
    }
    uint64_t gridHash() const {
        uint64_t gh = 0xcbf29ce484222325ull;
        for (auto &c : palette) {
            double d[4] = {c.r, c.g, c.b, c.a};
            gh = fnv(d, 32, gh);
        }
        gh = fnv(shake, 16, gh);
        gh = fnv(flags, 2, gh);
        uint8_t b[2] = {cursor, labelMode};
        gh = fnv(b, 2, gh);
        gh = fnv(&label, 4, gh);
        for (int i = 0; i < 4; i++) {
            int32_t w[2] = {lineFont[i], lineState[i]};
            gh = fnv(w, 8, gh);
        }
        return gh;
    }
    void cPrintAt(int y, const char *s, const neon::colour &c) override {
        cur = c;
        put("C " + std::to_string(y) + " " + (s ? s : "(null)") + " " + colHex(cur));
    }
    void cll(int y) override {
        put("L " + std::to_string(y));
    }
    void cls() override {
        put("S");
    }
    void clattr() override {
        put("A");
    }
    void setSize(int c, int r) override {
        put("Z " + std::to_string(c) + " " + std::to_string(r));
    }
    void quickMsg(const char *s, Vec2 pos, Vec2 sc, int row,
                  int seq) override { // hold -1, rate 0.25f, step 0
        put(std::string("Q ") + s + " " + h64(pos.x) + " " + h64(pos.y) + " " + h64(sc.x) + " " +
            h64(sc.y) + " -1 " + std::to_string(row) + " " + std::to_string(seq) + " " +
            h32(fu(0.25f)) + " " + h32(fu(0.0f)));
    }
    void setPalette(const neon::colour *c, int n) override {
        for (int i = 0; i < n && i < 11; i++)
            palette[i] = c[i];
    }
    void setFlags(bool f, bool p) override {
        flags[0] = f;
        flags[1] = p;
    }
    void setShake(double x, double y) override {
        shake[0] = x;
        shake[1] = y;
    }
    std::string take() {
        std::string out;
        for (size_t i = 0; i < log.size(); i++)
            out += (i ? " | " : "") + log[i];
        log.clear();
        return out;
    }
};
struct Placeholder : Entity {
    void run() override {}
}; // the witness's stand-in for the ship after the last cavern
struct Launcher : MapHost { // theGame::launch: factory, then PlayState::launch
    Game *g = nullptr;
    StripMap *map = nullptr;
    void launch(char ch, Vec2 pos, int column, int row) override {
        LaunchContext ctx;
        ctx.cavern = g->cavern;
        ctx.rand = g->rand;
        ctx.spriteSize = [](int) { return kSize; };
        if (auto req = cavern::launch(*map, ctx, ch, pos, column, row))
            g->launch(*req);
    }
};
std::string listHash(Entity *head) {
    uint64_t h = 0xcbf29ce484222325ull;
    int n = 0;
    for (Entity *e = head; e; e = e->next, n++) {
        // Invader records contain seven words (28 bytes), Grunt five (20).
        // The old 16-byte buffer overflowed while hashing those classes.
        char c = '?';
        uint8_t extra[28] = {};
        size_t en = 0;
        bool enemy = false;
        auto put = [&](char k, const void *p, size_t sz, bool isEnemy) {
            if (sz > sizeof extra) {
                std::fprintf(stderr, "entity diagnostic record too large: %zu\n", sz);
                std::abort();
            }
            c = k;
            if (sz)
                std::memcpy(extra, p, sz);
            en = sz;
            enemy = isEnemy;
        };
        if (auto *s = dynamic_cast<Sensor *>(e)) {
            uint8_t b[2] = {uint8_t(s->touching), uint8_t(s->hazard)};
            put('S', b, 2, false);
        } else if (auto *g = dynamic_cast<Gun *>(e))
            put('G', &g->left, 4, false);
        else if (auto *t = dynamic_cast<ThrustParticle *>(e))
            put('T', &t->life, 4, false);
        else if (auto *b = dynamic_cast<Bspark *>(e))
            put('B', &b->age, 4, false);
        else if (auto *x = dynamic_cast<Explo *>(e))
            put('E', &x->t, 4, false);
        else if (auto *s2 = dynamic_cast<Shot *>(e))
            put('H', &s2->grow, 4, false);
        else if (auto *q = dynamic_cast<Turret *>(e)) {
            double a[2] = {q->aim.x, q->aim.y};
            put('t', a, 16, true);
        } else if (auto *invBull = dynamic_cast<InvBull *>(e))
            put('Q', &invBull->fall, 8, false);
        else if (auto *enemyBull = dynamic_cast<EnemyBull *>(e))
            put('b', &enemyBull->fall, 8, false);
        else if (dynamic_cast<CavernGoal *>(e))
            put('g', nullptr, 0, true);
        else if (dynamic_cast<OneUp *>(e))
            put('o', nullptr, 0, true);
        else if (auto *faller = dynamic_cast<Faller *>(e))
            put('f', &faller->accel, 8, true);
        else if (auto *saucer = dynamic_cast<Saucer *>(e))
            put('s', &saucer->phase, 4, true);
        else if (auto *scrambleRocket = dynamic_cast<ScrambleRocket *>(e)) {
            uint8_t l = scrambleRocket->launched;
            put('k', &l, scrambleRocket->spriteIndex == 0x37 ? 1 : 0, true);
        } // +0x194 only written for 0x37
        else if (auto *rocket = dynamic_cast<Rocket *>(e))
            put('r', &rocket->timer, 4, true);
        else if (auto *goat = dynamic_cast<Goat *>(e))
            put('v', &goat->state, 4, true);
        else if (auto *motherSheep = dynamic_cast<MotherSheep *>(e))
            put('m', &motherSheep->state, 4, true);
        else if (auto *wavingMinotaur = dynamic_cast<WavingMinotaur *>(e))
            put('w', &wavingMinotaur->state, 4, true);
        else if (dynamic_cast<ScRocket *>(e))
            put('x', nullptr, 0, true);
        else if (auto *mHeart = dynamic_cast<MHeart *>(e))
            put('h', &mHeart->age, 4, false);
        else if (auto *floatingScore = dynamic_cast<FloatingScore *>(e))
            put('p', &floatingScore->alpha, 4, false);
        else if (auto *dStar = dynamic_cast<DStar *>(e))
            put('d', &dStar->phase, 4, false);
        else if (auto *restartPointer = dynamic_cast<RestartPointer *>(e)) {
            double a[2] = {restartPointer->screen.x, restartPointer->screen.y};
            put('R', a, 16, false);
        } else if (auto *pauseButton = dynamic_cast<PauseButton *>(e))
            put('U', &pauseButton->glow, 4, false);
        else if (auto *osj = dynamic_cast<Osj *>(e))
            put('J', &osj->life, 4, false);
        else if (auto *wavingBonusMinotaur = dynamic_cast<WavingBonusMinotaur *>(e))
            put('W', &wavingBonusMinotaur->frame, 4, false);
        else if (auto *titleStar = dynamic_cast<TitleStar *>(e))
            put('Y', &titleStar->spin, 4, false);
        else if (auto *openFeintButton = dynamic_cast<OpenFeintButton *>(e)) {
            uint8_t nextB = openFeintButton->pressed;
            put('F', &nextB, 1, false);
        } else if (auto *asteroid = dynamic_cast<Asteroid *>(e)) {
            uint8_t nextB[8];
            std::memcpy(nextB, &asteroid->spin, 4);
            std::memcpy(nextB + 4, &asteroid->generation, 4);
            put('A', nextB, 8, true);
        } else if (auto *invader = dynamic_cast<Invader *>(e)) {
            int32_t w[7] = {invader->score, 0,
                            invader->group, invader->fireChance,
                            invader->dying, invader->timer,
                            invader->frames};
            std::memcpy(&w[1], &invader->stepX, 4);
            put('I', w, 28, true);
        } else if (auto *galag = dynamic_cast<Galag *>(e)) {
            float w[4] = {galag->angle, galag->angleRate, galag->phase, galag->phaseRate};
            put('L', w, 16, true);
        } else if (auto *grunt = dynamic_cast<Grunt *>(e)) {
            int32_t w[5] = {grunt->period, 0, grunt->group, grunt->timer, grunt->frames};
            std::memcpy(&w[1], &grunt->speed, 4);
            put('Z', w, 20, true);
        } else if (dynamic_cast<Slab *>(e))
            put('C', nullptr, 0, true);
        else if (dynamic_cast<Ball *>(e))
            put('O', nullptr, 0, true);
        else if (auto *tet = dynamic_cast<Tet *>(e)) {
            int32_t w[2] = {tet->hits, 0};
            std::memcpy(&w[1], &tet->spinStep, 4);
            put('V', w, 8, true);
        }
        double v[4] = {e->pos.x, e->pos.y, e->vel.x, e->vel.y};
        const int32_t sp = e->spriteIndex;
        h = fnv(&c, 1, h);
        h = fnv(v, 32, h);
        h = fnv(&e->rot, 4, h);
        h = fnv(&e->scale.x, 8, h);
        if (enemy)
            h = fnv(&sp, 4, h);
        h = fnv(extra, en, h);
        if (gDump) {
            std::fprintf(stderr, "ent %02x", uint8_t(c));
            auto hex = [](const void *p, size_t k) {
                for (size_t i = 0; i < k; i++)
                    std::fprintf(stderr, "%02x", static_cast<const uint8_t *>(p)[i]);
            };
            hex(v, 32);
            hex(&e->rot, 4);
            hex(&e->scale.x, 8);
            if (enemy)
                hex(&sp, 4);
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
        std::fprintf(stderr, "usage: %s fixtures/play-ios.txt\n", argv[0]);
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
    Launcher host;
    std::unique_ptr<Game> G;
    Ship *ship = nullptr;
    neon::DarwinRandom rng;
    TestHud hud;
    std::string line, inLine, hudFrame, hudWant;
    int frames = 0, failed = 0, scenarios = 0, hudFailed = 0, prevT0 = 0;
    auto bgdraw = [&] { G->bgdrawState({0.0, G->cameraY}); }; // _bgdraw's state part
    auto checkHud = [&](int fr, const std::string &got) {
        if (got == hudWant)
            return;
        if (hudFailed < 3)
            std::fprintf(stderr, "scenario %d frame %d hud:\n want %s\n got  %s\n", scenarios, fr,
                         hudWant.c_str(), got.c_str());
        hudFailed++;
    };
    std::string pendingHud;
    int pendingFrame = -2;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream s(line);
        std::string kind;
        s >> kind;
        if (actual && (kind == "scenario" || kind == "menu"))
            actual << line << '\n';
        if (kind == "hud") { // the HUD stream of the preceding frame (or -1: newLevel)
            int fr;
            s >> fr;
            std::string rest;
            std::getline(s, rest);
            hudWant = rest.empty() ? "" : rest.substr(1);
            checkHud(fr, fr == pendingFrame ? pendingHud : std::string());
            pendingFrame = -3;
            continue;
        }
        if (kind == "scenario") {
            int cav, n, seed, lives = 3, approach = 0, saved = 0, goal = 0;
            std::string sy, fuel = "3f800000", shield = "3f800000", rx = "0", ry = "0";
            s >> cav >> n >> sy >> seed >> fuel >> shield >> rx >> ry >> lives >> approach >>
                saved >> goal;
            const CavernDefinition &cv = kCavernDefinitions[cav];
            G = std::make_unique<Game>();
            rng.seed(uint32_t(seed));
            hud = TestHud();
            // theGame::newLevel, map part (the core's newLevel does the same)
            map = std::make_unique<StripMap>();
            profile = std::make_unique<WallProfile>();
            flash = FlashChannels{};
            map->setCavern(&cv, cv.zoneLength);
            map->installFlashTables();
            map->bind(&host, profile.get(), &flash, &w9dc);
            hud.cls();
            hud.clattr();
            buildWallProfile(cv, map->sectorLength(), *profile);
            G->rand = &rng;
            G->map = map.get();
            G->flash = &flash;
            G->cavern = &cv;
            G->cavernIndex = cav;
            G->spriteSize = [](int) { return kSize; };
            G->hud = &hud;
            G->volume = 8;
            G->lives = 3;
            G->runState = 4;
            G->drawMode = 2;
            G->bgLayer = 2;
            host.g = G.get();
            host.map = map.get();
            G->createShip = [&] {
                ship = new Ship;
                ship->init(*G);
                G->addEntity(G->shipList, ship);
                G->keep = ship;
                return static_cast<Entity *>(ship);
            };
            G->pausePressed = [&] { G->marks += 'U'; };
            G->newLevelHook = [&] { G->marks += 'N'; };
            G->newLevelState();
            // scenario overrides (as the witness applies them after newLevel)
            {
                uint32_t u = uint32_t(std::stoul(fuel, nullptr, 16));
                std::memcpy(&G->fuel, &u, 4);
                u = uint32_t(std::stoul(shield, nullptr, 16));
                std::memcpy(&G->shield, &u, 4);
            }
            G->screenY = hd(sy);
            if (hd(rx) != 0.0 || hd(ry) != 0.0)
                G->respawnPos = {hd(rx), hd(ry)};
            G->lives = lives;
            G->approachTimer = approach;
            G->minosSaved = saved;
            if (goal) {
                G->flag203808 = true;
                G->byte31e = true;
            }
            bgdraw();
            pendingHud = hud.take();
            pendingFrame = -1;
            prevT0 = 0;
            scenarios++;
            continue;
        }
        if (kind == "menu") { // a menu scenario: from a menu state, no newLevel
            int n, seed, start, kd, furthest, bc, bl, bs, hiscore, finished, bonusTotal, scoreBase,
                sel;
            s >> n >> seed >> start >> kd >> furthest >> bc >> bl >> bs >> hiscore >> finished >>
                bonusTotal >> scoreBase >> sel;
            G = std::make_unique<Game>();
            rng.seed(uint32_t(seed));
            hud = TestHud();
            map = std::make_unique<StripMap>();
            flash = FlashChannels{};
            G->rand = &rng;
            G->map = map.get();
            G->flash = &flash;
            G->spriteSize = [](int) { return kSize; };
            G->hud = &hud;
            G->volume = 8;
            G->lives = 0;
            G->shipKind = kd;
            G->tipIndex = -1;
            G->drawMode = -1;
            G->bgLayer = -1;
            G->muted = true;
            G->awardEligibilityGate = 1; // constructor 0x59c48
            G->runState = start;
            G->selCavern[kd] = sel;
            G->prefs[kd * 100 + 1] = furthest;
            G->prefs[kd * 100] = hiscore;
            if (bs) {
                G->prefs[kd * 100 + bc + 0x10] = bs;
                G->prefs[kd * 100 + bc + 0x30] = bl;
            }
            if (start == 8) { // as goGameOver leaves it
                G->endScreenCountdown = 1000;
                G->drawMode = 3;
                G->buttonLatch = true;
                G->running = true;
                if (finished) {
                    G->gameFinished = true;
                    G->frameCount = 1;
                    G->bonusTotal = bonusTotal;
                    G->scoreBase = scoreBase;
                    G->scoreTotal = scoreBase;
                    G->addEntity(G->shipList, new Placeholder);
                }
            }
            G->newLevelHook = [&] { G->marks += 'N'; };
            G->pausePressed = [&] { G->marks += 'U'; };
            pendingHud.clear();
            pendingFrame = -2;
            prevT0 = 0;
            ship = nullptr;
            scenarios++;
            continue;
        }
        if (kind == "in") {
            int fr, t0, t1, btn, label = 0;
            std::string p[6];
            s >> fr >> t0 >> t1 >> p[0] >> p[1] >> p[2] >> p[3] >> p[4] >> p[5] >> btn >> label;
            hud.touchLabel = label;
            if (pendingFrame == -1 && !pendingHud.empty()) {
                hudWant = "";
                checkHud(-1, pendingHud);
            }
            G->touchDown[0] = t0;
            G->touchDown[1] = t1;
            G->touchPos[0][0] = hd(p[0]);
            G->touchPos[0][1] = hd(p[1]);
            G->touchPos[1][0] = hd(p[2]);
            G->touchPos[1][1] = hd(p[3]);
            G->analog = hd(p[4]);
            G->lateralTilt = hd(p[5]);
            G->prefsButtons = uint32_t(btn);
            if (t0 && !prevT0) {
                G->touchStartX[0] = hd(p[0]);
                G->touchStartY[0] = hd(p[1]);
                G->touchPressed[0] = true;
                G->touchTimer[0] = 0;
            } else if (!t0)
                G->touchPressed[0] = false;
            prevT0 = t0;
            G->samples.clear();
            G->marks.clear();
            const bool titleFrame = G->runState == 3;
            const int writesBefore = hud.rowWrites;
            G->runFrame();
            if (titleFrame &&
                (hud.rowDisplacement[2] != .5f || hud.rowWrites != writesBefore + 1)) {
                std::fprintf(stderr, "title row displacement not restored\n");
                return 1;
            }
            if (G->marks.find('N') == std::string::npos)
                bgdraw();
            if (std::getenv("MINOS_PLAY_TRACE")) {
                uint32_t u;
                std::memcpy(&u, &G->fuelShown, 4);
                std::fprintf(
                    stderr, "trace %d state %d phase %d timer %d held %d saved %d fuelShown %08x\n",
                    fr, G->runState, G->bonusPhase, G->bonusTimer, G->fireHeld, G->minosSaved, u);
            }
            pendingHud = hud.take();
            pendingFrame = fr;
            continue;
        }
        if (kind == "mout") {
            frames++;
            int fr = 0;
            s >> fr;
            static const int dumpScenarioM =
                std::getenv("MINOS_PLAY_DUMP_S") ? std::atoi(std::getenv("MINOS_PLAY_DUMP_S")) : -1;
            static const int dumpFrameM =
                std::getenv("MINOS_PLAY_DUMP") ? std::atoi(std::getenv("MINOS_PLAY_DUMP")) : -1;
            gDump = scenarios == dumpScenarioM && fr == dumpFrameM;
            const std::string hudText = pendingFrame == fr ? pendingHud : std::string();
            int hudCount = 0;
            if (!hudText.empty()) {
                hudCount = 1;
                for (size_t k = 0; k + 2 < hudText.size(); k++)
                    if (hudText.compare(k, 3, " | ") == 0)
                        hudCount++;
            }
            std::string hj = hudText;
            {
                size_t k;
                while ((k = hj.find(" | ")) != std::string::npos)
                    hj.replace(k, 3, "\n");
            }
            const uint64_t hh =
                hudText.empty() ? 0 : fnv(hj.data(), hj.size(), 0xcbf29ce484222325ull);
            std::string smp;
            for (auto &x : G->samples) {
                char b[24];
                std::snprintf(b, sizeof b, "%s%x:%x", smp.empty() ? "" : ",", x.first, x.second);
                smp += b;
            }
            if (smp.empty())
                smp = "-";
            uint64_t th = 0xcbf29ce484222325ull;
            th = fnv(map->tiles.data(), size_t(4 * (map->rows + 1) * (map->columns + 1)), th);
            {
                double fr2[2] = {map->fraction.x, map->fraction.y};
                th = fnv(fr2, 16, th);
            }
            for (int i = 0; i < 3; i++) {
                const auto &c = G->stmPalette[i];
                double d[4] = {c.r, c.g, c.b, c.a};
                th = fnv(d, 32, th);
            }
            const uint64_t ph = fnv(G->prefs, 0xc80, 0xcbf29ce484222325ull);
            char buf[2048];
            std::snprintf(
                buf, sizeof buf,
                "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %08x %d %d %d %d %016" PRIx64
                " %u %s %s %s %s %s %s %s %s %d:%016" PRIx64 " %016" PRIx64 " %016" PRIx64
                " %016" PRIx64 " %s",
                fr, G->runState, G->drawMode, G->bgLayer, G->keyCode, G->shipKind, G->selCavern[0],
                G->selCavern[1], G->selCavern[2], G->selCavern[3], G->maxCavern, G->cavernIndex,
                G->lives, G->scoreTotal, int(G->goalState), G->endScreenCountdown, G->infoPage,
                fu(G->bonusT), G->restartBest, G->muted, G->frameCount, G->tipIndex,
                du(G->scrollDelta.y), rng.state(), smp.c_str(), listHash(G->list2dc).c_str(),
                listHash(G->debris).c_str(), listHash(G->shots).c_str(),
                listHash(G->list2d8).c_str(), listHash(G->touchList).c_str(),
                listHash(G->bounceList).c_str(), listHash(G->list2ec).c_str(), hudCount, hh,
                hud.gridHash(), th, ph, G->marks.empty() ? "-" : G->marks.c_str());
            const std::string want = line.substr(5), got = buf;
            if (actual)
                actual << "mout " << got << '\n';
            if (want != got) {
                if (failed < 3 || std::getenv("MINOS_TRACE_FAILURES")) {
                    std::istringstream a(want), b(got);
                    std::string x, y;
                    int k = 0;
                    while (a >> x && b >> y) {
                        if (x != y) {
                            std::fprintf(stderr,
                                         "scenario %d frame %d menu field %d: want %s got %s\n",
                                         scenarios, fr, k, x.c_str(), y.c_str());
                            break;
                        }
                        k++;
                    }
                }
                failed++;
            }
            continue;
        }
        if (kind != "out")
            continue;
        frames++;
        if (pendingFrame >= 0 && !pendingHud.empty()) {
        } // compared when its hud line arrives
        char buf[4096];
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
            std::getenv("MINOS_PLAY_DUMP_S") ? std::atoi(std::getenv("MINOS_PLAY_DUMP_S")) : -1;
        static const int dumpFrame =
            std::getenv("MINOS_PLAY_DUMP") ? std::atoi(std::getenv("MINOS_PLAY_DUMP")) : -1;
        gDump = scenarios == dumpScenario && fr == dumpFrame;
        // HUD stream, cellGrid writes, flash timers, message slots
        const std::string hudText = pendingFrame == fr ? pendingHud : std::string();
        int hudCount = 0;
        if (!hudText.empty()) {
            hudCount = 1;
            for (size_t k = 0; k + 2 < hudText.size(); k++)
                if (hudText.compare(k, 3, " | ") == 0)
                    hudCount++;
        }
        std::string hudJoined;
        {
            std::string t = hudText;
            size_t k;
            while ((k = t.find(" | ")) != std::string::npos)
                t.replace(k, 3, "\n");
            hudJoined = t;
        }
        const uint64_t hh =
            hudText.empty() ? 0 : fnv(hudJoined.data(), hudJoined.size(), 0xcbf29ce484222325ull);
        uint64_t gh = 0xcbf29ce484222325ull;
        if (gDump) {
            std::fprintf(stderr, "grid");
            for (auto &c : hud.palette) {
                double d[4] = {c.r, c.g, c.b, c.a};
                std::fprintf(stderr, " ");
                for (int k = 0; k < 32; k++)
                    std::fprintf(stderr, "%02x", reinterpret_cast<uint8_t *>(d)[k]);
            }
            std::fprintf(stderr, " shake %g %g flags %d %d\n", hud.shake[0], hud.shake[1],
                         hud.flags[0], hud.flags[1]);
        }
        gh = hud.gridHash();
        uint64_t fh = 0xcbf29ce484222325ull;
        for (auto *a : {G->flashOn, G->flashCountReload, G->flashOff, G->flashTimer, G->flashCount})
            fh = fnv(a, 16, fh);
        fh = fnv(flash.lit, 4, fh);
        fh = fnv(flash.watched, 4, fh);
        fh = fnv(flash.seen, 4, fh);
        uint64_t sh = 0xcbf29ce484222325ull;
        for (auto &m : G->slots) {
            int32_t w[3] = {m.count, m.row, m.colour};
            sh = fnv(w, 12, sh);
            const char *t = m.text ? m.text : "(null)";
            sh = fnv(t, std::strlen(t) + 1, sh);
        }
        char tail[256];
        std::snprintf(tail, sizeof tail,
                      " %s %d %d %d:%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %s",
                      listHash(G->list2dc).c_str(), G->runState, G->gameOver, hudCount, hh, gh, fh,
                      sh, G->marks.empty() ? "-" : G->marks.c_str());
        std::snprintf(buf, sizeof buf,
                      "%d %08x %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
                      " %016" PRIx64 " %016" PRIx64
                      " %08x %08x %d %d %d %d %d %d %d %d %d %08x %08x %016" PRIx64 " %016" PRIx64
                      " %d %d %d %d %u %s %s %s %s %d %s %s %s%s",
                      fr, fu(ship->rot), du(ship->pos.x), du(ship->pos.y), du(ship->vel.x),
                      du(ship->vel.y), du(ship->mapPos.x), du(ship->mapPos.y), fu(ship->thrust),
                      fu(ship->thrustTarget), ship->isLanded, ship->dying, ship->type, ship->impact,
                      ship->sinceContact, ship->rotateHeld, ship->thrustHeld, ship->deathTimer,
                      ship->launchHold, fu(G->fuel), fu(G->shield), du(G->cameraY),
                      du(G->scrollDelta.y), G->lives, G->landed, G->respawnLanded, G->landingBonus,
                      rng.state(), sens.c_str(), smp.c_str(), listHash(G->debris).c_str(),
                      listHash(G->shots).c_str(), G->scoreTotal, listHash(G->touchList).c_str(),
                      listHash(G->bounceList).c_str(), listHash(G->list2ec).c_str(), tail);
        const std::string want = line.substr(4), got = buf;
        if (actual)
            actual << "out " << got << '\n';
        if (gDump)
            std::fprintf(stderr, "frame-state scenario %d frame %d\nwant %s\ngot  %s\n", scenarios,
                         fr, want.c_str(), got.c_str());
        if (want != got) {
            if (failed == 0 && std::getenv("MINOS_PLAY_SHOW"))
                std::fprintf(stderr, "want %s\ngot  %s\n", want.c_str(), got.c_str());
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
    std::printf(
        "whole play: %d scenarios, %d frames checked, %d mismatched, %d HUD streams mismatched\n",
        scenarios, frames, failed, hudFailed);
    return failed || hudFailed ? 1 : 0;
}
