// menu_states.cpp - theGame's menu states (Minos_armv7): theGame::run cases 1 (title set-up),
// 2 (the boot screen, then the title texts), 3 (title), 7 (select), 8 (game over / finished),
// 9 (info); goSelect 0x5e548, updateSelect 0x579b4, dynamicSelect 0x57b34, starReset
// 0x5e818, dynamicInfo 0x58268, dynamicGameOver 0x58f3c, dynamicGameFinished 0x6bac8,
// fireWorkExplosion 0x6b648, the stm layout announceCavern 0x60740 and run case 1 build,
// openfeintButton (run 0x5ec54, draw 0x56da8, onPressed 0x53e8c), and _draw's title
// sprites (doTitle 0x56e98, doTitleFlash 0x56260). Translated from the decompilation with
// instruction-derived loop counters, colours and binary64 steps retained explicitly.
#include "game_states.h"
#include "cavern_data.h"
#include <cstdio>
#include <cstring>

namespace minos {

const char *const kQuestNames[8] = {"The quest for", "The search for", "A mission into",
                                    "Discovering",   "To seek",        "The battle for",
                                    "Fighting for",  "The battle of"};
const char *const kRankNames[4] = {"NOVICE", "PILOT", "WARRIOR", "COMMANDER"};
const char *const kTips[30] = {"Minotaurs rescued",   "give both fuel and", "shield recharges.",
                               "Save some for the",   "journey out!",       "Try to land softly",
                               "when you can. This",  "impresses the",      "minotaurs, who will",
                               "give more bonus.",    "You don't have to",  "rescue every",
                               "minotaur to finish",  "a level, but they",  "prefer you do.",
                               "Create your own",     "restart point",      "whenever you like",
                               "by simply landing",   "your ship.",         "Minotaurs enjoy",
                               "cuddles, skritches,", "Digestive biccies,", "cups of tea and",
                               "the smell of oxen.",  "When all is said",   "and done, you",
                               "really can't beat",   "a lovely cup of",    "tea."};

namespace {
using cavern::kCavernDefinitions;
constexpr double kPi = 3.141592654;          // oxRNG(-pi, pi) bound (pool word pair)
constexpr uint32_t kBarColour = 0x3000ffffu; // the menus' label bars (hlin_a)
double bitsD(uint64_t u) {
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}
// _hline 0x1e7138: the scenery's top floor row (columns 1..19).
constexpr int32_t kHline[20] = {14, 14, 14, 2, 3, 4, 3, 4, 5, 14, 2, 5, 14, 2, 3, 4, 5, 1, 14, 14};
constexpr int32_t kBooms[7] = {14, 16, 17, 18, 19, 20, 21}; // _booms 0x1e6c48
} // namespace

// ------------------------------------------------------------------ scenery, stars
void PlayState::titleScenery() {
    // announceCavern / run case 1: no fractional scroll, every visible tile cleared, stm
    // palette [0] from the cavern, solid ground (tile 1) from row rows/2 + 4 down (except the
    // last row), that row's floor edge (0xe, then _hline), and two rows of two-layer tiles
    // (atlas row + k, k) at columns 5..14 of rows rows/2 + 7 and + 8.
    cavern::StripMap &m = *map;
    m.fraction = {0.0, 0.0};
    const int w = m.columns + 1;
    const int n = m.rows * w + w;
    for (int i = 0; i < n; i++)
        m.tiles[size_t(i)] = 0;
    stmPalette[0].unpackRGBA(kCavernDefinitions[cavernIndex].palette[0]);
    const int h = m.rows / 2, r0 = h + 4;
    for (int y = r0; y < m.rows; y++)
        for (int x = 0; x < m.columns; x++)
            m.tiles[size_t(y * w + x)] = 1;
    m.tiles[size_t(r0 * w)] = 0xe;
    for (int i = 1; i < 0x14; i++)
        m.tiles[size_t(r0 * w + i)] = kHline[i];
    const int a = m.atlasColumns;
    static const int kTop[10] = {6, 8, 7, 8, 8, 7, 8, 8, 7, 9};
    static const int kBottom[10] = {10, 12, 12, 11, 11, 12, 11, 12, 12, 13};
    for (int i = 0; i < 10; i++) {
        m.tiles[size_t((h + 7) * w + 5 + i)] = (a + kTop[i]) * 0x100 | kTop[i];
        m.tiles[size_t((h + 8) * w + 5 + i)] = (a + kBottom[i]) * 0x100 | kBottom[i];
    }
}

void PlayState::addTitleStars() {
    for (int i = 0; i < 0x32; i++) {
        auto *t = new TitleStar;
        t->g = this;
        t->pos.y = rnd(-0.5, 0.5);
        t->randomiseStuff(); // vtable +0x18
        t->vel.x = 0;
        addEntity(list2dc, t);
    }
}

void PlayState::starReset() { // 0x5e818
    resetLists(0);
    addTitleStars();
}

// ------------------------------------------------------------------ run cases 1-3, 7-9
void PlayState::menuState() {
    if (stateHook && stateHook())
        return;
    switch (runState) {
    case 1: { // title set-up
        bgColour[1] = 0;
        bgColour[0] = 0;
        bgColour[2] = 0;
        bgBright = 0;
        fbRot = 0;
        fbPos[0] = 0;
        fbPos[1] = 0;
        drawMode = 0;
        bgLayer = 0;
        word484 = 0;
        cavernIndex = 0;
        bgFade[0] = 0.99f;
        bgFade[1] = 0.99f; // 0x3f7d70a4
        map->fraction = {0.0, 0.0};
        muted = true;
        word298 = 0;
        byte447 = false;
        word9d8 = 6000;
        resetLists(0);
        if (hud)
            hud->goA8(0x1c, 0x24, 1, 3);
        titleScenery();
        if (hud)
            hud->setCursor(false);
        runState = 2;
        byte51 = false;
        frameCount = 0;
        paused = false;
        running = true;
        playSample(int(libcRand() & 1) + 0x42, volume << 12);
        break;
    }
    case 2:
        if (!startupReported) {
            goalState = 0xf;
            startupReported = true;
        } // OSReport "MSTO"
        if (frameCount == 0x14) {
            if (hud) {
                hud->goA8(9, 7, 0, 1);
                hud->setCursor(false);
                hud->cls();
                hud->clattr();
                hud->quickMsg("COPYRIGHT 2012", {0.0, 0.38}, {2.0, 1.0}, 0, 6);
                hud->quickMsg("ATAURUS", {0.0, 0.43}, {2.0, 1.0}, 1, 6);
                hud->quickMsg(kRankNames[shipKind], {0.0, 0.22}, {2.0, 1.0}, 2, 6);
                hud->setLine(0, 0xa2, -1);
                hud->setLine(1, 0xa2, -1);
                hud->setLine(2, 0xa2, -1);
            }
            drawMode = 1;
            runState = 3;
            bgLayer = 1;
            resetLists(0);
            addTitleStars();
        }
        break;
    case 3:
        if (hud) {
            hud->cPrintAt(1, "CAVERNS", colours[0]);
            hud->cPrintAt(2, "OF", colours[0]);
            hud->setRowDisplacement(2, 0.5f); // original row header +4, half-cell title centering
            hud->cPrintAt(3, "MINOS", colours[0]);
        }
        if (firePressed) {
            static const int kVoices[6] = {0, 1, 2, 3, 4, 4};
            const uint32_t a = libcRand();
            const uint32_t b = libcRand();
            playSample(kVoices[(b & 1) + (a & 3)], volume << 12);
            byte398 = true;
            goSelect();
        }
        break;
    case 7:
        dynamicSelect();
        if (keyCode == 0)
            break;
        playSample(0x34, volume << 12);
        switch (keyCode) {
        case 1:
            if (--shipKind < 0)
                shipKind = 3;
            updateSelect();
            break; // difficulty -
        case 2:
            if (3 < ++shipKind)
                shipKind = 0;
            updateSelect();
            break; // difficulty +
        case 3:
        case 9: // NEW GAME / BEST GAME
            muted = false;
            restartBest = keyCode == 9;
            resetLists(0); // OSReport "After reset"
            tutorialShown = false;
            byte47c = false;
            minosSavedTotal = 0;
            word478 = 0;
            running = true;
            scoreTotal = 0;
            startFromSelect();
            break;
        case 7:
            if (maxCavern < ++selCavern[shipKind])
                selCavern[shipKind] = 0;
            updateSelect();
            break;
        case 8:
            if (--selCavern[shipKind] < 0)
                selCavern[shipKind] = maxCavern;
            updateSelect();
            break;
        case 0x11: // INFO
            if (hud) {
                hud->cls();
                hud->clattr();
                hud->clm();
            }
            starReset();
            drawMode = 3;
            runState = 9;
            infoPage = 0;
            break;
        case 0x18:
            goalState = 0xe;
            break; // HISCORES (the platform's board)
        default:
            break;
        }
        break;
    case 8:
        if (!gameFinished)
            dynamicGameOver();
        else
            dynamicGameFinished();
        break;
    case 9:
        dynamicInfo();
        if (keyCode != 0) {
            playSample(0x34, volume << 12);
            if (keyCode == 0x12) {
                infoPage = (infoPage + 1) & 3;
                if (hud) {
                    hud->cls();
                    hud->clattr();
                }
            } else if (keyCode == 0x13)
                goSelect();
        }
        break;
    default:
        break;
    }
}

void PlayState::startFromSelect() { // run case 7, label LAB_0006dcf8
    const int sel = selCavern[shipKind];
    cavernIndex = sel;
    if (!restartBest)
        lives = 3;
    else {
        const int k = shipKind * 100 + sel;
        lives = prefs[k + 0x30];
        scoreTotal = prefs[k + 0x10];
    }
    if (muted)
        cavernIndex = startCavern;
    if (byte447) {
        lives = 1;
        scoreTotal = 0;
        cavernIndex = 0;
    }
    word298 = 0;
    byte70 = true;
    cavern = &kCavernDefinitions[cavernIndex];
    if (muted && newLevelHook)
        newLevelHook(); // (not reached: case 3 / 9 clear +0x2ad)
    announceCavern();
}

// ------------------------------------------------------------------ select
void PlayState::goSelect() { // 0x5e548
    bgColour[1] = 0;
    bgColour[0] = 0;
    bgColour[2] = 0;
    bgBright = 0;
    if (hud)
        hud->setSize(0x19, 0x1e);
    runState = 7;
    drawMode = 4;
    bgLayer = 3;
    resetLists(0);
    addTitleStars();
    if (!updatedMenus) {
        auto *b = new OpenFeintButton;
        b->spriteIndex = 0x5c;
        b->scale = {1.4, 1.0}; // 0x3ff6666666666666
        b->pressed = true;
        b->g = this;
        b->baseSize = 0.08f; // bytes 0a d7 a3 3d
        b->grow = 0;
        b->growRate = 0;
        b->pos = {bitsD(0x3fa47ae147ae147bull), bitsD(0x3fdc28f5c28f5c29ull)}; // (0.04, 0.44)
        b->size = 0.05f;                                                       // 0x3d4ccccd
        b->kill = false;
        addEntity(debris, b);
    }
    buttonLatch = true;
    updateSelect();
}

void PlayState::updateSelect() { // 0x579b4
    if (!hud)
        return;
    hud->cls();
    const uint32_t r = libcRand();
    hud->quickMsg(kQuestNames[r & 7], {0.0, -0.25}, {2.0, 1.0}, 0, 3);
    hud->quickMsg(kCavernDefinitions[selCavern[shipKind]].name, {0.0, -0.2}, {2.0, 1.0}, 1, 3);
    hud->setLine(0, 0xa2, 1);
    hud->setLine(1, 0xa2, 1);
}

void PlayState::dynamicSelect() {          // 0x57b34
    int m = prefs[shipKind * 100 + 1] + 3; // furthest cavern + 3
    maxCavern = m;
    if (0x14 < m)
        maxCavern = 0x14;
    if (hud) {
        hud->cls();
        hud->clattr();
    }
    if (maxCavern < selCavern[shipKind]) {
        selCavern[shipKind] = maxCavern;
        updateSelect();
    }
    if (!hud)
        return;
    if (0 < maxCavern)
        hud->cPrintAt(1, "Select cavern", colours[0]);
    const int rec = shipKind * 100 + selCavern[shipKind];
    const bool best = 0 < prefs[rec + 0x10];
    std::snprintf(textBuf, sizeof textBuf, "Cavern %d\n", selCavern[shipKind] + 1);
    hud->cPrintAt(4, textBuf, colours[3]);
    hud->setLabelMode(true);
    for (int k = 3; k != 6; k++) { // the loop counter starts at 3 (instructions)
        if (0 < maxCavern) {
            hud->setLabel(8);
            hud->hlin(999, kBarColour, 0, 0, 6, k, 1.0f);
            hud->printAt(1, 4, "-", colours[1]);
            hud->setLabel(7);
            hud->hlin(999, 0, kBarColour, 0x12, 0x18, k, 1.0f);
            hud->printAt(0x17, 4, "+", colours[1]);
        }
        hud->setLabel(1);
        hud->hlin(999, kBarColour, 0, 0, 6, k + 0xc, 1.0f);
        hud->printAt(1, 0x10, "-", colours[1]);
        hud->setLabel(2);
        hud->hlin(999, 0, kBarColour, 0x12, 0x18, k + 0xc, 1.0f);
        hud->printAt(0x17, 0x10, "+", colours[1]);
        hud->setLabel(3);
        hud->hlin(999, kBarColour, 0, 0, 6, k + 0x13, 1.0f);
        hud->printAt(1, 0x17, "NEW GAME", colours[1]);
        if (best) {
            hud->setLabel(9);
            hud->hlin(999, 0, kBarColour, 0x12, 0x18, k + 0x13, 1.0f);
            hud->printAt(0xf, 0x17, "BEST GAME", colours[1]);
        }
        hud->setLabel(0x11);
        hud->hlin(999, kBarColour, 0, 0, 6, k + 0x18, 1.0f);
        hud->printAt(1, 0x1c, "INFO", colours[1]);
        hud->setLabel(0x18);
        hud->hlin(999, 0, kBarColour, 0x12, 0x18, k + 0x18, 1.0f);
        hud->printAt(0x10, 0x1c, "HISCORES", colours[1]);
    }
    hud->setLabelMode(false);
    if (best) {
        std::snprintf(textBuf, sizeof textBuf, "Restart Best: %d", prefs[rec + 0x10]);
        hud->cPrintAt(10, textBuf, colours[2]);
        const int l = prefs[rec + 0x30];
        const char *s = "with 1 life";
        if (1 < l) {
            std::snprintf(textBuf, sizeof textBuf, "with %d lives", l);
            s = textBuf;
        }
        hud->cPrintAt(0xb, s, colours[2]);
    }
    hud->cPrintAt(0xd, "Select difficulty", colours[0]);
    std::snprintf(textBuf, sizeof textBuf, "%s Mission", kRankNames[shipKind]);
    hud->cPrintAt(0x13, textBuf, colours[3]);
    std::snprintf(textBuf, sizeof textBuf, "Your hiscore: %d", prefs[shipKind * 100]);
    hud->cPrintAt(0x14, textBuf, colours[3]);
}

// ------------------------------------------------------------------ info
void PlayState::dynamicInfo() { // 0x58268
    if (!hud)
        return;
    Hud &H = *hud;
    auto P = [&H](int x, int y, const char *s) { H.printAt(x, y, s); };
    switch (infoPage) {
    case 0:
        H.cPrintAt(1, "CAVERNS OF MINOS", colours[1]);
        H.cPrintAt(2, "================", colours[1]);
        H.printAt(3, 5, "Created by", colours[4]);
        H.printAt(5, 6, "JEFF MINOTAUR", colours[4]);
        H.printAt(3, 7, "and", colours[4]);
        H.printAt(5, 8, "GILESGOAT", colours[4]);
        H.printAt(5, 0xb, "Wedi eu gwneud", colours[2]);
        H.printAt(7, 0xc, "yng Nghymru", colours[2]);
        H.printAt(5, 0xe, "Made in Wales", colours[2]);
        H.printAt(3, 0x11, "Handcrafted for you", colours[3]);
        P(3, 0x12, "with wool from our");
        P(3, 0x13, "own flock.");
        H.printAt(3, 0x15, "Hand rinse only.", colours[5]);
        P(3, 0x16, "Do not machine wash");
        P(3, 0x17, "or iron.");
        break;
    case 1:
        H.cPrintAt(1, "LLAMASOFT POLICY", colours[1]);
        H.cPrintAt(2, "================", colours[1]);
        H.printAt(1, 4, "We are dedicated to", colours[3]);
        P(1, 5, "creating iOS games that");
        P(1, 6, "do not suck. We promise:");
        H.printAt(1, 8, "- Controls will always", colours[5]);
        P(1, 9, "be excellent.  No horrid");
        P(1, 10, "onscreen joysticks.");
        H.printAt(1, 0xc, "- The game will be", colours[2]);
        P(1, 0xd, "playable, fun and fair.");
        H.printAt(1, 0xf, "- You get the whole", colours[5]);
        P(1, 0x10, "game for the initial");
        P(1, 0x11, "purchase price. We will");
        P(1, 0x12, "never ask you to pay");
        P(1, 0x13, "extra for things that");
        P(1, 0x14, "should've been there");
        P(1, 0x15, "in the first place.");
        H.printAt(1, 0x17, "Search the App Store", colours[4]);
        P(1, 0x18, "for LLAMASOFT and buy");
        P(1, 0x19, "with confidence.");
        break;
    case 2:
        H.printAt(1, 1, "Why not try one of the", colours[1]);
        P(1, 2, "other LLAMASOFT iOS");
        P(1, 3, "games?");
        H.printAt(3, 5, "MINOTAUR RESCUE", colours[2]);
        H.printAt(5, 7, "MINOTRON", colours[4]);
        H.printAt(3, 9, "DEFLEX", colours[2]);
        H.printAt(5, 0xb, "GOATUP", colours[4]);
        H.printAt(1, 0xd, "If you like this one", colours[5]);
        P(1, 0xe, "you will probably enjoy");
        P(1, 0xf, "those too.");
        H.printAt(1, 0x12, "Keep an eye on the App", colours[3]);
        P(1, 0x13, "Store for new LLAMASOFT");
        P(1, 0x14, "releases. We are quite");
        P(1, 0x15, "prolific!");
        break;
    case 3:
        H.printAt(1, 1, "If you like what we do,", colours[1]);
        P(1, 2, "we would appreciate");
        P(1, 3, "your help.");
        H.printAt(4, 5, "PREACH IT BROTHA!", colours[4]);
        H.printAt(1, 7, "Tell your friends about", colours[2]);
        P(1, 8, "the games and encourage");
        P(1, 9, "them to try them out.");
        H.printAt(4, 0xb, "USE YOUR VOTE!", colours[4]);
        H.printAt(1, 0xd, "Leave a rating and a", colours[2]);
        P(1, 0xe, "review on our App");
        P(1, 0xf, "Store pages.");
        H.printAt(1, 0x12, "It helps us to be seen", colours[1]);
        P(1, 0x13, "in a crowded market.");
        H.printAt(4, 0x16, "THANK YOU!", colours[4]);
        H.printAt(4, 0x18, "DIOLCH YN FAWR!", colours[4]);
        break;
    default:
        break;
    }
    H.setLabelMode(true);
    for (int y = 0x1b; y <= 0x1d; y++) { // three rows of DONE / MORE bars
        H.setLabel(0x13);
        H.hlin(999, kBarColour, 0, 0, 6, y, 1.0f);
        H.printAt(1, 0x1c, "DONE", colours[1]);
        H.setLabel(0x12);
        H.hlin(999, 0, kBarColour, 0x12, 0x18, y, 1.0f);
        H.printAt(0x14, 0x1c, "MORE", colours[1]);
    }
    H.setLabelMode(false);
}

// ------------------------------------------------------------------ game over / finished
void PlayState::dynamicGameOver() { // 0x58f3c
    std::snprintf(textBuf, sizeof textBuf, "Score: %d", scoreTotal);
    if (hud)
        hud->printAt(2, 1, textBuf, colours[0]);
    if (0 < endScreenCountdown) {
        if (--endScreenCountdown != 0 && !firePressed)
            return;
        drawMode = 5;
        bgLayer = 3;
        endScreenCountdown = 0;
        buttonLatch = true;
        if (++tipIndex == 6)
            tipIndex = 0;
        firePressed = false;
    }
    double s = scrollDelta.y; // +0x348 eases to 0.0015 (binary64)
    if (s < 0.0015) {
        s = s + 5e-05;
        scrollDelta.y = s;
    }
    if (0.0015 < s)
        scrollDelta.y = s - 5e-05;
    const int n = minosSavedTotal;
    if (n == 0)
        std::strcpy(textBuf, "No minotaurs");
    if (0 < n) {
        if (n == 1)
            std::strcpy(textBuf, "One minotaur");
        else
            std::snprintf(textBuf, sizeof textBuf, "%d minotaurs", n);
    }
    if (hud) {
        hud->cPrintAt(0xd, textBuf, colours[0]);
        hud->cPrintAt(0xe, "rescued this game!", colours[0]);
        hud->cPrintAt(0x11, "TOP TIPS", colours[3]);
        for (int i = 0; i < 5; i++)
            hud->printAt(1, 0x13 + 2 * i, kTips[tipIndex * 5 + i], colours[2]);
    }
    if (firePressed)
        runState = 1;
}

void PlayState::dynamicGameFinished() { // 0x6bac8
    if ((libcRand() & 0xfff) < 0x10) {
        if ((libcRand() & 7) == 0) {
            if (!muted)
                playSample(0x32, volume << 12);
        } else {
            const uint32_t r = libcRand();
            if (!muted)
                playSample(int((r & 0x3ff) % 6) + 6, volume << 14);
        }
    }
    if ((libcRand() & 0xfff) < 0x40)
        fireWorkExplosion();
    const int total = int(float(bonusTotal) * bonusT) + scoreBase; // binary32 product, truncated
    std::snprintf(textBuf, sizeof textBuf, "Score: %d", total);
    if (hud)
        hud->printAt(2, 1, textBuf, colours[0]);
    bool finish = false;
    if (endScreenCountdown < 900) {
        if (hud)
            hud->printAt(1, 0xc, "CONGOATULATIONS!", colours[0]);
        if (endScreenCountdown < 800) {
            if (hud) {
                hud->printAt(1, 0xe, "You beat every", colours[1]);
                hud->printAt(1, 0xf, "cavern!", colours[1]);
            }
            if (endScreenCountdown < 600) {
                if (hud) {
                    hud->printAt(1, 0x11, "the MOTHERSHEEP", colours[4]);
                    hud->printAt(1, 0x12, "is happy!", colours[4]);
                }
                if (endScreenCountdown < 400) {
                    if (hud) {
                        hud->printAt(1, 0x14, "I think a nice", colours[5]);
                        hud->printAt(1, 0x15, "cup of tea is", colours[5]);
                        hud->printAt(1, 0x16, "in order!", colours[5]);
                    }
                    if (endScreenCountdown < 300) {
                        std::snprintf(textBuf, sizeof textBuf, "Please take %d", bonusTotal);
                        if (hud) {
                            hud->printAt(1, 0x18, textBuf, colours[0]);
                            hud->printAt(1, 0x19, "bonus points for", colours[0]);
                            hud->printAt(1, 0x1a, "completion!", colours[0]);
                        }
                        float f = bonusT;
                        if (f < 1.0f) {
                            f = float(double(f) + 0.01);
                            bonusT = f;
                        } // binary64 step
                        if (1.0f < f)
                            bonusT = 1.0f;
                        if (endScreenCountdown < 1) {
                            if (endScreenCountdown != 0)
                                return;
                            finish = true;
                        }
                    }
                }
            }
        }
    }
    if (!finish) {
        const int t = endScreenCountdown;
        int u = t - 1;
        endScreenCountdown = u;
        if (fireHeld) {
            u = t - 0xb;
            endScreenCountdown = u;
            if (u < 0) {
                buttonLatch = true;
                u = 0;
                endScreenCountdown = 0;
                firePressed = false;
            }
            if (u < 300)
                bonusT = float(double(bonusT) + 0.1); // binary64 step
            if (1.0f < bonusT)
                bonusT = 1.0f;
        }
        if (0 < u && !firePressed)
            return;
        endScreenCountdown = 0;
    }
    if (bonusT == 1.0f && firePressed) {
        scoreTotal = total;
        if (prefs[shipKind * 100] < total) {
            prefs[shipKind * 100] = total;
            reportKind = shipKind;
            reportScore = scoreTotal;
            goalState = 2;
        }
        runState = 1;
    }
}

void PlayState::fireWorkExplosion() { // 0x6b648
    const uint32_t r = libcRand();
    if (!muted)
        playSample(kBooms[(r & 0x7fff) % 7], volume << 12);
    const double x = rnd(-0.5, 0.5);
    const double y = rnd(-0.5, 0.5);
    auto *e = new Explo;
    e->from = 2.0f; // bytes 00 00 00 40
    e->to = float(rnd(4.0, 12.0));
    e->rate = 0.016f; // 0x3c83126f
    const int n = int(rnd(10.0, 40.0));
    e->spriteIndex = 4;
    e->rot = float(rnd(-kPi, kPi));
    e->t = 0;
    e->g = this;
    e->baseScale = {1.0, 1.0};
    e->pos = {x, y};
    addEntity(debris, e);
    for (int i = 0; i < n; i++) {
        auto *b = new Bspark;
        b->vel.y = rnd(-0.02, -0.005);
        b->vel.x = rnd(-0.01, 0.01);
        b->ageRate = float(rnd(0.001, 0.006));
        b->exploChance = 0.5f;
        b->pos = {x, y};
        b->rot = float(rnd(-kPi, kPi));
        b->age = 0;
        b->spriteIndex = 3;
        b->maxScale = 1.0f;
        b->g = this;
        b->colourIndex = 1;
        b->size = 0.02f; // 0x3ca3d70a
        addEntity(debris, b);
    }
}

// ------------------------------------------------------------------ openfeintButton
void OpenFeintButton::run() { // 0x5ec54
    PlayState &G = *g;
    gameButtonRun(*this, baseSize, pressed, grow, growRate,
                  [&G] { G.goalState = 0xe; }); // onPressed 0x53e8c
    if (kill && !G.paused)
        dead = true;
}

int OpenFeintButton::draw(EntityRenderer &r, Vec2 screen) { // 0x56da8
    auto *sr = dynamic_cast<SpriteRenderer *>(&r);
    if (!sr)
        return 0;
    SpriteFields f;
    f.angle = double(rot);
    f.col = neon::colour(1.0, 1.0, 1.0, 1.0);
    f.frame = spriteIndex + 2;
    const float d = size + size;
    f.size = {double(d) * screen.x * scale.x, double(d) * screen.y * scale.y};
    f.pos = {pos.x * screen.x, pos.y * screen.y};
    f.preserveFlags = true; // original leaves sprite +0x5c untouched
    sr->drawSprite(f);
    return 6;
}

// ------------------------------------------------------------------ _draw's title sprites
bool titleLogoSprite(const PlayState &g, Vec2 sc, SpriteFields &f) { // doTitle 0x56e98
    Vec2 size;
    switch (g.shipKind) {
    case 0:
        size = {0.3, 0.12};
        f.frame = 3;
        f.col.unpackRGBA(0x4880ffffu);
        break;
    case 1:
        size = {0.21, bitsD(0x3fc1eb851eb851ebull)};
        f.frame = 0xc;
        f.col.unpackRGBA(0x48ffffffu);
        break;
    case 2:
        size = {0.3, 0.12};
        f.frame = 0x4d;
        f.col = neon::colour(1.0, 1.0, 1.0, 1.0);
        break;
    case 3:
        size = {0.12, bitsD(0x3fc47ae147ae147cull)};
        f.frame = 0xd;
        f.col.unpackRGBA(0xe0e0ffffu);
        break;
    default:
        return false;
    }
    f.pos = {0.0 * sc.x, 0.05 * sc.y};
    f.size = {size.x * sc.x, size.y * sc.y};
    f.orFlags = 3;
    f.angle = double(g.phaseSin) * 0.03;
    return true;
}

void titleFlashSprite(Vec2 sc, SpriteFields &f) { // doTitleFlash 0x56260
    f.pos = {0.1 * sc.x, -0.35 * sc.y};
    f.size = {0.3 * sc.x, 0.2 * sc.y};
    f.orFlags = 3;
    f.frame = 3;
    f.col.unpackRGBA(0xffc040ffu);
    f.angle = 0; // (_draw mode 0 clears it before)
}

} // namespace minos
