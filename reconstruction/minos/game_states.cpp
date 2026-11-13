// Game play states; shared state and interfaces are in game_states.h.
#include "game_states.h"
#include "bge.h"
#include "cavern_data.h"
#include "cavern_draw.h"
#include "neon/entity_list.h"
#include "neon/foundation_rules.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace minos {

const char *const kGoMsg[9] = { // goMsg (0x23b140)
    "Please fetch me", "out the cavern!", "Now bring",   "back out!",  "Dock with the",
    "MOTHERSHEEP!",    "Thank you for",   "bringing me", "EXTRA DUDE!"};

namespace {
using cavern::kCavernDefinitions;
inline Vec2 rotated(Vec2 v, double a) {
    neon::vec2d r{v.x, v.y};
    r.rotateBy(a);
    return {r.x, r.y};
}

const int32_t kSyncShotNoises[4] = {0x39, 0x3a, 0x39, 0x46}; // syncShotNoises (0x23af40)
const char *const kLandingRatings[5] = {"UNBULLIEVABULL!", "ADMIRABULL!", "ENVIABULL!",
                                        "TOLERABULL!",
                                        "QUESTIONABULL!"}; // landingRatings (0x23b0cc)
const uint32_t kBcols[7] = {5,         0xff0000ffu, 0xff00ffu, 0xffff00ffu,
                            0xffffffu, 0xff00ffffu, 0xe}; // bcols (0x231bd0)
const int32_t kBonusMinoFrames[9] = {12, 13, 14, 15, 16,
                                     15, 14, 13, 2}; // _aFrames.78836 (0x1e6cb4)
const char *const kTaurus[19] = {                    // taurus (0x23b0e0), initStars' pattern
    "  *              *  ", "**                **", "**                **", " **     ****     ** ",
    "  *** **    ** ***  ", "    ***      ***    ", "    *          *    ", "    * **    ** *    ",
    "    * **    ** *    ", "    *          *    ", "     *        *     ", "     *        *     ",
    "     *  *  *  *     ", "     *  *  *  *     ", "      *      *      ", "      **    **      ",
    "      * **** *      ", "      **    **      ", "        ****        "};
const char *const kGrades[8] = {"S", "A+", "A", "B",
                                "C", "D",  "E", "F"}; // sequenceBonuses' local table

// Neon_Internals::getDynamicColour (0xf330, the same code as Minotron's; the engine rule).
neon::colour dynamicColour(int type, float alpha, int frame) {
    static constexpr neon::PaletteRules rules{neon::WashRule::TriangularRainbow, 12, 4};
    const auto ch = neon::paletteColour(type, alpha, uint32_t(frame), rules);
    return neon::colour(ch.r, ch.g, ch.b, ch.a);
}

int32_t zoneWord(
    const PlayState &g) { // caverns[+0x450] + 0x124 + 4 * sector (host bound: 0 past the table)
    return g.sector >= 0 && g.sector < cavern::kMaxZones ? g.cavern->zoneArgs[g.sector] : 0;
}
} // namespace

// ------------------------------------------------------------------ controlBlock
void PlayState::openPause() { // pauseButton::onPressed 0x59e3c
    if (gameOver)
        return;
    paused = true;
    if (updatedMenus)
        purgeTouches();
    auto addButton = [this](PauseControl::Action action, int art, Vec2 p, double sx,
                            bool randomGrow, float baseSize = 0.08f) {
        auto *b = new PauseControl(action);
        b->g = this;
        b->spriteIndex = art;
        b->pos = p;
        b->size = b->baseSize = baseSize;
        b->scale = {sx, 1.0};
        if (randomGrow)
            b->growRate = float(rnd(0.05, 0.2));
        addEntity(debris, b);
    };
    addButton(PauseControl::Action::Resume, 8, {0.0, updatedMenus ? menuLayout::PauseY : -0.08},
              3.5, true);
    if (updatedMenus) {
        for (auto *e = debris; e; e = e->next)
            if (dynamic_cast<Osj *>(e))
                e->dead = true;
        tutorial = false;
        addButton(PauseControl::Action::Quit, menuArt::NewGame, {-0.25, 0.26}, 3.0, true, .065f);
        addButton(PauseControl::Action::Scores, menuArt::HighScores, {0.0, -0.02}, 5.0, true,
                  .065f);
        addButton(PauseControl::Action::Exit, menuArt::Exit, {0.25, 0.26}, 3.0, false, .065f);
        refreshPauseOptions();
        for (int k = 0; k < 2; ++k) {
            auto *hint = new Osj;
            hint->g = this;
            hint->pauseGuide = true;
            hint->spriteIndex = k == 0 ? 0x49 : 0x4a;
            hint->scale = {1, .2};
            hint->size = .08f;
            addEntity(debris, hint);
        }
        return; // Three source-ordered random draws; Exit adds none.
    }
    addButton(PauseControl::Action::Quit, 9, {0.0, 0.32}, 2.4, true);
    auto *scores = new OpenFeintButton;
    scores->g = this;
    scores->spriteIndex = 0x5c;
    scores->pos = {0.0, 0.1};
    scores->size = scores->baseSize;
    scores->scale = {1.4, 1.0};
    scores->pressed = true;
    scores->kill = true;
    scores->growRate = float(rnd(0.05, 0.2));
    addEntity(debris, scores);
    if (iPad)
        addButton(PauseControl::Action::Handed, 0x5d, {0.0, -0.25}, 3.0, false);
}

void PlayState::refreshPauseOptions() {
    if (!updatedMenus || !paused)
        return;
    for (auto *e = debris; e; e = e->next)
        if (auto *b = dynamic_cast<PauseControl *>(e))
            if (b->action == PauseControl::Action::Touch || b->action == PauseControl::Action::Tilt)
                b->dead = true;
    auto add = [&](PauseControl::Action action, int art, double x) {
        auto *b = new PauseControl(action);
        b->g = this;
        b->spriteIndex = art;
        b->pos = {x, menuLayout::ControlsY};
        b->size = b->baseSize = .045f;
        b->scale = {3.2, 1};
        addEntity(debris, b);
    };
    const bool showTilt = tiltAvailable && controllerAvailable;
    if (touchAvailable)
        add(PauseControl::Action::Touch, menuArt::Touch1, showTilt ? -.24 : 0);
    if (showTilt)
        add(PauseControl::Action::Tilt, menuArt::TiltOn, touchAvailable ? .24 : 0);
}

void PauseControl::activate() {
    switch (action) {
    case Action::Resume:
        g->paused = false;
        if (g->updatedMenus)
            g->purgeTouches();
        break;         // 0x53e00
    case Action::Quit: // 0x53e10
        g->paused = false;
        g->scoreTotal = 0;
        g->frameCount = 0;
        g->runState = 1;
        g->goalState = 0x11;
        g->buttonLatch = true;
        break;
    case Action::Handed: // 0x53e50
        g->prefsControl = (g->prefsControl & 1) ? 0 : 1;
        g->goalState = 0x11;
        g->buttonLatch = true;
        break;
    case Action::Scores:
        g->goalState = 0xe;
        break;
    case Action::Exit:
        g->exitToLauncherRequested = true;
        break;
    case Action::Touch:
        if (!g->touchAvailable)
            break;
        if (!g->touchGameplayEnabled) {
            g->touchGameplayEnabled = true;
            g->touchLayout = 0;
        } else if (g->touchLayout < 2)
            ++g->touchLayout;
        else {
            g->touchLayout = 0;
            if (g->controllerAvailable)
                g->touchGameplayEnabled = false;
        }
        g->iPad = g->touchLayout != 0;
        g->prefsControl = g->touchLayout == 2 ? 1 : 0;
        g->purgeTouches();
        g->goalState = 0x11;
        g->buttonLatch = true;
        break;
    case Action::Tilt:
        if (g->tiltAvailable && g->controllerAvailable) {
            g->tiltEnabled = !g->tiltEnabled;
            g->goalState = 0x11;
            g->buttonLatch = true;
        }
        break;
    }
}

void PauseControl::run() {
    if (action == Action::Touch)
        spriteIndex =
            g->touchGameplayEnabled ? menuArt::Touch1 + g->touchLayout : menuArt::TouchOff;
    if (action == Action::Tilt)
        spriteIndex = g->tiltEnabled ? menuArt::TiltOn : menuArt::TiltOff;
    if (action == Action::Handed)
        scale.x = (g->prefsControl & 1) ? -3.0 : 3.0;
    gameButtonRun(*this, baseSize, pressed, grow, growRate, [this] { activate(); });
    if (!g->paused)
        dead = true;
}

void PlayState::joyWatchdog() { // controlBlock::joyWatchdog
    int held = 0, stale = 0;
    for (int i = 0; i < 4; i++) {
        if (!touchDown[i])
            continue;
        held++;
        if (500 < ++touchTimer[i])
            stale++;
    }
    if (stale != 0 && held == stale)
        purgeTouches();
}

void PlayState::runUnpausedEntities() { // 0x54310
    auto dispose = [this](Entity *e) { this->dispose(e); };
    neon::entities::runAndCull(shipList, dispose);
    neon::entities::runAndCull(list2d8, dispose);
    neon::entities::runAndCull(list2dc, dispose);
    runShots();
    neon::entities::runAndCull(touchList, dispose);
    neon::entities::runAndCull(bounceList, dispose);
    neon::entities::runAndCull(list2f0, dispose);
    neon::entities::runAndCull(list2ec, dispose);
}

void PlayState::clearMessages(int upTo) {
    for (int k = 0; k < upTo; k++)
        if (slots[k].text) {
            if (hud)
                hud->cll(slots[k].row);
            slots[k].text = nullptr;
        }
}

// ------------------------------------------------------------------ theGame::run
void PlayState::runFrame() { // 0x6c020
    if (!hudReady) {         // +0x440
        fireHeld = false;
        if (!hud)
            return;
        hudReady = true;
    }
    if (!paused) {
        frameCount++;
        float p = float(double(phase) + 0.02);
        phase = p;
        if (!(p < 1.0f))
            phase = p - 1.0f;                                     // vsub.f32
        phaseSin = std::sin(float(double(phase) * 6.2831853072)); // sinf of a binary64 product
    }
    hud->setFlags(inputFlipped, paused);
    if (!byte6e)
        byte6e = true;
    joyWatchdog();
    if (goalState < 0) {
        if (goalState == -1)
            purgeTouches();
        goalState = 0;
    }
    byte6d = false;
    // A press: touch 0 starting or any hardware button (prefs +0xc84 & 0xfff).
    firePressed = false;
    if (!buttonLatch) {
        if (touchPressed[0] || (prefsButtons & 0xfff)) {
            firePressed = true;
            fireHeld = true;
            buttonLatch = true;
            if (touchPressed[0])
                (void)hud->labelAt(touchStartX[0], touchStartY[0]);
        }
    } else if (!touchPressed[0] && !(prefsButtons & 0xfff)) {
        buttonLatch = false;
        keyRepeat = 0;
        keyCount = 0;
        fireHeld = false;
    }
    const uint32_t b = prefsButtons;
    keyCode = 0;
    if (fireHeld) {
        if (keyRepeat < 1) {
            keyCode = hud->labelAt(touchStartX[0], touchStartY[0]);
            if (runState == 7) {
                if (b & 8)
                    keyCode = 8;
                if (b & 2)
                    keyCode = 7;
                if ((b & 0xff0) && runState == 7)
                    keyCode = prefs[shipKind * 100 + selCavern[shipKind & 3] + 0x10] < 1 ? 3 : 9;
            }
            keyRepeat = keyCount < 1 ? 0x1e : 7;
            keyCount++;
        } else {
            keyRepeat--;
        }
    }
    if (menuActions.activateLabel)
        keyCode = menuActions.activateLabel;
    if (menuActions.advance)
        firePressed = true;
    menuActions = {};
    // D-pad bits 8 (left) / 2 (right) step the analog axis +0x284; no direction: it decays.
    if ((b & 0xf) == 0)
        analog = analog * 0.2;
    else if (b & 10) {
        if (b & 8) {
            if (analog <= 0.0) {
                analog = analog - 0.07999999821186066;
                if (analog < -1.0)
                    analog = -1.0;
            } else
                analog = -0.07999999821186066;
        }
        if (b & 2) {
            if (analog < 0.0)
                analog = 0.07999999821186066;
            else {
                analog = analog + 0.07999999821186066;
                if (analog > 1.0)
                    analog = 1.0;
            }
        }
    }
    // +0xa08: +0x38 with a 0.08 dead zone, clamped to +-0.3 (binary32 store).
    {
        const float j = float(physicalTiltEnabled() ? lateralTilt : 0);
        shotDrift = j;
        if (double(std::fabs(j)) < 0.08)
            shotDrift = 0;
        else {
            shotDrift = j < 0.0f ? float(double(j) + 0.08) : float(double(j) - 0.08);
            if (double(shotDrift) < -0.3)
                shotDrift = -0.3f;
            else if (double(shotDrift) > 0.3)
                shotDrift = 0.3f;
        }
    }

    if (controllerInfluence != 0)
        shotDrift = float(std::clamp(controllerInfluence, -.3, .3));

    switch (runState) {
    case 4:
        if (!paused) {
            screenY = screenY + double(float(scrollSpeed - screenY)) * 0.004;
            doLauncher(zoneWord(*this) & 0xff00);
        }
        if (flag203808)
            bgeSetup(zoneWord(*this) & 0xff);
        break;
    case 5: {
        float f = bgBright;
        if (0.6 < double(f)) {
            f = float(double(f) * 0.997);
            bgBright = f;
        }
        if (double(f) < 0.6)
            bgBright = float(double(f) + 0.001);
        const double k = double(bgBright);
        bgColour[0] = float(k * colours[4].r);
        bgColour[1] = float(k * colours[4].g);
        bgColour[2] = float(k * colours[4].b);
        double s = scrollDelta.y; // +0x348 eases to 0.0015
        if (s < 0.0015) {
            s = s + 5e-05;
            scrollDelta.y = s;
        }
        if (s > 0.0015)
            scrollDelta.y = s - 5e-05;
        sequenceBonuses();
        if (!firePressed || bonusPhase != 5)
            break;
        const int c = ++cavernIndex;
        const int base = shipKind * 100;
        const int bestLives = prefs[base + c + 0x30];
        if (bestLives <= lives) {
            bool better;
            if (bestLives < lives && prefs[base + c + 0x0f] < scoreTotal)
                better = true;
            else
                better = prefs[base + c + 0x10] < scoreTotal;
            if (better) {
                prefs[base + c + 0x30] = lives;
                prefs[base + c + 0x10] = scoreTotal;
                const uint32_t r = libcRand();
                if (!muted)
                    playSample(int((r & 0x3ff) % 6) + 6, volume << 13);
                goalState = 0x11;
            }
        }
        if (prefs[base + 1] < c) {
            prefs[base + 1] = c;
            goalState = 0x11;
        }
        if (c >= cavern::kCavernCount) {
            gameFinished = true;
            goGameOver();
            break;
        } // caverns[c] == NULL
        cavern = &kCavernDefinitions[c];
        announceCavern(); // bl at 0x6c684 (the decompilation shows a fall-through)
        break;
    }
    case 6:
        if (hud) {
            hud->cPrintAt(1, "ENTERING", colours[3]);
            std::snprintf(textBuf, sizeof textBuf, "CAVERN %d", cavernIndex + 1);
            hud->printAt(3, 2, textBuf, colours[3]);
        }
        if (firePressed) {
            if (hud) {
                hud->setSize(0x14, 0x1e);
                hud->cls();
            }
            bgLayer = 2;
            drawMode = 2;
            muted = false;
            runState = 4;
            resetLists(0);
            if (newLevelHook)
                newLevelHook(); // newLevel()
        }
        break;
    default:
        menuState(); // title 1-3, select 7, game over 8, info 9
        break;
    }
    if (!running)
        return; // +0x2ac

    colours[0].unpackRGBA(cavern::colourFromSequence(
        float(int32_t((uint32_t(frameCount) & 0x1f) << 1)) / 63.0f, cavern::kA8ColourSequence));
    static const int kTypes[10] = {8, 5, 0, 4, 2, 3, 1, 6, 7, 9};
    for (int i = 0; i < 10; i++) {
        const neon::colour c = dynamicColour(kTypes[i], 1.0f, frameCount);
        colours[i + 1].r = c.r;
        colours[i + 1].g = c.g;
        colours[i + 1].b = c.b;
        colours[i + 1].a = c.a;
    }
    hud->setPalette(colours, 10); // +0x4d8..+0x7a8 -> grid +0x1f70.. (+0x7f8 is not copied)

    if (!muted) {
        if (runState == 4) {
            // Flash channels (+0x2078b8..+0x207910): sample on a seen flash, then the timers.
            for (int i = 0; i < 4; i++) {
                flash->watched[i] = 0;
                if (clearAll < 1) {
                    if (flash->seen[i] && !muted)
                        playSample(kSyncShotNoises[i], volume << 12);
                    flash->seen[i] = 0;
                    int t = flashTimer[i];
                    if (-1 < t) {
                        flashTimer[i] = --t;
                        if (t < 1) {
                            const int c = flashCount[i];
                            if (c < 1) {
                                flashTimer[i] = flashOff[i];
                                flash->lit[i] = 0;
                                flashCount[i] = flashCountReload[i];
                            } else {
                                flashTimer[i] = flashOn[i];
                                flash->watched[i] = 1;
                                flash->lit[i] = 1;
                                flashCount[i] = c - 1;
                            }
                        }
                    }
                } else {
                    flash->lit[i] = 0;
                }
            }
            if (!paused && 0 < clearAll)
                clearAll--;
            int a = approachTimer; // +0x20380c (motherSheep::hitBy starts it)
            if (a == 0) {
                if (!paused)
                    missionTime++;
            } else if (0 < a) {
                approachTimer = a + 1;
                if (800 < a + 1 || firePressed)
                    gameOver = true;
            }
            if (tutorial && iPad)
                tutorial = false;
            a = approachTimer;
            if (a < 200) {
                if (!tutorial) {
                    hud->setPrintColour(colours[2]);
                    std::snprintf(textBuf, sizeof textBuf, "Score: %d", scoreTotal);
                    hud->printAt(2, 1, textBuf);
                    hud->printAt(2, 2, "Shield: ");
                    hud->printAt(2, 3, "Fuel:   ");
                }
            } else if (a == 200) {
                hud->cll(1);
                hud->cll(2);
                hud->cll(3);
            }
            if (flag203808)
                minos::launchBGEs(*this); // 0x5c884 (bge.cpp)
            if (!gameOver) {
                for (auto &m : slots) {
                    if (0 < m.count) {
                        hud->cll(m.row);
                        if (0 < --m.count)
                            hud->cPrintAt(m.row, m.text, colours[m.colour]);
                    }
                }
            } else if (lives < 1) {
                if (prefs[shipKind * 100] < scoreTotal) {
                    prefs[shipKind * 100] = scoreTotal;
                    reportKind = shipKind;
                    reportScore = scoreTotal;
                    goalState = 2;
                }
                goGameOver();
            } else {
                runState = 5;
                bgLayer = 3;
                drawMode = 3;
                buttonLatch = true;
                fireHeld = false;
                bonusPhase = 0;
                bonusTimer = 0x32;
                timeBonus = 0;
                fuelBonus = 0;
                hud->cls();
            }
        }
        if (shake != 0) { // +0x4c4: HUD shake
            if (shake & 1) {
                const double x = rnd(-0.2, 0.2);
                const double y = rnd(-0.2, 0.2);
                hud->setShake(x, y);
            }
            if (--shake == 0)
                hud->setShake(0, 0);
        }
    } else {
        word39c = 0;
        shake = 0;
    }
    if (!paused)
        runUnpausedEntities();
    runDebris();
}

// ------------------------------------------------------------------ _bgdraw (state part)
void PlayState::bgdrawState(Vec2 camera) { // 0x7d12c without the drawing
    if (bgLayer == 1) {
        stmPalette[1] = colours[1];
    } else if (bgLayer == 2) {
        if (!flag203808)
            byte31e = false;
        sector = prepareVisibleLines(*map, camera);
        stmPalette[1] = colours[3];
        stmPalette[2] = colours[0];
    }
}

// ------------------------------------------------------------------ reset, announceCavern
void PlayState::resetLists(int keep) { // 0x5654c
    byte442 = false;
    auto drop = [this](Entity *&head) {
        while (head) {
            Entity *e = head;
            head = e->next;
            dispose(e);
        }
    };
    if (!(keep & 1))
        drop(shipList);
    if (!(keep & 2))
        drop(list2d8);
    if (!(keep & 4))
        drop(list2dc);
    if (!(keep & 0x100))
        drop(list2f0);
    if (!(keep & 8))
        drop(shots);
    if (!(keep & 0x10))
        drop(debris);
    if (!(keep & 0x20))
        drop(touchList);
    if (!(keep & 0x40))
        drop(bounceList);
    if (!(keep & 0x80))
        drop(list2ec);
}

void PlayState::announceCavern() { // 0x60740
    bgColour[0] = bgColour[1] = bgColour[2] = 0;
    bgBright = 0;
    if (hud)
        hud->setSize(0xe, 10);
    titleScenery(); // the stm layout (menu_states.cpp)
    runState = 6;
    drawMode = 1;
    bgLayer = 1;
    resetLists(0);
    for (int i = 0; i < 0x32; i++) {
        auto *t = new TitleStar;
        t->g = this;
        t->pos.y = rnd(-0.5, 0.5);
        t->randomiseStuff();
        t->vel.x = 0;
        addEntity(list2dc, t);
    }
    buttonLatch = true;
    const uint32_t r = libcRand();
    if (hud) {
        hud->quickMsg(kQuestNames[r & 7], {0.0, 0.38}, {2.0, 1.0}, 0,
                      6); // scale (2, 1): the decompilation shows 0
        hud->quickMsg(cavern->name, {0.0, 0.43}, {2.0, 1.0}, 1, 6);
        hud->setLine(0, 0xa2, -1); // lines[0] / [1] +0xb8: the monospaced font
        hud->setLine(1, 0xa2, -1);
    }
}

void TitleStar::randomiseStuff() { // 0x560f0
    PlayState &G = *g;
    pos.x = G.rnd(-0.5, 0.5);
    vel.y = G.rnd(0.0005, 0.005);
    spin = float(G.rnd(-0.02, 0.02));
    col.unpackRGBA(kRcols[int((G.libcRand() & 0xff) % 0xb) + 1]);
    col.a = G.rnd(0.2, 0.6);
    const double s = G.rnd(0.0005, 0.01);
    spriteIndex = 0;
    size = float(s);
}

void TitleStar::run() { // star::run 0x53e9c
    pos.y = pos.y + vel.y;
    if (0.6 < pos.y) {
        randomiseStuff();
        pos.y = -0.6;
    }
    rot = rot + spin;
}

// ------------------------------------------------------------------ goGameOver (state part)
namespace {
const double kOverY = [] {
    uint64_t u = 0xbfc70a3d70a3d70bull;
    double d;
    std::memcpy(&d, &u, 8);
    return d;
}();
} // namespace
void PlayState::goGameOver() { // 0x602dc
    bgColour[0] = bgColour[1] = bgColour[2] = 0;
    bgBright = 0;
    endScreenCountdown = 1000;
    runState = 8;
    playSample(0x45, volume << 12);
    const uint32_t r = libcRand();
    if (!muted)
        playSample(int((r & 0x3ff) % 6) + 6, volume << 14);
    drawMode = 3;
    buttonLatch = true;
    if (hud) {
        if (!gameFinished) {
            hud->quickMsg("GAME", {0.0, -0.33}, {4.0, 4.0}, 0, 2);
            hud->quickMsg("OVER", {0.0, kOverY}, {4.0, 4.0}, 1, 2);
        } else {
            hud->quickMsg("GAME", {0.0, -0.33}, {4.0, 4.0}, 0, 3);
            hud->quickMsg("FINISHED", {0.0, kOverY}, {4.0, 4.0}, 1, 3);
        }
        hud->setLine(0, 0xa2, -1);
        hud->setLine(1, 0xa2, -1);
    }
    clearMessages(10);
    if (hud) {
        hud->cll(2);
        hud->cll(3);
    }
    if (gameFinished) {
        if (hud) {
            hud->cls();
            hud->clattr();
        }
        frameCount = 1;
        bonusTotal = lives * 10000;
        scoreBase = scoreTotal;
        bonusT = 0;
    }
    if (goGameOverHook)
        goGameOverHook();
}

// ------------------------------------------------------------------ awardLanding
void PlayState::awardLanding() { // 0x74798 (wavingMinotaur::jumpIntoShip)
    if (!bonusPending)
        return;
    const int currentRating = landingBonus;
    const int grade = 5 - currentRating; // 0 best .. 4
    int voice = 0x25 - grade;
    const int points = (grade * -10 + 0x32) * currentRating * currentRating;
    bonusPending = false;
    if (grade == 1)
        voice = (libcRand() & 0xf) == 0 ? 0x31 : 0x24;
    if (!muted)
        playSample(voice, volume << 12);
    setMessage(4, 0x5a, 0x12, 2, kLandingRatings[grade]);
    score(points / (grade < 1 ? 2 : 5) + 1, bonusPos, {4.0, 4.0});
    const int hearts = points / 5;
    for (int i = 0; i < hearts; i++) {
        const double t = rnd(0.0, 1.0);
        const Vec2 v = rotated({0.015, 0.0}, double(float(t)) * 3.141592654);
        auto *h = new MHeart;
        h->pos = {bonusPos.x + v.x * 3.0,
                  bonusPos.y + v.y * 3.0}; // vec2d * 3.0 (instruction 0x74a60)
        h->init(*this);
        h->vel = v;
        h->col.unpackRGBA(cavern::colourFromSequence(float(t), kRcols));
        h->spin = float(rnd(-0.3, 0.3));
        h->size = float(rnd(0.003, 0.01));
        h->accel.y = rnd(0.0001, 0.0005);
        const double x = bonusPos.x;
        h->accel.x = 0.0 <= x ? -rnd(x * 0.0001, x * 0.0007) : -rnd(x * 0.0007, x * 0.0001);
        h->spriteIndex = 0;
        addEntity(debris, h);
    }
    if (grade == 0) {
        // Perfect-landing streak +0x458 and its goals (3 at 1, 4 at 5, 5 at 10); every path
        // ends at the bonus heart.
        const int streak = ++perfectLandingStreak;
        if (streak == 1 && !muted && awardEligibilityGate < 2 && !(goalBits & 8))
            achieve(8, 7, "'AYYYYYUP'");
        if (streak == 5 && !muted && awardEligibilityGate < 2 && !(goalBits & 0x10))
            achieve(0x10, 8, "'SUPER PONY'");
        if (streak == 10 && !muted && awardEligibilityGate < 2 && !(goalBits & 0x20))
            achieve(0x20, 9, "'ULTRA PONY'");
    } else {
        perfectLandingStreak = 0;
        if (voice != 0x31)
            return;
    }
    auto *h = new MHeart; // LAB_00074bf8: the bonus heart
    h->pos = bonusPos;
    h->init(*this);
    h->vel.y = -0.001;
    h->size = 0.05f;
    h->sizeRate = 0.001f;
    h->vel.x = bonusPos.x * -0.03;
    h->alphaScale = 0.9f;
    h->spriteIndex = 0x1f;
    h->pos.y = h->pos.y + -0.07500000111758709;
    if (voice == 0x31)
        h->scale.x = -h->scale.x;
    addEntity(debris, h);
}

// ------------------------------------------------------------------ sequenceBonuses (state 5)
void PlayState::sequenceBonuses() { // 0x5d380
    float speed = 20.0f;
    int step = 0x14;
    if (!fireHeld) {
        step = 1;
        speed = 1.0f;
    }
    if (bonusTimer < 1) {
        switch (bonusPhase) {
        case 0:
            bonusTimer = 0x1e;
            bonusPhase = 1;
            if (hud)
                hud->printAtRgba(2, 7, "Minotaur Rescue:", 0xff803fffu);
            if (0 < minosSaved) {
                if (minosSaved == 1) {
                    bonusX = 0;
                    bonusStep = 0;
                } else {
                    bonusX = -0.4f;
                    bonusStep = float(0.8 / double(float(minosSaved - 1)));
                }
            }
            break;
        case 1:
            bonusTimer = 5;
            fuelShown = 0;
            if (minosSaved == 0) {
                minosSaved = -1;
                bonusPhase = 2;
            } else {
                minosSaved--;
                if (shipKind == 2) {
                    if (!muted)
                        playSample(0x4a, volume << 12);
                } else if (!muted)
                    playSample(0x28, volume << 12);
                auto *m = new WavingBonusMinotaur;
                m->g = this;
                m->frame = float(rnd(0.0, 7.0));
                m->spriteIndex = 0xc;
                m->size = 0.34f;
                m->scale = {1.2, 1.4};
                m->col.a = 0;
                m->leaving = 0;
                m->pos = {double(bonusX), -0.1};
                addEntity(debris, m);
                bonusX = bonusX + bonusStep; // binary32
                if (minosSaved < 1) {
                    bonusPhase = 2;
                    bonusTimer = 0x1e;
                }
            }
            break;
        case 2: {
            const float f = fuel;
            bonusTimer = 1;
            if (fuelShown < f) {
                const float s = float(double(fuelShown) + double(speed) * 0.016);
                fuelShown = s;
                if (s > f) { // vcmpe; ble (strictly above)
                    fuelShown = f;
                    bonusTimer = 0xa0;
                    if (!muted)
                        playSample(0x25, volume << 12);
                    bonusPhase = 3;
                    const float q = float(missionTime - parTime) / float(parTimeBase - parTime);
                    rating = q * 4.0f + 3.0f;
                    if (rating < 0.0f)
                        rating = 0;
                    ratingShown = rating;
                    if (double(rating) > 7.9)
                        rating = 7.9f;
                    timeBonus = int32_t(double(ratingShown) * 2000.0);
                }
                fuelBonus = respawnedThisCavern ? 0 : int32_t(double(fuelShown) * 20000.0);
            }
            break;
        }
        case 3:
            bonusPhase = 4;
            bonusTimer = 0x1e;
            bonusTotal = fuelBonus + timeBonus + rescueBonus;
            scoreBase = scoreTotal;
            bonusT = 0;
            break;
        case 4: {
            bonusTimer = 1;
            float t = float(double(bonusT) + double(speed) * 0.01);
            bonusT = t;
            if (t > 1.0f) {
                bonusT = 1.0f;
                t = 1.0f;
                buttonLatch = true;
                bonusPhase = 5;
            } // ble 0x5da18: strictly above
            scoreTotal = int32_t(float(bonusTotal) * t) + scoreBase;
            break;
        }
        default:
            bonusTimer = 0x1e;
        }
        return;
    }
    if (hud) {
        std::snprintf(textBuf, sizeof textBuf, "Score: %d", scoreTotal);
        hud->printAt(2, 1, textBuf, colours[2]);
        std::snprintf(textBuf, sizeof textBuf, "CAVERN %d SUMMARY", cavernIndex + 1);
        hud->printAt(2, 4, textBuf, colours[1]);
        if (0 < bonusPhase) {
            if (minosSaved < 0)
                hud->printAt(4, 9, "SORRY, NO BONUS", colours[4]);
            else {
                std::snprintf(textBuf, sizeof textBuf, "BONUS %d", rescueBonus);
                hud->printAt(4, 9, textBuf, colours[4]);
            }
            if (1 < bonusPhase) {
                std::snprintf(textBuf, sizeof textBuf, "Fuel Left %d%%",
                              int32_t(double(fuelShown) * 100.0));
                hud->printAtRgba(2, 0xf, textBuf, 0xff803fffu);
                if (fuelBonus < 1)
                    hud->printAt(4, 0x11, "SORRY, NO BONUS", colours[4]);
                else {
                    hud->cll(0x11);
                    std::snprintf(textBuf, sizeof textBuf, "BONUS %d", fuelBonus);
                    hud->printAt(4, 0x11, textBuf, colours[4]);
                }
                if (2 < bonusPhase) {
                    int m = missionTime / 60;
                    std::snprintf(textBuf, sizeof textBuf, "Mission time %d:%02d", m / 60, m % 60);
                    hud->printAtRgba(2, 0x13, textBuf, 0xff803fffu);
                    m = parTime / 60;
                    std::snprintf(textBuf, sizeof textBuf, "Par time %d:%02d", m / 60, m % 60);
                    hud->printAtRgba(2, 0x15, textBuf, 0xff803fffu);
                    hud->printAt(0x11, 0x15, kGrades[7 - int(rating)], colours[2]);
                    std::snprintf(textBuf, sizeof textBuf, "BONUS %d", timeBonus);
                    hud->printAt(4, 0x17, textBuf, colours[4]);
                    if (3 < bonusPhase) {
                        const int v = int32_t(float(bonusTotal) * bonusT);
                        scoreTotal = v + scoreBase;
                        std::snprintf(textBuf, sizeof textBuf, "TOTAL BONUS: %d", v);
                        hud->printAt(1, 0x1a, textBuf, colours[3]);
                    }
                }
            }
        }
    }
    bonusTimer -= step;
}

// ------------------------------------------------------------------ background effects
void PlayState::bgeIntervalSetup() { // 0x54bbc
    bgeIntervals++;
    if (bgeKind == 2) {
        bgeOrigin.x = rnd(-0.2, 0.2);
        bgeOrigin.y = -0.6;
        bgeDx = float(rnd(0.07, 0.13));
        if (libcRand() & 1)
            bgeDx = -bgeDx;
        bgeAngle = float(rnd(-3.141592654, 3.141592654));
        bgeSpin = float(rnd(-8e-05, 4.5e-05));
        bgeSpeed = float(rnd(0.006, 0.012));
        bgeSide = int32_t(libcRand() & 1);
    } else if (bgeKind == 5) {
        bgeOrigin.x = (libcRand() & 1) == 0 ? 0.6 : -0.6;
    } else if (bgeKind == 1) {
        bgeOrigin.x = (libcRand() & 1) == 0 ? -0.59 : 0.59;
        bgeOrigin.y = -0.6;
        bgeColour = kBcols[int((libcRand() & 0xff) % 5) + 1];
    }
}

void PlayState::bgeSetup(int kind) { // 0x54e7c
    if (bgeCurrent == kind)
        return;
    bgeCurrent = kind;
    bgeKind = kind;
    bgeIntervals = 0;
    static const int kPeriod[7] = {0x32, 0x1e, 0xf, 0x19, 0x23, 5, 0x19};
    static const int kGap[7] = {250, 150, 150, 450, 250, 250, 150}; // bytes 0xfa / 0x96 / 0xc2,0x01
    if (0 <= kind && kind <= 6) {
        bgePeriod = kPeriod[kind];
        bgeWaves = cavern->bgeWavesByKind[kind];
        bgeGapReload = kGap[kind];
        bgeIntervalSetup();
    }
    bgeTimer = bgePeriod;
    bgeGap = bgeGapReload;
    bgeLeft = bgeWaves;
}

// ------------------------------------------------------------------ newLevel (state part)
void PlayState::newLevelState() { // 0x636f0, after the map part
    const cavern::CavernDefinition &cv = *cavern;
    clearAll = 0;
    reset310 = false;
    gameFinished = false;
    bgeRate = 0.3f;
    cleanRun = true;
    perfectLandingStreak = 0;
    cameraY = 0; // +0x320..+0x32c
    fuel = 1.0f;
    shield = 1.0f;
    launcherTimer = 0;
    missionTime = 0;
    rescueBonus = 0;
    landingBonus = 0;
    static const float kParScale[4] = {1.0f, 1.8f, 0.8f, 2.0f};
    const int par = int32_t(float(cv.parTimeBase) * kParScale[shipKind & 3]);
    parTimeBase = par;
    rocketFlags = uint32_t(cv.flags); // +0x203804 = cavern +0x2f4
    parTime = par + par / 4;
    respawnedThisCavern = false;
    fuelWarn = false;
    byte442 = false;
    fuelCritical = false;
    byte443 = false;
    shieldWarn = false;
    pauseRequested = false;
    byte31e = false;
    gameOver = false;
    scrollDelta = {0, 0};
    word478 = 0;
    screenY = 0;
    shake = 0;
    phase = 0;
    scrollSpeed = screenY;
    for (int k = 0; k < 9; k++)
        slots[k].count = 0;
    pauseGlowRequested = true;
    slots[9].count = 0;
    flag203808 = false;
    minosSaved = 0;
    int sectors = 0;
    while (sectors < cavern::kMaxZones && cv.zones[sectors] >= 0)
        sectors++;
    scrollLimit = double(map->sectorLength() * sectors - 24); // +0x36c (binary64)
    for (int k = 0; k < 4; k++) {
        flashOn[k] = cv.flashTiming[k].onFrames;
        flashCountReload[k] = cv.flashTiming[k].cycles;
        flashOff[k] = cv.flashTiming[k].offFrames;
        flashTimer[k] = flashOn[k];
        flashCount[k] = flashCountReload[k];
        flash->watched[k] = 0;
        flash->lit[k] = 0;
    }
    bgeSetup(-1);
    landed = false;
    sector = 0;
    approachTimer = 0;
    // The host's createShip selects the distinct source class by +0x9e4.
    Entity *ship = createShip ? createShip() : nullptr;
    // The per-cell rand table: cell flags of rows 0..2047 cleared, the word 2048 rows later a
    // fresh rand() (262144 draws, row-major).
    for (int r = 0; r < 0x800; r++)
        for (int c = 0; c < 0x80; c++) {
            map->cells[size_t(r) * cavern::StripMap::kMapColumns + size_t(c)] = 0;
            map->cells[size_t(r + 0x800) * cavern::StripMap::kMapColumns + size_t(c)] =
                int32_t(rand->next());
        }
    initStars(0.8f, 0.8f, 0x8000ffffu, 0.004f, float(cv.starPrewarmScroll[0]));
    initStars(0.6f, 0.6f, 0xff0080ffu, 0.004f, float(cv.starPrewarmScroll[1]));
    initStars(0.5f, 0.5f, 0xc040ffffu, 0.004f, float(cv.starPrewarmScroll[2]));
    if (auto *s = dynamic_cast<Ship *>(ship))
        respawnPos = s->mapPos;
    respawnLanded = false;
    respawnRot = 0;
    {
        auto *p = new RestartPointer;
        p->spriteIndex = 0x2d;
        p->g = this;
        p->size = 0.05f;
        p->col.blue(0.4);
        p->respawn = respawnPos;
        p->screen = {respawnPos.x / 20.0 - 0.5, respawnPos.y / 30.0 - 0.5};
        addEntity(debris, p);
    }
    {
        auto *b = new PauseButton;
        b->size = 0.08f;
        b->glow = 0;
        b->spriteIndex = 7;
        b->g = this;
        b->pos = {0.0, updatedMenus ? menuLayout::PauseY : 0.08};
        b->scale.x = 2.4;
        b->baseSize = 0.08f;
        b->pressed = true;
        b->grow = 0;
        b->growRate = 0;
        addEntity(debris, b);
    }
    setMessage(1, 0x32, 0x12, 2, kGoMsg[0]);
    setMessage(2, 0x3c, 0x14, 4, cv.name);
    setMessage(3, 0x46, 0x16, 2, kGoMsg[1]);
    if (!tutorialShown && cavernIndex == 0) {
        for (int k = 0; k < 2; k++) {
            auto *o = new Osj;
            if (!iPad)
                o->pos = {0.0, k == 0 ? 0.42 : -0.4};
            else
                o->pos = {k == 0 ? -0.3 : 0.3, 0.35};
            o->spriteIndex = k == 0 ? 0x49 : 0x4a;
            o->g = this;
            tutorial = true;
            o->colourIndex = 1;
            o->life = 4.0f;
            o->size = 0.1f;
            o->scale = {1.0, 0.2};
            addEntity(debris, o);
        }
        tutorialShown = true;
    }
}

void PlayState::initStars(float w, float h, uint32_t rgba, float size,
                          float scroll) { // 0x55d28 (taurus)
    const int pre = int32_t(double(scroll / 25.0f) / 0.02);
    scrollDelta.y = -0.02; // +0x348 while the stars are run ahead
    const int cols = int(std::strlen(kTaurus[0])), rows = 19;
    const float dx = (1.0f / float(cols)) * w, dy = (1.0f / float(rows)) * h;
    float y = h * -0.5f + 0.0f;
    const float x0 = 0.0f + w * -0.5f;
    for (int r = 0; r < rows; r++) {
        float x = x0;
        for (int c = 0; c < cols; c++) {
            if (kTaurus[r][c] == '*') {
                auto *s = new DStar;
                s->presetting = true;
                s->pos = {double(x), double(y)};
                s->depth = float(rnd(0.5, 1.5));
                s->col.unpackRGBA(rgba);
                s->size = size;
                s->phase = 0;
                s->phaseRate = float(rnd(0.5, 2.0));
                s->spriteIndex = 3;
                s->g = this;
                for (int i = 0; i < pre; i++)
                    s->run();
                s->presetting = false;
                addEntity(list2dc, s);
            }
            x = x + dx;
        }
        y = y + dy;
    }
    scrollDelta.y = 0;
}

// ------------------------------------------------------------------ entities
void DStar::run() { // 0x573a0
    const double s = g->scrollDelta.y;
    pos.y = pos.y - double(depth) * s;
    if (pos.y < -0.55)
        pos.y = pos.y + 1.01;
    if (!(pos.y < 0.55))
        pos.y = pos.y - 1.01;
    phase = float(double(phase) + double(phaseRate) * s);
    if (presetting)
        return;
    col.a = double(float(0.3 + (double(std::cos(phase)) * 0.5 + 0.5) * 0.7));
    rot = phase * 5.0f;
    const double ap = double(std::fabs(phase));
    if (ap < 0.3) {
        const double k = 1.0 + (ap / -0.3 + 1.0) * 3.0;
        scale = {k, k};
    } else
        scale = {1.0, 1.0};
}

void RestartPointer::run() { // 0x595d4
    PlayState &G = *g;
    col = G.colours[0];
    col.a = G.runState == 4 ? 0.8 : 0.0;
    if (!G.paused)
        screen = {screen.x - G.scrollDelta.x, screen.y - G.scrollDelta.y};
    if (respawn.x != G.respawnPos.x || respawn.y != G.respawnPos.y) {
        respawn = G.respawnPos;
        screen = G.shipList->pos;
    }
    pos.x = screen.x;
    const float s = std::sin(
        float(double(float(int32_t(uint32_t(G.frameCount) & 0x3f)) / 63.0f) * 6.2831853072));
    pos.y = (screen.y - 0.08) + double(s) * 0.04;
}

void PauseButton::run() { // 0x5ed48, then gameButton::run 0x5e988
    PlayState &G = *g;
    if (G.pauseGlowRequested) {
        glow = 1.0f;
        G.pauseGlowRequested = false;
    }
    if (G.gameOver)
        dead = true;
    if (G.paused) {
        col.a = 0;
        return;
    }
    if (G.pauseRequested) {
        G.pauseRequested = false;
        if (G.pausePressed)
            G.pausePressed(); // vtable +0x18: onPressed
        glow = 0;
    }
    if (G.touchDown[0] && (G.updatedMenus ? menuLayout::PauseY : -0.08) < G.touchStartY[0] &&
        G.touchStartY[0] < (G.updatedMenus ? menuLayout::PauseY + .08 : 0.16)) {
        if (glow < 1.0f)
            glow = float(double(glow) + 0.1);
    } else if (0.0f < glow) {
        glow = float(double(glow) * 0.98);
    }
    if (1.0f < glow)
        glow = 1.0f;
    col.a = double(glow);
    gameButtonRun(*this, baseSize, pressed, grow, growRate, [&G] {
        if (G.pausePressed)
            G.pausePressed();
    });
}

int PauseButton::draw(EntityRenderer &r, Vec2 screen) {
    return r.drawEntity(*this, screen);
}

void Osj::run() { // 0x55620
    PlayState &G = *g;
    if (pauseGuide) {
        if (!G.paused) {
            dead = true;
            return;
        }
        life = 4;
    }
    if (G.updatedMenus) {
        const bool steer = spriteIndex == 0x49;
        if (!G.iPad)
            pos = {0, steer ? .43 : -.32};
        else
            pos = {((steer == (G.touchLayout != 2)) ? -.3 : .3), .41};
        if (pauseGuide && !G.touchGameplayEnabled) {
            col.a = 0;
            return;
        }
    } else {
        const bool flip = (G.prefsControl & 1) != 0;
        pos.x = (spriteIndex == 0x49) == !flip ? -0.3 : 0.3;
    }
    life = float(double(life) - 0.005);
    const float s = std::sin(life * 6.0f);
    size = float((double(s) * 0.5 + 0.5) * 0.1 + 0.1);
    if (life < 0.0f) {
        life = 0;
        dead = true;
        G.tutorial = false;
    }
    if (pauseGuide)
        size = .1f;
}

int Osj::draw(EntityRenderer &r, Vec2 screen) {
    if (pauseGuide && (!g->paused || !g->touchGameplayEnabled))
        return 0; // 0x552fc
    PlayState &G = *g;
    if (auto *sr = dynamic_cast<SpriteRenderer *>(&r)) {
        SpriteFields f;
        f.frame = 5;
        f.col = G.colours[colourIndex];
        f.col.a = 1.0f < life ? 1.0 : double(life);
        f.pos = {pos.x * screen.x, pos.y * screen.y};
        f.size = {screen.x * double(size), screen.y * double(size)};
        if (pos.x <= 0.2)
            f.size.x = f.size.x * 3.0;
        f.angle = 0;
        f.preserveFlags = true; // osj::draw does not write sprite +0x5c
        sr->drawSprite(f);
    }
    col = G.colours[4];
    const float a = life * 0.5f;
    col.a = 1.0 < double(a) ? 1.0 : double(a);
    return r.drawEntity(*this, screen);
}

void WavingBonusMinotaur::run() { // 0x53f1c
    float f = float(double(frame) + 0.2);
    frame = f;
    if (8.0f <= f) {
        f = f - 8.0f;
        frame = f;
    }
    spriteIndex = kBonusMinoFrames[int(f)];
    if (0 < leaving) {
        size = float(double(size) + 0.001);
        col.a = col.a - 0.05;
        if (col.a < 0.0) {
            col.a = 0;
            dead = true;
        }
        return;
    }
    if (0.04 < double(size))
        size = float(double(size) * 0.95);
    if (col.a < 1.0)
        col.a = col.a + 0.005;
    if (g->gameFinished)
        leaving = 1;
}

// ------------------------------------------------------------------ _draw's HUD (mode 2)
int playHudSprites(const PlayState &g, Vec2 sc, SpriteFields out[16]) { // 0x64868
    int n = 0;
    if (!(g.approachTimer < 200) || g.tutorial)
        return 0;
    auto bar = [&](double x, double y, float v, double k) {
        SpriteFields &f = out[n++];
        f.angle = 0;
        f.frame = 2;
        f.flags = 0;
        f.pos = {x * sc.x, y * sc.y};
        f.size = {double(v) * k * sc.x, 0.02 * sc.y};
        f.col.unpackRGBA(cavern::iclerp(0xff0000ffu, 0xff00ffu, v));
    };
    bar(-0.05, -0.425, g.shield, 0.45);
    bar(-0.15, -0.39, g.fuel, 0.55);
    float x = -0.4f;
    for (int i = 0; i < g.lives - 1 && n < 16; i++) {
        SpriteFields &f = out[n++];
        f.angle = 0;
        f.frame = 0x13;
        f.flags = 0;
        f.col = neon::colour(1.0, 1.0, 1.0, 1.0);
        f.pos = {double(x) * sc.x, -0.36 * sc.y};
        f.size = {0.04 * sc.x, 0.045 * sc.y};
        x = float(double(x) + 0.06);
    }
    return n;
}

} // namespace minos
