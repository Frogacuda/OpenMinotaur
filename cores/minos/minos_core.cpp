// Minos host binding. Each 60 Hz tick advances WaveGen, game logic and effect
// stacks before presenting the surface. Local records and achievement tracking
// replace the retired online services; preferences retain the original blob layout.
#include "neon/backend_setup.h"
#include "startup_screen.h"
#include "core_api.h"
#include "minos_game_host.h"
#include "engine/game.h"
#include "enemies.h"
#include "engine/neon_audio.h"
#include "engine/neon_device.h"
#include "engine/neon_modules.h"
#include "engine/neon_resources.h"
#include "engine/neon_sprite.h"
#include "engine/neon_synth.h"
#include "neon/sample_audio.h"
#include "touch_input.h"
#include "motion_input.h"
#include "neon/controller_session.h"
#include "grid_menus.h"
#include "local_records.h"
#include "loading_art.h"
#include "menu_art.h"
#include "menu_highlight.h"
#include <pugixml.hpp>
#include <sstream>

#include <glad/gl.h>
#include <cmath>
#include "neon/input.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>

namespace NEON_GAME {
namespace Neon_Internals {
WaveState *Waves = nullptr;
}
} // namespace NEON_GAME

namespace mino {
namespace {

using namespace NEON_GAME::Neon_Internals;

constexpr int kLogicalW = 320, kLogicalH = 480;
constexpr double kTick = 1.0 / 60.0;
using Sample = neon::audio::Sample;
using SampleMixer = neon::audio::SampleMixer;
using neon::audio::parseWav;
SampleMixer *gMixer = nullptr;

// Permission-cleared original launch art, using the ordinary texture loader.
// Older imports omitted Default.png, so startup does not require reimporting.
class MinosAssets final : public Host {
  public:
    Host *target = nullptr;
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        if (name == "__minos_launch.png") {
            out.assign(std::begin(minos::LoadingArt), std::end(minos::LoadingArt));
            return true;
        }
        if (name == "__minos_menu.png") {
            out.assign(std::begin(minos::MenuAtlas), std::end(minos::MenuAtlas));
            return true;
        }
        if (!target->readAsset(name, out))
            return false;
        if (name == "NeonResources.xml") {
            // Append a normal literal/page and sprite-map entries in memory.
            // Imported artwork/catalogs are never overwritten or reimported.
            pugi::xml_document doc, extra;
            if (!doc.load_buffer(out.data(), out.size(), pugi::parse_default,
                                 pugi::encoding_utf8) ||
                !extra.load_string(minos::MenuCatalog))
                return false;
            auto root = doc.child("NeonResources");
            auto extension = extra.child("NeonResources");
            auto map = root.child("Spritemaps").child("Spritemap");
            if (map.attribute("entries").as_int() != 96) {
                target->log("minos: unexpected base sprite catalog");
                return false;
            }
            root.child("Literals").append_copy(extension.child("Literals").first_child());
            root.child("SpritePages").append_copy(extension.child("SpritePages").first_child());
            for (auto entry : extension.child("Spritemaps").child("Spritemap").children("Spentry"))
                map.append_copy(entry);
            map.attribute("entries").set_value(105);
            root.attribute("literals").set_value(3);
            root.attribute("spritepages").set_value(3);
            std::ostringstream xml;
            doc.save(xml);
            const auto text = xml.str();
            out.assign(text.begin(), text.end());
        }
        return true;
    }
    std::string saveDir() const override {
        return target->saveDir();
    }
    void log(const char *format, ...) override {
        char text[2048];
        va_list args;
        va_start(args, format);
        std::vsnprintf(text, sizeof text, format, args);
        va_end(args);
        target->log("%s", text);
    }
    std::string adaptShader(const std::string &s, int stage) const override {
        return target->adaptShader(s, stage);
    }
    void lockAudio() override {
        target->lockAudio();
    }
    void unlockAudio() override {
        target->unlockAudio();
    }
    void requestQuit() override {
        target->requestQuit();
    }
    void unlockAchievement(const std::string &id) override {
        target->unlockAchievement(id);
    }
    bool presentStartupFrame() override {
        return target->presentStartupFrame();
    }
};

class MinosCore final : public Core {
    mino::StartupScreen startup_;

  public:
    explicit MinosCore(neon::ShaderCycleState &history) : shaderHistory_(history) {
        renderState_.shaderCycles = history;
    }
    const CoreInfo &info() const override;

    bool init(Host &host, const SurfaceInfo &s) override {
        if (!startup_.begin(host, s))
            return false;
        auto bind = renderState_.bind();
        host_ = &host;
        assets_.target = &host;
        surface_ = s;
        std::vector<uint8_t> xml;
        if (!assets_.readAsset("NeonResources.xml", xml)) {
            host.log("minos: NeonResources.xml missing or unsupported");
            return false;
        }
        std::string err;
        if (!res_.load(std::string(xml.begin(), xml.end()), &err)) {
            host.log("minos: resources: %s", err.c_str());
            return false;
        }
        renderState_.attachResources(res_);
        waves_ = WaveState{};
        Waves = &waves_;
        dev_.setHost(&assets_);
        dev_.Init(0, kLogicalW, kLogicalH, s.framebuffer, s.width, s.height, 0);
        dev_.InitAllShaders();
        dev_.SetupDefaultProjection(-1, -1, PROJ_ORTHO);
        renderState_.attachDevice(dev_);
        neon::backend::initializeDrawing(ctx_, dev_, {0, 20000}, {1, 24000},
                                         neon::backend::DrawingOrder::AllocateBothThenConfigure,
                                         neon::backend::BufferFailure::IgnoreReturn);
        renderState_.attachDrawing(ctx_);
        loadingLiteral_ = 0;
        dev_.InitLiteralTextures(1, {"__minos_launch.png"});
        if (!dev_.literal(0))
            return false;
        std::vector<Sample> samples;
        for (const SampleEntry &e : res_.samples()) {
            Sample smp;
            smp.maxInstances = std::max(1, e.maxInstances);
            smp.priority = e.priority;
            std::vector<uint8_t> wav;
            if (!host.readAsset(e.file, wav) || !parseWav(wav, smp))
                host.log("minos: sample %s not loaded", e.file.c_str());
            samples.push_back(std::move(smp));
        }
        mixer_.setSamples(std::move(samples));
        gMixer = &mixer_;
        auto literals = res_.literals();
        loadingLiteral_ = int(literals.size());
        literals.push_back("__minos_launch.png");
        dev_.InitLiteralTextures(int(literals.size()), literals);
        std::vector<uint8_t> gridFile;
        if (!host.readAsset("testGrid.ox", gridFile)) {
            host.log("minos: testGrid.ox missing");
            return false;
        }
        std::vector<superSaver> cells;
        if (!loadGrid(gridFile, cells) || cells.empty() || !cells[0].hasDNA) {
            host.log("minos: testGrid.ox unreadable");
            return false;
        }
        effect_ = cells[0].createEffectFromDNA(&dev_, 0, &gameHost_);
        if (!effect_ || !gameHost_.hasGame()) {
            host.log("minos: game effect not created: %s", cells[0].error.c_str());
            return false;
        }
        auto *last = neon::backend::lastStack(*effect_);
        ctx_.effect = effect_;
        ctx_.effectStack = last ? last->stackIndex : 0;
        loadSave();
        loadControls();
        const bool haveRecords = records_.load(host.saveDir());
        host.observeOriginalAchievements(records_.achievements);
        if (!haveRecords) {
            records_.importPersonalBests(game()->play.prefs);
            saveRecords();
        }
        scoreGrid_.width = kLogicalW;
        scoreGrid_.height = kLogicalH;
        scoreGrid_.fontY = 0xa2; // Minos's Atari character set (goA8).
        scoreGrid_.setSize(40, 24);
        return true;
    }

    void resize(const SurfaceInfo &s) override {
        startup_.resize(s);
        auto bind = renderState_.bind();
        surface_ = s;
        dev_.SetBackbuffer(s.framebuffer, kLogicalW, kLogicalH, s.width, s.height);
    }

    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        auto bind = renderState_.bind();
        if (!suspended_)
            if (auto *g = game())
                g->play.lateralTilt = motion_.sample(*host_, g->inputFlipped, g->state == 4);
        acc_ += suspended_ ? 0.0 : std::min(dt, 0.25);
        int ticks = 0;
        while (acc_ >= kTick && ticks < 4) {
            acc_ -= kTick;
            tick();
            ticks++;
        }
        if (ticks == 4)
            acc_ = 0;
        present();
    }

    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        auto *g = game();
        if (!g || suspended_ || surface_.width <= 0 || surface_.height <= 0)
            return;
        controllerFocus_ = false;
        minos::Vec2 p{double(e.x) / surface_.width - 0.5, double(e.y) / surface_.height - 0.5};
        if (scorePage_ != ScorePage::Closed) {
            if (e.phase == TouchPhase::Down && scoreTouchId_ < 0) {
                scoreTouchId_ = e.id;
                scoreTouchLabel_ = scoreGrid_.getLabel({p.x, p.y});
            } else if (e.phase == TouchPhase::Cancel) {
                scoreTouchId_ = -1;
                scoreTouchLabel_ = 0;
            } else if (e.phase == TouchPhase::Up && e.id == scoreTouchId_)
                scoreTouchId_ = -1;
            return;
        }
        if (g->inputFlipped) {
            p.x = -p.x;
            p.y = -p.y;
        }
        switch (e.phase) {
        case TouchPhase::Down:
            touch_.down(g->play, e.id, p);
            break;
        case TouchPhase::Move:
            touch_.move(g->play, e.id, p);
            break;
        case TouchPhase::Up:
            touch_.up(g->play, e.id);
            break;
        case TouchPhase::Cancel:
            touch_.cancel(g->play);
            break;
        }
    }
    // Controller actions enter the original hardware-button path directly:
    // stick x / d-pad -> the original's d-pad bits 8 / 2 (theGame::run ramps the analog axis
    // +0x284), South / right shoulder / right trigger -> the thrust bit 0x10 (prefs +0xc84).
    void pads(const PadState *pads, int count) override {
        if (startup_.pads(pads, count))
            return;
        controllers_.captureLatched(pads, count);
        if (!controllers_.connected())
            controllerFocus_ = false;
        if (host_->keyboardNavigationAvailable())
            controllerFocus_ = true;
    }
    void mixAudio(float *out, int frames) override {
        mixer_.mix(out, frames);
    }
    void pause(bool paused) override {
        suspended_ = paused;
        acc_ = 0;
        if (paused) {
            if (host_)
                motion_.release(*host_);
            mixer_.stopAll();
            controllers_.clear();
            inputArmed_ = false;
            if (auto *g = game()) {
                g->play.lateralTilt = 0;
                touch_.cancel(g->play);
                if (g->state == 4 && !g->play.paused)
                    g->play.openPause();
            }
        }
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        auto *g = game();
        if (!g)
            return false;
        inputArmed_ = false;
        if (scorePage_ != ScorePage::Closed) {
            if (scorePage_ == ScorePage::Name)
                finishScoreName();
            else
                closeScores();
            return true;
        }
        if (g->play.paused) {
            g->play.paused = false;
            touch_.cancel(g->play);
            return true;
        }
        if (g->state == 4 || g->state == 5 || g->state == 6) {
            g->play.openPause();
            return true;
        }
        if (g->state == 9) {
            g->play.goSelect();
            return true;
        }
        if (g->state == 7) {
            g->play.runState = 1;
            return true;
        }
        return false;
    }
    void save() override {
        writeSave();
        saveRecords();
        saveControls();
    }

    void shutdown() override {
        startup_.release();
        if (host_)
            motion_.release(*host_);
        auto bind = renderState_.bind();
        if (scorePage_ == ScorePage::Name)
            finishScoreName(); // Keep a reported record if the app closes during entry.
        save();
        highlighter_.restore();
        gMixer = nullptr;
        delete effect_;
        effect_ = nullptr;
        gameHost_.releaseGame();
        renderState_.disconnect();
        Waves = nullptr;
        shaderHistory_ = renderState_.shaderCycles;
        dev_.Destroy(true);
    }

  private:
    theGame *game() const {
        return effect_ ? gameHost_.game() : nullptr;
    }

    // The prefs block (theGame +0x34): AppDelegate saveState / didFinishLaunching keep 0xd00
    // bytes in NSUserDefaults ("deflex_savedata"). Missing or version (+0xc8c) != 0x3e9:
    // words 0..799 zero, +0xc80 = 5, +0xc88 = 0; always +0xc84 = 0 (live buttons), +0xc8c = 0x3e9.
    // Words 0..799 are the PlayState's prefs[] (scores per ship kind and cavern).
    static constexpr size_t kSaveSize = 0xd00;
    std::vector<uint8_t> save_;
    int32_t &sdi(size_t off) {
        return *reinterpret_cast<int32_t *>(save_.data() + off);
    }
    std::string savePath() const {
        return host_->saveDir() + "minos.sav";
    }
    void loadSave() {
        save_.assign(kSaveSize, 0);
        bool ok = false;
        if (FILE *f = std::fopen(savePath().c_str(), "rb")) {
            ok = std::fread(save_.data(), 1, kSaveSize, f) == kSaveSize && sdi(0xc8c) == 0x3e9;
            std::fclose(f);
        }
        if (!ok) {
            std::fill(save_.begin(), save_.begin() + 800 * 4, 0);
            sdi(0xc80) = 5;
            sdi(0xc88) = 0;
        }
        sdi(0xc84) = 0;
        sdi(0xc8c) = 0x3e9;
        if (theGame *g = game()) {
            std::memcpy(g->play.prefs, save_.data(), sizeof g->play.prefs);
            g->play.prefsControl = uint32_t(sdi(0xc88));
        }
    }
    void loadControls() {
        auto &p = game()->play;
        int version = 0, layout = 0, touch = 1, tilt = 1;
        if (FILE *f = std::fopen((host_->saveDir() + "controls.txt").c_str(), "r")) {
            if (std::fscanf(f, "%d %d %d %d", &version, &layout, &touch, &tilt) == 4 &&
                version == 1 && layout >= 0 && layout <= 2) {
                p.touchLayout = layout;
                p.touchGameplayEnabled = touch != 0;
                p.tiltEnabled = tilt != 0;
                p.iPad = layout != 0;
                p.prefsControl = layout == 2 ? 1 : 0;
            }
            std::fclose(f);
        }
    }
    void saveControls() {
        if (!host_ || !game())
            return;
        const auto &p = game()->play;
        if (FILE *f = std::fopen((host_->saveDir() + "controls.txt").c_str(), "w")) {
            std::fprintf(f, "1 %d %d %d\n", p.touchLayout, int(p.touchGameplayEnabled),
                         int(p.tiltEnabled));
            std::fclose(f);
        }
    }
    void writeSave() {
        theGame *g = game();
        if (!g || save_.size() != kSaveSize || !host_)
            return;
        std::memcpy(save_.data(), g->play.prefs, sizeof g->play.prefs);
        sdi(0xc88) = int32_t(g->play.prefsControl);
        if (FILE *f = std::fopen(savePath().c_str(), "wb")) {
            std::fwrite(save_.data(), 1, kSaveSize, f);
            std::fclose(f);
        }
    }
    neon::input::ControllerFrame controllers_;
    neon::input::PadMenu padMenu_;
    minos::MenuHighlight<cellGrid, gridCell> highlighter_;
    minos::TouchInput touch_;
    minos::MotionInput motion_;
    SurfaceInfo surface_;
    bool suspended_ = false, inputArmed_ = true, controllerFocus_ = false;

    enum class ScorePage { Closed, Table, Name };
    ScorePage scorePage_ = ScorePage::Closed;
    minos::LocalRecords records_;
    cellGrid scoreGrid_; // Separate grid: original HUD/menu state is never replaced.
    neon::input::PadMenu scoreMenu_;
    mino::grid::Highlighter<cellGrid, gridCell, &gridCell::bgOn> scoreHighlight_;
    int scoreBoard_ = 0, scoreRank_ = -1, entryScore_ = 0, entryCavern_ = 0;
    int64_t scoreTouchId_ = -1;
    int scoreTouchLabel_ = 0;
    std::string entryName_;
    static constexpr const char *boardNames_[4] = {"MINOS", "VIC LANDER", "TAXI", "ASTEROIDS"};

    void saveRecords() {
        if (host_ && !records_.save(host_->saveDir()))
            host_->log("minos: local record save failed; retry pending");
    }
    void drawScorePage() {
        scoreHighlight_.restore();
        if (scorePage_ == ScorePage::Name)
            mino::grid::drawNameEntry(&scoreGrid_, entryName_, entryScore_, entryCavern_);
        else
            mino::grid::drawScoreTable(&scoreGrid_, records_.scores, scoreBoard_,
                                       boardNames_[scoreBoard_], 0x400080ffu, scoreRank_, "CAVE");
    }
    void openScores(bool entry) {
        auto &p = game()->play;
        touch_.cancel(p);
        inputArmed_ = false;
        scoreTouchId_ = -1;
        scoreTouchLabel_ = 0;
        scoreRank_ = -1;
        scoreBoard_ = std::clamp(entry ? p.reportKind : p.shipKind, 0, 3);
        scorePage_ = entry ? ScorePage::Name : ScorePage::Table;
        if (entry) {
            entryName_ = records_.scores.lastName;
            entryScore_ = p.reportScore;
            entryCavern_ = std::clamp(p.cavernIndex + 1, 1, minos::cavern::kCavernCount);
        }
        scoreMenu_ = {};
        drawScorePage();
    }
    void finishScoreName() {
        records_.scores.lastName = mino::grid::finalName(entryName_);
        scoreRank_ = records_.scores.insert(scoreBoard_, records_.scores.lastName, entryScore_,
                                            entryCavern_);
        records_.scores.dirty = true;
        saveRecords();
        scorePage_ = ScorePage::Table;
        inputArmed_ = false;
        scoreTouchLabel_ = 0;
        scoreMenu_ = {};
        drawScorePage();
    }
    void closeScores() {
        scoreHighlight_.restore();
        scorePage_ = ScorePage::Closed;
        scoreTouchLabel_ = 0;
        scoreTouchId_ = -1;
        inputArmed_ = false;
        touch_.cancel(game()->play);
        // showLeaderBoard sends -1 back to the original game to purge touches.
        game()->play.goalState = -1;
        saveRecords();
    }
    void consumeReports() {
        auto &p = game()->play;
        if (records_.observe(p.goalBits))
            saveRecords();
        host_->observeOriginalAchievements(records_.achievements);
        const int event = p.goalState;
        if (event <= 0)
            return;
        p.goalState = 0; // EAGLView consumes the positive event byte once.
        if (event == 2) {
            writeSave();
            if (records_.scores.qualifies(p.reportKind, p.reportScore))
                openScores(true);
        } else if (event == 3 || event == 0xe) {
            openScores(false);
            if (event == 3) {
                scoreBoard_ = std::clamp(p.reportKind, 0, 3);
                drawScorePage();
            }
        } else if (event == 0x11) {
            writeSave();
            saveControls();
        }
        // 4..13: lifetime mask captured above; original WELL DONE messages stay
        // in the simulation. 0xf/0x10: startup/display/prefs already bound here.
    }
    void scoreInput(float x, float y, bool confirm, bool cancel) {
        scoreHighlight_.restore();
        if (cancel) {
            back();
            return;
        }
        int label = scoreMenu_.update(
            mino::grid::scanButtons(&scoreGrid_), int(scorePage_), x, y, confirm, false, 0,
            {scorePage_ == ScorePage::Name ? mino::grid::kLabelKey0 : mino::grid::kLabelBack}, {});
        if (scoreTouchLabel_) {
            label = scoreTouchLabel_;
            scoreTouchLabel_ = 0;
        }
        if (scorePage_ == ScorePage::Table) {
            if (label == mino::grid::kLabelBack)
                closeScores();
            else if (label == mino::grid::kLabelNextTable || label == mino::grid::kLabelTableName) {
                scoreBoard_ = (scoreBoard_ + 1) % 4;
                scoreRank_ = -1;
                drawScorePage();
            }
        } else {
            const int action = mino::grid::nameEntryKey(label, entryName_);
            if (action == 2)
                finishScoreName();
            else if (action == 1) {
                drawScorePage();
                if (entryName_.size() == HighScores::kNameLen)
                    scoreMenu_.sel = mino::grid::kLabelEnd;
            }
        }
        if (scorePage_ != ScorePage::Closed && controllerFocus_)
            scoreHighlight_.highlight(&scoreGrid_, scoreMenu_.sel, ++scoreGrid_.frameCount,
                                      [](const gridCell &) { return false; });
    }

    void pauseMenu(minos::PlayState &p, float x, float y, bool confirm) {
        std::vector<neon::input::MenuItem> items;
        for (auto *e = p.debris; e; e = e->next) {
            if (e->dead)
                continue;
            if (!dynamic_cast<minos::PauseControl *>(e) &&
                !dynamic_cast<minos::OpenFeintButton *>(e))
                continue;
            neon::input::MenuItem item;
            item.label = dynamic_cast<minos::PauseControl *>(e)
                             ? static_cast<minos::PauseControl *>(e)->menuLabel()
                             : e->spriteIndex;
            item.cx = e->pos.x * 100;
            item.cy = e->pos.y * 100;
            item.x0 = item.x1 = int(item.cx);
            item.y0 = item.y1 = int(item.cy);
            items.push_back(item);
        }
        const int action = padMenu_.update(items, 100, x, y, confirm, false, 0, {8}, {});
        for (auto *e = p.debris; e; e = e->next) {
            if (auto *b = dynamic_cast<minos::PauseControl *>(e)) {
                if (action == b->menuLabel())
                    b->activate();
                b->col = neon::colour(
                    1, controllerFocus_ && padMenu_.sel == b->menuLabel() ? 0.4 : 1, 1, 1);
            } else if (auto *scores = dynamic_cast<minos::OpenFeintButton *>(e)) {
                if (action == scores->spriteIndex)
                    p.goalState = 0xe;
                scores->size = controllerFocus_ && padMenu_.sel == scores->spriteIndex
                                   ? 0.09f
                                   : scores->baseSize;
            }
        }
    }

    void tick() {
        theGame *g = game();
        if (!g)
            return;
        highlighter_.restore();
        float sx = 0, sy = 0, worldAxis = 0;
        bool thrust = false, held = false;
        uint32_t pressed = 0;
        for (const auto &p : controllers_.consume())
            if (p.connected) {
                const auto a = neon::input::movement(p);
                const float influence = neon::input::worldInfluenceAxis(p);
                if (std::fabs(influence) > std::fabs(worldAxis))
                    worldAxis = influence;
                if (std::fabs(a.x) > std::fabs(sx))
                    sx = a.x;
                if (std::fabs(a.y) > std::fabs(sy))
                    sy = a.y;
                thrust |= bool(p.buttons & (neon::input::South | neon::input::RightShoulder));
                held |= p.buttons != 0 || std::fabs(a.x) > 0.5f || std::fabs(a.y) > 0.5f ||
                        std::fabs(p.rx) > .25f || p.lt > .25f || p.rt > .25f;
                pressed |= p.pressed;
            }
        if (!inputArmed_) {
            if (!held)
                inputArmed_ = true;
            pressed = 0;
            sx = sy = 0;
            thrust = false;
        }
        if (pressed || std::fabs(sx) > 0.5f || std::fabs(sy) > 0.5f)
            controllerFocus_ = true;
        const auto menu = neon::input::menuButtons(pressed);
        if (scorePage_ != ScorePage::Closed) {
            scoreInput(sx, sy, menu.confirm, menu.cancel || menu.pause);
            return; // Platform modal: freeze simulation, RNG and original menu timers.
        }
        const bool oldTouch = g->play.touchAvailable, oldTilt = g->play.tiltAvailable,
                   oldController = g->play.controllerAvailable;
        g->play.touchAvailable = host_->touchControlsAvailable();
        g->play.tiltAvailable = host_->motion().available;
        g->play.controllerAvailable =
            std::any_of(controllers_.slots().begin(), controllers_.slots().end(),
                        [](const PadState &p) { return p.connected; });
        if (!g->play.controllerAvailable)
            g->play.touchGameplayEnabled = true;
        if (oldTouch != g->play.touchAvailable || oldTilt != g->play.tiltAvailable ||
            oldController != g->play.controllerAvailable)
            g->play.refreshPauseOptions();
        const int priorState = g->play.runState;
        const bool priorPause = g->play.paused;
        if (menu.cancel && (g->state == 1 || g->state == 2 || g->state == 3)) {
            save();
            host_->requestQuit();
            return;
        } else if ((menu.pause || menu.cancel) &&
                   (g->play.paused || g->state == 4 || g->state == 5 || g->state == 6)) {
            back();
            pressed = 0;
        } else if (g->play.paused) {
            pauseMenu(g->play, sx, sy, menu.confirm);
        } else if (g->state == 7 || g->state == 9) {
            bool touched = false;
            for (bool down : g->play.touchDown)
                touched |= down;
            if (!touched && g->grid) {
                const auto buttons = mino::grid::scanButtons(g->grid);
                g->play.menuActions.activateLabel = padMenu_.update(
                    buttons, g->state, sx, sy, menu.confirm, menu.cancel, 0, {3, 0x13}, {0x13});
                if (menu.cancel && g->state == 7)
                    back();
            }
        } else if (menu.advance)
            g->play.menuActions.advance = true;
        waves_.beat = waves_.beat < 800 ? waves_.beat + 1 : 0; // WaveGen
        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        dev_.alphaEnable(true);
        dev_.SetAlphaBlend(1, 1);
        double stick = sx;
        if (g->state != 4 || g->play.paused) {
            stick = 0;
            thrust = false;
        }

        g->play.controllerInfluence = double(worldAxis) * .3;
        g->setPadInput(stick, thrust);
        g->run();
        if (g->play.runState != priorState || g->play.paused != priorPause)
            inputArmed_ = false;
        if (controllerFocus_ && !g->play.paused && g->grid && (g->state == 7 || g->state == 9))
            highlighter_.highlight(g->grid, padMenu_.sel, uint32_t(g->frameCount),
                                   g->play.colours[1]);
        effect_->aspect = 1.0f;
        effect_->RunEffect();
        consumeReports();
        if (g->play.runState != priorState || g->play.paused != priorPause)
            saveControls();
        if (g->play.exitToLauncherRequested) {
            g->play.exitToLauncherRequested = false;
            save();
            host_->requestQuit();
        }
        drawn_ = true;
    }

    // DrawEffect: the effect's output as one sprite over the whole surface.
    void present() {
        if (!drawn_)
            return;
        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        ctx_.main.SetCurrenRenderTarget(-1);
        ctx_.aux.SetCurrenRenderTarget(-1);
        dev_.alphaEnable(false);
        dev_.SetupDefaultProjection(kLogicalW, kLogicalH, PROJ_ORTHO);
        dev_.ClearScreen(255, 0, 0, 0);
        D3DXMatrixTranslation(&ctx_.main.world, kLogicalW * -0.5f, kLogicalH * 0.5f, 0.0f);
        renderState_.beginBatch();
        sprite s;
        s.pos = vec2d(0, 0);
        s.width = kLogicalW;
        s.height = kLogicalH;
        s.col.white(1.0);
        s.frame = INT_MIN;
        s.draw();
        purge();
        if (scorePage_ != ScorePage::Closed)
            presentScores();
        dev_.PresentFrame();
    }

    void presentLoading() {
        dev_.BeginFrame();
        dev_.SetCurrenRT(-1);
        ctx_.main.SetCurrenRenderTarget(-1);
        ctx_.aux.SetCurrenRenderTarget(-1);
        dev_.alphaEnable(false);
        dev_.SetupDefaultProjection(kLogicalW, kLogicalH, PROJ_ORTHO);
        dev_.ClearScreen(255, 0, 0, 0);
        D3DXMatrixTranslation(&ctx_.main.world, kLogicalW * -.5f, kLogicalH * .5f, 0);
        renderState_.beginBatch();
        ctx_.main.SetSourceFromFont(-1, -1, -1);
        ctx_.main.SetSourceFromRTs(-1, -1, -1);
        ctx_.main.SetSourceFromLiterals(loadingLiteral_, -1, -1);
        renderState_.sourceTexture = loadingLiteral_;
        sprite image;
        image.frame = -1;
        image.width = kLogicalW;
        image.height = kLogicalH;
        image.col.white(1);
        image.setupSimple(renderState_.base);
        const auto *texture = dev_.literal(loadingLiteral_);
        if (!texture)
            return;
        const float u = float(texture->imageW) / texture->width,
                    v = float(texture->imageH) / texture->height;
        const float uv[6][2] = {{0, 0}, {0, v}, {u, v}, {u, v}, {u, 0}, {0, 0}};
        for (int i = 0; i < 6; ++i) {
            auto &vertex = renderState_.base[i];
            vertex.u0 = vertex.u1 = vertex.u2 = uv[i][0];
            vertex.v0 = vertex.v1 = vertex.v2 = uv[i][1];
        }
        renderState_.cursor = renderState_.base + 6;
        renderState_.count = 6;
        purge();
        dev_.PresentFrame();
    }

    void presentScores() {
        // Use the existing cell emitter/font and shared score layouts without
        // running another game-facing cellGrid module or altering its HUD.
        dev_.alphaEnable(true);
        dev_.SetAlphaBlend(0x302, 0x303);
        std::vector<NeonVertex> vertices(41 * 25 * 12);
        int count = 0;
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 40; ++x) {
                auto &c = scoreGrid_.rows[y][x];
                c.pos = {x * (kLogicalW / 40.0) - kLogicalW * .5,
                         y * (kLogicalH / 24.0) - kLogicalH * .5};
                c.special = 0;
                if (c.bgOn) {
                    c.pass = 0;
                    count += c.outch(vertices.data() + count, &scoreGrid_, 0x7f, c.bg.packARGB());
                }
                if (c.fgOn) {
                    c.pass = 1;
                    count += c.outch(vertices.data() + count, &scoreGrid_, unsigned(c.ch),
                                     c.col.packARGB());
                }
            }
        dev_.ClearScreen(255, 0, 0, 0);
        ctx_.main.SetSourceFromFont(-1, -1, -1);
        ctx_.main.SetSourceFromRTs(-1, -1, -1);
        ctx_.main.SetSourceFromLiterals(1, -1, -1);
        D3DXMatrixIdentity(&ctx_.main.world);
        ctx_.main.SetVertexBufferWO(0);
        ctx_.main.UpdateBufferFromMem(vertices.data(), count);
        ctx_.main.Render(count);
    }

    neon::ShaderCycleState &shaderHistory_;
    RenderSession renderState_;
    Host *host_ = nullptr;
    MinosAssets assets_;
    int loadingLiteral_ = 0;
    NeonResManager res_;
    NeonDevice dev_;
    NeonDrawContext ctx_;
    minos::module::GameHost gameHost_;
    NeonEffect *effect_ = nullptr;
    WaveState waves_;
    SampleMixer mixer_;
    double acc_ = 0;
    bool drawn_ = false;
};

const CoreInfo kInfo = {"minos",         "Caverns of Minos", kLogicalW,  kLogicalH,
                        320.0f / 480.0f, 320.0f / 480.0f,    "minos.png"};
const CoreInfo &MinosCore::info() const {
    return kInfo;
}

} // namespace

extern const CoreEntry kMinosCore = {&kInfo, []() -> Core * {
                                         static neon::ShaderCycleState history;
                                         return new MinosCore(history);
                                     }};

} // namespace mino

namespace NEON_GAME {
void AudioSystemPlaySample(int sample, int volume, int flags) {
    if (mino::gMixer)
        mino::gMixer->play(sample, volume, flags);
}
} // namespace NEON_GAME
