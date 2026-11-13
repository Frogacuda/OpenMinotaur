// game_states.h - Caverns of Minos theGame play states (Minos_armv7): theGame::run 0x6c020 (the
// per-frame input/colour/bookkeeping/list work and the play-state switch: 4 play, 5 bonuses,
// 6 entering a cavern), runUnpausedEntities 0x54310, awardLanding 0x74798, sequenceBonuses
// 0x5d380, goGameOver 0x602dc (state part), BGESetup 0x54e7c / BGEIntervalSetup 0x54bbc,
// the state part of newLevel 0x636f0, controlBlock::joyWatchdog, and the entities newLevel
// and sequenceBonuses create: dstar (run 0x573a0; initStars 0x55d28), restartPointer (run
// 0x595d4), pauseButton (run 0x5ed48 with gameButton::run 0x5e988), osj (run 0x55620,
// draw 0x552fc), wavingBonusMinotaur (run 0x53f1c). Translated from the decompilation with
// instruction-derived float/double boundaries retained explicitly.
#pragma once
#include "menu_art_ids.h"
#include <cmath>
#include "enemies.h"

namespace minos {

// The cellGrid calls theGame makes for its HUD text (Neon_Internals::cellGrid, Minos).
// printAt(x, y, s) prints with the grid's current colour (+0x2a8c); the colour overloads set
// it first. The grid's own state (palette copies, flags, shake offsets) is set through the
// remaining calls.
struct Hud {
    virtual ~Hud() = default;
    virtual void setPrintColour(const neon::colour &c) = 0; // memcpy -> +0x2a8c
    virtual void printAt(int x, int y, const char *s) = 0;  // 0x42d90
    virtual void printAt(int x, int y, const char *s, const neon::colour &c) {
        setPrintColour(c);
        printAt(x, y, s);
    } // 0x42f8c
    virtual void printAtRgba(int x, int y, const char *s, uint32_t rgba) { // 0x43350
        neon::colour c;
        c.unpackRGBA(rgba);
        setPrintColour(c);
        printAt(x, y, s);
    }
    virtual void cPrintAt(int y, const char *s, const neon::colour &c) = 0; // 0x44800 (centred)
    virtual void cll(int y) = 0;
    virtual void cls() = 0;
    virtual void clattr() = 0;
    virtual void setSize(int cols, int rows) {
        (void)cols;
        (void)rows;
    }
    // getPos(touch start position) + lpeek: the label under the touch (menus); 0 in play.
    virtual int labelAt(double x, double y) {
        (void)x;
        (void)y;
        return 0;
    }
    // run's direct writes: palette copies (+0x1f70 + 0x50 k, 11 colours), +0x297e input flip,
    // +0x3104 paused, the shake offsets +0x2b1c / +0x2b24.
    virtual void setPalette(const neon::colour *c, int n) {
        (void)c;
        (void)n;
    }
    virtual void setFlags(bool flipped, bool paused) {
        (void)flipped;
        (void)paused;
    }
    virtual void setShake(double x, double y) {
        (void)x;
        (void)y;
    }
    // QuickMsg(text, pos, vel, -1, slot, colour, 0.25, 0) (goGameOver / title / announceCavern)
    virtual void quickMsg(const char *s, Vec2 pos, Vec2 scale, int slot, int colour) {
        (void)s;
        (void)pos;
        (void)scale;
        (void)slot;
        (void)colour;
    }
    // The menus' calls and direct writes (menu_states.cpp): goA8 0x485a8, hlin_a 0x43dbc, clm
    // 0x430ac, the cursor flag +0x297f, the label mode +0x297c and label +0x2a84 (hlin_a and
    // printAt tag the cells they write while the mode is set; getPos/lpeek read them back),
    // and a QuickMsg line's font +0xb8 / state +0x98 (state < 0: not written).
    virtual void goA8(int c, int r, int bx, int by) {
        (void)c;
        (void)r;
        (void)bx;
        (void)by;
    }
    virtual void hlin(int bg, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) {
        (void)bg;
        (void)c0;
        (void)c1;
        (void)x0;
        (void)x1;
        (void)y;
        (void)alpha;
    }
    virtual void clm() {}
    virtual void setCursor(bool on) {
        (void)on;
    }
    virtual void setLabelMode(bool on) {
        (void)on;
    }
    virtual void setLabel(int label) {
        (void)label;
    }
    virtual void setRowDisplacement(int row, float cells) = 0; // row header +4
    virtual void setLine(int row, int font, int state) {
        (void)row;
        (void)font;
        (void)state;
    }
};

struct DStar : PlayEntity { // dstar (initStars), vtable 0x1ed11c, 0x180 bytes; run 0x573a0
    void run() override;
    float depth = 0;         // +0x170 rand(0.5, 1.5): parallax against the scroll
    float phase = 0;         // +0x174
    float phaseRate = 0;     // +0x178 rand(0.5, 2.0)
    bool presetting = false; // +0x17c set while initStars runs it ahead
};

struct RestartPointer : PlayEntity { // vtable 0x1ed1dc, 0x18c bytes; run 0x595d4
    void run() override;
    Vec2 respawn; // +0x16c / +0x174 the respawn point it shows (cells, lines)
    Vec2 screen;  // +0x17c / +0x184 screen position (scrolls with the map)
};

struct PauseButton
    : PlayEntity { // pauseButton : gameButton, vtable 0x1ed2f0, 0x184 bytes; run 0x5ed48
    void run() override;
    int draw(EntityRenderer &r, Vec2 screen) override; // gameButton::draw 0x5416c (entity::draw)
    float baseSize = 0.08f;                            // +0x16c
    bool pressed = false;                              // +0x170
    float grow = 0, growRate = 0; // +0x174, +0x178 (gameButton::run: grow += growRate when set)
    float glow = 0;               // +0x180 alpha target (pauseButton)
};

// The three small gameButton subclasses share only the original button mechanics.
// Their actions remain separate from the platform's controller focus handling.
struct PauseControl : PlayEntity {
    enum class Action { Resume, Quit, Handed, Scores, Exit, Touch, Tilt };
    explicit PauseControl(Action a) : action(a) {}
    void run() override;
    void activate();
    int menuLabel() const {
        return action == Action::Touch  ? menuArt::Touch1
               : action == Action::Tilt ? menuArt::TiltOn
                                        : spriteIndex;
    }
    Action action;
    float baseSize = 0.08f;
    bool pressed = true;
    float grow = 0, growRate = 0;
};

struct Osj
    : PlayEntity { // osj (cavern 0 hints), vtable 0x1ece1c, 0x174 bytes; run 0x55620, draw 0x552fc
    void run() override;
    int draw(EntityRenderer &r, Vec2 screen) override;
    bool pauseGuide = false;
    float life = 4.0f;       // +0x16c (newLevel writes the bytes of 4.0f)
    int32_t colourIndex = 1; // +0x170 colours[] index of the hint quad
};

struct TitleStar : PlayEntity { // titleStar (announceCavern), vtable 0x1ed21c, 0x170 bytes; run =
                                // star::run 0x53e9c
    void run() override;
    void randomiseStuff(); // 0x560f0
    float spin = 0;        // +0x16c added to rot
};

struct WavingBonusMinotaur
    : PlayEntity { // vtable 0x1ecf7c, 0x174 bytes; run 0x53f1c (sequenceBonuses)
    void run() override;
    float frame = 0;     // +0x16c 0..8
    int32_t leaving = 0; // +0x170
};

// theGame::_draw's play-state HUD (mode 2): the shield and fuel bars and the spare lives,
// as SpriteFields in screen units (0x64868). Returns the number of sprites written to out.
int playHudSprites(const PlayState &g, Vec2 screen, SpriteFields out[16]);

// gameButton::run 0x5e988 (pauseButton, openfeintButton): grow, colour, the touch test and
// the size easing; onPressed is called when a touch first lands on the button.
template <class OnPressed>
void gameButtonRun(PlayEntity &b, float baseSize, bool &pressed, float &grow, float growRate,
                   OnPressed onPressed) {
    PlayState &G = *b.g;
    if (growRate != 0.0f)
        grow = grow + growRate;
    if (!G.paused) {
        b.col.r = 1.0;
        b.col.g = 1.0;
        b.col.b = 1.0;
    }
    float sz = b.size;
    if (!G.iPad)
        sz = float(double(sz) * 1.2);
    const double hx = std::fabs(b.scale.x) * double(sz), hy = std::fabs(b.scale.y) * double(sz);
    bool over = false;
    for (int i = 0; i < 4 && !over; i++) {
        if (!G.touchDown[i])
            continue;
        const double dx = G.touchPos[i][0] - b.pos.x, dy = G.touchPos[i][1] - b.pos.y;
        if (std::fabs(dx) < hx && std::fabs(dy) < hy)
            over = true;
    }
    if (!over)
        pressed = false;
    else if (!pressed) {
        pressed = true;
        onPressed();
    }
    if (baseSize < b.size)
        b.size = float(double(b.size) * 0.9);
}

// The menus (menu_states.cpp).
extern const char *const kQuestNames[8]; // questNames 0x23b010
extern const char *const kRankNames[4];  // rankNames 0x23b130 (the ship kinds / difficulties)
extern const char *const kTips[30];      // tips 0x23b034: 6 entries of 5 lines (dynamicGameOver)

// gameButton (0x5e988) as pauseButton and openfeintButton use it: grows by growRate, white
// unless paused, pressed (onPressed, vtable +0x18) when a touch first lands inside
// |scale| * size (x 1.2 on iPhone), size eases back to baseSize.
struct OpenFeintButton
    : PlayEntity { // openfeintButton : gameButton, vtable 0x1ed260, 0x180 bytes; run 0x5ec54
    void run() override;
    int draw(EntityRenderer &r, Vec2 screen) override; // 0x56da8
    float baseSize = 0.08f;                            // +0x16c
    bool pressed = false;                              // +0x170
    float grow = 0, growRate = 0;                      // +0x174, +0x178
    bool kill = false; // +0x17c pause menu sets it; retires the button once not paused
};

// theGame::_draw's title sprites (modes 0, 1, 4): doTitle 0x56e98 (the logo for +0x9e4, tilted
// by the phase sine) and doTitleFlash 0x56260. False: doTitle's kind is out of range and the
// sprite keeps the previous draw's size, frame and colour (not modelled).
bool titleLogoSprite(const PlayState &g, Vec2 screen, SpriteFields &out);
void titleFlashSprite(Vec2 screen, SpriteFields &out);

} // namespace minos
