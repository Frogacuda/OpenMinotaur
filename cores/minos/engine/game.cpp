// game.cpp - Caverns of Minos theGame for the minos core (see game.h for status).
#include "game.h"
#include "neon_audio.h"
#include "neon_resources.h"
#include "cavern_draw_emit.h"
#include "neon/entity_list.h"
#include <cstdlib>
#include <cstring>

namespace NEON_GAME {
namespace Neon_Internals {

using namespace minos::cavern;

// The cellGrid side of theGame's HUD (the copies of the Minos cellGrid members it writes).
struct theGame::CoreHud final : minos::Hud {
    theGame *g;
    explicit CoreHud(theGame *game) : g(game) {}
    cellGrid *grid() const {
        return g->grid;
    }
    void setPrintColour(const neon::colour &c) override {
        if (grid())
            grid()->printColour = c;
    }
    void printAt(int x, int y, const char *s) override {
        if (grid())
            grid()->printAt(x, y, s);
    }
    void cPrintAt(int y, const char *s, const neon::colour &c) override {
        if (!grid())
            return;
        grid()->printColour = c;
        grid()->cPrintAt(y, s);
    }
    void cll(int y) override {
        if (grid())
            grid()->cll(y);
    }
    void cls() override {
        if (grid())
            grid()->cls();
    }
    void clattr() override {
        if (grid())
            grid()->clattr();
    }
    void setSize(int c, int r) override {
        if (grid())
            grid()->setSize(c, r);
    }
    void setPalette(const neon::colour *c, int n) override {
        if (grid())
            for (int i = 0; i < n && i < 16; i++)
                grid()->palette2[i] = c[i]; // +0x1f70 + 0x50 k
    }
    void setFlags(bool flipped, bool paused) override {
        if (grid()) {
            grid()->f297e = flipped;
            grid()->noProp = paused;
        }
    }
    void setShake(double x, double y) override {
        if (grid()) {
            grid()->scrollX = x;
            grid()->scrollY = y;
        }
    }
    void quickMsg(const char *s, minos::Vec2 pos, minos::Vec2 sc, int slot, int colour) override {
        if (grid())
            grid()->QuickMsg(s, vec2d(pos.x, pos.y), vec2d(sc.x, sc.y), -1, slot, colour, 0.25f,
                             0.0f);
    }
    int labelAt(double x, double y) override {
        // Host touches may extend into letterboxes; the original view did not.
        return grid() && x >= -0.5 && x < 0.5 && y >= -0.5 && y < 0.5
                   ? grid()->getLabel(vec2d(x, y))
                   : 0;
    } // getPos + lpeek
    void goA8(int c, int r, int bx, int by) override {
        if (grid())
            grid()->goA8(c, r, bx, by);
    }
    void hlin(int bg, uint32_t c0, uint32_t c1, int x0, int x1, int y, float alpha) override {
        if (grid())
            grid()->hlin_a(bg, c0, c1, x0, x1, y, alpha);
    }
    void clm() override {
        if (grid())
            grid()->clm();
    }
    void setCursor(bool on) override {
        if (grid())
            grid()->cursorOn = on;
    } // +0x297f
    void setLabelMode(bool on) override {
        if (grid())
            grid()->labelMode = on;
    } // +0x297c
    void setLabel(int label) override {
        if (grid())
            grid()->curLabel = label;
    } // +0x2a84
    void setRowDisplacement(int row, float cells) override {
        if (grid() && row >= 0 && row <= grid()->rowsN)
            grid()->rowX[row] = cells;
    }
    void setLine(int row, int font, int state) override { // lines[row] +0xb8 / +0x98
        if (!grid() || row < 0 || row >= cellGrid::kMaxCells || !grid()->lines[row])
            return;
        grid()->lines[row]->font = font;
        if (state >= 0)
            grid()->lines[row]->state = state;
    }
};

void theGame::Play::playSample(int id, int vol) {
    NEON_GAME::AudioSystemPlaySample(id, vol, 0);
}

theGame::theGame()
    : hud_(std::make_unique<CoreHud>(this)), map_(std::make_unique<StripMap>()),
      profile_(std::make_unique<WallProfile>()) {
    // Constructor 0x59c48 values used so far: bgLayer (+0x8) -1, state 1, volume 8, frameCount
    // 0, cavernIndex 0, shipKind 0; reset(0).
    play.rand = &rand_; // play.hud: set once the cellGrid is attached (run's +0x440 test)
    play.updatedMenus = true;
    play.map = map_.get();
    play.flash = &flash_;
    play.runState = state;
    // sprite::getSize 0xfb2c returns the catalog's raw pixel width/height.
    // getScale and the launch factory form their own aspect ratios from it.
    play.spriteSize = [](int frame) {
        sprite art;
        art.frame = frame;
        const auto size = art.getSize();
        return minos::Vec2{size.x, size.y};
    };
    play.createShip = [this]() -> minos::Entity * {
        // newLevel 0x636f0 selects distinct classes by +0x9e4.
        minos::Ship *ship = nullptr;
        switch (play.shipKind) {
        case 0:
            ship = new minos::Ship;
            break;
        case 1:
            ship = new minos::VicLander;
            break;
        case 2:
            ship = new minos::Taxi;
            break;
        case 3:
            ship = new minos::AsteroidsSpaceship;
            break;
        default:
            return nullptr;
        }
        ship->init(play);
        play.addEntity(play.shipList, ship);
        return ship;
    };
    play.newLevelHook = [this] {
        cavernIndex = play.cavernIndex;
        newLevel();
    };
    play.pausePressed = [this] { play.openPause(); };
    play.tipIndex = -1; // +0x394
    play.muted = true;  // +0x2ad
    play.drawMode = -1;
    play.bgLayer = -1;             // +0x4 / +0x8
    play.awardEligibilityGate = 1; // +0x488
    devEntry_ = std::getenv("MINO_MINOS_CAVERN") || std::getenv("MINO_MINOS_BONUS") ||
                std::getenv("MINO_MINOS_LASTLIFE") ||
                std::getenv("MINO_MINOS_GOAL"); // development only
    reset(0);
}

theGame::~theGame() {
    reset(0);
}

void theGame::reset(int keep) {
    play.resetLists(keep); // 0x5654c
}

void theGame::addDebris(minos::Entity *e) {
    neon::entities::prepend(play.debris, *e);
}

void theGame::launch(char ch, minos::Vec2 pos, int column, int row) {
    LaunchContext ctx;
    ctx.cavern = &kCavernDefinitions[cavernIndex];
    ctx.rand = &rand_;
    ctx.spriteSize = play.spriteSize;
    if (auto req = minos::cavern::launch(*map_, ctx, ch, pos, column, row)) {
        launches++;
        play.launch(*req);
    }
}

void theGame::newLevel() {
    // 0x636f0, map part: reset, the stm's flash tables and palette, the HUD grid cleared,
    // zoneLength/sectorLength, initPlanetMap. Then the state part (PlayState::newLevelState:
    // counters, flash timers, BGESetup, the ship, the per-cell rand table, stars, pointer,
    // pause button, messages, cavern-0 hints).
    reset(0);
    const CavernDefinition &cv = kCavernDefinitions[cavernIndex];
    map_->bind(this, profile_.get(), &flash_, &word9dc_);
    map_->installFlashTables();
    for (int i = 0; i < 16; i++)
        play.stmPalette[i].unpackRGBA(cv.palette[i]);
    if (grid) {
        grid->cls();
        grid->clattr();
    }
    map_->setCavern(&cv, cv.zoneLength);                   // zoneLength / sectorLength globals
    buildWallProfile(cv, map_->sectorLength(), *profile_); // initPlanetMap
    camera = {0, 0};
    play.cavern = &cv;
    play.cavernIndex = cavernIndex;
    play.volume = volume;
    // The source has one +0x9e4 field. Menus update PlayState directly; this
    // presentation mirror must not overwrite the selected difficulty at launch.
    shipKind = play.shipKind;
    play.newLevelState();
}

void theGame::startGame() {
    // Development entry (MINO_MINOS_CAVERN / _BONUS / _LASTLIFE / _GOAL set): straight into a
    // cavern as the select state's start (case 3: reset, counters) and announceCavern's state
    // 6 -> 4 transition would, skipping the title and select screens.
    if (const char *c = std::getenv("MINO_MINOS_CAVERN"))
        cavernIndex = std::atoi(c) % 21; // development only
    else
        cavernIndex = 0;
    play.lives = 3;
    play.scoreTotal = 0;
    play.minosSavedTotal = 0;
    play.word478 = 0;
    play.muted = false;
    play.gameFinished = false;
    gridPending_ = true; // setSize(0x14, 0x1e) + cls once the grid is attached
    play.bgLayer = 2;
    play.drawMode = 2;
    play.runState = 4;
    newLevel();
    play.buttonLatch = true; // the starting press does not count
    if (std::getenv("MINO_MINOS_BONUS")) {
        play.approachTimer = 1;
        play.minosSaved = 3;
    } // development: start the cavern-end sequence
    if (std::getenv("MINO_MINOS_LASTLIFE")) {
        play.lives = 1;
        play.shield = 0;
    } // development: game over at once
    if (std::getenv("MINO_MINOS_GOAL")) {
        play.flag203808 = true;
        play.byte31e = true;
    } // development: as after the goal (BGEs)
}

void theGame::run() {
    if (!play.hud && grid) {
        play.hud = hud_.get();
        grid->typeSkip = &play.touchDown[0];
    }
    if (devEntry_) {
        devEntry_ = false;
        startGame();
    }
    if (gridPending_ && grid) {
        grid->setSize(0x14, 0x1e);
        grid->cls();
        gridPending_ = false;
    }
    play.inputFlipped = inputFlipped;
    play.runFrame();
    sync();
}

void theGame::sync() {
    drawMode = play.drawMode;
    bgLayer = play.bgLayer;
    frameCount = play.frameCount;
    state = play.runState;
    cavernIndex = play.cavernIndex;
    shipKind = play.shipKind;
    running = play.running;
    for (int i = 0; i < 3; i++)
        fbColour[i] = play.bgColour[i];
    fbColour[3] = play.bgBright;
    fbScaleX = play.bgFade[0] != 0 ? play.bgFade[0] : 1.0f;
    fbScaleY = play.bgFade[1] != 0 ? play.bgFade[1] : 1.0f;
    camera = {0, play.cameraY}; // +0x320 / +0x328 (_bgdraw reads them)
}

void theGame::setPadInput(double stickX, bool thrustButton) {
    uint32_t b = thrustButton ? 0x10u : 0u;
    if (stickX < -0.3)
        b |= 8; // left: run() ramps +0x284 down
    else if (stickX > 0.3)
        b |= 2; // right: up
    play.prefsButtons = b;
}

void theGame::setMapColour(sprite &s, const QuadColour &c) const {
    if (c.fromSequence)
        s.col.unpackRGBA(c.rgba);
    else
        s.col = play.stmPalette[c.paletteIndex &
                                15]; // original reads stm+4+80*i unchecked (shipped: 0..4)
}

namespace {
// entity::draw 0x4113c (as Minotron's: colour, flash, angle, frame = sprite + 2, width/height
// = (size + size) * screen * scale, position * screen, z, draw delay), and the sprite-field
// drawing osj and the HUD bars use.
struct CoreRenderer final : minos::SpriteRenderer {
    sprite *s;
    int boundPage = 0;
    explicit CoreRenderer(sprite *sp) : s(sp) {}
    ~CoreRenderer() override {
        bindPage(0);
    }
    void bindPage(int page) {
        if (page == boundPage || !s->batchStack)
            return;
        s->setupPurge();
        s->batchCount = 0;
        s->batchPtr = s->batchStack->verts;
        auto &render = s->batchStack->render;
        render.SetSourceFromFont(-1, -1, -1);
        render.SetSourceFromRTs(-1, -1, -1);
        render.SetSourceFromLiterals(page, -1, -1);
        boundPage = page;
    }
    void selectSpritePage(int frame) {
        const auto *catalog = renderSession().resources;
        const auto *entry = catalog && frame >= 0 ? catalog->sprite(frame) : nullptr;
        bindPage(entry ? entry->page : 0);
    }
    int drawEntity(minos::Entity &e, minos::Vec2 sz) override {
        if (e.spriteIndex < 0)
            return 0;
        s->col = e.col;
        if (e.flashFrames != 0) {
            e.flashFrames--;
            s->col.b = 0;
            const float t = float((e.flashFrames) & 7) / 7.0f;
            s->col.g = double(t);
            s->col.r = double(t) * 0.5 + 0.5;
        }
        s->angle = double(e.rot);
        s->frame = e.spriteIndex + kLitBase;
        s->width = double(e.size + e.size) * sz.x * e.scale.x;
        s->height = double(e.size + e.size) * sz.y * e.scale.y;
        s->pos = vec2d{e.pos.x * sz.x, e.pos.y * sz.y};
        s->zOffset = e.zOffset;
        if (e.drawDelay > 0) {
            e.drawDelay--;
            return 0;
        }
        selectSpritePage(s->frame);
        s->setupChecking();
        return 6;
    }
    int drawSprite(const minos::SpriteFields &f) override {
        s->pos = vec2d{f.pos.x, f.pos.y};
        s->width = f.size.x;
        s->height = f.size.y;
        s->angle = f.angle;
        s->frame = f.frame;
        if (f.orFlags)
            s->flags |= f.orFlags;
        else if (!f.preserveFlags)
            s->flags = f.flags;
        s->col = f.col;
        selectSpritePage(s->frame);
        s->setupChecking();
        return 6;
    }
};
} // namespace

int theGame::_bgdraw(sprite *s, vec2d screen) {
    // 0x7d12c. Mode 0 (title boot screen): the stm. Mode 1 (title, entering a cavern): stm
    // palette [1] = colours[1], the stars (+0x2dc), then the stm (the scenery titleScenery
    // laid out). Mode 2 (play): +0x31e cleared unless +0x203808, the visible lines refilled
    // (sector +0x207914), stm palette [1] = colours[3] and [2] = colours[0], the stars, then
    // the map while the cavern-end counter is below 200. Mode 3 (bonuses, select, info, game
    // over): the stars.
    CoreRenderer r(s);
    const minos::Vec2 sz{screen.x, screen.y};
    auto drawStm = [&] {
        const MapDrawPass pass = drawMap(*map_, {screen.x, screen.y});
        emitMapDrawPass(pass, *s, [this](sprite &sp, const QuadColour &c) { setMapColour(sp, c); });
    };
    if (bgLayer == 0) {
        drawStm();
    } else if (bgLayer == 1) {
        play.bgdrawState(camera);
        for (minos::Entity *e = play.list2dc; e; e = e->next)
            e->draw(r, sz);
        s->angle = 0;
        drawStm();
    } else if (bgLayer == 2) {
        play.bgdrawState(camera);
        for (minos::Entity *e = play.list2dc; e; e = e->next)
            e->draw(r, sz);
        s->angle = 0;
        if (play.approachTimer < 200)
            drawStm();
    } else if (bgLayer == 3) {
        for (minos::Entity *e = play.list2dc; e; e = e->next)
            e->draw(r, sz);
    }
    s->setupPurge();
    return 0;
}

int theGame::_draw(sprite *s, vec2d screen) {
    // 0x64868. Mode 0 (boot screen): the stars, then doTitleFlash. Mode 1 (title, entering a
    // cavern): doTitle. Modes 2 (play), 3 (bonuses, game over, info) and 4 (select): lists
    // +0x2d0, +0x2d8, +0x2e8, +0x2e4, +0x2e0, +0x2d4; mode 2 then the shield and fuel bars and
    // the spare lives, mode 4 doTitle. Mode 5 (game over tips) draws nothing.
    CoreRenderer r(s);
    const minos::Vec2 sz{screen.x, screen.y};
    minos::SpriteFields f;
    if (drawMode == 0) {
        for (minos::Entity *e = play.list2dc; e; e = e->next)
            e->draw(r, sz);
        s->angle = 0;
        minos::titleFlashSprite(sz, f);
        r.drawSprite(f);
    } else if (drawMode == 1) {
        s->angle = 0;
        if (minos::titleLogoSprite(play, sz, f))
            r.drawSprite(f);
    } else if (drawMode >= 2 && drawMode <= 4) {
        for (minos::Entity *lst : {play.shots, play.list2d8, play.bounceList, play.touchList,
                                   play.debris, play.shipList})
            for (minos::Entity *e = lst; e; e = e->next)
                e->draw(r, sz);
        if (drawMode == 2) {
            minos::SpriteFields hf[16];
            const int n = minos::playHudSprites(play, sz, hf);
            for (int i = 0; i < n; i++)
                r.drawSprite(hf[i]);
        } else if (drawMode == 4) {
            if (minos::titleLogoSprite(play, sz, f))
                r.drawSprite(f);
        }
    }
    s->setupPurge();
    return 0;
}

} // namespace Neon_Internals
} // namespace NEON_GAME
