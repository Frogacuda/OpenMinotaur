// game_main.cpp - spaceWars, the game itself: the attract sequence, menus,
// game setup (ships, tanks, jets), level flow, scoring, the HUD and the
// info / story scrollers, and the per-frame update of every entity list.
#include "neon/entity_list.h"
#include "game.h"
#include "game_entities.h"
#include "neon_audio.h"
#include "neon_modules.h"
#include "grid_menus.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace NEON_GAME {
namespace Neon_Internals {

namespace {

// Banner colours per game mode (solar, deep space, tanks, jets) for the
// info screen and for the title / start screens.
const uint32_t kInfoBanner[6] = {0x8000c0ffu, 0xc00008ffu, 0x208020ffu,
                                 0x208020ffu, 0x208020ffu, 0x005090ffu};
const uint32_t kTitleBanner[6] = {0x400080ffu, 0xc00008ffu, 0x208020ffu,
                                  0x208020ffu, 0x208020ffu, 0x005090ffu};

// Attract: the logo fades in through these colours, then the bars.
const uint32_t kTitleFade[8] = {0x0000ffffu, 0xff0000ffu, 0x00ff00ffu, 0xff00ffffu,
                                0x00ffffffu, 0xffff00ffu, 0xffff80ffu, 0xffffffffu};
const uint32_t kTitleBars[7] = {0xff200080u, 0xff800080u, 0xffff0080u, 0x40ff0080u,
                                0x0080ff80u, 0x2000ff80u, 0x6000c080u};
const int kDings[7] = {28, 29, 30, 31, 32, 31, 30};
const int kGruntTable[6] = {1, 4, 5, 6, 7, 7};

// Menu cell colours: selected / unselected button.
constexpr uint32_t kSelOn = 0xf210007fu;
constexpr uint32_t kSelOff = 0x0000307fu;

constexpr int kHumSample[2] = {12, 13};
int g_humRefs[2] = {0, 0}; // saucerSound()::refcount

// Text line `i` of a scroller page, nullptr for a blank line; past either
// end reads as the "^" end marker (the original read the neighbouring data).
const char *pageLine(const char *const *page, int lines, int i) {
    if (i < 0 || i >= lines)
        return "^";
    return page[i];
}

void deleteList(entity *&list) {
    neon::entities::destroyList(list, [](entity *e) { delete e; });
}

// Runs every entity of `list` and drops the dead ones.
void runList(entity *&list) {
    neon::entities::runAndCull(list, [](entity *e) { delete e; });
}

// The deep-space / jets arena: three nested rings of blocks.
void jetsArena(cellGrid *g) {
    g->clattr();
    g->qhlin_a(1, 0x707080ffu, 8, 0xe, 8, 1.0f);
    g->qhlin_a(1, 0x707080ffu, 6, 0x10, 9, 1.0f);
    g->qhlin_a(1, 0x707080ffu, 5, 0x11, 10, 1.0f);
}

} // namespace

// ================================================================ audio ====

void spaceWars::playSample(int n) const {
    AudioSystemPlaySample(n, volume << 12, 0);
}

void spaceWars::playGrunt() const {
    unsigned a = unsigned(neonRand()), b = unsigned(neonRand());
    playSample(kGruntTable[(b & 1) + (a & 3)]);
}

// A saucer appears: start its hum if none of its size is humming yet.
void spaceWars::saucerSound(int big) {
    if (!demo && g_humRefs[big] == 0)
        AudioSystemPlaySample(kHumSample[big], volume << 10, 1000);
    g_humRefs[big]++;
}

// A saucer leaves or dies (big < 0: silence both hums).
void spaceWars::releaseSaucerSound(int big) {
    if (big < 0) {
        AudioSystemStopSample(kHumSample[0]);
        g_humRefs[0] = 0;
        AudioSystemStopSample(kHumSample[1]);
        g_humRefs[1] = 0;
    } else if (big < 2 && g_humRefs[big] > 0 && --g_humRefs[big] == 0) {
        AudioSystemStopSample(kHumSample[big]);
    }
}

// ========================================================== construction ====

spaceWars::spaceWars() {
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 8; i++) {
            startBonus[t][i] = 15000 * i;
        }
    defaultHighScores();
    inited = false;
    dingSent = false;
    shellCommand = 0;
    demo = true;
    planetsOn = minotaursOn = sunOn = shields3 = true;
    postBoard = 0;
    survival = false;
    bouncyShots = true;

    // The original asks UIDevice: an iPad allows four players.
    isPad = false;
    maxPlayers = 2;
    reset(0);

    frameCount = 0x82;
    lastMode = 0;
    lastPlayers = 1;
    volume = 4;
    lastStartLevel = 0;
    state = 1;
    subMode = 0;
    mode = 0;
    goalsDone = 0;
    maze = 1;
    nPlayers = 1;
    warp = 0;
    level = 0;
    startLevel = 0;
    levelEnding = false;
    startBonusDue = false;
    paused = false;
    pauseArmed = false;
}

spaceWars::~spaceWars() {
    for (entity **l :
         {&players, &planets, &list154, &bullets, &particles, &minotaurs, &saucers, &enemyShots})
        deleteList(*l);
}

// Deletes the entity lists whose bit in `keep` is clear (1 players,
// 2 planets, 4 list154, 8 bullets, 0x10 particles, 0x20 minotaurs,
// 0x40 saucers and mines, 0x80 enemy shots); keep 0 also stops the game.
void spaceWars::reset(int keep) {
    releaseSaucerSound(-1);
    if (!(keep & 1))
        deleteList(players);
    if (!(keep & 2)) {
        deleteList(planets);
        sun = nullptr; // (left dangling in the original)
    }
    if (!(keep & 4))
        deleteList(list154);
    if (!(keep & 8))
        deleteList(bullets);
    if (!(keep & 0x10))
        deleteList(particles);
    if (!(keep & 0x20))
        deleteList(minotaurs);
    if (!(keep & 0x40))
        deleteList(saucers);
    if (!(keep & 0x80))
        deleteList(enemyShots);
    if (keep == 0)
        running = false;
    saucerCount = 0;
}

// ============================================================ game setup ====

void spaceWars::newGame() {
    reset(0);
    running = true;

    sun = nullptr;
    level = (demo || survival) ? 0 : startLevel;
    switch (mode) {
    case 0:
    case 1: {
        saucerCount = 0;
        if (!demo && nPlayers == 1 && mode < 2)
            startBonusDue = true;
        sunOn = mode == 0;
        warp = 0;
        massScale = 1.0f;
        subMode = 0;
        rescued = 0;

        massGrowth = 1.002f;
        levelsNoRescue = 0;
        levelsCleared = 0;
        int lv = survival ? 1 : 3;
        for (int i = 0; i < 4; i++) {
            shields[i] = 10;
            scores[i] = 0;
            lives[i] = lv;
        }
        bgColour = 0;
        int i = 0;
        for (; i < nPlayers; i++) {
            spaceCraft *s = new spaceCraft();
            s->init(this, i);
            addEntity(s, &players);
        }
        if (sunOn) {
            theSun *s = new theSun();
            s->mass = planetsOn ? 150000.0f : 600000.0f;
            s->hp = 5;
            s->size = 0.02f;
            s->game = this;
            sun = s;
            addEntity(s, &planets);
        }
        joyZones = i;
        break;
    }
    case 2:
        subMode = 1;
        timeLeft = 0x18c0;
        makeMaze();
        for (int i = 0; i < 4; i++) {
            shields[i] = 10;
            scores[i] = 0;
            lives[i] = 0;
        }
        bgColour = 0x3f00003fu;
        break;
    case 3:
        subMode = 2;
        for (int i = 0; i < 4; i++) {
            shields[i] = 10;
            scores[i] = 0;
            lives[i] = 9;
        }
        bgColour = 0x00105f3fu;
        jetsArena(grid);
        break;
    }
    newLevel();
}

void spaceWars::newLevel() {
    levelEnding = false;
    levelDone = false;
    if (subMode == 1) {
        int n = nPlayers < 2 ? 2 : nPlayers;
        for (int i = 0; i < n; i++) {
            tank *t = new tank();
            t->init(this, i);
            if (i > nPlayers - 1) { // the CPU fills the second seat
                t->autopilot = true;
                t->type = 7;
            }
            addEntity(t, &players);
        }
        joyZones = nPlayers;
    } else if (subMode == 2) {
        int n = nPlayers < 2 ? 2 : nPlayers;
        for (int i = 0; i < n; i++)
            for (int slot = 0; slot < 3; slot++) {
                jet *j = new jet();
                if (i > nPlayers - 1) {
                    j->autopilot = true;
                    j->type = 7;
                }
                j->init(this, i, slot);
                addEntity(j, &players);
            }
        joyZones = nPlayers;
    } else if (subMode == 0) {
        minesHit = 1;
        blackHole = false;
        maxSaucers = level >> 3;
        saucerTimer = 300 - level * 16;
        planetsSpawned = 0;
        rescuedThisLevel = 0;
        int sh = shields3 ? 3 : 9;
        for (int &s : shields)
            s = sh;
        for (entity *e = players; e; e = e->next)
            e->hp = sh;
        if (planetsOn) {
            int n = (level == 1) + (level >> 2) + 4;
            if (n > 9)
                n = 10;
            for (int i = 0; i < n; i++) {
                thePlanet *p = new thePlanet();
                p->type = 2;
                p->mass = 20000.0f;
                p->generation = 2;
                p->hp = 15;
                p->wrapWidth = p->wrapHeight = 1.6;
                p->elasticity = 0.3f;
                p->game = this;
                p->reSpawn();
                addEntity(p, &planets);
            }
        }
    }
}

// The tanks maze: walls mirrored into all four quarters.
void spaceWars::makeMaze() {
    cellGrid *g = grid;
    g->clattr();
    constexpr uint32_t c = 0xf0c002ffu;
    if (maze == 1) {
        g->qvlin_a(1, c, 4, 7, 0x13, 0.4f);
        g->qhlin_a(1, c, 10, 0xf, 0xb, 0.4f);
        g->qvlin_a(1, c, 9, 0xb, 5, 0.4f);
        g->qhlin_a(1, c, 4, 5, 8, 0.4f);
    } else if (maze == 2) {
        g->qvlin_a(1, c, 8, 0xb, 5, 0.4f);
        g->qhlin_a(1, c, 4, 5, 7, 0.4f);
        g->qhlin_a(1, c, 5, 7, 3, 0.4f);
        g->qhlin_a(1, c, 8, 9, 0xb, 0.4f);
        g->qhlin_a(1, c, 0xc, 0xe, 5, 0.4f);
        g->qhlin_a(1, c, 0xc, 0xc, 6, 0.4f);
        g->qvlin_a(1, c, 1, 2, 0x13, 0.4f);
    }
}

// =========================================================== spawning =====

// A background star drifting out (faster at the end of a level).
void spaceWars::runStarField() {
    double chance = levelDone ? 80.0 : 20.0;
    if (oxRNG::rand(100.0) > chance)
        return;
    starFieldStar *s = new starFieldStar();
    s->game = this;
    s->size = 0.006f;
    s->col.white(1.0);
    s->pos.x = oxRNG::rand(-0.5, 0.5);
    s->pos.y = oxRNG::rand(-0.5, 0.5);
    double extra = levelDone ? 0.01 : 0.0;
    s->spriteIndex = 0x8c;
    s->fade = float(oxRNG::rand(0.002) + 0.005 + extra);
    addEntity(s, &particles);
}

// Ships: now and then a minotaur drifts in from the edge. Tanks: one
// appears somewhere on the grid after a countdown.
void spaceWars::randomMinotaur() {
    if (subMode == 0) {
        double chance = levelDone ? double(0.8f) : double(0.2f);
        if (oxRNG::rand(100.0) > chance)
            return;
        collectible *m = new collectible();
        m->pos = vec2d(0.0, 0.7);
        m->spin = float(oxRNG::rand(-0.01, 0.01));
        m->pos.rotateBy(oxRNG::rand() * 6.2831853072);
        m->vel = m->pos * double(-0.0008f);
        m->mass = 200.0f;
        m->size = 0.03f;
        m->drag = vec2d(1.0, 1.0);
        m->game = this;
        m->wrapWidth = m->wrapHeight = 1.3;
        m->spriteIndex = 0x97;
        m->type = 4;
        m->scaleY = 1.6;
        addEntity(m, &minotaurs);
    } else if (subMode == 1 && !demo) {
        if (oxRNG::rand(100.0) > 0.2)
            return;
        tankMinotaur *m = new tankMinotaur();
        m->game = this;
        m->countdown = 0x240;
        m->spriteIndex = 0x97;
        m->type = 4;
        m->scaleY = 1.8;
        m->mass = 200.0f;
        m->size = 0.02f;
        m->pos.x = oxRNG::rand(-0.4, 0.4);
        m->pos.y = oxRNG::rand(-0.3, 0.3) + 0.20833333333333334;
        cellGrid::CellPos c = grid->getPos(m->pos);
        m->cellX = c.x;
        m->cellY = c.y;
        addEntity(m, &minotaurs);
    }
}

// Saucers fly in from the left or right while the level lasts, more and
// tougher ones as the level number rises.
void spaceWars::randomSaucer() {
    static const int kSprites[8] = {0x9d, 0x9e, 0xa9, 0xaa, 0xab, 0xab, 0xac, 0x9b};
    static const float kScaleX[8] = {1.0f, 1.2f, 1.4f, 1.0f, 1.0f, 0.85f, 1.5f, 1.3f};
    static const float kScaleY[8] = {1.0f, 1.2f, 0.8f, 1.0f, 1.0f, 0.85f, 1.5f, 1.3f};
    static const int kColour[8] = {5, 9, 7, 8, 9, 5, 1, 2};
    static const unsigned kMove[8] = {4, 4, 0x1c, 3, 0xd, 0xd, 3, 3};
    static const float kAccel[8] = {0.0f, 0.0f, 0.2f, 0.01f, 0.02f, 0.03f, 0.02f, 0.04f};
    if (levelDone)
        return;
    if (saucerCount < 0)
        saucerCount = 0;
    if (saucerCount > maxSaucers)
        return;
    if (saucerTimer >= 1) {
        saucerTimer--;
        return;
    }
    int d = (level & 3) + (level >> 4) + ((level & 7) >> 1);
    if (!(oxRNG::rand(double(float(200 - 16 * d))) <= 0.4))
        return;
    saucerTimer = 300 - 32 * d;
    saucerCount++;
    int big = (neonRand() & 0x1f) < (level & 0xf) ? 1 : 0;
    saucerSound(big);
    int side = neonRand();
    saucer *s = new saucer();
    s->kind = (level >> 2) & 3;
    s->big = big;
    s->elasticity = 0.8f;
    // (Integer division in the original: both are always the base speed.)
    s->maxVel.x = double(float(((level >> 2) % 5) / 5)) * 0.004 + 0.004;
    s->maxVel.y = double(float(((level >> 1) % 3) / 3)) * 0.003 + 0.003;
    s->game = this;
    s->pos.y = oxRNG::rand(-0.3, 0.3);
    s->pos.x = -0.6;
    s->vel.x = 0.002;
    int k = s->kind * 2 + big;
    s->scaleX = double(kScaleX[k]);
    s->scaleY = double(kScaleY[k]);
    s->colourType = kColour[k];
    s->spriteIndex = kSprites[k];
    s->moveFlags = kMove[k];
    s->accel = kAccel[k];
    s->burst = 0;
    s->vel.y = oxRNG::rand(0.001, 0.002);
    s->moveTimer = oxRNG::irand(50.0) + 0x32;
    s->drag = vec2d(1.0, 1.0);
    s->type = 5;
    s->col.green(1.0);
    s->hp = 0;
    s->aimedCount = 0;
    s->shotTimer = s->shotReload = 100 - 5 * level;
    if (side & 1) {
        s->pos.x = -s->pos.x;
        s->vel.x = -s->vel.x;
    }
    if (neonRand() & 1)
        s->vel.y = -s->vel.y;
    if (big) {
        s->size = float(double(s->size) * 0.7);
        s->vel = s->vel * 1.3;
    }
    s->wrapWidth = 1.5;
    s->wrapHeight = 1.0;
    addEntity(s, &saucers);
}

// ============================================================== scoring ====

void spaceWars::score(int points, int player) {
    static const int kExtraLife[4] = {10000, 50000, 100000, 500000};
    int old = scores[player];
    int now = old + points;
    scores[player] = now;
    if (now > 999999 && !(goalsDone & 0x40))
        achieveGoal(6);
    if (survival)
        return;
    bool extra = false;
    for (int t : kExtraLife) {
        if (now < t)
            return;
        if (old < t) {
            extra = true;
            break;
        }
    }
    if (!extra) {
        for (int t = 1000000; t <= now; t += 1000000)
            if (old < t) {
                extra = true;
                break;
            }
        if (!extra)
            return;
    }
    int l = lives[player];
    if (l >= 10)
        return;
    lives[player] = l + 1;
    if (l + 1 == 10)
        achieveGoal(9);
    if (!demo)
        playSample(0x14);
    entity *p = players;
    while (p && p->group != player + 1)
        p = p->next;
    if (!p)
        return; // (the original walks off the end of the list)
    _particle *x = new _particle();
    x->pos = p->pos;
    if (p->pos.y < 0.0)
        x->pos.y += double(p->size + p->size);
    else
        x->pos.y += double(p->size * -2.0f);
    x->vel = x->pos * -0.01;
    x->spriteIndex = 0xb5;
    x->fromDynamic = player + 1;
    x->toDynamic = player + 1;
    x->scaleX = 3.0;
    x->scaleY = 0.8;
    x->size = p->size * 0.5f;
    x->sizeEnd = 12.0f;
    addEntity(x, &particles);
}

void spaceWars::achieveGoal(int n) {
    static const char *const kGoals[10] = {
        "~9999-= LOVEABULL =-",    "~9999-= MANIC MINER =-",   "~9999-= VALUABULL =-",
        "~9999-= RESPONSABULL =-", "~9999-= IRRESISTABULL =-", "~9999-= MATADOR =-",
        "~9999-= MILLIONAIRE =-",  "~9999-= WARPED =-",        "~9999-= HORNS EXALTED =-",
        "~9999-= MAXOTAUR =-",
    };
    if (demo)
        return;
    int k = nPlayers < 2 ? mode : nPlayers;
    if (k >= 2 || (goalsDone & (1u << n)))
        return;
    shellCommand = int8_t(n + SHELL_ACHIEVEMENT);
    if (!levelDone)
        grid->cls();
    grid->cPrintAt(0x13, "~7599*** NICE PLAY ***");
    grid->cPrintAt(0x15, kGoals[n]);
    grid->visible = true;
    msgTimer = 0xf0;
    goalsDone |= 1u << n;
    if (!demo)
        AudioSystemPlaySample(0x12, volume << 12, 0);
}

// Single-player ships games started past level 1 get the start bonus at
// the end of their first level.
int spaceWars::doStartBonus() {
    if (nPlayers >= 2 || mode >= 2 || survival || !startBonusDue)
        return 0;
    if (level > 0 && level < 8) {
        int bonus = startBonus[mode][level];
        score(bonus, 0);
        if (entity *p = players) {
            scoreExplosion *x = new scoreExplosion();
            x->count = 1;
            x->colourType = 5;
            x->from.yellow(1.0);
            x->to.red(0.0);
            x->from.a = 1.0;
            x->to.a = 0.5;
            x->rot = p->rot;
            x->seed = unsigned(neonRand());
            x->pos = p->pos;
            x->vel = p->vel * 0.1;
            x->game = this;
            x->size = p->size;
            x->setScore(bonus, p->pos);
            x->digitScale = 3.0f;
            x->digitPos.y -= 0.1;
            addEntity(x, &particles);
            grid->cPrintAt(6, "~9275>>>   START BONUS AWARDED   <<<");
        }
    }
    startBonusDue = false;
    return 1;
}

// ================================================================== HUD ====

// One player's score line: shield pips above (or below), then the lives
// and the score (right-aligned for players on the right).
int spaceWars::drawScoreDigits(int alignRight, sprite *s, float w, int player) {
    vec2d savedPos = s->pos;
    double savedW = s->width, savedH = s->height;
    double y0 = s->pos.y, h = s->height;
    s->frame = 0x8d;
    double pip = s->height / 5.0;
    s->height = pip;
    s->width = pip * 1.3;
    double x = double(w * -0.5f) + s->pos.x + pip * 1.3 * 0.5;
    s->pos.x = x;
    if (y0 < 0.0)
        s->pos.y += (h + h) / 5.0 + h * 0.5;
    else
        s->pos.y -= (h + h) / 5.0 + h * 0.5;
    if (alignRight)
        s->pos.x = x + double(w * 14.0f) + double(shields[player]) * s->width * -1.5;
    for (int i = 0; i < shields[player]; i++) {
        s->setupChecking();
        s->pos.x += s->width * 1.5;
    }
    s->pos = savedPos;
    s->width = savedW;
    s->height = savedH;

    int buf[8];
    double step = double(w);
    auto drawDigits = [&](int from, int to) {
        for (int i = from; i < to; i++) {
            s->frame = buf[i] + 0x8e;
            s->setupChecking();
            s->pos.x += step;
        }
    };
    auto firstNonZero = [&](bool advance) {
        int i = 0;
        while (i < 7 && buf[i] == 0) {
            if (advance)
                s->pos.x += step;
            i++;
        }
        return i;
    };
    if (!alignRight) {
        extractScore(buf, 2, lives[player], 0);
        drawDigits(0, 2);
    } else {
        extractScore(buf, 8, scores[player], 0);
        drawDigits(firstNonZero(true), 8);
    }
    s->pos.x += double(w * 4.0f);
    if (!alignRight) {
        extractScore(buf, 8, scores[player], 0);
        drawDigits(firstNonZero(false), 8);
    } else {
        extractScore(buf, 2, lives[player], 0);
        drawDigits(0, 2);
    }
    return 0;
}

// Tanks / jets: each team's kills, then (player 4) the time left.
int spaceWars::drawKillsDigits(sprite *s, float w, int player) {
    static float ph2 = 0, ph3 = 0;
    int buf[2];
    int n = nPlayers < 2 ? 2 : nPlayers;
    if (player < 4) {
        if (player >= n) {
            s->pos.x += double(w * 4.0f);
            return 0;
        }
        extractScore(buf, 2, lives[player], 0);
    } else {
        ph2 = float(double(ph2) + 0.07);
        ph3 = float(double(ph3) + 0.09);
        s->col.r = 0.75;
        s->col.g = 0.5 + (double(std::sin(ph2)) + 0.5) * 0.3;
        s->col.b = 0.5 + (double(std::sin(ph3)) + 0.5) * 0.4;
        s->col.a = 1.0;
        extractScore(buf, 2, timeLeft >> 6, 0);
    }
    if ((timeLeft >> 6) < 15) {
        if (frameCount & 0x20)
            s->col.a = 0.0;
        if (timeLeft < 1 && joyZones > 0) {
            // Time's up: the leaders stay lit.
            unsigned lead = 0;
            int best = -1;
            for (int i = 0; i < joyZones && i < 4; i++) {
                if (best < lives[i]) {
                    lead = 1u << i;
                    best = lives[i];
                } else if (best == lives[i]) {
                    lead |= 1u << i;
                }
            }
            if ((lead >> player) & 1)
                s->col.a = 1.0;
        }
    }
    double step = double(w);
    s->frame = buf[0] + 0x8e;
    s->setupChecking();
    s->frame = buf[1] + 0x8e;
    s->pos.x += step;
    s->setupChecking();
    s->pos.x += step + double(w + w);
    return 0;
}

int spaceWars::showTankLives(sprite *s, vec2d sz) {
    const vec2d dim(0.041880342563080465, 0.04);
    s->angle = 0;
    s->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    s->pos = vec2d(-0.46905982871845975, -0.47) * sz;
    s->width = dim.x * sz.x;
    s->height = dim.y * sz.y;
    int n = subMode == 2 ? 4 : 5;
    int total = 0;
    for (int i = 0; i < n; i++) {
        if (i < 4)
            s->col.unpackRGBA(teamColours[i]);
        total += drawKillsDigits(s, float(dim.x * 1.2999999523162842 * sz.x), i);
    }
    return total;
}

// The score lines in the corners (one per player).
void spaceWars::showScore(sprite *s, vec2d sz) {
    if (!showHud)
        return;
    if (unsigned(subMode - 1) < 2u) {
        showTankLives(s, sz);
        return;
    }
    s->angle = 0;
    s->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    auto place = [&](double x, double y, uint32_t c) {
        s->pos = vec2d(x, y) * sz;
        s->col.unpackRGBA(c);
    };
    switch (joyZones) {
    case 1: {
        const vec2d dim(0.05384615723198003, 0.04);
        s->pos = vec2d(-0.46307692138400997, -0.47) * sz;
        s->width = dim.x * sz.x;
        s->height = dim.y * sz.y;
        s->col.unpackRGBA(teamColours[0]);
        drawScoreDigits(0, s, float(dim.x * 1.2999999523162842 * sz.x), 0);
        break;
    }
    case 2: {
        const vec2d dim(0.03846154087998574, 0.04);
        s->width = dim.x * sz.x;
        s->height = dim.y * sz.y;
        float w = float(dim.x * 1.2999999523162842 * sz.x);
        place(dim.x * 0.5 - 0.49, -0.47, teamColours[0]);
        drawScoreDigits(0, s, w, 0);
        place(-0.19, 0.49 - 0.04, teamColours[1]);
        drawScoreDigits(1, s, w, 1);
        break;
    }
    case 3:
    case 4: {
        const vec2d dim(0.02197802335999185, 0.04);
        s->width = dim.x * sz.x;
        s->height = dim.y * sz.y;
        float w = float(dim.x * 1.2999999523162842 * sz.x);
        place(dim.x * 0.5 - 0.49, -0.47, teamColours[0]);
        drawScoreDigits(0, s, w, 0);
        place(0.09 + dim.x * 0.5, -0.47, teamColours[3]);
        drawScoreDigits(1, s, w, 3);
        place(dim.x * 0.5 - 0.49, 0.49 - 0.04, teamColours[2]);
        drawScoreDigits(0, s, w, 2);
        place(0.09 + dim.x * 0.5, 0.49 - 0.04, teamColours[1]);
        drawScoreDigits(1, s, w, 1);
        break;
    }
    default:
        break;
    }
}

// ============================================================== screens ====

namespace {
// Feedback for the menu screens: a dim grey haze that slowly zooms.
void menuFeedback(controlBlock *c) {
    c->fbColour[0] = c->fbColour[1] = c->fbColour[2] = 0.4f;
    c->fbColour[3] = 0.2f;
    c->fbScaleX = c->fbScaleY = 1.04f;
}
} // namespace

void spaceWars::infoScreen() {
    static const char *const kAbout[4] = {
        "~9909ABOUT SOLAR MINOTAUR RESCUE FRENZY",
        "~9909ABOUT DEEP SPACE MINOTAUR MADNESS",
        "~9909ABOUT TANKS!",
        "~9909ABOUT JETS!",
    };
    cellGrid *g = grid;
    g->labelMode = true;
    g->curLabel = 0;
    menuFeedback(this);
    uint32_t c = kInfoBanner[mode];
    float alpha = 1.0f;
    for (int i = 0; i < 5; i++) {
        g->curLabel = 0x12;
        g->hlin_a(999, c, 0, 0, 0x13, i, alpha);
        g->curLabel = 4;
        g->hlin_a(999, 0, c, 0x14, 0x27, i, alpha);
        g->curLabel = 0x15;
        g->hlin_a(999, c, 0, 0, 0x13, 0x17 - i, alpha);
        g->curLabel = mode < 2 ? 0x10 : 0x15;
        g->hlin_a(999, 0, c, 0x14, 0x27, 0x17 - i, alpha);
        g->curLabel = 0x14;
        g->hlin_a(999, c, 0, 0, 0x13, i + 6, 0.4f);
        g->hlin_a(999, 0, c, 0x14, 0x27, i + 6, 0.4f);
        alpha -= 0.15f;
    }
    g->curLabel = 0x14;
    g->cPrintAt(8, kAbout[mode]);
    g->curLabel = 0x12;
    g->printAt(2, 2, "~0599ABOUT LLAMASOFT");
    g->curLabel = 4;
    g->printAt(0x16, 2, "~0599MINOTAUR PROJECT");
    g->curLabel = 0x15;
    g->printAt(8, 0x15, "~0599BACK");
    g->curLabel = mode < 2 ? 0x10 : 0x15;
    if (mode < 2) {
        g->printAt(0x1a, 0x15, "~0599HIGH SCORES");
        g->hlin_a(3, c, 0x14, 0x14, 0x12, 0.4f);
    } else {
        g->printAt(0x1b, 0x15, "~0599BACK");
    }
    g->labelMode = false;
}

void spaceWars::pauseScreen() {
    cellGrid *g = grid;
    g->cPrintAt(3, "~9999*** PAUSED ***");
    g->labelMode = true;
    auto button = [&](int label, uint32_t a, uint32_t b, int y0) {
        g->curLabel = label;
        for (int y = y0; y < y0 + 3; y++) {
            g->hlin_a(999, a, b, 0xc, 0x13, y, 1.0f);
            g->hlin_a(999, b, a, 0x14, 0x1b, y, 1.0f);
        }
    };
    button(0x17, 0x400080ffu, 0x000020ffu, 5);
    g->cPrintAt(6, "~9099UNPAUSE");
    // Local Title uses the vacant middle row in the original gradient layout.
    button(0x22, 0x400080ffu, 0x000020ffu, 9);
    g->cPrintAt(10, "~9099TITLE");
    button(0x21, 0x800020ffu, 0x100010ffu, 0xd);
    g->cPrintAt(0xe, "~9099EXIT");
    g->labelMode = false;
    g->printAt(5, 0x12, "~99090   1   2   3   4   5   6   7");
    g->labelMode = true;
    for (int i = 0; i < 8; i++) {
        g->curLabel = i + 0x19;
        uint32_t c = volume == i ? kSelOn : kSelOff;
        for (int y = 0x11; y <= 0x13; y++)
            g->hlin_a(999, c, i * 4 + 4, i * 4 + 6, y, 1.0f);
    }
    g->labelMode = false;
    g->cPrintAt(0x15, "~9099GAME AUDIO LEVEL");
}

void spaceWars::printTitleScreen() {
    static const char *const kTitles[4] = {
        "~9909SOLAR MINOTAUR RESCUE FRENZY",
        "~9909DEEP SPACE MINOTAUR MADNESS",
        "~9909TANKS!",
        "~9909JETS!",
    };
    cellGrid *g = grid;
    g->labelMode = true;
    g->curLabel = 0;
    menuFeedback(this);
    uint32_t c = kTitleBanner[mode];
    float alpha = 1.0f;
    for (int i = 0; i < 5; i++) {
        g->curLabel = 1;
        g->hlin_a(999, c, 0, 0, 0x13, i, alpha);
        g->hlin_a(999, 0, c, 0x14, 0x27, i, alpha);
        g->curLabel = 0x11;
        g->hlin_a(999, c, 0, 0, 0x13, 0x17 - i, alpha);
        g->curLabel = 5;
        g->hlin_a(999, 0, c, 0x14, 0x27, 0x17 - i, alpha);
        alpha -= 0.15f;
    }
    g->curLabel = 1;
    g->cll(2);
    g->cPrintAt(0, "~0389 -- SELECT GAME --");
    g->curLabel = 0x23; // (port) the game's name: the cursor's button for label 1
    g->cPrintAt(2, mino::grid::padded(kTitles[mode]).c_str());
    g->curLabel = 0x11;
    g->printAt(8, 0x15, "~9099INFO");
    g->curLabel = 5;
    g->printAt(0x1b, 0x15, "~0999PLAY");
    // (Port: the OpenFeint button, label 0x18, opens the local high scores.)
    g->curLabel = 0x18;
    for (int y = 6; y <= 8; y++) {
        g->hlin_a(999, 0x181818ffu, 0x080808ffu, 0xc, 0x13, y, 1.0f);
        g->hlin_a(999, 0x080808ffu, 0x181818ffu, 0x14, 0x1b, y, 1.0f);
    }
    g->cPrintAt(7, "~9909HIGH SCORES");
    // (Port: EXIT, back to the collection's game selector.)
    g->curLabel = 0x22;
    for (int y = 10; y <= 12; y++) {
        g->hlin_a(999, 0x800020ffu, 0x100010ffu, 0xc, 0x13, y, 1.0f);
        g->hlin_a(999, 0x100010ffu, 0x800020ffu, 0x14, 0x1b, y, 1.0f);
    }
    g->cPrintAt(11, "~9099EXIT");
    g->labelMode = false;
}

void spaceWars::startGameScreen() {
    static const char *const kPlayers[5] = {nullptr, "ONE PLAYER", "TWO PLAYERS", "THREE PLAYERS",
                                            "FOUR PLAYERS"};
    cellGrid *g = grid;
    g->labelMode = true;
    uint32_t c = kTitleBanner[mode];
    float alpha = 1.0f;
    for (int i = 0; i < 5; i++) {
        g->curLabel = 2;
        g->hlin_a(999, c, 0, 0, 0x13, i, alpha);
        g->hlin_a(999, 0, c, 0x14, 0x27, i, alpha);
        g->curLabel = 6;
        g->hlin_a(999, c, 0, 0, 0x13, 0x17 - i, alpha);
        g->curLabel = 3;
        g->hlin_a(999, 0, c, 0x14, 0x27, 0x17 - i, alpha);
        alpha -= 0.15f;
    }
    if (mode < 2) {
        // Starting level 1-8 and survival.
        g->labelMode = false;
        g->printAt(5, 7, "~99091   2   3   4   5   6   7   8");
        g->labelMode = true;
        for (int i = 0; i < 8; i++) {
            g->curLabel = i + 7;
            uint32_t sc = startLevel == i ? kSelOn : kSelOff;
            for (int y = 6; y <= 8; y++)
                g->hlin_a(999, sc, i * 4 + 4, i * 4 + 6, y, 1.0f);
        }
        uint32_t sc = startLevel > 7 ? kSelOn : kSelOff;
        g->curLabel = 0x16;
        for (int y = 0xd; y <= 0xf; y++)
            g->hlin_a(999, sc, 0xc, 0x1c, y, 1.0f);
        g->cPrintAt(0xe, "~9099SURVIVAL");
        g->labelMode = false;
        g->cll(4);
        g->cPrintAt(4, "~0389 - SELECT STARTING LEVEL -");
        g->cll(0x11);
        g->cPrintAt(0x11, "~0389");
        if (startLevel < 8) {
            char buf[40];
            std::snprintf(buf, sizeof buf, "START BONUS %d", startBonus[mode][startLevel]);
            g->cPrintAt(0x11, buf);
        }
    }
    g->labelMode = true;
    if (mode == 2) {
        g->curLabel = 0xf;
        for (int y = 10; y <= 0xe; y++) {
            g->hlin_a(999, 0xf000c0ffu, 0, 0xf, 0x13, y, 0.4f);
            g->hlin_a(999, 0, 0xf000c0ffu, 0x14, 0x19, y, 0.4f);
        }
        g->cPrintAt(0xc, "~9919CHANGE MAZE");
    }
    g->curLabel = 2;
    g->cll(2);
    g->cPrintAt(0, "~0389 - SELECT NUMBER OF PLAYERS -");
    g->curLabel = 0x24; // (port) the player count: the cursor's button for label 2
    g->cPrintAt(2,
                mino::grid::padded(kPlayers[nPlayers < 1 || nPlayers > 4 ? 1 : nPlayers]).c_str());
    g->curLabel = 6;
    g->printAt(8, 0x15, "~9099BACK");
    g->curLabel = 3;
    g->printAt(0x1b, 0x15, "~0999START");
    g->labelMode = false;
}

// The "about" pages: a 48x28 text scroller dragged with a finger; it
// bounces at either end. Touching the bottom-right corner leaves.
int spaceWars::minoInfoScreen(bool back) {
    static int lastFrame = -1;
    static bool bounce = false;
    static int touchdown = 0;
    static float scrollVel = 0, lastJoy = 0;
    static int tp = 0, lineCount = 0;
    static const int kCols[6] = {0x30, 0x30, 0x30, 0x30, 0x30, 0x30};
    static const int kRows[6] = {0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c};
    cellGrid *g = grid;
    int pg = infoPage < 0 || infoPage > 5 ? 0 : infoPage;
    const char *const *page = kInfoPages[pg];
    int lines = kInfoPageLines[pg];
    auto line = [&](int i) { return pageLine(page, lines, i); };

    if (frameCount - 1 != lastFrame) {
        reset(0);
        fbColour[0] = fbColour[1] = fbColour[2] = 0.4f;
        fbColour[3] = 0.45f;
        fbScaleX = fbScaleY = 1.0f;
        entity *backLabel = new entity(); // the "back" corner marker
        backLabel->pos = vec2d(0.41, 0.35);
        backLabel->size = 0.06f;
        backLabel->scaleY = 1.8;
        backLabel->col.white(0.5);
        backLabel->spriteIndex = 0xb6;
        backLabel->game = this;
        addEntity(backLabel, &particles);
        waitRelease = false;
        scrollVel = 0;
        g->setSize(kCols[pg], kRows[pg]);
        lineCount = 0;
        running = true;
        titleMode = false;
        showHud = false;
        subMode = 0;
        tp = 0;
        for (; tp <= g->rowsN; tp++) {
            g->cll(tp, 0x20);
            const char *s = line(tp);
            if (s && *s != '^')
                g->printAt(0, tp, s);
        }
    }
    lastFrame = frameCount;

    if (back) {
        playSample(0xe);
        lastFrame = -1;
        g->setSize(40, 24);
        return 1;
    }
    if (infoControllerMode && !joyActive[0]) {
        int end = 0;
        while (end < lines && (!line(end) || *line(end) != '^'))
            ++end;
        neon::input::scrollText(g, tp, end, infoScrollAxis, .3, line);
        scrollVel = 0;
        bounce = false;
        touchdown = 0;
        return 0;
    }
    if (bounce) {
        g->scroll(0, scrollVel);
        if (g->scrollCount[2]) {
            g->cll(0, 0x20);
            int i = tp - g->rowsN - g->scrollCount[2] - 1;
            if (i >= 0)
                g->printAt(0, 0, line(i));
            tp -= g->scrollCount[2];
        }
        if (g->scrollCount[3]) {
            g->cll(g->rowsN, 0x20);
            const char *s = line(tp);
            if (s && *s != '^')
                g->printAt(0, g->rowsN, s);
            tp += g->scrollCount[3];
        }
        g->scrollCount[2] = g->scrollCount[3] = 0;
        scrollVel = 0;
        if (!joyActive[0])
            bounce = false;
        return 0;
    }

    if (!joyActive[0]) {
        touchdown = 0;
    } else if (!touchdown) {
        if (joyOrigin[0].x > 0.36 && joyOrigin[0].y > 0.27) {
            playSample(0xe);
            lastFrame = -1;
            g->setSize(40, 24);
            return 1;
        }
        touchdown = 1;
        lastJoy = float(joyDelta[0].y);
        scrollVel = 0;
    } else {
        scrollVel = float((double(lastJoy) - joyDelta[0].y) * 0.8);
    }
    g->scroll(0, scrollVel);
    // New lines at the bottom.
    for (int n = g->scrollCount[3]; n > 0; n--) {
        g->cll(g->rowsN, 0x20);
        const char *s = line(tp);
        if (s && *s != '^')
            g->printAt(0, g->rowsN, s);
        tp++;
        const char *next = line(tp);
        if (next)
            bounce = *next == '^';
    }
    // New lines at the top.
    int up = g->scrollCount[2];
    if (up) {
        bool top = false;
        for (int k = 0; k < up; k++) {
            g->cll(k, 0x20);
            int i = tp - g->rowsN - up - 1 + k;
            if (i < 0)
                top = true;
            else
                g->printAt(0, k, line(i));
        }
        tp -= up;
        bounce = top;
    }
    if (!bounce)
        scrollVel *= 0.98f;
    else if (up == 0) {
        if (g->scrollCount[3] != 0 && scrollVel > 0.0f)
            scrollVel = -scrollVel;
    } else if (scrollVel < 0.0f) {
        scrollVel = -scrollVel;
    }
    g->scrollCount[2] = g->scrollCount[3] = 0;
    (void)lineCount;
    return 0;
}

// The attract-mode text scrollers (story, title roll, how to play) over a
// demo game. Returns 1 when one ends or the screen is touched.
int spaceWars::storyTextScroll(bool advance) {
    static int lastFrame = -1;
    static int scrollerNum = 1;
    static int tp = 0, lineCount = 0;
    static const bool kSkipOdd[3] = {true, false, true};
    static const bool kCentred[3] = {false, true, true};
    cellGrid *g = grid;
    if (frameCount - 1 != lastFrame) {
        waitRelease = false;
        g->cls();
        g->clattr();
        lineCount = 0;
        scrollerNum++;
        if (scrollerNum >= 3)
            scrollerNum = 0;
        tp = 0;
        if (scrollerNum == 1) {
            fbColour[0] = 0.1f;
            fbColour[1] = 0.6f;
            fbColour[2] = 0.1f;
        } else {
            fbColour[0] = 0.3f;
            fbColour[1] = 0.2f;
            fbColour[2] = 0.8f;
        }
        fbColour[3] = 0.8f;
        titleMode = false;
        showHud = false;
        subMode = 0;
        minotaursOn = sunOn = planetsOn = true;
        nPlayers = (neonRand() & 3) + 1;
        if (nPlayers > maxPlayers)
            nPlayers = maxPlayers;
        newGame();
    }
    lastFrame = frameCount;
    g->scroll(0, 0.03f);
    if (g->scrollCount[3] != 0) {
        g->scrollCount[3] = 0;
        g->cll(g->rowsN, 0x20);
        g->clla(g->rowsN);
        lineCount++;
        if (kSkipOdd[scrollerNum] && !(lineCount & 1))
            return 0;
        const char *const *page = kStoryPages[scrollerNum];
        int lines = kStoryPageLines[scrollerNum];
        const char *s = pageLine(page, lines, tp);
        if (s && *s != '^') {
            if (kCentred[scrollerNum])
                g->cPrintAt(g->rowsN, s);
            else
                g->printAt(0, g->rowsN, s);
        }
        tp++;
        const char *next = pageLine(page, lines, tp);
        if (next && *next == '^') {
            lastFrame = -1;
            return 1;
        }
    }
    if (!joyActive[0] && !advance)
        return 0;
    playSample(0xe);
    lastFrame = -1;
    return 1;
}

// =================================================== (port) high scores ====
// Local tables standing in for OpenFeint's leaderboards, drawn on the grid
// in the style of the game's own menus. The shell loads and saves them.

namespace {
const char *const kBoardNames[spaceWars::kHiBoards] = {
    "~9909SOLAR MINOTAUR RESCUE FRENZY",
    "~9909DEEP SPACE MINOTAUR MADNESS",
    "~9909SOLAR - SURVIVAL",
    "~9909DEEP SPACE - SURVIVAL",
};
} // namespace

// Default tables: the original's own high score names, 10000 down (5000
// down for survival).
void spaceWars::defaultHighScores() {
    static const std::vector<std::string> kNames = {
        "STINKY OX", "GRUNTLE",  "MOOING",     "MARY",       "BEASTIE",  "COW FACE",
        "BULLY",     "BO VINE",  "FLUFFY",     "SHEEPIE",    "BIG EARS", "BIG NOSE",
        "ATAURUS",   "VINDALOO", "FLAPPYEARS", "WILDEBEEST",
    };
    hs.init(kHiBoards, kNames, 10000, 1000);
    for (int b = 2; b < kHiBoards; b++)
        for (int r = 0; r < mino::HighScores::kRows; r++)
            hs.at(b, r).score /= 2;
}

void spaceWars::highScoreScreen() {
    menuFeedback(this);
    mino::grid::drawScoreTable(grid, hs, hiBoard, kBoardNames[hiBoard], kTitleBanner[hiBoard & 1],
                               hiNew);
}

void spaceWars::nameEntryScreen() {
    menuFeedback(this);
    mino::grid::drawNameEntry(grid, entryName, entryScore, entryLevel);
}

void spaceWars::finishNameEntry() {
    hs.lastName = mino::grid::finalName(entryName);
    hiNew = hs.insert(entryBoard, hs.lastName, entryScore, entryLevel);
    hs.dirty = true;
    hiBoard = entryBoard;
    hiReturn = 1;
    hiTimer = 0;
    highScoreScreen();
    state = 0x71;
}

void spaceWars::leaveHighScores() {
    cellGrid *g = grid;
    g->cls();
    g->clattr();
    hiNew = -1;
    waitRelease = true;
    if (hiReturn == 1) { // after a game: on to the title
        g->visible = true;
        bgColour = 0;
        state = 1;
        frameCount = 0x7f;
        return;
    }
    newGame();
    if (hiReturn == 3)
        infoScreen();
    else
        printTitleScreen();
    state = 3;
}

// ================================================================= run ====

void spaceWars::run() {
    const auto actions = menuActions;
    menuActions = {};
    const bool menuActive = joyActive[0] || actions.activateLabel || actions.advance;
    infoControllerMode = actions.controller;
    infoScrollAxis = actions.scroll;
    static int idleTimer = 600;
    static int dinger = 0, dingctr = 30;
    static float phase1 = 0, phase2 = 0;
    cellGrid *g = grid;
    if (!inited) {
        if (!g)
            return;
        inited = true;
    }
    if (!paused)
        frameCount++;
    g->shrink = inputFlipped;
    if (shellCommand < 0) {
        if (shellCommand == SHELL_PURGE_TOUCHES)
            purgeTouches();
        shellCommand = 0;
    }

    bool sparkle = demo;
    if (resetRequest) {
        resetRequest = false;
        goalsDone = 0;
        if (!demo && mode < 2) {
            g->cls();
            g->clattr();
            frameCount = 0x80;
            reset(0);
            state = 1;
        }
    }
    if (sparkle) {
        // Attract mode: touches throw sparks.
        // Controller activation has explicit UI feedback, not a synthetic finger.
        for (int i = 0; i < 5; i++) {
            if (i < 4 ? !touchActive[i] : !actions.feedback)
                continue;
            for (int k = 0; k < 4; k++) {
                _particle *x = new _particle();
                x->pos = i < 4 ? touchPos[i] : vec2d(actions.feedbackX, actions.feedbackY);
                x->vel = vec2d(0, oxRNG::rand(0.001, 0.01));
                x->vel.rotateBy(oxRNG::rand(6.2831853072));
                x->size = 0.004f;
                x->from.yellow(1.0);
                x->fromDynamic = 0;
                x->to.red(0.0);
                addEntity(x, &particles);
            }
        }
    }

    switch (state) {
    case 0: // (unused) a character rain before the title
        fbColour[0] = fbColour[1] = 0;
        fbColour[2] = fbColour[3] = 0.8f;
        fbRot = fbX = fbY = 0;
        fbScaleX = fbScaleY = 0.99f;
        g->cls(frameCount & 0x7f);
        if (frameCount > 0x7f) {
            g->cls(0x23);
            state = 1;
            frameCount = 0;
        }
        break;

    case 1: { // fade to the title
        fbColour[0] = fbColour[1] = 0;
        fbColour[2] = 0.8f;
        fbColour[3] = 0.7f;
        fbRot = fbX = fbY = 0;
        fbScaleX = fbScaleY = 0.99f;
        colour c;
        c.unpackRGBA(kTitleFade[(frameCount >> 4) & 7]);
        c.a = double(float(frameCount & 0xf) * 0.0625f);
        g->ccol(c);
        g->clattr();
        dinger = 0;
        dingctr = 30;
        if (frameCount > 0x7f) {
            mode = 0;
            msgTimer = 0;
            demo = true;
            idleTimer = 600;
            bgColour = 0;
            reset(0);
            colour w;
            w.white(1.0);
            g->ccol(w);
            g->cls(0x20);
            for (int i = 0; i < 7; i++)
                g->hlin_a(1, kTitleBars[i], 0, 0x27, i + 1, 1.0f);
            state = 2;
            frameCount = 0;
            pauseArmed = false;
            paused = false;
            running = true;
            titleMode = true;
            showHud = true;
        }
        break;
    }

    case 2: // the title: dings, credits, then the story scroller
        if (dinger > 0 && !dingSent) {
            shellCommand = SHELL_SCREEN_ON;
            dingSent = true;
        }
        if (dinger < 7 && --dingctr < 1) {
            dingctr = 15;
            playSample(kDings[dinger]);
            dinger++;
        }
        if (frameCount == 0x78)
            g->printAt(0x10, 9, "PRESENTS");
        if (frameCount == 0x96) {
            g->printAt(5, 0xb, "SOLAR MINOTAUR RESCUE FRENZY");
            playGrunt();
        }
        if (frameCount == 0xff)
            g->printAt(3, 0x14, "BECAUSE");
        if (frameCount == 0x10e) {
            g->printAt(5, 0x16, "EVERYBODY LOVES A MINOTAUR (tm)");
            playGrunt();
        }
        if (frameCount > 600)
            state = 0x6f;
        if (!waitRelease) {
            if (menuActive) {
                idleTimer = 600;
                waitRelease = true;
                playSample(0xe);
                state = 3;
                running = false;
                showHud = false;
                titleMode = false;
                g->cls(0x20);
                g->clattr();
                mode = lastMode;
                nPlayers = lastPlayers;
                newGame();
                printTitleScreen();
            }
        } else if (!joyActive[0]) {
            waitRelease = false;
        }
        break;

    case 3: // the menus
        if (waitRelease) {
            if (!joyActive[0])
                waitRelease = false;
        } else if (menuActive) {
            idleTimer = 600;
            waitRelease = true;
            int label = actions.activateLabel ? actions.activateLabel : g->getLabel(joyOrigin[0]);
            switch (label) {
            case 1:    // next game type
            case 0x23: // (port) the same, on the game's name
                if (++mode >= 4)
                    mode = 0;
                else if (mode == 3 && nPlayers > 2)
                    nPlayers = 2;
                g->clattr();
                newGame();
                printTitleScreen();
                playSample(0xe);
                break;
            case 2:    // number of players
            case 0x24: // (port) the same, on the count
                nPlayers++;
                if (mode == 3 ? nPlayers > 2 : nPlayers > maxPlayers)
                    nPlayers = 1;
                g->cls();
                g->clattr();
                newGame();
                startGameScreen();
                playSample(0xe);
                break;
            case 3: // START
                playSample(0xe);
                g->cls();
                g->clattr();
                g->cPrintAt(9, "~5079TAP x2");
                g->cPrintAt(0xe, "~5079TO PAUSE");
                msgTimer = 0x28;
                flash = 30.0f;
                if (subMode == 0)
                    fbScaleX = fbScaleY = 0.99f;
                demo = false;
                lastMode = mode;
                lastStartLevel = startLevel;
                lastPlayers = nPlayers;
                showHud = true;
                pauseArmed = true;
                state = 4;
                reset(0);
                newGame();
                break;
            case 4: // info: about the Minotaur Project
                playSample(0xe);
                waitRelease = true;
                infoPage = 0;
                reset(0);
                state = 0x70;
                break;
            case 5: // PLAY: the start screen
                playSample(0xe);
                g->cls();
                nPlayers = lastPlayers;
                if (mode == 3 && lastPlayers > 2)
                    nPlayers = 2;
                startLevel = lastStartLevel;
                if (lastStartLevel > 7)
                    survival = true;
                startGameScreen();
                break;
            case 6: // BACK to the title screen
                playSample(0xe);
                g->cls();
                g->clattr();
                mode = lastMode;
                if (lastMode == 3 && nPlayers > 2)
                    nPlayers = 2;
                newGame();
                printTitleScreen();
                break;
            case 0xf: // change maze
                playSample(0xe);
                if (++maze > 2)
                    maze = 0;
                g->cls();
                g->clattr();
                newGame();
                startGameScreen();
                break;
            case 0x22: // (port) EXIT: ask the shell to leave the game
                playSample(0xe);
                shellCommand = SHELL_EXIT;
                break;
            case 0x10: // high scores (info screen)
            case 0x18: // high scores (title screen; was OpenFeint)
                playSample(0xe);
                hiBoard = (mode < 2 ? mode : 0) + (survival && mode < 2 ? 2 : 0);
                hiReturn = label == 0x10 ? 3 : 2;
                hiNew = -1;
                waitRelease = true;
                highScoreScreen();
                state = 0x71;
                break;
            case 0x11: // INFO
                g->cls();
                g->clattr();
                newGame();
                playSample(0xe);
                infoScreen();
                break;
            case 0x12: // info: about Llamasoft
                playSample(0xe);
                infoPage = 5;
                waitRelease = true;
                reset(0);
                state = 0x70;
                break;
            case 0x14: // about this game type
                playSample(0xe);
                infoPage = mode + 1;
                waitRelease = true;
                reset(0);
                state = 0x70;
                break;
            case 0x15: // BACK from the info screen
                playSample(0xe);
                g->cls();
                g->clattr();
                if (mode == 3 && nPlayers > 2)
                    nPlayers = 2;
                newGame();
                printTitleScreen();
                break;
            case 0x16: // survival
                survival = true;
                startLevel = 8;
                playSample(0xe);
                startGameScreen();
                newGame();
                break;
            default:
                if (label > 6 && label < 0xf) { // starting level
                    playSample(0xe);
                    survival = false;
                    startLevel = label - 7;
                    startGameScreen();
                    newGame();
                }
                break;
            }
        }
        if (--idleTimer < 1) {
            frameCount = 0x80;
            reset(0);
            state = 1;
            g->cls();
            g->clattr();
        }
        break;

    case 4: // playing
        if (!paused) {
            pauseTapTimer--;
            if (subMode == 1) {
                if (--timeLeft < 1) {
                    demo = true;
                    g->cPrintAt(10, "GAME OVER");
                    msgTimer = 0;
                    int n = nPlayers < 2 ? 2 : nPlayers;
                    int best = -1, win = 0;
                    for (int i = 0; i < n; i++) {
                        int v = lives[i];
                        if (v == best) { // a tie (with the leader so far)
                            win = 4;
                            break;
                        }
                        if (best < v) {
                            best = v;
                            win = i;
                        }
                    }
                    g->cPrintAt(0xf, teamWinMessages[win]);
                    state = 5;
                    frameCount = 0;
                    waitRelease = true;
                }
            } else if (subMode == 2) {
                int n = nPlayers < 2 ? 2 : nPlayers;
                int alive = 0, last = 0;
                for (int i = 0; i < n; i++) {
                    entity *e = players;
                    while (e && static_cast<craft *>(e)->joy != i)
                        e = e->next;
                    if (e) {
                        alive++;
                        last = i;
                    }
                }
                if (alive < 2) {
                    demo = true;
                    g->printAt(0xf, 8, "GAME OVER");
                    msgTimer = 0;
                    g->cPrintAt(0x11, teamWinMessages[last]);
                    state = 5;
                    frameCount = 0;
                    waitRelease = true;
                }
            } else if (subMode == 0) {
                if (!players) {
                    g->visible = true;
                    demo = true;
                    g->cls(0x20);
                    g->printAt(0xf, 10, "GAME OVER");
                    state = 5;
                    waitRelease = true;
                    pauseArmed = false;
                    msgTimer = 0;
                    frameCount = 0;
                    AudioSystemPlaySample(3, volume << 12, 0);
                } else if (levelEnding) {
                    bool planetsLeft = false;
                    for (entity *e = planets; e; e = e->next)
                        if (e->type == 2)
                            planetsLeft = true;
                    if (!planetsLeft) {
                        levelDone = true;
                        frameCount = 0;
                        g->visible = true;
                        if (msgTimer == 0)
                            g->cls(0x20);
                        char msg[60];
                        std::snprintf(msg, sizeof msg, "~5396LEVEL %d COMPLETE", level + 1);
                        if (!demo)
                            playSample(0x10);
                        if (level == 0x19)
                            achieveGoal(8);
                        g->cPrintAt(9, msg);
                        doStartBonus();
                        level++;
                        levelsCleared++;
                        if (mode == 0 && levelsCleared == 3)
                            achieveGoal(3);
                        if (rescuedThisLevel == 0)
                            levelsNoRescue++;
                        if (levelsNoRescue == 3)
                            achieveGoal(5);
                        state = 6;
                    }
                }
            }
        } else {
            g->visible = true;
            g->cls();
            g->clattr();
            pauseScreen();
            pausedState = 4;
            state = 7;
        }
        break;

    case 5: // game over: back to the title on a touch (or after a while)
        if (!waitRelease) {
            if (menuActive || frameCount > 0x3ff) {
                idleTimer = 600;
                showHud = false;
                if (menuActive)
                    playSample(0xe);

                int n = mode < 2 ? nPlayers : mode;
                waitRelease = true;
                if (n < 2) {
                    postBoard = survival ? mode + 2 : mode;
                    postScore = scores[0];
                    shellCommand = SHELL_POST_SCORE;
                    // (Port: a local high score instead of OpenFeint's.)
                    if (hs.qualifies(postBoard, scores[0])) {
                        entryScore = scores[0];
                        entryLevel = level + 1;
                        entryBoard = postBoard;
                        entryName = hs.lastName;
                        hiTimer = 0;
                        g->visible = true;
                        reset(0); // (clear the field for the entry screen)
                        nameEntryScreen();
                        state = 0x72;
                        break;
                    }
                }
                bgColour = 0;
                state = 1;
                frameCount = 0x7f;
            }
        } else if (!joyActive[0] && frameCount > 0xff) {
            waitRelease = false;
        }
        break;

    case 6: // level complete: a breather with minotaurs, then the next
        if (!paused) {
            randomMinotaur();
            if (frameCount > 0x17f) {
                newLevel();
                if (msgTimer == 0)
                    g->cls();
                g->cPrintAt(0xd, "~9479-= SHIELDS RENEWED =-");
                msgTimer = 0x78;
                state = 4;
            }
        } else {
            g->visible = true;
            g->cls();
            g->clattr();
            pauseScreen();
            pausedState = 6;
            state = 7;
        }
        break;

    case 7: // paused
        if (!waitRelease) {
            if (menuActive) {
                waitRelease = true;
                int label =
                    actions.activateLabel ? actions.activateLabel : g->getLabel(joyOrigin[0]);
                if (label == 0x21) { // exit to iOX
                    playSample(0xe);
                    shellCommand = SHELL_EXIT;
                } else if (label == 0x22) { // title: original Quit Game route
                    playSample(0xe);
                    g->cls();
                    g->clattr();
                    frameCount = 0x80;
                    reset(0);
                    state = 1;
                } else if (label == 0x17) { // unpause
                    playSample(0xe);
                    paused = false;
                    pauseTapTimer = 0;
                    g->cls();
                    g->clattr();
                    if (mode < 2)
                        g->visible = false;
                    if (mode == 2)
                        makeMaze();
                    if (mode == 3)
                        jetsArena(g);
                    state = pausedState;
                } else if (label > 0x18 && label < 0x21) { // volume
                    volume = label - 0x19;
                    playGrunt();
                    pauseScreen();
                }
            }
        } else if (!joyActive[0]) {
            waitRelease = false;
        }
        break;

    case 0x6f:
        if (storyTextScroll(actions.advance)) {
            waitRelease = true;
            frameCount = 0x80;
            state = 1;
        }
        break;

    case 0x71:   // (port) the high score table
    case 0x72: { // (port) entering a name for it
        bool entry = state == 0x72;
        if (++hiTimer > (entry ? 60 * 60 : 60 * 30)) { // nobody there
            if (entry)
                finishNameEntry();
            else
                leaveHighScores();
            break;
        }
        if (waitRelease) {
            if (!joyActive[0])
                waitRelease = false;
            break;
        }
        if (!menuActive)
            break;
        waitRelease = true;
        int label = actions.activateLabel ? actions.activateLabel : g->getLabel(joyOrigin[0]);
        if (!entry) {
            if (label == 2 || label == mino::grid::kLabelTableName) { // next table
                playSample(0xe);
                hiBoard = (hiBoard + 1) % kHiBoards;
                hiNew = -1;
                highScoreScreen();
            } else if (label == 6) {
                playSample(0xe);
                leaveHighScores();
            }
            break;
        }
        int k = mino::grid::nameEntryKey(label, entryName);
        if (k == 0)
            break;
        if (k == 2) {
            playSample(0xe);
            finishNameEntry();
            break;
        }
        playSample(0xe);
        hiTimer = 0;
        nameEntryScreen();
        break;
    }

    case 0x70:
        if (minoInfoScreen(actions.back)) {
            waitRelease = true;
            frameCount = 0x80;
            g->cls(0x20);
            g->clattr();
            newGame();
            infoScreen();
            state = 3;
        }
        break;

    default:
        break;
    }

    // ---------------------------------------------------------- common ----
    if (!running)
        return;
    phase1 += 0.01f;
    if (phase1 > 6.2831855f)
        phase1 -= 6.2831855f;
    phase2 += 0.034f;
    pulse *= 0.95f;
    if (demo)
        warp *= 0.99f;
    if (mode < 2) {
        if (!demo) {
            if (sunOn) {
                if (!blackHole)
                    warp *= 0.99f;
                else if ((warp += 0.01f) > 1.0f)
                    warp = 1.0f;
            }
            if (!paused) {
                // The feedback trails slowly cycle colour; planet hits
                // pulse them and the black hole warps them.
                float p = pulse;
                fbColour[0] = (p * 0.15f + 0.4f) * (std::sin(phase1) * 0.5f + 0.5f) + 0.1f;
                fbColour[1] = p * 0.5f + 0.1f + (std::cos(phase2) * 0.5f + 0.5f) * 0.2f;
                fbColour[3] = 0.99f;
                fbColour[2] =
                    (p * 0.2f + 0.6f) * (std::sin(phase1 + 3.1415927f) * 0.5f + 0.5f) + 0.1f;
                float s = warp * 0.2f + 0.993f;
                fbScaleX = fbScaleY = s;
                fbRot = warp * 0.03f;
            } else {
                fbColour[0] = fbColour[1] = fbColour[2] = 0.1f;
                fbColour[3] = 0.7f;
                fbScaleX = fbScaleY = 0.99f;
                fbRot = 0;
            }
        } else {
            warp *= 0.99f;
            fbRot = warp * 0.03f;
        }
    }
    if (!paused && msgTimer > 0 && --msgTimer == 0) {
        if (mode < 2)
            g->visible = false;
        else
            g->cls();
    }
    if (paused)
        return;
    if (subMode == 0) {
        runStarField();
        randomSaucer();
        if (!planetsOn && minotaursOn)
            randomMinotaur();
    } else if (subMode == 1) {
        if (minotaursOn)
            randomMinotaur();
    }
    runList(players);
    runList(bullets);
    runList(planets);
    runList(list154);
    runList(particles);
    runList(minotaurs);
    runList(saucers);
    runList(enemyShots);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
