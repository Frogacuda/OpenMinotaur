// game_run.cpp - theGame::run, the per-frame state machine.
// State transitions follow the original instruction order.
//
// States (theGame::state):
//   1   reset to the boot screen          2   boot screen ("Cattle Electronics")
//   3   menus (title / level select / info), driven by cellGrid touch labels
//   4   playing                           5   game over
//   6   "shields renewed" pause before the next attempt
//   7   paused                            8   level intro ("LEVEL n")
//   9   level complete                    111 story scroller (attract)
//   112 info pages (minoInfoScreen)
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

namespace {
const char *const kBlankRow = "                  ";

int s_idleTimer = 600;         // run()::idleTimer
int s_tr = 0;                  // run()::_tr (explodeLastBeast result)
bool s_buttonDebounce = false; // run()::buttonDebounce
float s_interpolant = 0.0f;    // run()::interpolant (screen transition)

// Steps the feedback look of the level-complete / game-over fades.
void fadeLook(theGame *g, float t, double scaleBase) {
    double d = double(t);
    float sc = float(scaleBase + d * 0.29);
    g->fbScaleX = sc;
    g->fbScaleY = sc;
    float c = float(0.9 + d * -0.7);
    g->fbColour[0] = c;
    g->fbColour[1] = c;
    g->fbColour[2] = float(0.9 + d * -0.1);
}

void runList(entity *&list) {
    neon::entities::runAndCull(list, [](entity *e) { delete e; });
}
} // namespace

// Touch hit-testing and controller focus supply the same menu activation ID.
void theGame::menuInput(int activationLabel) {
    cellGrid *g = grid;
    s_idleTimer = 6000;
    waitRelease = true;
    int label = activationLabel ? activationLabel : g->getLabel(joyOrigin[0]);
    int step = 1;

    auto startGame = [&](bool endurance) {
        if (!endurance) {
            endurance = false;
            practice = false;
            playSample(5);
        }
        frontEndTouchMode = 0;
        fbScaleX = fbScaleY = 0.7f;
        if (endurance)
            selLevel = 0;
        fbColour[2] = fbColour[1] = fbColour[0] = 0.9f;
        confirmResumeReset = false;
        if (endurance) {
            practice = true;

            lastStartLevel = 0;
            lastMode = banner;
            lastPlayers = nPlayers;
        } else {
            lastMode = banner;
            lastStartLevel = selLevel;
            lastPlayers = nPlayers;
        }
        playSample(level % 5 == 4 ? 0x20 : 0x1f);
        state = 8;
        s_interpolant = 0.0f;
        reset(0);
        demo = false;
        makeArena();
        newGame();
    };
    auto toTitle = [&]() {
        newGame();
        printTitleScreen();
    };
    auto levelChanged = [&]() {
        startGameScreen();
        demoLevel = selLevel;
        newGame();
    };

    switch (label) {
    case 1:    // title banner: next game mode
    case 0x2c: // (port) the same, on the mode's name
        selLevel = 0;
        gameMode = (gameMode + step) & 3;
        g->clattr();
        newGame();
        printTitleScreen();
        playSample(5);
        break;
    case 2: // level-select banner: number of players
        nPlayers++;
        if (banner == 3 ? nPlayers > 2 : nPlayers > maxPlayers)
            nPlayers = 1;
        g->cls();
        g->clattr();
        newGame();
        startGameScreen();
        playSample(5);
        break;
    case 9: // RESUME BEST
        resumeBest = true;
        [[fallthrough]];
    case 3: // START
        startGame(false);
        break;
    case 4: // MINOTAUR PROJECT
        playSample(5);
        waitRelease = true;
        infoPage = 0;

        reset(0);
        state = 112;
        break;
    case 5: { // PLAY: to the level-select screen

        playSample(5);
        g->cls();
        frontEndTouchMode = 0;
        practice = false;
        nPlayers = lastPlayers;
        furthest = 0;
        int last;
        if (best(gameMode, 0, 0) < 1) {
            last = -1;
        } else {
            int i = 1;
            for (;; i++) {
                furthest = i;
                if (best(gameMode, i, 0) < 1 || i + 1 == 101)
                    break;
            }
            last = i - 1;
        }
        furthest = last;
        if (lastStartLevel > last)
            lastStartLevel = last;
        selLevel = lastStartLevel;
        startGameScreen();
        break;
    }
    case 6: // BACK (level select -> title)
        playSample(5);
        g->cls();
        g->clattr();
        confirmResumeReset = false;
        banner = lastMode;
        if (banner == 3 && nPlayers > 2)
            nPlayers = 2;
        toTitle();
        break;
    case 7: // level +1
        playSample(5);
        confirmResumeReset = false;
        selLevel = selLevel + 1;
        if (selLevel > furthest)
            selLevel = furthest;
        levelChanged();
        break;
    case 8: // level -1
        playSample(5);
        selLevel--;
        confirmResumeReset = false;
        if (selLevel < 0)
            selLevel = 0;
        levelChanged();
        break;
    case 16:   // HIGH SCORES (info screen)
    case 0x2a: // (port) HIGH SCORES (title screen)
        playSample(5);
        hiBoard = gameMode & 3;
        hiReturn = label == 16 ? 3 : 2;
        hiNew = -1;
        hiTimer = 0;
        waitRelease = true;
        highScoreScreen();
        state = 0x71;
        break;
    case 0x2b: // (port) EXIT: ask the shell to leave the game
        playSample(5);
        shellCommand = 32;
        waitRelease = true;
        break;
    case 17: // INFO
        g->cls();
        g->clattr();
        newGame();
        playSample(5);
        infoScreen();
        break;
    case 18: // ABOUT LLAMASOFT
        playSample(5);
        infoPage = 2;
        waitRelease = true;
        reset(0);
        state = 112;
        break;
    case 20: // ABOUT MINOTRON
        playSample(5);
        infoPage = 1;
        waitRelease = true;
        reset(0);
        state = 112;
        break;
    case 21: // BACK (info -> title)
        playSample(5);
        g->cls();
        g->clattr();
        if (banner == 3 && nPlayers > 2)
            nPlayers = 2;
        toTitle();
        break;
    case 22:
        practice = true;
        playSample(5);
        startGameScreen();
        newGame();
        break;
    case 24:
        shellCommand = 14;
        break;
    case 35: // level +10
        playSample(5);
        confirmResumeReset = false;
        selLevel += 10;
        if (selLevel > furthest)
            selLevel = furthest;
        levelChanged();
        break;
    case 36: // level -10
        playSample(5);
        selLevel -= 10;
        confirmResumeReset = false;
        if (selLevel < 0)
            selLevel = 0;
        demoLevel = selLevel;
        startGameScreen();
        newGame();
        break;
    case 37: // RESET RESUME BEST (asks to confirm)
        playSample(0x26);
        confirmResumeReset = true;
        startGameScreen();
        break;
    case 38: // CONFIRM RESET RESUME BEST
        playSample(5);
        confirmResumeReset = false;
        best(gameMode, selLevel, 0) = 3;
        best(gameMode, selLevel, 1) = 0;
        shellCommand = 17;
        startGameScreen();
        break;
    case 39: // PLAY ENDURANCE
        startGame(true);
        break;
    case 40:
        playSample(0x2e);
        confirmResumeReset = false;
        startGameScreen();
        break;
    case 41: { // control scheme
        int &c = sd(0xc98);
        c++;
        if (sd(0xc98) > 2)
            sd(0xc98) = 0;
        printTitleScreen();
        playSample(5);
        shellCommand = 17;
        break;
    }
    default:
        if (label >= 25 && label <= 32) { // audio level 0..7
            volume = label - 25;
            sd(0xc80) = volume;
            shellCommand = 17;
            int r1 = neonRand();
            int r2 = neonRand();
            AudioSystemPlaySample(kGruntSamples[(r2 & 1) + (r1 & 3)], volume << 12, 0);
            infoScreen();
        }
        break;
    }
}

// Level complete (state 4 with every spawn done): goals, resume-best save,
// and the switch to state 9.
void theGame::levelComplete() {
    auto goal = [&](int ach, unsigned bit, int cmd) {
        msgTimer = 60;
        if (message != 0)
            eraseMessage();
        message = 4;
        achievement = ach;
        msgTimer = 240;
        goalsDone |= bit;
        shellCommand = char(cmd);
        if (!demo)
            playSample(60);
    };
    int lv = ++level;
    if (rescuedAllBeforeKills == 0) {
        f1198[3] = 0;
    } else if (f1198[3] == 5 && nPlayers <= 1 && banner <= 1 && !(goalsDone & 0x80)) {
        goal(7, 0x80, 11);
        lv = level;
    }
    if ((gameMode == 3 || gameMode == 0) && lv > 4 && deathsThisRun <= 0 && !demo &&
        nPlayers <= 1 && banner <= 1 && !(goalsDone & 0x40)) {
        goal(6, 0x40, 10);
        lv = level;
    }
    if (gameMode == 3 && deathsThisRun <= 0 && lv > 19 && !demo && nPlayers <= 1 && banner <= 1 &&
        !(goalsDone & 0x200)) {
        goal(9, 0x200, 13);
        lv = level;
    }
    newLevelBest = false;
    if (lv <= 99) {
        int &bl = best(gameMode, lv, 0);
        if (lives[0] >= bl && score[0] >= best(gameMode, lv, 1)) {
            bl = lives[0];
            best(gameMode, level, 1) = score[0];
            shellCommand = 17;
            if (!demo)
                playSample(61);
            newLevelBest = true;
        }
    }
    s_interpolant = 1.0f;
    levelBonusStep = 0;
    levelBonusPoints = 0;
    s_tr = 1;
    levelEnding = true;
    pauseArmed = false;
    state = 9;
    if (!demo)
        playSample(30);
}

void theGame::run() {
    const auto actions = menuActions;
    menuActions = {};
    infoControllerMode = actions.controller;
    infoScrollAxis = actions.scroll;
    cellGrid *g = grid;
    if (!gridConfigured) {
        if (!g)
            return;
        gridConfigured = true;
        g->bgLiteral = 0;
        g->glyphLiteral = 0;
        g->fontY = 0x1b0;
        g->passScale[1] = vec2d(0.98, 0.98);
        g->origin.x = 2.5;
        g->maxChar = 12345;
        g->hashMode = true;
    }
    if (!paused)
        frameCount++;
    g->f297e = inputFlipped;
    if (!saveData) {
        shellCommand = 16; // ask the shell to load the save data
        return;
    }
    if (!savedVolumeLoaded) {
        volume = sd(0xc80);
        if (unsigned(volume) > 8)
            volume = 5;
        savedVolumeLoaded = true;
    }
    joyWatchdog();
    if (shellCommand < 0) {
        if (shellCommand == -1)
            purgeTouches();
        shellCommand = 0;
    }
    if (resumeResetRequested) { // app resumed: back to the boot screen unless playing
        resumeResetRequested = false;
        goalsDone = 0;
        if (!demo && banner <= 1) {
            g->cls();
            g->clattr();
            frameCount = 128;
            reset(0);
            state = 1;
        }
    }
    bool pressed = false;
    if (controller.advance) {
        if (!s_buttonDebounce) {
            s_buttonDebounce = true;
            pressed = true;
        }
    } else {
        s_buttonDebounce = false;
    }
    bool touchOrButton = joyActive[0] || pressed || actions.activateLabel || actions.advance;
    char buf[80];

    switch (state) {
    case 1:
        banner = 0;
        fbColour[2] = 0.8f;
        fbColour[1] = 0;
        fbColour[0] = 0;
        fbRot = 0;
        fbColour[3] = 0.7f;
        fbX = 0;
        fbY = 0;
        demo = true;
        fbScaleX = fbScaleY = 0.99f;
        confirmResumeReset = false;
        practice = false;
        demoLevel = 0;
        msgTimer = 0;
        s_idleTimer = 600;
        bgColour = 0;
        reset(0);
        spawnCount = 0;
        bootScreen();
        frameCount = 0;
        state = 2;
        running = true;
        frontEndTouchMode = 0;

        pauseArmed = false;
        paused = false;
        break;
    case 2:
        if (!bootNotificationSent) {
            shellCommand = 15;
            bootNotificationSent = true;
        }
        if (frameCount > 600)
            state = 111;
        if (waitRelease) {
            if (!joyActive[0])
                waitRelease = false;
        } else if (touchOrButton) {
            s_idleTimer = 6000;
            waitRelease = true;
            playSample(5);
            running = false;
            state = 3;

            frontEndTouchMode = 1;
            g->cls(0x20);
            g->clattr();
            banner = lastMode;
            nPlayers = lastPlayers;
            newGame();
            printTitleScreen();
        }
        break;
    case 3:
        if (waitRelease) {
            if (!joyActive[0])
                waitRelease = false;
        } else if (touchOrButton) {
            menuInput(actions.activateLabel);
        }
        if (--s_idleTimer <= 0) {
            frameCount = 0;
            reset(0);
            state = 1;
            g->cls();
            g->clattr();
        }
        break;
    case 4:
        if (paused) {
            if (message > 0)
                eraseMessage();
            message = 0;
            msgTimer = 0;
            pauseScreen();
            pausedState = 4;
            state = 7;
        } else if (msgTimer == 0) {
            f2d4--;
        }
        break;
    case 5:
        if (s_interpolant < 1.0f) {
            float t = float(double(s_interpolant) + 0.01);
            if (t > 1.0f)
                t = 1.0f;
            s_interpolant = t;
            fadeLook(this, t, 0.7);
        }
        if (waitRelease) {
            if (!joyActive[0] && frameCount > 255)
                waitRelease = false;
        } else if (joyActive[0] || pressed || frameCount >= 1024) {
            s_idleTimer = 600;

            if (joyActive[0])
                playSample(5);
            waitRelease = true;
            if (banner <= 1 && nPlayers <= 1) {
                postScoreValue = score[0];
                postScoreCategory = endurance ? gameMode + 4 : gameMode;
                shellCommand = 2;
                // (Port: a local high score instead of OpenFeint's.)
                if (hs.qualifies(postScoreCategory, score[0])) {
                    entryScore = score[0];
                    entryLevel = level + 1;
                    entryBoard = postScoreCategory;
                    entryName = hs.lastName;
                    hiTimer = 0;
                    reset(0);
                    nameEntryScreen();
                    state = 0x72;
                    break;
                }
            }
            bgColour = 0;
            state = 1;
            frameCount = 0;
        }
        break;
    case 6:
        if (paused) {
            g->visible = true;
            g->cls();
            g->clattr();
            pauseScreen();
            pausedState = 6;
            state = 7;
        } else if (frameCount >= 384) {
            newLevel();
            if (msgTimer == 0)
                g->cls();
            g->cPrintAt(13, "~9479-= SHIELDS RENEWED =-");
            msgTimer = 120;
            state = 4;
        }
        break;
    case 7: {
        if (waitRelease) {
            if (!joyActive[0])
                waitRelease = false;
            break;
        }
        if (!touchOrButton)
            break;
        waitRelease = true;
        int label = actions.activateLabel ? actions.activateLabel
                    : pressed             ? 23
                                          : g->getLabel(joyOrigin[0]);
        if (label == 24) {
            playSample(5);
            shellCommand = 14;
        } else if (label == 33) { // EXIT to iOX
            playSample(5);
            shellCommand = 32;
        } else if (label == 34) { // TITLE: original Quit Game route
            playSample(5);
            g->cls();
            g->clattr();
            frameCount = 0;
            pauseArmed = false;
            demo = true;
            demoLevel = 0;
            deathPauseFrames = 0;
            spawnCount = 0;
            reset(0);
            state = 1;
        } else if (label == 23) { // UNPAUSE
            playSample(5);
            paused = false;
            f2d4 = 0;
            g->cls();
            g->clattr();
            makeArena();
            purgeTouches();
            state = 4;
        }
        break;
    }
    case 8: {
        float t = float(double(s_interpolant) + 0.02);
        s_interpolant = t;
        float inv = 1.0f - t;
        float sc = float(0.997 + double(inv) * 0.29);
        fbScaleX = fbScaleY = sc;
        float c = float(0.9 + double(t) * -0.7);
        fbColour[0] = c;
        fbColour[1] = c;
        fbColour[2] = float(0.9 + double(t) * -0.1);
        if (t < 1.0f) {
            std::snprintf(buf, sizeof buf, "LEVEL %d", level + 1);
            g->cPrintAt(4, buf, 0xffff0080);
            g->cPrintAt(7, levelNames[level], 0xff00ffff);
        } else {
            g->printAt(1, 4, kBlankRow);
            g->printAt(1, 7, kBlankRow);
            demo = false;
            pauseArmed = true;
            if (deathPauseFrames < 0)
                restartLevel();
            else
                newLevel();
            state = 4;
        }
        break;
    }
    case 9: {
        if (frameCount & 1) {
            s_tr = explodeLastBeast();
            if (s_tr == 0) {
                for (entity *e = beasts; e; e = e->next)
                    pixelShatter(nullptr, e, vec2d(1, 1), 40, 0, 1, 0, 2);
                for (entity *e = obstacles; e; e = e->next)
                    pixelShatter(nullptr, e, vec2d(1, 1), 40, 0, 1, 0, 2);
            }
        }
        float t = float(double(s_interpolant) - 0.01);
        s_interpolant = t;
        if (t < 0.0f) {
            t = 0.0f;
            s_interpolant = 0.0f;
        }
        fadeLook(this, t, 0.7);
        if (t <= 0.0f) {
            g->printAt(1, 4, kBlankRow);
            g->printAt(1, 6, kBlankRow);
            g->printAt(1, 8, kBlankRow);
            newLevelBest = false;
            if (level > 99) { // completed the last level
                if (s_tr > 0)
                    break;
                if (!demo)
                    playSample(14);
                level = 0;
                demo = true;
                demoLevel = 0;
                g->cPrintAt(3, "GAME OVER");
                pauseArmed = false;
                state = 5;
                msgTimer = 0;
                frameCount = 0;
                waitRelease = true;
            } else {
                levelFlags = 0;
                if (s_tr > 0)
                    break;
                if (level % 5 == 4) {
                    if (!demo)
                        playSample(0x20);
                } else if (!demo) {
                    playSample(0x1f);
                }
                state = 8;
            }
        } else {
            std::snprintf(buf, sizeof buf, "LEVEL %d COMPLETE", level);
            g->cPrintAt(4, buf, 0xffff0040);
            if (newLevelBest)
                g->cPrintAt(6, "NEW RESUME BEST!", gcol[0].packRGBA());
            std::snprintf(buf, sizeof buf, "BeastieBonus %d", levelBonusPoints);
            g->cPrintAt(8, buf, 0xff00ff4f);
        }
        break;
    }
    case 111:
        if (storyTextScroll() || pressed) {
            waitRelease = true;
            frameCount = 0;
            reset(0);
            state = 1;
        }
        break;
    case 0x71: // (port) high score table
    case 0x72: // (port) name entry
        scoreScreensInput(actions.activateLabel);
        break;
    case 112:
        if (minoInfoScreen() || pressed || actions.back) {
            waitRelease = true;
            frameCount = 0;
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

    // ---------------------------------------------------------------- tail
    if (!running)
        return;
    gcol[0].generate();
    gcol[1] = getDynamicColour(8, 1.0f, frameCount);
    gcol[2] = getDynamicColour(5, 1.0f, frameCount);
    gcol[3] = getDynamicColour(0, 1.0f, frameCount);
    gcol[4] = getDynamicColour(4, 1.0f, frameCount);
    gcol[5] = getDynamicColour(2, 1.0f, frameCount);
    gcol[6] = getDynamicColour(3, 1.0f, frameCount);
    gcol[7] = getDynamicColour(1, 1.0f, frameCount);
    gcol[8] = getDynamicColour(6, 1.0f, frameCount);
    gcol[9] = getDynamicColour(7, 1.0f, frameCount);
    gcol[10] = getDynamicColour(9, 1.0f, frameCount);
    if (demo) {
        fb90[12] = 0.0f;
        shakeFrames = 0;
    } else {
        float &fade = fb90[12]; // 0xbc0 the bomb flash
        double d = double(fade);
        if (d > 0.01) {
            float c0 = float(0.9 + d * -0.7);
            fade = float(d * 0.9);
            fbColour[0] = c0;
            if (double(fade) <= 0.01) {
                fbColour[1] = 0.2f;
                fbColour[0] = 0.2f;
                fbColour[2] = 0.8f;
            }
        }
        if (shakeFrames != 0) { // screen shake
            if (shakeFrames & 1) {
                g->scrollX = rng.rand(-0.2, 0.2);
                g->scrollY = rng.rand(-0.2, 0.2);
            }
            if (--shakeFrames == 0) {
                g->scrollY = 0;
                g->scrollX = 0;
            }
        }
    }
    if (paused)
        return;

    if (deathPauseFrames > 0) {
        if (--deathPauseFrames == 0) { // the death pause is over
            if (--lives[0] < 1) {
                if (!demo)
                    playSample(14);
                if (furthest < level) {
                    playSample(60);
                    g->cPrintAt(7, "~9499FURTHEST EVER!");
                    g->cPrintAt(9, congratulatoryMessages[neonRand() & 7]);
                    furthest = level;
                }
                updateStats();
                demo = true;
                s_interpolant = 1.0f;
                g->cPrintAt(3, "~9909GAME OVER");
                pauseArmed = false;
                state = 5;
                msgTimer = 0;
                frameCount = 0;
                waitRelease = true;
            } else {
                // Remember what is left of the level and restart it.
                int n = 0;
                for (entity *e = obstacles; e; e = e->next)
                    if (e->type == 5)
                        n++;
                nObstacles = n;
                n = 0;
                for (entity *e = beasts; e; e = e->next)
                    if (e->type == 1 && !static_cast<beast *>(e)->rescued)
                        n++;
                nBeasts = n;
                for (entity *e = enemies; e; e = e->next)
                    spawnQueue[spawnCount++] = e->type;
                reset(0);
                deathPauseFrames = -1;
                s_interpolant = 0.0f;
                pauseArmed = false;
                state = 8;
            }
        }
    } else if (msgTimer <= 0 && spawnQueueDrained && liveEnemyCount <= 0) {
        bool waiting = false;
        if ((levelFlags & 0x10) && beasts) { // herd wave: every beast must be rescued
            int n = 0;
            for (entity *e = beasts; e; e = e->next)
                if (e->type == 1 && !static_cast<beast *>(e)->rescued)
                    n++;
            waiting = n > 0;
        }
        if (!waiting) {
            if (demo) {
                if (state > 2) {
                    reset(0);
                    newLevel();
                }
            } else if (state == 4) {
                levelComplete();
            }
        }
    }

    if (--beamCycleFrames < 1)
        beamCycleFrames = beamCycleLength;
    driftPhaseX += driftPhaseStepX;
    driftPhaseY += driftPhaseStepY;
    float s = std::sin(driftPhaseX) * std::sin(driftPhaseY);
    driftAngle = float(double(s) * 1.0471975513333334 + 3.141592654);
    worldDrift = vec2d(0, double(-driftSpeed));
    worldDrift.rotateBy(double(driftAngle));
    if (spawnCount < 1) {
        spawnQueueDrained = true;
    } else {
        genLevel();
        if (spawnCount < 1)
            spawnQueueDrained = true;
    }
    if (!levelEnding) {
        if (msgTimer > 0) {
            if (--msgTimer == 0)
                eraseMessage();
            else
                drawMessage();
        }
        if (hotShotFrames > 0 && !hotShotsPermanent)
            hotShotFrames--;
        if (beastLoveFrames > 0)
            beastLoveFrames--;
        if (beastAttractionFrames != 0)
            beastAttractionFrames--;
    }
    if (paused)
        return;
    updateStats();
    liveEnemyCount = 0;
    runList(players);
    runList(projectiles);
    runList(obstacles);
    runList(particles);
    runList(beasts);
    runList(enemies);
    runList(enemyShots);
}

} // namespace Neon_Internals
} // namespace NEON_GAME
