// game_main.cpp - theGame: front-end screens, level setup, spawning helpers.
// (The per-frame state machine is in game_run.cpp, the spawn switch in
// game_gen.cpp.)
#include "neon/entity_list.h"
#include <cstdio>

#include "game.h"
#include "game_entities.h"
#include "game_tables.h"
#include "neon_audio.h"
#include "neon_modules.h"
#include "grid_menus.h"

namespace NEON_GAME {
namespace Neon_Internals {

bool useRobot = false, autoAim = false, reducedPowerups = false, hard = false;

namespace {
constexpr double kPi = 3.141592654;
const char *const kBlankRow = "                  ";

int levelDesc(int level, int i) {
    return levelDescriptions[level * 20 + i];
}

void deleteList(entity *&list) {
    neon::entities::destroyList(list, [](entity *e) { delete e; });
}
} // namespace

void theGame::playSample(int n) const {
    AudioSystemPlaySample(n, volume << 12, 0);
}

// ----------------------------------------------------------- construction --

theGame::theGame() {
    movementTouchStarted = false;
    demo = true;
    confirmResumeReset = false;
    message = 0;
    gridConfigured = false;
    demoLevel = 0;
    bootNotificationSent = false;
    resumeBest = false;
    levelFlags = 0;
    shellCommand = 0;
    postScoreCategory = 0;
    defaultHighScores();
    practice = false;
    endurance = false;
    furthest = 9;
    // The original caps players at two even after checking the iPad idiom.
    isPad = false;
    maxPlayers = 2;
    reset(0);
    lastMode = 0;
    lastPlayers = 1;
    lastStartLevel = 0;
    gameMode = isPad ? 0 : 2;
    state = 1;
    frameCount = 0;
    volume = 4;
    banner = 0;
    goalsDone = 0;
    nPlayers = 1;
    level = 0;
    selLevel = 0;
    spawnQueueDrained = false;
    spawnCount = 0;
    paused = false;
    pauseArmed = false;
}

theGame::~theGame() {
    reset(0);
}

// ------------------------------------------------------------------ input --

void theGame::processJoyUp(unsigned id) {
    neon::input::endTouch(*this, id);
}

void theGame::processJoyDown(unsigned id, vec2d p) {
    if (inputFlipped)
        p = p * -1.0;
    if (!pauseArmed) {
        paused = false;
        f2d4 = 0;
    } else if (p.y < -0.4) {
        paused = true; // a touch in the score bar
    }
    neon::input::beginTouch(*this, id, p, neon::input::TouchTracking::FrameStamped, frameCount);
    neon::input::claimTouchJoystick(*this, 0, id, p);
}

void theGame::processJoyMove(unsigned id, vec2d p) {
    if (inputFlipped)
        p = p * -1.;
    neon::input::moveTouch(*this, id, p, neon::input::TouchTracking::FrameStamped);
}

// ---------------------------------------------------------------- drawing --

// Sprites queued by the cellGrid's hash-mode text ("#X#" in printed strings).
void theGame::drawDeferredSprites() {
    cellGrid *g = grid;
    if (!g)
        return;
    int n = g->labelCount;
    spr->flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    for (int k = n - 1; k >= 0; k--) {
        int c = g->labelChar[k];
        spr->pos = g->labelPos[k];
        int frame = kTheSprites[c];
        spr->frame = frame + kLitBase;
        spr->width = double(kSizesX[c]) * drawSize.x;
        spr->height = double(kSizesY[c]) * drawSize.y;
        bool minotaur = false;
        if (frame == 0xbb) {
            spr->pos.y += spr->height / 1.5;
            if (c == 1) {
                spr->col.orange(1.0);
                spr->setupChecking();
                minotaur = true;
            } else {
                spr->col.white(1.0);
                spr->setupChecking();
                minotaur = c < 8;
            }
        } else if (c == 1) {
            spr->col.orange(1.0);
            spr->setupChecking();
            minotaur = true;
        } else {
            spr->col.white(1.0);
            spr->setupChecking();
            minotaur = c < 8;
        }
        if (minotaur) {
            // Powerup icons (A..H) get a frame drawn over them.
            spr->col.white(1.0);
            spr->frame = 0x3a;
            spr->width = drawSize.x * 0.09;
            spr->height = drawSize.y * 0.09;
            spr->pos = g->labelPos[k];
            spr->setupChecking();
        }
    }
    g->labelCount = 0;
}

void theGame::_draw(sprite *s, vec2d sz) {
    spr = s;
    drawSize = sz;
    float a, b;
    if (!demo && !paused) {
        a = 1.0f;
        b = 2.0f;
    } else {
        a = 0.4f;
        b = 0.8f;
    }
    if (bgColour != 0) {
        spr->pos = vec2d(0, 0);
        spr->width = drawSize.x;
        spr->height = drawSize.y;
        spr->col.unpackRGBA(bgColour);
        spr->frame = 0x32;
        spr->setupChecking();
    }
    drawList(projectiles, a);
    drawList(obstacles, a);
    drawList(enemies, a);
    drawList(enemyShots);
    drawList(beasts);
    drawList(particles, b);
    drawList(players);
    drawDeferredSprites();
    drawInfoBack();
}

void theGame::drawInfoBack() {
    if (state != 112)
        return;
    // Keep the navigation marker independent of the arena's particle lifetime.
    entity back;
    back.pos = {.41, .35};
    back.size = .06f;
    back.scaleY = 1.8;
    back.col.white(.5);
    back.spriteIndex = 0xdc;
    back.draw(spr, drawSize);
}

// ----------------------------------------------------- spawn positioning ---

// A random arena position at least (minDx, minDy) away from the first
// player on both axes; `margin` shrinks the area. Gives up after 255 tries.
vec2d theGame::newPosition(float minDx, float minDy, float margin) {
    vec2d p0 = players ? players->pos : vec2d(0, 0.12);
    double lox = double(margin - 0.46f), hi = double(0.45f - margin), loy = double(margin - 0.38f);
    vec2d r;
    for (int tries = 255;;) {
        r.x = rng.rand(lox, hi);
        r.y = rng.rand(loy, hi);
        vec2d d = r - p0;
        if (std::fabs(d.x) > double(minDx) && std::fabs(d.y) > double(minDy))
            return r;
        if (--tries == 0)
            return vec2d(0, 0.35);
    }
}

vec2d theGame::newPosition(float minDist) {
    vec2d p0 = players ? players->pos : vec2d(0, 0.12);
    double t = double(minDist * 0.5f);
    vec2d r;
    for (int tries = 255;;) {
        r.x = rng.rand(-0.45, 0.45);
        r.y = rng.rand(-0.35, 0.45);
        vec2d d = r - p0;
        if (std::fabs(d.x) > t && std::fabs(d.y) > t)
            return r;
        if (--tries == 0)
            return vec2d(0, -0.35);
    }
}

// Deletes the entity lists whose bit in `keep` is clear.
// Bit 0x4 belonged to an unpopulated list; other masks retain their source values.
void theGame::reset(int keep) {
    spawnQueueDrained = false;
    if (!(keep & 1))
        deleteList(players);
    if (!(keep & 2))
        deleteList(obstacles);
    if (!(keep & 8))
        deleteList(projectiles);
    if (!(keep & 0x10))
        deleteList(particles);
    if (!(keep & 0x20))
        deleteList(beasts);
    if (!(keep & 0x40))
        deleteList(enemies);
    if (!(keep & 0x80))
        deleteList(enemyShots);
}

// ------------------------------------------------------- text messages -----

void theGame::drawMessage() {
    if (demo)
        return;
    cellGrid *g = grid;
    switch (message) {
    case 1:
        g->cPrintAt(4, "BOOM!", 0xffff0040);
        if (shakeFrames == 0)
            shakeFrames = 10;
        break;
    case 2:
        g->cPrintAt(4, "Last Beast", 0x8000ff40);
        g->cPrintAt(7, "GET!", 0x8000ff40);
        break;
    case 3:
        g->cPrintAt(4, "-- GOT YOU! --", 0xf060ff40);
        break;
    case 4:
        g->cPrintAt(3, "EXCELLENT PLAY!", 0xf060ff40);
        g->cPrintAt(5, achievementMessages[achievement], gcol[1].packRGBA());
        break;
    }
}

void theGame::eraseMessage() {
    if (demo)
        return;
    cellGrid *g = grid;
    switch (message) {
    case 1:
    case 3:
        g->printAt(1, 4, kBlankRow);
        break;
    case 2:
        g->printAt(1, 4, kBlankRow);
        g->printAt(1, 7, kBlankRow);
        break;
    case 4:
        g->printAt(1, 3, kBlankRow);
        g->printAt(1, 5, kBlankRow);
        break;
    }
}

// ------------------------------------------------------ front-end screens --

void theGame::pauseScreen() {
    cellGrid *g = grid;
    g->printAt(1, 3, kBlankRow);
    g->cPrintAt(3, "~9999*** PAUSED ***");
    g->labelMode = true;
    g->curLabel = 0x17;
    g->hlin_a(999, 0x101010ff, 0x48080808, 5, 9, 5, 1.0f);
    g->hlin_a(999, 0x48080808, 0x101010ff, 10, 15, 5, 1.0f);
    g->printAt(1, 5, kBlankRow);
    g->cPrintAt(5, "~9909UNPAUSE");
    g->curLabel = 0x22;
    g->hlin_a(999, 0x101010ff, 0x48080808, 5, 9, 7, 1.0f);
    g->hlin_a(999, 0x48080808, 0x101010ff, 10, 15, 7, 1.0f);
    g->printAt(1, 7, kBlankRow);
    g->cPrintAt(7, "~9909TITLE");
    g->curLabel = 0x21;
    g->hlin_a(999, 0x101010ff, 0x08084808, 5, 9, 9, 1.0f);
    g->hlin_a(999, 0x08084808, 0x101010ff, 10, 15, 9, 1.0f);
    g->printAt(1, 9, kBlankRow);
    g->cPrintAt(9, "~9099EXIT");
    g->labelMode = false;
}

void theGame::infoScreen() {
    cellGrid *g = grid;
    frontEndTouchMode = 0;
    g->setSize(40, 24);
    g->labelMode = true;
    g->curLabel = 0;
    fbColour[0] = fbColour[1] = fbColour[2] = 0.4f;
    fbColour[3] = 0.2f;
    fbScaleX = fbScaleY = 1.04f;
    uint32_t bc = kBannerColoursInfo[banner];
    float a = 1.0f;
    int i = 0;
    for (;; i++) {
        g->curLabel = 0x12;
        g->hlin_a(999, bc, 0, 0, 19, i, a);
        g->curLabel = 4;
        g->hlin_a(999, 0, bc, 20, 39, i, a);
        g->curLabel = 0x15;
        g->hlin_a(999, bc, 0, 0, 19, 23 - i, a);
        g->curLabel = 0x10;
        g->hlin_a(999, 0, bc, 20, 39, 23 - i, a);
        g->curLabel = 0x14;
        g->hlin_a(999, bc, 0, 0, 19, i + 6, 0.4f);
        g->hlin_a(999, 0, bc, 20, 39, i + 6, 0.4f);
        a -= 0.15f;
        if (i + 1 == 5)
            break;
    }
    g->curLabel = i + 0x10;
    static const char *const kAbout[] = {"~9909ABOUT MINOTRON: 2112"};
    g->cPrintAt(8, banner == 0 ? kAbout[0] : nullptr);
    g->curLabel = 0x12;
    g->printAt(2, 2, "~0599ABOUT LLAMASOFT");
    g->curLabel = 4;
    g->printAt(0x16, 2, "~0599MINOTAUR PROJECT");
    g->curLabel = 0x15;
    g->printAt(8, 0x15, "~0599BACK");
    g->curLabel = 0x10;
    g->printAt(0x1a, 0x15, "~0599HIGH SCORES");
    g->labelMode = false;
    g->printAt(5, 0xd, "~99090   1   2   3   4   5   6   7");
    g->labelMode = true;
    for (int v = 0; v < 8; v++) {
        g->curLabel = v + 0x19;
        uint32_t c = volume == v ? 0xf210007f : 0x307f;
        int x0 = v * 4 + 4, x1 = v * 4 + 6;
        g->hlin_a(999, c, x0, x1, 0xc, 1.0f);
        g->hlin_a(999, c, x0, x1, 0xd, 1.0f);
        g->hlin_a(999, c, x0, x1, 0xe, 1.0f);
    }
    g->labelMode = false;
    g->cPrintAt(0x10, "~9099GAME AUDIO LEVEL");
}

void theGame::printTitleScreen() {
    cellGrid *g = grid;
    g->labelMode = true;
    g->curLabel = 0;
    g->setSize(40, 24);

    fbColour[0] = fbColour[1] = fbColour[2] = 0.2f;
    fbColour[3] = 0.1f;
    fbScaleX = fbScaleY = 1.04f;
    static const char *const kModes[] = {"~9909NORMAL", "~9909ASSISTED", "~9909SIMPLIFIED",
                                         "~9909HARD"};
    static const char *const kControls[] = {"~0999CONTROL ADAPTIVE", "~0999CONTROL RIGHT HANDED",
                                            "~0999CONTROL LEFT HANDED"};
    static const char *const kHelp[] = {"Standard game. Solo, aim and fire",
                                        "manually, full range of powerup",
                                        "pickups available.",
                                        "Player is assisted by an",
                                        "indestructible helper droid.  Some",
                                        "powerups unavailable.",
                                        "Player has only to move and avoid",
                                        "being killed.  Aiming and firing is",
                                        "fully automatic.",
                                        "In Hard mode the only powerups",
                                        "available are those from original",
                                        "Llamatron."};
    uint32_t bc = kBannerColoursTitle[gameMode];
    float a = 1.0f;
    int i = 0;
    for (;; i++) {
        g->curLabel = 1;
        g->hlin_a(999, bc, 0, 0, 19, i, a);
        g->hlin_a(999, 0, bc, 20, 39, i, a);
        g->curLabel = 0x11;
        g->hlin_a(999, bc, 0, 0, 19, 23 - i, a);
        g->curLabel = 5;
        g->hlin_a(999, 0, bc, 20, 39, 23 - i, a);
        a -= 0.15f;
        if (i + 1 == 5)
            break;
    }
    g->curLabel = i - 3;
    for (int r = 2; r <= 0x11; r++)
        g->cll(r);
    g->curLabel = 0x27;
    g->hlin_a(0x27, 0x180040ff, 0x4800f0ff, 0, 0xf, 6, 0.2f);
    g->hlin_a(0x27, 0x180040ff, 0x4800f0ff, 0, 0xf, 7, 0.2f);
    g->hlin_a(0x27, 0x180040ff, 0x4800f0ff, 0, 0xf, 8, 0.2f);
    g->printAt(1, 7, "~0999PLAY ENDURANCE");
    g->curLabel = 0x29;
    g->hlin_a(0x29, 0x4800f0ff, 0x180040ff, 0x12, 0x27, 6, 0.2f);
    g->hlin_a(0x29, 0x4800f0ff, 0x180040ff, 0x12, 0x27, 7, 0.2f);
    g->hlin_a(0x29, 0x4800f0ff, 0x180040ff, 0x12, 0x27, 8, 0.2f);
    int ctl = sd(0xc98);
    g->printAt(0x13, 7, ctl >= 0 && ctl < 3 ? kControls[ctl] : nullptr);
    g->curLabel = 1;
    g->cPrintAt(0xb, kHelp[gameMode * 3]);
    g->cPrintAt(0xd, kHelp[gameMode * 3 + 1]);
    g->cPrintAt(0xf, kHelp[gameMode * 3 + 2]);
    g->cPrintAt(0, "~0389 -- SELECT GAME MODE --");
    g->curLabel = 0x2c; // (port) the mode's name: the cursor's button for label 1
    g->cPrintAt(2, mino::grid::padded(kModes[gameMode]).c_str());
    g->curLabel = 0x11;
    g->printAt(8, 0x15, "~9099INFO");
    g->curLabel = 5;
    g->printAt(0x1b, 0x15, "~0999PLAY");
    // (Port: HIGH SCORES, and EXIT back to the collection's game selector.)
    for (int y = 0x10; y <= 0x11; y++) {
        g->curLabel = 0x2a;
        g->hlin_a(999, 0x181818ffu, 0x080808ffu, 3, 0x12, y, 1.0f);
        g->curLabel = 0x2b;
        g->hlin_a(999, 0x800020ffu, 0x100010ffu, 0x15, 0x24, y, 1.0f);
    }
    g->curLabel = 0x2a;
    g->printAt(5, 0x11, "~9909HIGH SCORES");
    g->curLabel = 0x2b;
    g->printAt(0x1b, 0x11, "~9099EXIT");
    frontEndTouchMode = 1;
    g->labelMode = false;
}

void theGame::startGameScreen() {
    cellGrid *g = grid;
    g->setSize(40, 24);
    g->labelMode = true;
    g->curLabel = 2;
    uint32_t bc = kBannerColoursStart[banner];
    float a = 1.0f;
    for (int i = 0;; i++) {
        g->curLabel = 2;
        g->hlin_a(999, bc, 0, 0, 19, i, a);
        g->hlin_a(999, 0, bc, 20, 39, i, a);
        g->curLabel = 6;
        g->hlin_a(999, bc, 0, 0, 19, 23 - i, a);
        g->curLabel = selLevel < 1 ? 3 : 9;
        g->hlin_a(999, 0, bc, 20, 39, 23 - i, a);
        if (i + 1 == 5)
            break;
        a -= 0.15f;
    }
    g->curLabel = 0x28;
    for (int r = 5; r <= 0x11; r++)
        g->cll(r);
    if (selLevel > 0) {
        if (!confirmResumeReset) {
            g->curLabel = 0x25;
            g->hlin_a(0x25, 0x0040ffff, 0x0040ffff, 10, 0x1e, 0xc, 1.0f);
            g->hlin_a(0x25, 0x0040ffff, 0x0040ffff, 10, 0x1e, 0xd, 1.0f);
            g->hlin_a(0x25, 0x0040ffff, 0x0040ffff, 10, 0x1e, 0xe, 1.0f);
            g->cPrintAt(0xd, "~0999RESET RESUME BEST");
        } else {
            g->curLabel = 0x26;
            g->hlin_a(0x26, 0x8000ff, 0x8000ff, 5, 0x22, 0xc, 1.0f);
            g->hlin_a(0x26, 0x8000ff, 0x8000ff, 5, 0x22, 0xd, 1.0f);
            g->hlin_a(0x26, 0x8000ff, 0x8000ff, 5, 0x22, 0xe, 1.0f);
            g->cPrintAt(0xd, "~0909CONFIRM RESET RESUME BEST");
        }
    }
    g->labelMode = false;
    if (furthest > 0) {
        g->printAt(1, 7, "~9909-   --");
        g->printAt(0x21, 7, "++   +");
        g->labelMode = true;
        g->curLabel = 8;
        for (int r = 6; r <= 8; r++)
            g->hlin_a(999, 0x307f, 0, 2, r, 1.0f);
        g->curLabel = 7;
        for (int r = 6; r <= 8; r++)
            g->hlin_a(999, 0x307f, 0x25, 0x27, r, 1.0f);
        g->curLabel = 0x24;
        for (int r = 6; r <= 8; r++)
            g->hlin_a(999, 0x307f, 4, 7, r, 1.0f);
        g->curLabel = 0x23;
        for (int r = 6; r <= 8; r++)
            g->hlin_a(999, 0x307f, 0x1f, 0x23, r, 1.0f);
        g->labelMode = false;
    }
    g->cPrintAt(7, levelNames[selLevel]);
    char buf[80];
    if (selLevel < 1) {
        std::snprintf(buf, sizeof buf, "Begin at level 1.");
    } else {
        std::snprintf(buf, sizeof buf, "Level %d  Best: %d lives, %d points", selLevel + 1,
                      best(gameMode, selLevel, 0), best(gameMode, selLevel, 1));
    }
    g->cPrintAt(10, buf);
    if (furthest > 0) {
        std::snprintf(buf, sizeof buf, "Furthest: level %d, %d points", furthest + 1,
                      best(gameMode, furthest, 1));
        g->cPrintAt(0x10, buf);
    }
    g->labelMode = false;
    g->cll(2);
    g->cPrintAt(2, "~0389 - SELECT LEVEL -");
    g->cll(0x11);
    g->cPrintAt(0x11, "~0389");
    g->labelMode = true;
    g->curLabel = 6;
    g->printAt(8, 0x15, "~9099BACK");
    if (selLevel < 1) {
        g->curLabel = 3;
        g->printAt(0x1b, 0x15, "~0999START");
    } else {
        g->curLabel = 9;
        g->printAt(0x18, 0x15, "~0999RESUME BEST");
    }
    g->labelMode = false;
}

void theGame::updateStats() {
    cellGrid *g = grid;
    if (demo)
        return;
    g->cll(0);
    g->printAt(0, 0, "~9999           00    00");
    g->poke(8, 0, char(-0x80), 0xffff00ff);
    g->poke(0xe, 0, char(-0x7d), 0xff00ffff);
    g->poke(9, 0, char(-0x7f), 0xffff00ff);
    g->poke(0xf, 0, char(-0x7c), 0xff00ffff);
    g->poke(10, 0, char(-0x7e), 0xffff00ff);
    g->poke(0x10, 0, char(-0x7b), 0xff00ffff);
    g->poke(0x11, 0, char(lives[0] / 10 + '0'));
    g->poke(0x12, 0, char(lives[0] % 10 + '0'));
    if (level < 99) {
        g->poke(0xb, 0, char((level + 1) / 10 + '0'));
        g->poke(0xc, 0, char((level + 1) % 10 + '0'));
    } else {
        g->printAt(0xb, 0, "~9009$$");
    }
    int digits[10];
    int *d = extractScore(digits, 10, score[0], -1);
    if (d[0] >= 0) {
        for (int i = 0;; i++) {
            g->poke(i, 0, char(d[i] + '0'), 0xffffffff);
            if (!(d[i + 1] >= 0 && i + 1 <= 7))
                break;
        }
    }
    if (!(levelFlags & 0x10)) {
        g->printAt(8, 2, "    ");
    } else {
        uint32_t c = (gcol[1].packRGBA() & 0xffffff00) | 0x20;
        g->poke(8, 2, char(-0x7a), c);
        g->poke(9, 2, char(-0x79), c);
        g->poke(10, 2, char(-0x78), c);
        g->poke(0xb, 2, char(-0x77), c);
    }
}

void theGame::makeArena() {
    if (demo)
        return;
    cellGrid *g = grid;
    g->setSize(20, 12);
    g->cls();
    g->cllabel();
    bgColour = 0;
    g->printAt(0, 1, "~9009[\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\]");
    g->printAt(0, 0xb, "{||||||||||||||||||} ");
    for (int r = 2; r <= 10; r++) {
        g->printAt(0, r, "_");
        g->printAt(0x13, r, "^");
    }
    for (int y = 4; y != 8; y++)
        for (int x = 7; x <= 0xc; x++)
            g->lpoke(x, y, 1);
    updateStats();
}

// --------------------------------------------------------- enemy shots -----

// Fires a shootableEnemyBullet from `from`. kind: 0 horizontal (towards v.x
// sign), 1 static, 2 random-direction "cake" bullet, 3/7 velocity v, 4 homing
// along v, 5 vertical, 6 static mine.
void theGame::shootFrom(entity *from, vec2d v, int kind, int spriteNo, int flightMode, float sz,
                        uint32_t rgba) {
    // (The original also allocates a second, unused bullet here and leaks it.)
    shootableEnemyBullet *b = new shootableEnemyBullet();
    b->init(this);
    b->killingShotSurvives = false;
    b->framesRemaining = 300;
    b->edgeBehavior = 1;
    b->flightMode = flightMode;
    b->pos = from->pos;
    b->baseSprite = spriteNo;
    b->spriteIndex = spriteNo;
    b->col.unpackRGBA(rgba);
    b->size = sz;
    auto aimed = [&](float spinPerFrame) {
        b->vel = v;
        b->invulnerableFrames = 10;
        b->spinPerFrame = spinPerFrame;
        b->rot = float(std::atan2(-v.y, v.x) - 1.570796327);
        b->pos = b->pos + b->vel;
    };
    switch (kind) {
    case 0:
        b->vel.y = 0;
        if (v.x > 0.0) {
            b->vel.x = 0.005;
            b->rot = -1.5707964f;
        } else {
            b->vel.x = -0.005;
            b->rot = 1.5707964f;
        }
        b->spinPerFrame = float(rng.rand(-0.1, 0.1));
        break;
    case 1:
        b->vel = vec2d(0, 0);
        b->spinPerFrame = 0;
        break;
    case 2:
        b->killingShotSurvives = true;
        b->seekHeading = float(rng.rand(-kPi, kPi));
        b->seekSpeed = cakeBullSpeeds[level / 10];
        b->spinPerFrame = 0;
        b->seekTurnRate = 0.1f;
        b->spriteAngleOffset = -2.3561945f;
        break;
    case 3:
        aimed(0.0f);
        break;
    case 4:
        b->killingShotSurvives = true;
        b->seekHeading = float(std::atan2(-v.y, v.x) - 1.570796327);
        b->seekSpeed = float(v.length());
        b->spinPerFrame = 0;
        b->seekTurnRate = 0.1f;
        b->spriteAngleOffset = -2.3561945f;
        break;
    case 5:
        b->vel.x = 0;
        if (v.y > 0.0) {
            b->vel.y = 0.005;
            b->rot = 0.0f;
        } else {
            b->vel.y = -0.005;
            b->rot = 3.1415927f;
        }
        b->spinPerFrame = float(rng.rand(-0.1, 0.1));
        break;
    case 6:
        b->vel = vec2d(0, 0);
        b->spinPerFrame = 0;
        b->flightMode = 3;
        b->seekSpeed = lickerMineSpeeds[level - 80];
        break;
    case 7:
        aimed(0.04f);
        break;
    }
    addEntity(b, &enemyShots);
}

void theGame::swarmerSplit(entity *e) {
    int i = level - 70;
    int n = numSwarmers[i];
    float spread = swarmerSpread[i];
    float step = spread / float(n);
    vec2d d(0, 0);
    if (players)
        d = e->pos - players->pos;
    double ang = std::atan2(-d.y, d.x);
    if (n > 0) {
        float a = float(ang - 1.570796327) + spread * -0.5f;
        for (int k = 0; k < n; k++) {
            vec2d v(0, 0.003);
            v.rotateBy(double(a));
            a += step;
            shootFrom(e, v, 3, 0xa7, 0, 0.015f, 0xffffffff);
        }
    }
}

// Breaks e's sprite into cols x rows shatterBits that fly apart (frames > 0)
// or converge from outside (frames < 0, the "rez" effect). Removes e.
void theGame::pixelShatter(entity *src, entity *e, vec2d v, int frames, int type, int cols,
                           int rows, int mode) {
    sprite s;
    entity *player = players;
    s.frame = e->spriteIndex + kLitBase;
    vec2d sz = s.getSize();
    int wi = int(sz.x), hi = int(sz.y);
    if (cols < 1)
        cols = wi / 2;
    if (rows < 1)
        rows = hi / 2;
    int pw = wi / cols, ph = hi / rows;
    double dpw = double(float(pw)), dph = double(float(ph));
    vec2d scl = vec2d(e->scaleX / sz.x, e->scaleY / sz.y) * vec2d(dpw, dph);
    double x0 = e->pos.x + e->scaleX / dpw * 0.5 * double(e->size) * -2.0;
    double stepTx = double(2.0f / float(cols));
    double stepTy = double(2.0f / float(rows));
    if (rows > 0) {
        double y = e->pos.y + double(e->size) * (e->scaleY / dph * 0.5) * -2.0;
        float inv = 1.0f / float(frames);
        float invNeg = 1.0f / -float(frames);
        double ty = -1.0;
        int py = 0;
        for (int r = 0;; r++) {
            double sizeR = double(e->size);
            if (cols > 0) {
                double pyc = double(float(py)) + 0.5;
                double x = x0, tx = -1.0;
                double vty = ty * 0.01;
                int px = 0;
                for (int c = 0;; c++) {
                    shatterBit *b = new shatterBit();
                    b->rate = 0.02f;
                    b->f19c = 1.0f;
                    b->type = type;
                    if (type == 1 && player)
                        b->target = player->pos;
                    b->game = this;
                    b->pos = vec2d(x, y);
                    b->size = e->size;
                    b->scaleX = scl.x;
                    b->scaleY = scl.y;
                    b->pieceH = dph;
                    b->pieceW = dpw;
                    b->spriteIndex = e->spriteIndex;
                    b->col = e->col;
                    b->texX = double(float(px)) + 0.5;
                    b->texY = pyc;
                    switch (mode) {
                    case 0:
                        b->vel = vec2d(vty, vty);
                        if (src)
                            b->vel.x = vty * e->pos.x;
                        break;
                    case 1: {
                        double k = tx * 0.01;
                        b->vel = vec2d(k, k);
                        if (src)
                            b->vel.x = k * e->pos.x;
                        break;
                    }
                    case 2:
                        b->vel.y = vty;
                        b->vel.x = vty * e->pos.x;
                        break;
                    case 3:
                        b->vel.y = tx * 0.01;
                        b->vel.x = tx * 0.01 * e->pos.x;
                        break;
                    }
                    b->vel = b->vel * v;
                    if (frames > 0) {
                        b->rate = inv;
                    } else {
                        b->rate = invNeg;
                        b->pos = b->pos + b->vel * double(-float(frames));
                        b->vel = b->vel * -1.0;
                    }
                    addEntity(b, &particles);
                    px += pw;
                    sizeR = double(e->size);
                    if (c + 1 == cols)
                        break;
                    x += scl.x * sizeR + scl.x * sizeR;
                    tx += stepTx;
                }
            }
            py += ph;
            if (r + 1 == rows)
                break;
            ty += stepTy;
            y += scl.y * sizeR + scl.y * sizeR;
        }
    }
    e->dead = true;
}

void theGame::bootScreen() {
    cellGrid *g = grid;

    g->setSize(24, 12);
    bgColour = 0x546e007f;
    g->cPrintAt(1, " ~9999& ~9909& ~0949& ~1519&   ~7769& ~9219& ~0199& ~1119&");
    g->cPrintAt(3, "~9999Cattle Electronics");
    g->cPrintAt(4, "presents");
    g->cPrintAt(7, "MINOTRON: 2112");
    g->cPrintAt(10, "Copr @ 2011 Llamasoft");
    reset(0);
}

// ---------------------------------------------------------- level setup ----

void theGame::newLevel() {
    rescuedAllBeforeKills = 0;
    spawnQueueDrained = false;
    f1198[0] = 0;

    restarts = 0;
    levelEnding = false;
    pickupBonusIndex = 0;
    fiveWayShots = false;
    bombsCollected = 0;
    deathPauseFrames = 0;
    rescueChain = 0;
    beamCycleLength = 200;
    beamCycleFrames = 200;
    beamFireFrames = 100;
    makeArena();

    for (entity *p = players; p; p = p->next)
        p->health = 3;
    static const float kInit[] = {0.02f, 0.015f, 0.01f, 0,    0,    0,    0.5f,  0.5f,
                                  0.2f,  0.3f,   0.7f,  0.2f, 0,    0.2f, 0.15f, 0.1f,
                                  0,     0,      0,     0.1f, 0.9f, 0.2f, 0.3f,  0.7f};
    for (int i = 0; i < 24; i++)
        if (i != 12 && (i < 16 || i > 18))
            fb90[i] = kInit[i];
    shakeFrames = 0;
    fb90[12] = 0.0f; // 0xbc0
    spawnCount = 0;
    int nGrunts = levelDesc(level, 2);
    for (int k = 0; k < nGrunts; k++)
        spawnQueue[spawnCount++] = 4;
    if (levelDesc(level, 3) >= 0) {
        int i = 3;
        do {
            for (int k = 0; k < levelDesc(level, i + 1); k++)
                spawnQueue[spawnCount++] = levelDesc(level, i);
            i += 2;
        } while (uint32_t(levelDesc(level, i)) < 0x80000000u);
    }
    levelFlags = gameFlags[level];
    gruntDelay = maxGruntDelay[level] == 0 ? minGruntDelay[level] + nGrunts : maxGruntDelay[level];
    gruntSpeed = gruntSpeeds[level];
    gruntSpeedMul = gruntSpeedMultipliers[level];
    stepSpeedAdd = stepSpeedAdders[level];
    maxGruntSpeed = maxGruntSpeeds[level];
    hotShotFrames = 0;
    forcedPowerupKind = 0;
    if (level > 0x62)
        beastLoveFrames = 0;
    extraLove = extraLoveDurations[level];
    powerupTimer = 10;
    beastAttractionFrames = 0;
    shotSpeed = 0.01f;
    shotDelay = 12;
    hotShotsPermanent = false;
    spreadShots = false;
    reflectiveShots = false;
    if (spreadPowerupDue) {
        spreadPowerupDue = false;
        if (!useRobot) {
            powerupTimer = 0;
            forcedPowerupKind = 3;
        }
    }
    if (!players) {
        for (int k = 0; k < nPlayers; k++) {
            playerMinotaur *p = new playerMinotaur();
            p->game = this;
            addEntity(p, &players);
            p->init(this);
        }
    } else {
        static_cast<playerMinotaur *>(players)->rescued = 0;
        static_cast<playerMinotaur *>(players)->trailIndex = 0;
    }
    for (int k = 0; k < levelDesc(level, 1) * 2; k++) {
        staticObstacle *o = new staticObstacle();
        o->init(this);
        addEntity(o, &obstacles);
    }
    for (int k = 0; k < levelDesc(level, 0); k++) {
        beast *b = new beast();
        b->init(this);
        addEntity(b, &beasts);
    }
}

void theGame::newGame() {
    reset(0);
    f1198[1] = f1198[2] = f1198[3] = 0;
    bonusPickupChain = 0;
    deathsThisRun = 0;
    reducedPowerups = false;
    useRobot = false;
    autoAim = false;
    hard = false;
    if (gameMode == 2) {
        autoAim = true;
        reducedPowerups = true;
    } else if (gameMode == 3) {
        hard = true;
    } else if (gameMode == 1) {
        useRobot = true;
        reducedPowerups = true;
    }
    running = true;
    spawnCount = 0;
    message = 0;
    forcePowerup = false;
    deathPauseFrames = 0;
    beastLoveFrames = 200;
    driftAngle = 2.3561945f;
    driftSpeed = 0.004f;
    gravity = vec2d(0, 0.0007); // 0x3f46f0068db8bac7
    colour c;
    c.phase[0] = c.phase[1] = c.phase[2] = 0;
    c.speed[0] = 0.3f;
    c.speed[1] = 0.04f;
    c.speed[2] = 0.005f;
    c.amp[0] = 0.8f;
    c.amp[1] = 0.7f;
    c.amp[2] = 0.7f;
    c.base[0] = 0.2f;
    c.base[1] = 0.3f;
    c.base[2] = 0.3f;
    gcol[0] = c;
    driftPhaseX = float(rng.rand(6.2831853072));
    driftPhaseStepX = float(rng.rand(-0.01, 0.01));
    driftPhaseY = float(rng.rand(6.2831853072));
    driftPhaseStepY = float(rng.rand(-0.01, 0.01));
    level = demo ? demoLevel : selLevel;
    // Per-player stats. With "resume best" the saved score and lives are
    // loaded (the original always writes player 1's slots).
    for (int p = 0; p < 4; p++) {

        if (!resumeBest) {
            score[p] = 0;
            lives[p] = practice ? 1 : 3;
        } else {
            score[0] = best(gameMode, level, 1);
            lives[0] = best(gameMode, level, 0);
        }
    }
    resumeBest = false;
    if (practice) {
        score[0] = 0;
        level = 0;
        lives[0] = 1;
        endurance = true;
    }
    bgColour = 0;
    spreadPowerupDue = false;
    if (useRobot && !demo) {
        aiDroid *d = new aiDroid(this);
        addEntity(d, &particles);
    }
    if (demo)
        newLevel();
}

// ------------------------------------------------- scrolling text screens --

int theGame::minoInfoScreen() {
    static int lastFrame = -1, tp = 0, lineCount = 0, touchdown = 0;
    static bool bounce = false;
    static float scrollVel = 0, lastJoy = 0;
    static const char *const *const kPages[3] = {kInfoText0, kInfoText1, kInfoText2};
    static const int kCount[3] = {191, 229, 172};
    int page = infoPage;
    cellGrid *g = grid;
    frontEndTouchMode = 2;
    const char *const *text = kPages[page];
    auto line = [&](int i) -> const char * {
        return i >= 0 && i < kCount[page] ? text[i] : nullptr;
    };
    if (frameCount - 1 != lastFrame) {
        reset(0);
        waitRelease = false;
        fbColour[0] = fbColour[1] = fbColour[2] = 0.4f;
        fbColour[3] = 0.45f;
        fbScaleX = fbScaleY = 1.0f;
        scrollVel = 0.0f;
        g->setSize(48, 28);
        lineCount = 0;
        running = true;
        frontEndTouchMode = 0;

        tp = 0;
        if (g->rowsN >= 0) {
            do {
                g->cll(tp, 0x20);
                g->printAt(0, tp, line(tp));
                tp++;
            } while (tp <= g->rowsN);
        }
    }
    lastFrame = frameCount;
    if (infoControllerMode && !joyActive[0]) {
        // Keep controller scrolling bounded and stop on release. Touch retains
        // its original drag/inertia path, using the same text cursor afterwards.
        int end = 0;
        while (end < kCount[page] && (!line(end) || *line(end) != '^'))
            end++;
        neon::input::scrollText(g, tp, end, infoScrollAxis, 0.3, line);
        scrollVel = 0;
        bounce = false;
        touchdown = 0;
        return 0;
    }
    if (bounce) {
        g->scroll(0.0f, scrollVel);
        if (g->scrollCount[2] != 0) {
            g->cll(0, 0x20);
            int n = g->scrollCount[2];
            int i = tp - g->rowsN - n - 1;
            if (i >= 0) {
                g->printAt(0, 0, line(i));
                n = g->scrollCount[2];
            }
            tp -= n;
        }
        if (g->scrollCount[3] != 0) {
            g->cll(g->rowsN, 0x20);
            const char *s = line(tp);
            if (s && *s != '^')
                g->printAt(0, g->rowsN, s);
            tp += g->scrollCount[3];
        }
        g->scrollCount[2] = 0;
        g->scrollCount[3] = 0;
        if (!joyActive[0])
            bounce = false;
        scrollVel = 0.0f;
        return 0;
    }
    if (!joyActive[0]) {
        if (touchdown)
            touchdown = 0;
    } else if (!touchdown) {
        if (0.36 < joyOrigin[0].x && 0.27 < joyOrigin[0].y) {
            playSample(5);
            lastFrame = -1;
            g->setSize(40, 24);
            return 1;
        }
        touchdown = 1;
        lastJoy = float(joyDelta[0].y);
        if (!bounce)
            scrollVel = 0.0f;
    } else {
        scrollVel = float((double(lastJoy) - joyDelta[0].y) * 0.8);
    }
    g->scroll(0.0f, scrollVel);
    int n = g->scrollCount[3];
    if (n != 0 && n > 0) {
        for (;;) {
            g->cll(g->rowsN, 0x20);
            const char *s = line(tp);
            if (s && *s != '^')
                g->printAt(0, g->rowsN, s);
            tp++;
            s = line(tp);
            if (s) {
                bounce = *s == '^';
            }
            if (--n == 0)
                break;
        }
    }
    n = g->scrollCount[2];
    if (n != 0) {
        bool top = false;
        if (n >= 1) {
            for (int r = 0; r < n; r++) {
                g->cll(r, 0x20);
                n = g->scrollCount[2];
                int i = tp - g->rowsN - n - 1 + r;
                if (i < 0)
                    top = true;
                else {
                    g->printAt(0, r, line(i));
                    n = g->scrollCount[2];
                }
            }
        }
        tp -= n;
        bounce = top;
    }
    if (!bounce) {
        scrollVel *= 0.98f;
    } else if (g->scrollCount[2] == 0) {
        if (g->scrollCount[3] != 0 && scrollVel > 0.0f)
            scrollVel = -scrollVel;
    } else if (scrollVel < 0.0f) {
        scrollVel = -scrollVel;
    }
    g->scrollCount[3] = 0;
    g->scrollCount[2] = 0;
    (void)lineCount;
    return 0;
}

int theGame::storyTextScroll() {
    static int lastFrame = -1, scrollerNum = 1, tp = 0, lineCount = 0;
    static const char *howTo[232];
    static bool howToBuilt = false;
    if (!howToBuilt) {
        for (int i = 0; i < kHowToPlayCount; i++)
            howTo[kHowToPlay[i].index] = kHowToPlay[i].text;
        howToBuilt = true;
    }
    static const bool kCentred[3] = {false, true, false};
    static const bool kHalfSpeed[3] = {true, false, false};
    const char *const *texts[3] = {kStoryText0, kStoryText1, howTo};
    const int counts[3] = {53, 145, 232};
    cellGrid *g = grid;
    if (frameCount - 1 != lastFrame) {
        waitRelease = false;
        g->cls();
        g->clattr();
        g->setSize(40, 24);
        lineCount = 0;
        bgColour = 0;
        if (++scrollerNum > 2)
            scrollerNum = 0;
        tp = 0;
        frontEndTouchMode = 0;

        fbColour[0] = 0.3f;
        fbColour[1] = 0.2f;
        fbColour[2] = 0.8f;
        fbColour[3] = 0.8f;
        nPlayers = (neonRand() & 3) + 1;
        if (maxPlayers < nPlayers)
            nPlayers = maxPlayers;
        newGame();
    }
    lastFrame = frameCount;
    g->scroll(0.0f, 0.03f);
    if (g->scrollCount[3] != 0) {
        g->scrollCount[3] = 0;
        g->cll(g->rowsN, 0x20);
        g->clla(g->rowsN);
        lineCount++;
        if (kHalfSpeed[scrollerNum] && (lineCount & 1) == 0)
            return 0;
        const char *const *t = texts[scrollerNum];
        auto line = [&](int i) -> const char * { return i < counts[scrollerNum] ? t[i] : nullptr; };
        if (!kCentred[scrollerNum])
            g->printAt(0, g->rowsN, line(tp));
        else
            g->cPrintAt(g->rowsN, line(tp));
        tp++;
        const char *s = line(tp);
        if (s && *s == '^') {
            lastFrame = -1;
            return 1;
        }
    }
    if (!joyActive[0])
        return 0;
    playSample(5);
    lastFrame = -1;
    return 1;
}

// ------------------------------------------------- materialise effects -----

void theGame::rez(entity *e, int mode, float speed, int frames) {
    double s = double(speed);
    switch (mode) {
    case 0:
        pixelShatter(nullptr, e, vec2d(s, s), -frames, 0, 1, 0, 2);
        break;
    case 1:
        pixelShatter(nullptr, e, vec2d(0, s), -frames, 0, 1, 0, 0);
        break;
    case 2:
        pixelShatter(nullptr, e, vec2d(s, 0), -frames, 0, 0, 1, 1);
        break;
    case 3:
        pixelShatter(nullptr, e, vec2d(0, s), -frames, 0, 1, 0, 0);
        pixelShatter(nullptr, e, vec2d(s, 0), -frames, 0, 0, 1, 1);
        break;
    }
    e->drawDelay = frames;
    e->dead = false;
}

void theGame::derez(entity *e, int mode, float speed, int frames) {
    double s = double(speed);
    switch (mode) {
    case 0:
        pixelShatter(nullptr, e, vec2d(s, s), frames, 0, 1, 0, 2);
        break;
    case 1:
        pixelShatter(nullptr, e, vec2d(0, s), frames, 0, 1, 0, 0);
        break;
    case 2:
        pixelShatter(nullptr, e, vec2d(s, 0), frames, 0, 0, 1, 1);
        break;
    case 3:
        pixelShatter(nullptr, e, vec2d(0, s), frames, 0, 1, 0, 0);
        pixelShatter(nullptr, e, vec2d(s, 0), frames, 0, 0, 1, 1);
        break;
    }
    e->dead = true;
}

void theGame::restartLevel() {
    deathPauseFrames = 0;
    spawnQueueDrained = false;
    beamCycleFrames = beamCycleLength;
    if (!demo)
        playSample(level % 5 == 4 ? 0x20 : 0x1f);

    powerupTimer = 1;
    rescueChain = 0;
    restarts++;
    for (int k = 0; k < nPlayers; k++) {
        playerMinotaur *p = new playerMinotaur();
        p->game = this;
        addEntity(p, &players);
        p->init(this);
    }
    if (useRobot && !demo) {
        aiDroid *d = new aiDroid(this);
        addEntity(d, &particles);
    }
    for (int k = 0; k < nObstacles; k++) {
        staticObstacle *o = new staticObstacle();
        o->init(this);
        addEntity(o, &obstacles);
    }
    beastAttractionFrames = 0;
    int extra = restarts - 2;
    if (extra > 2)
        extra = 3;
    if (restarts > 3)
        beastAttractionFrames = 0x3c;
    if (nBeasts < extra)
        nBeasts = extra;
    for (int k = 0; k < nBeasts; k++) {
        beast *b = new beast();
        b->init(this);
        addEntity(b, &beasts);
    }
    if ((restarts > 1 || level > 0x59) && !useRobot) {
        powerupTimer = 0;
        forcedPowerupKind = 3;
    }
    if (levelFlags & 4) {
        if (maxGruntDelay[level] == 0)
            gruntDelay = minGruntDelay[level] + levelDesc(level, 2);
        gruntSpeed = gruntSpeeds[level];
        gruntSpeedMul = gruntSpeedMultipliers[level];
        stepSpeedAdd = stepSpeedAdders[level];
        maxGruntSpeed = maxGruntSpeeds[level];
    }
}

// ----------------------------------------------------- splitting enemies ---

void theGame::spinySplit(entity *e) {
    int n = spikeNums[level];
    float step = float(6.2831853072 / double(float(n) + float(n)));
    float step2 = step + step;
    if (!demo)
        playSample(0x21);
    float a = step;
    for (int k = 0; k < n; k++) {
        shootableEnemyBullet *b = new shootableEnemyBullet();
        b->init(this);
        b->edgeBehavior = 1;
        b->rot = a;
        b->vel = vec2d(0, -0.005000001);
        b->flightMode = 0;
        b->vel.rotateBy(double(a));
        b->pos = e->pos;
        b->spriteIndex = 0x60;
        b->size = 0.01f;
        b->scaleY = 2.0;
        addEntity(b, &enemyShots);
        a += step2;
    }
}

void theGame::zombeastSplit(entity *e) {
    if (!demo)
        playSample(0x35);
    for (int k = 0; k < 5; k++) {
        for (int side = 0; side < 2; side++) {
            shrapnel *s = new shrapnel();
            s->game = this;
            s->type = 0x1a;
            s->spriteIndex = 0x82;
            s->size = float(oxRNG::rand(0.003, 0.01));
            s->f168 = 0x78;
            s->vel.x = side == 0 ? rng.rand(-0.004, -0.002) : rng.rand(0.002, 0.004);
            s->vel.y = rng.rand(-0.001, 0.001);
            s->pos = e->pos;
            addEntity(s, &enemies);
        }
    }
}

void theGame::splash(int n, vec2d p) {
    if (!demo)
        playSample(0x34);
    for (int k = 0; k < n; k++) {
        droplet *d = new droplet();
        d->vel.y = rng.rand(-0.03, -0.001);
        d->vel.x = rng.rand(-0.01, 0.01);
        d->from = colour(0.2, 1.0, 1.0, 0.7);
        d->to.blue(0.2);
        d->sizeEnd = 2.0f;
        d->spriteIndex = 0x30;
        d->game = this;
        d->fade = 0.05f;
        d->pos = p;
        addEntity(d, &particles);
    }
}

void theGame::checkPowerup(vec2d p) {
    if (!forcePowerup) {
        if (--powerupTimer < 1) {
            unsigned r = unsigned(neonRand() + 100) & 0x3f;
            powerupTimer = hard ? int(r << 1) : int(r);
            powerup *u = new powerup();
            u->init(this);
            u->pos = p;
            addEntity(u, &beasts);
        }
    } else {
        int saved = forcedPowerupKind;
        forcedPowerupKind = 6;
        if (!demo)
            playSample(0x10);
        powerup *u = new powerup();
        u->init(this);
        u->pos = p;
        addEntity(u, &beasts);
        forcePowerup = false;
        forcedPowerupKind = saved;
    }
}

// Herd waves: at the level end the rescued beasts explode one by one into
// bonuses. Returns 1 while there are beasts left to explode.
int theGame::explodeLastBeast() {
    playerMinotaur *p = static_cast<playerMinotaur *>(players);
    if (!p || p->rescued <= 0)
        return 0;
    int n = --p->rescued;
    for (entity *e = beasts; e; e = e->next) {
        if (e->type != 1)
            continue;
        beast *b = static_cast<beast *>(e);
        if (b->rescued && n == b->followingSlot) {
            pixelShatter(nullptr, e, vec2d(double(0.2f), double(0.2f)), 0x28, 0, 1, 0, 2);
            littleBonus *lb = new littleBonus();
            lb->init(this);
            lb->pos = e->pos;
            addEntity(lb, &particles);
            if (!demo)
                playSample(0x11);
            return 1;
        }
    }
    return 0;
}

// ------------------------------------------------------------- scoring -----

bool theGame::goalAllowed(unsigned bit) const {
    return !demo && nPlayers < 2 && banner < 2 && !(goalsDone & bit);
}

// Shows "EXCELLENT PLAY!" with achievement message `ach`, marks it done
// in goalsDone and asks the shell to unlock it (command `cmd`).
void theGame::goal(int ach, unsigned bit, int cmd) {
    msgTimer = 60;
    if (message != 0)
        eraseMessage();
    message = 4;
    achievement = ach;
    msgTimer = 240;
    goalsDone |= bit;
    shellCommand = int8_t(cmd);
    if (!demo)
        playSample(60);
}

// Adds to player 1's score. Crossing 1,000,000 is a goal; crossing 10,000,
// 20,000 and every further 20,000 earns a powerup (until life/level 99).
void theGame::addScore(int points) {
    int old = score[0];
    int now = old + points;
    score[0] = now;
    if (practice)
        return;
    if (now > 999999 && old < 1000000 && goalAllowed(0x20))
        goal(5, 0x20, 9);
    if (now <= 9999)
        return;
    bool crossed = false;
    if (old < 10000) {
        crossed = true;
    } else if (now > 19999) {
        for (int t = 20000; t <= now; t += 20000)
            if (old < t) {
                crossed = true;
                break;
            }
    }
    if (crossed && lives[0] <= 98 && level <= 98)
        forcePowerup = true;
}

// ----------------------------------------------------------- damage ------

// `e` loses `amount` hit points (hit by `by`, or by a bomb when null). On
// death: maybe a powerup, optional shatter, a score popup when the bonus
// multiplier is on, score with extra-life/powerup thresholds, and the
// "explosive shots" blast entity.
void theGame::damage(entity *by, entity *e, int amount, int points, int shatter) {
    // The "explosive shots" level flag: a blast at every hit.
    auto blast = [&]() {
        expandaBull *x = new expandaBull();
        x->spriteIndex = 0x91;
        x->game = this;
        x->size = 0.01f;
        x->type = 0x31;
        x->pos = e->pos;
        addEntity(x, &projectiles);
        if (!demo)
            playSample(0x19);
    };
    e->hp -= amount;
    if (e->hp < 1) {
        if (points > 0)
            checkPowerup(e->pos);
        if ((signed char)shatter != 0)
            pixelShatter(by, e, vec2d(1.0, 1.0), 0x1e, 0, 1, 0, 2);
        int mult = 1;
        if (spreadShots) {
            // score multiplier popup (x3, or x6 with fiveWayShots)
            littleBonus *lb = new littleBonus();
            lb->pos = e->pos;
            lb->game = this;
            lb->spriteIndex = fiveWayShots ? 0x8d : 0x8c;
            lb->framesRemaining = 0x32;
            lb->paletteIndex = 1;
            lb->growthPerFrame = 1.0f;
            lb->size = 0.02f;
            lb->scaleY = 1.1;
            lb->vel.y = -0.0008;
            mult = fiveWayShots ? 6 : 3;
            addEntity(lb, &particles);
        }
        addScore(points * mult);
        if (levelFlags & 8)
            blast();
        if (!by) {
            e->hitBy(nullptr);
            return;
        }
    } else {
        e->flashFrames = 0x20;
        if (!by)
            return;
        if (!demo)
            playSample(0x22);
    }
    if (hotShotFrames < 1 && by->type != 3)
        by->dead = true;
    if (level > 97)
        by->dead = true;
    if (levelFlags & 8)
        blast();
}

// ------------------------------------------------------------- movement ----

namespace {
// Shared arena edge handling of the seek functions (x: -0.46..0.45,
// y: -0.38..0.45). edge 1 kills, 2 bounces by elasticity, 0 stops.
void moveClipped(entity *e, int edge) {
    vec2d np = e->pos + e->vel;
    double vx = e->vel.x;
    double sz = double(e->size);
    if ((vx > 0.0 && 0.44999998807907104 < sz + np.x) ||
        (vx < 0.0 && np.x - sz < -0.46000000834465027)) {
        if (edge == 1)
            e->dead = true;
        else if (edge == 2)
            e->vel.x = -(vx * double(e->elasticity));
        else if (edge == 0)
            e->vel.x = 0;
    } else {
        e->pos.x = np.x;
    }
    double vy = e->vel.y;
    if ((vy <= 0.0 || sz + np.y <= 0.44999998807907104) &&
        (vy >= 0.0 || -0.3799999952316284 <= np.y - sz)) {
        e->pos.y = np.y;
        return;
    }
    if (edge == 1)
        e->dead = true;
    else if (edge == 2)
        e->vel.y = -(vy * double(e->elasticity));
    else if (edge == 0)
        e->vel.y = 0;
}
} // namespace

// Accelerates by (seekAccelX, seekAccelY) towards the target (or the arena centre), up
// to speeds (speedLimitX, speedLimitY) per axis, then applies drag.
void theGame::inertialSeek(entity *e, entity *target, int edge) {
    vec2d d = target ? target->pos - e->pos : e->pos * -1.0;
    float vx = float(e->vel.x);
    float nx = float(d.x < 0.0 ? double(vx) - e->seekAccelX : double(vx) + e->seekAccelX);
    if (double(std::fabs(nx)) < e->speedLimitX)
        e->vel.x = double(nx);
    float vy = float(e->vel.y);
    float ny = float(d.y < 0.0 ? double(vy) - e->seekAccelY : double(vy) + e->seekAccelY);
    if (double(std::fabs(ny)) < e->speedLimitY)
        e->vel.y = double(ny);
    e->vel = e->vel * e->drag;
    moveClipped(e, edge);
}

// Turns heading seekHeading by seekTurnRate per frame towards the target and moves at seekSpeed.
void theGame::vectorSeek(entity *e, entity *target, int edge) {
    vec2d d = target ? target->pos - e->pos : e->pos * -1.0;
    float diff = float(std::atan2(-d.y, d.x) - double(e->seekHeading));
    if (3.141592654 < double(std::fabs(diff)))
        diff = -diff;
    float h = diff <= 0.0f ? e->seekHeading - e->seekTurnRate : e->seekHeading + e->seekTurnRate;
    e->seekHeading = h;
    if (3.1415927f < h) {
        h -= 6.2831855f;
        e->seekHeading = h;
    }
    if (h < -3.1415927f)
        e->seekHeading = h + 6.2831855f;
    e->vel = vec2d(0, double(-e->seekSpeed));
    e->vel.rotateBy(double(e->seekHeading) - 1.570796327);
    moveClipped(e, edge);
}

// Moves straight at speed seekSpeed towards the target; targetDistance = distance to it.
void theGame::moveTowards(entity *e, entity *target, int edge) {
    vec2d d = target ? target->pos - e->pos : e->pos * -1.0;
    e->targetDistance = float(d.length());
    vec2d n = d.normalize();
    e->vel = n * double(e->seekSpeed);
    e->vel = e->vel * e->drag;
    moveClipped(e, edge);
}

} // namespace Neon_Internals
} // namespace NEON_GAME

// =================================================== (port) high scores ====
// Local tables standing in for OpenFeint's leaderboards (shared/grid_menus.h
// draws the screens). The shell loads and saves them.

namespace NEON_GAME {
namespace Neon_Internals {

namespace {
const char *const kBoardNames[theGame::kHiBoards] = {
    "~9909NORMAL",
    "~9909ASSISTED",
    "~9909SIMPLIFIED",
    "~9909HARD",
    "~9909NORMAL - ENDURANCE",
    "~9909ASSISTED - ENDURANCE",
    "~9909SIMPLIFIED - ENDURANCE",
    "~9909HARD - ENDURANCE",
};

void scoreFeedback(theGame *g) {
    g->fbColour[0] = g->fbColour[1] = g->fbColour[2] = 0.2f;
    g->fbColour[3] = 0.1f;
    g->fbScaleX = g->fbScaleY = 1.04f;
}
} // namespace

void theGame::defaultHighScores() {
    static const std::vector<std::string> kNames = {
        "LLAMA",  "YAK",     "CAMEL", "SHEEP", "GOAT", "ALPACA",
        "VICUNA", "GUANACO", "OX",    "BISON", "ZEBU", "MINOTAUR",
    };
    hs.init(kHiBoards, kNames, 100000, 10000);
    for (int b = 4; b < kHiBoards; b++)
        for (int r = 0; r < mino::HighScores::kRows; r++)
            hs.at(b, r).score /= 2;
}

void theGame::highScoreScreen() {
    scoreFeedback(this);
    grid->setSize(40, 24);
    mino::grid::drawScoreTable(grid, hs, hiBoard, kBoardNames[hiBoard],
                               kBannerColoursTitle[hiBoard & 3], hiNew);
}

void theGame::nameEntryScreen() {
    scoreFeedback(this);
    grid->setSize(40, 24);
    mino::grid::drawNameEntry(grid, entryName, entryScore, entryLevel);
}

void theGame::finishNameEntry() {
    hs.lastName = mino::grid::finalName(entryName);
    hiNew = hs.insert(entryBoard, hs.lastName, entryScore, entryLevel);
    hs.dirty = true;
    hiBoard = entryBoard;
    hiReturn = 1;
    hiTimer = 0;
    highScoreScreen();
    state = 0x71;
}

void theGame::leaveHighScores() {
    cellGrid *g = grid;
    g->cls();
    g->clattr();
    hiNew = -1;
    waitRelease = true;
    if (hiReturn == 1) { // after a game: on to the title
        bgColour = 0;
        state = 1;
        frameCount = 0;
        return;
    }
    newGame();
    if (hiReturn == 3)
        infoScreen();
    else
        printTitleScreen();
    state = 3;
}

void theGame::scoreScreensInput(int activationLabel) {
    bool entry = state == 0x72;
    if (++hiTimer > (entry ? 60 * 60 : 60 * 30)) { // nobody there
        if (entry)
            finishNameEntry();
        else
            leaveHighScores();
        return;
    }
    if (waitRelease) {
        if (!joyActive[0])
            waitRelease = false;
        return;
    }
    if (!joyActive[0] && !activationLabel)
        return;
    waitRelease = true;
    int label = activationLabel ? activationLabel : grid->getLabel(joyOrigin[0]);
    if (!entry) {
        if (label == mino::grid::kLabelNextTable || label == mino::grid::kLabelTableName) {
            playSample(5);
            hiBoard = (hiBoard + 1) % kHiBoards;
            hiNew = -1;
            highScoreScreen();
        } else if (label == mino::grid::kLabelBack) {
            playSample(5);
            leaveHighScores();
        }
        return;
    }
    int k = mino::grid::nameEntryKey(label, entryName);
    if (k == 0)
        return;
    playSample(5);
    hiTimer = 0;
    if (k == 2)
        finishNameEntry();
    else
        nameEntryScreen();
}

} // namespace Neon_Internals
} // namespace NEON_GAME
