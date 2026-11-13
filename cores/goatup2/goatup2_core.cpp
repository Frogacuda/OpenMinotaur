// GoatUp 2 reconstructed frontend, campaign and portable save binding.
#include "startup_screen.h"
#include "stage_backend.h"
#include "stage_setup.h"
#include "stage_controls.h"
#include "stage_session.h"
#include "stage_frontend.h"
#include "loading_art.h"
#include "bank_storage.h"
#include "editor_view.h"
#include "neon/entity_list.h"
#include "neon/integer.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <chrono>
namespace mino {
namespace {
// Bundled permission-cleared launch art shares the ordinary texture upload path.
class GoatAssetHost final : public Host {
  public:
    Host *target = nullptr;
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        if (name == "__goatup2_launch.png") {
            goatup2::loadingArt(out);
            return true;
        }
        return target->readAsset(name, out);
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
    bool presentStartupFrame() override {
        return target->presentStartupFrame();
    }
};
// Use the existing save directory to preserve records and custom level banks.
// Keep the established save-directory identity so cleanup does not discard local progress.
const CoreInfo Info = {"goatup2", "GoatUp 2",     320, 480, .75f, .75f, "grunt.png",
                       "goatup2", "goatup2-stage"};
class GoatStageCore final : public Core {
    mino::StartupScreen startup_;
    using UI = goatup2::StageFrontend;

  public:
    const CoreInfo &info() const override {
        return Info;
    }
    bool init(Host &host, const SurfaceInfo &surface) override {
        if (!startup_.begin(host, surface))
            return false;
        host_ = &host;
        assets_.target = &host;
        surface_ = surface;
        if (!backend_.init(assets_, surface, false))
            return false;
        backend_.foreground = [this](goatup2::Neon_Internals::sprite &target, double w, double h) {
            backend_.drawOverlay(target, w, h, frontend_.foregroundDraws());
        };
        std::vector<uint8_t> data;
        std::string error;
        storage_ = goatup2::BankStorage(std::filesystem::u8path(host.saveDir()));
        for (int i = 0; i < 4; ++i) {
            auto &bank = frontend_.banks[size_t(i)];
            if (!host.readAsset("DEFAULT_" + std::to_string(i), data) ||
                !goatup2::readBank(data, bank, error) ||
                !goatup2::validateRuntimeBank(bank, error)) {
                host.log("GoatUp bank: %s", error.c_str());
                return false;
            }
            editorBankSource_[size_t(i)] = data;
            if (!storage_.loadEdited(i, bank, editorBankSource_[size_t(i)], error))
                host.log("GoatUp 2 edited bank %d ignored: %s", i, error.c_str());
            storage_.source(i, editorBankSource_[size_t(i)], bank);
            if (!storage_.load(i, bank, error))
                host.log("GoatUp 2 save %d ignored: %s", i, error.c_str());
        }
        if (!storage_.loadTouchMode(frontend_.touchMode, error, &frontend_.tiltEnabled))
            host.log("GoatUp 2 preferences ignored: %s", error.c_str());
        motionAvailable_ = host.enableMotion(true);
        host.enableMotion(frontend_.tiltEnabled && motionAvailable_);
        controls_.hand = frontend_.touchMode ? goatup2::TouchHand::MovementRight
                                             : goatup2::TouchHand::MovementLeft;
        const auto &stage = frontend_.banks[0].stages[0];
        world_ = goatup2::createOriginalStage(
            stage, 0, backend_.resources(), {0, 0, 0, false, 0}, {0, 0, 0, 8, false},
            frontend_.banks[3].decoderRandomState, false, nullptr, false);
        frontend_.capabilities(host.touchControlsAvailable(), motionAvailable_, *world_);
        frontend_.initialize(*world_);
        presentation_.stars.reset(world_->random);
        frontend_.enter(UI::Page::Title, *world_);
        std::vector<neon::audio::Sample> samples;
        for (const auto &entry : backend_.resources().samples()) {
            neon::audio::Sample sample;
            sample.maxInstances = entry.maxInstances;
            sample.priority = entry.priority;
            if (!host.readAsset(entry.file, data) || !neon::audio::parseWav(data, sample))
                return false;
            samples.push_back(std::move(sample));
        }
        mixer_.setSamples(std::move(samples));
        host.log("GoatUp 2: all four campaigns; Gentle/1.0, original touch. Start/Back pauses.");
        return true;
    }
    void resize(const SurfaceInfo &surface) override {
        startup_.resize(surface);
        surface_ = surface;
        backend_.resize(surface);
        controls_.cancel();
        frontend_.cancelInput();
        editor_.cancel();
    }
    void pads(const PadState *pads, int count) override {
        if (startup_.pads(pads, count))
            return;
        if (suspended_ || fileSelection_)
            return;
        editor_.keyboardNavigation = host_->keyboardNavigationAvailable();
        if (frontend_.page == UI::Page::Editor) {
            editor_.pads(pads, count);
            for (int i = 0; pads && i < count; ++i)
                if (pads[i].connected) {
                    if (pads[i].pressed & PAD_BACK) {
                        back();
                        return;
                    }
                    if (editor_.mode == 4 && (pads[i].pressed & PAD_START)) {
                        frontend_.paused = !frontend_.paused;
                        controls_.cancel();
                    }
                }
            if (editor_.mode == 4 && !frontend_.paused)
                controls_.pads(pads, count);
            return;
        }
        frontend_.keyboardNavigation = host_->keyboardNavigationAvailable();
        frontend_.pads(pads, count);
        for (int i = 0; pads && i < count; ++i)
            if (pads[i].connected && (pads[i].pressed & PAD_BACK)) {
                if (!back())
                    host_->requestQuit();
                return;
            }
        for (int i = 0; pads && i < count; ++i)
            if (pads[i].connected && (pads[i].pressed & PAD_START) &&
                frontend_.page == UI::Page::Playing) {
                togglePause();
                break;
            }
        if (frontend_.page == UI::Page::Playing && !frontend_.paused)
            controls_.pads(pads, count);
    }
    void touch(const TouchEvent &e) override {
        if (startup_.touch(e))
            return;
        if (suspended_ || fileSelection_)
            return;
        const neon::vec2d point{double(e.x) / std::max(1, surface_.width) - .5,
                                double(e.y) / std::max(1, surface_.height) - .5};
        if (frontend_.page == UI::Page::Editor) {
            editor_.touch(e.phase, e.id, point);
            if (editor_.mode == 4 && !frontend_.paused &&
                (controls_.ownsTouch(e.id) || !editor_.consumes(point)))
                controls_.touch(e.phase, e.id, point);
            return;
        }
        frontend_.touch(e.phase, e.id, point);
        if (frontend_.page == UI::Page::Playing && !frontend_.paused)
            controls_.touch(e.phase, e.id, point);
    }
    bool back() override {
        if (startup_.blocking())
            return true;
        if (frontend_.page == UI::Page::Editor) {
            if (editor_.mode == 4)
                finishEditorTest();
            else {
                if (!commitEditor())
                    return true;
                save();
                enter(UI::Page::Preview);
                preview();
            }
            return true;
        }
        if (frontend_.page == UI::Page::Attract) {
            finishAttract();
            return true;
        }
        if (frontend_.page == UI::Page::Playing) {
            togglePause();
            return true;
        }
        if (frontend_.page == UI::Page::Preview) {
            mixer_.play(57, world_->medium.sampleVolume << 12, 0);
            enter(UI::Page::Banks);
            return true;
        }
        if (frontend_.page == UI::Page::Banks || frontend_.page == UI::Page::Credits) {
            enter(UI::Page::Title);
            return true;
        }
        if (frontend_.page == UI::Page::Title) {
            save();
            host_->requestQuit();
            return true;
        }
        return false;
    }
    void pause(bool value) override {
        if (value)
            save();
        suspended_ = value;
        controls_.cancel();
        frontend_.cancelInput();
        editor_.cancel();
        if (value)
            mixer_.stopAll();
    }
    void save() override {
        if (!host_ || !world_ || saving_)
            return;
        saving_ = true;
        if (frontend_.page == UI::Page::Editor && editor_.mode != 4)
            commitEditor();
        std::string error;
        for (int i = 0; i < 4; ++i)
            if (editedDirty_[size_t(i)]) {
                if (storage_.saveEdited(i, editorBankSource_[size_t(i)], error))
                    editedDirty_[size_t(i)] = false;
                else
                    host_->log("GoatUp 2 edited bank %d save failed: %s", i, error.c_str());
            }
        for (int i = 0; i < 4; ++i)
            if (!storage_.save(i, frontend_.banks[size_t(i)], error))
                host_->log("GoatUp 2 save %d failed: %s", i, error.c_str());
        if (!storage_.saveTouchMode(frontend_.touchMode, error, frontend_.tiltEnabled))
            host_->log("GoatUp 2 preferences save failed: %s", error.c_str());
        saving_ = false;
    }
    void mixAudio(float *output, int frames) override {
        mixer_.mix(output, frames);
    }
    void shutdown() override {
        startup_.release();
        save();
        if (host_)
            host_->enableMotion(false);
        mixer_.stopAll();
        world_.reset();
        backend_.release();
    }
    void frame(double dt) override {
        if (startup_.frame(dt))
            return;
        if (!world_)
            return;
        frontend_.capabilities(host_->touchControlsAvailable(), motionAvailable_, *world_);
        controls_.motion(frontend_.tiltEnabled ? host_->motion() : MotionState{},
                         frontend_.tiltEnabled);
        if (!suspended_) {
            accumulator_ += std::clamp(dt, 0., .25);
            while (accumulator_ >= 1. / 60) {
                step();
                accumulator_ -= 1. / 60;
            }
        } else
            accumulator_ = 0;
        if (!rendered_)
            prepareRender();
        backend_.present();
    }

  private:
    void bankFileActions(int action) {
        std::string error;
        const int slot = action >= UI::BankSave0   ? action - UI::BankSave0
                         : action >= UI::BankLoad0 ? action - UI::BankLoad0
                                                   : action - UI::BankRename0;
        auto &bank = frontend_.banks[size_t(slot)];
        if (action >= UI::BankSave0) {
            std::vector<uint8_t> data;
            if (goatup2::BankStorage::encodeRecords(editorBankSource_[size_t(slot)], bank, data,
                                                    error) &&
                storage_.saveNamed(frontend_.names[size_t(slot)], data, error))
                mixer_.play(9, world_->medium.sampleVolume << 8, 0);
        } else if (action >= UI::BankLoad0) {
            pendingFileSlot_ = slot;
            fileSelection_ = host_->chooseDataFile();
            if (!fileSelection_)
                error = "file selection is unavailable on this host";
            controls_.cancel();
            frontend_.cancelInput();
        } else {
            auto renamed = bank;
            renamed.gid = int(world_->random.next());
            std::vector<uint8_t> data;
            if (goatup2::replaceEditedBankHeader(editorBankSource_[size_t(slot)], renamed, data,
                                                 error)) {
                bank = std::move(renamed);
                editorBankSource_[size_t(slot)] = std::move(data);
                storage_.rebase(slot, editorBankSource_[size_t(slot)]);
                editedDirty_[size_t(slot)] = true;
                frontend_.names[size_t(slot)] = goatup2::generateOriginalName(
                    world_->random, uint32_t(bank.gid), goatup2::NameMode::Bank);
                save();
            }
        }
        if (!error.empty())
            host_->log("GoatUp 2 bank operation failed: %s", error.c_str());
    }
    void pollBankFile() {
        if (!fileSelection_)
            return;
        std::string path;
        {
            std::lock_guard<std::mutex> lock(fileSelection_->mutex);
            if (!fileSelection_->done)
                return;
            path = fileSelection_->path;
        }
        fileSelection_.reset();
        frontend_.cancelInput();
        if (path.empty())
            return;
        std::string error;
        std::vector<uint8_t> bytes;
        goatup2::Bank loaded;
        if (!host_->readDataFile(path, bytes, goatup2::BankCapacity) ||
            !goatup2::readBank(bytes, loaded, error) ||
            !goatup2::validateRuntimeBank(loaded, error)) {
            host_->log("GoatUp 2 bank load rejected: %s",
                       error.empty() ? "unreadable or oversized file" : error.c_str());
            return;
        }
        const size_t slot = size_t(pendingFileSlot_);
        frontend_.banks[slot] = std::move(loaded);
        editorBankSource_[slot] = std::move(bytes);
        storage_.rebase(int(slot), editorBankSource_[slot]);
        editedDirty_[slot] = true;
        frontend_.names[slot] = goatup2::generateOriginalName(
            world_->random, uint32_t(frontend_.banks[slot].gid), goatup2::NameMode::Bank);
        save();
        enter(UI::Page::Banks);
    }
    void editorWorld() {
        auto next = goatup2::createOriginalStage(
            editor_.document.stage(), frontend_.level, backend_.resources(), world_->allocation,
            world_->medium, world_->random.state(), false, world_.get(), false);
        next->displayFade = 1;
        next->retainEffectsFrom(*world_);
        world_ = std::move(next);
        presentation_.attach(*world_, editor_.document.stage().displayWidth,
                             editor_.document.stage().displayHeight);
        backend_.clearHud();
        controls_.cancel();
    }
    void openEditor() {
        if (frontend_.bank == 0)
            return; // original first bank is protected
        editor_.open(frontend_.banks[size_t(frontend_.bank)].stages[size_t(frontend_.level)]);
        editor_.bankName = frontend_.names[size_t(frontend_.bank)];
        editor_.levelName = frontend_.previewName;
        enter(UI::Page::Editor);
        editorWorld();
    }
    bool commitEditor(bool assignName = true) {
        if (!editor_.changed)
            return true;
        std::string error;
        if (!editor_.document.commit(world_->random, error, assignName)) {
            editor_.error = error;
            return false;
        }
        return publishEditor();
    }
    bool publishEditor() {
        const size_t slot = size_t(frontend_.bank);
        std::string error;
        std::vector<uint8_t> encoded;
        if (!goatup2::replaceEditedStage(editorBankSource_[slot], frontend_.level,
                                         editor_.document.serialized(), encoded, error)) {
            editor_.error = error;
            return false;
        }
        goatup2::Bank parsed;
        if (!goatup2::readBank(encoded, parsed, error)) {
            editor_.error = error;
            return false;
        }
        frontend_.banks[slot] = std::move(parsed);
        editorBankSource_[slot] = std::move(encoded);
        storage_.rebase(int(slot), editorBankSource_[slot]);
        editedDirty_[slot] = true;
        editor_.changed = false;
        editor_.error.clear();
        if (!editor_.document.generatedName.empty())
            editor_.levelName = editor_.document.generatedName;
        world_->working = editor_.document.stage().cells;
        world_->backup = world_->working;
        world_->initialBackup = world_->working;
        presentation_.attach(*world_, editor_.document.stage().displayWidth,
                             editor_.document.stage().displayHeight);
        save();
        return true;
    }
    void beginEditorTest() {
        if (!commitEditor())
            return;
        const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(frontend_.level)];
        bool goat = false;
        for (const auto &p : stage.objects)
            goat |= p.type == 0;
        if (!stage.seed || !goat) {
            editor_.error = "PLACE A GOAT BEFORE TESTING";
            return;
        }
        // 0x45d38 calls the placement factory and terrain re-read directly.
        // Unlike normal Play, it does not name-seed or rebuild the stars.
        auto test = goatup2::createOriginalStage(stage, frontend_.level, backend_.resources(),
                                                 world_->allocation, world_->medium,
                                                 world_->random.state(), false, world_.get());
        test->retainEffectsFrom(*world_);
        world_ = std::move(test);
        world_->resetCurrents();
        world_->lives = 0;
        editor_.previousMode = editor_.mode;
        editor_.mode = 4;
        frontend_.paused = false;
        editor_.cancel();
        controls_.beginStage();
        backend_.clearHud();
        presentation_.attach(*world_, stage.displayWidth, stage.displayHeight);
        mixer_.stopAll();
    }
    void finishEditorTest() {
        editor_.mode = editor_.previousMode;
        editor_.cancel();
        frontend_.paused = false;
        editorWorld();
        mixer_.stopAll();
    }
    void editorStep() {
        using E = goatup2::EditorView;
        world_->animationFrameFlags = uint8_t(frame_);
        if (editor_.mode != 4)
            world_->advanceEffects();
        const int action = editor_.tick(*world_);
        if (editor_.mode == 4) {
            if (!frontend_.paused) {
                presentation_.requestCamera(*world_);
                const auto input =
                    controls_.tick(*world_, world_->goat && world_->goat->state == 0);
                goatup2::updateStagePalette(*world_, frame_);
                world_->runLocomotion(input.movement, input.jump, frame_, uint8_t(frame_));
            }
            if (action == E::Mode)
                finishEditorTest();
        } else {
            if (action == E::Mode) {
                editor_.document.clearSelection();
                editor_.mode = (editor_.mode + 1) % 4;
                editor_.cancel();
            } else if (action == E::Test)
                beginEditorTest();
            else if (action == E::Return) {
                if (commitEditor()) {
                    enter(UI::Page::Preview);
                    preview();
                }
            } else if (action == E::Clear) {
                editor_.document.clear();
                editor_.changed = true;
                commitEditor(false);
            } else if (action == E::Rename) {
                editor_.document.settings().seed = 0;
                editor_.changed = true;
                commitEditor();
            } else if (action == E::Undo) {
                if (editor_.document.undo())
                    publishEditor();
            } else if (action >= E::ShiftLeft && action <= E::ShiftDown) {
                const int direction = action == E::ShiftLeft    ? 0
                                      : action == E::ShiftRight ? 1
                                      : action == E::ShiftUp    ? 2
                                                                : 3;
                editor_.document.shift(direction, world_->bounds, world_->layout.cellSize);
                editor_.changed = true;
            }
            if (frontend_.page == UI::Page::Editor && editor_.mode != 4) {
                if (editor_.changed && !editor_.stroke)
                    commitEditor();
                world_->working = editor_.document.stage().cells;
                world_->preparePresentation();
                goatup2::updateStagePalette(*world_, frame_);
            }
        }
        prepareRender();
        for (const auto &event : world_->sounds)
            if (event.kind == goatup2::RuntimeSound::Kind::Stop)
                mixer_.stop(event.sample);
            else
                mixer_.play(event.sample, int(event.gain), event.sourceOption);
    }
    void togglePause() {
        if (session_.phase() != goatup2::StageSession::Phase::Playing)
            return;
        frontend_.setPaused(!frontend_.paused, *world_);
        world_->originalPauseFlag = frontend_.paused;
        controls_.cancel();
        mixer_.stopAll();
        mixer_.play(72, world_->medium.sampleVolume << 12, 0);
    }
    void enter(UI::Page page) {
        if (page == UI::Page::Title)
            world_->clearActors(); // 0x40958 full teardown
        frontend_.enter(page, *world_);
        world_->originalPauseFlag = false;
        controls_.cancel();
        backend_.clearHud();
        hud_.text.clear();
        hud_.clearRows.clear();
    }
    void preview() {
        const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(frontend_.level)];
        // Original preview 0x48cbc(...,1) parses/name-seeds the template, not
        // the placement factory. Preserve the owner palette/time across it.
        auto name = goatup2::generateOriginalName(world_->random, uint32_t(stage.seed),
                                                  goatup2::NameMode::Level);
        auto next = goatup2::createOriginalStage(
            stage, frontend_.level, backend_.resources(), world_->allocation, world_->medium,
            world_->random.state(), false, world_.get(), false);
        next->displayFade = 1;
        next->retainEffectsFrom(*world_);
        world_ = std::move(next);
        levelName_ = std::move(name);
        frontend_.previewName = levelName_;
        presentation_.attach(*world_, stage.displayWidth, stage.displayHeight);
    }
    void beginAttract() {
        // 0x4c824 resets stars before its ordinal draw; 0x4ca24 resets
        // them again before the ordinary name-seed/factory path.
        presentation_.stars.reset(world_->random);
        const auto ordinal = world_->random.next() & 15U;
        frontend_.bank = 0; // +d20 maps bank zero; 0x4ca24 clears bank selector +d0c
        std::vector<int> slots;
        for (int i = 0; i < 20; ++i)
            if (frontend_.banks[size_t(frontend_.bank)].stages[size_t(i)].seed)
                slots.push_back(i);
        // A short user bank has no corresponding original initialized ordinal.
        // Show credits instead of accessing an uninitialized original slot.
        if (ordinal >= slots.size()) {
            presentation_.stars.reset(world_->random);
            enter(UI::Page::Credits);
            return;
        }
        frontend_.level = slots[ordinal];
        const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(frontend_.level)];
        presentation_.stars.reset(world_->random);
        attractMuted_ = world_->medium.splashSoundSuppressed;
        auto medium = world_->medium;
        medium.splashSoundSuppressed = true;
        auto loaded = goatup2::loadOriginalStage(stage, frontend_.level, backend_.resources(),
                                                 world_->allocation, medium, frontend_.bank == 0,
                                                 world_.get());
        if (!loaded.world->goat) {
            enter(UI::Page::Credits);
            return;
        } // malformed user bank, not an original campaign
        loaded.world->retainEffectsFrom(*world_);
        world_ = std::move(loaded.world);
        levelName_ = std::move(loaded.name);
        world_->attractTicks = 1500;
        enter(UI::Page::Attract);
        presentation_.attract = true;
        presentation_.attach(*world_, stage.displayWidth, stage.displayHeight);
        world_->cameraSettleFrames = 0;
        presentation_.camera.fastFrames = 0;
        world_->preparePresentation();
        mixer_.stopAll();
    }
    void finishAttract() {
        world_->attractTicks = 0;
        world_->medium.splashSoundSuppressed = attractMuted_;
        presentation_.attract = false;
        const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(frontend_.level)];
        world_ = goatup2::createOriginalStage(stage, frontend_.level, backend_.resources(),
                                              world_->allocation, world_->medium,
                                              world_->random.state(), false, world_.get(), false);
        mixer_.stopAll();
        enter(UI::Page::Title);
    }
    void play() {
        const int index = frontend_.level;
        const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(index)];
        if (std::none_of(stage.objects.begin(), stage.objects.end(),
                         [](const auto &object) { return object.type == 0; })) {
            host_->log("GoatUp 2: place a goat in this level before playing it");
            return;
        }
        // 0x4aec8 -> 0x4ca24: preview already established name state; stars
        // precede the actual stage-name reseed and placement construction.
        presentation_.stars.reset(world_->random);
        auto next =
            goatup2::loadOriginalStage(stage, index, backend_.resources(), world_->allocation,
                                       world_->medium, frontend_.bank == 0, world_.get());
        next.world->retainEffectsFrom(*world_);
        world_ = std::move(next.world);
        levelName_ = std::move(next.name);
        session_.setBank(frontend_.banks[size_t(frontend_.bank)]);
        session_.beginRun(*world_, index);
        enter(UI::Page::Playing);
        controls_.beginStage();
        presentation_.attach(*world_, stage.displayWidth, stage.displayHeight);
        world_->preparePresentation();
        mixer_.stop(66);
        mixer_.play(9, world_->medium.sampleVolume << 9, 0);
    }
    void requestReset() {
        // 0x4a640 + 0x42f28's +bfd branch: one spare, remove level score from
        // total, restore terrain and reconstruct only the four source lists.
        world_->explode(*world_->goat, 400);
        --world_->lives;
        world_->stageTotal = neon::integers::subtract32(world_->stageTotal, world_->score);
        world_->score = 0;
        pendingReset_ = true;
        frontend_.setPaused(false, *world_);
        world_->originalPauseFlag = false;
        controls_.cancel();
    }
    void resetStageActors() {
        pendingReset_ = false;
        world_->working = world_->initialWorking;
        world_->backup = world_->initialBackup;
        for (auto **list : {&world_->grassBonuses, &world_->enemies, &world_->contactObjects})
            neon::entities::destroyList(*list, [](goatup2::StageActor *a) { delete a; });
        world_->goat.reset();
        goatup2::PlacementTable placements;
        placements.load(frontend_.banks[size_t(frontend_.bank)].stages[size_t(world_->stageIndex)]);
        world_->spawnOriginalObjects(placements);
        world_->preparePresentation();
        frontend_.setPaused(false, *world_);
        world_->originalPauseFlag = false;
        controls_.cancel();
    }
    void action(int action) {
        if ((action >= UI::BankRename0 && action <= UI::BankRename3) ||
            (action >= UI::BankLoad0 && action <= UI::BankLoad3) ||
            (action >= UI::BankSave0 && action <= UI::BankSave3)) {
            bankFileActions(action);
            return;
        }
        if (action == UI::Begin) {
            enter(UI::Page::Banks);
            return;
        }
        if (action >= UI::Choose0 && action <= UI::Choose3) {
            frontend_.bank = action - UI::Choose0;
            frontend_.level = 0;
            enter(UI::Page::Preview);
            preview();
            return;
        }
        if (action == UI::Next || action == UI::Previous) {
            frontend_.level = (frontend_.level + (action == UI::Next ? 1 : 19)) % 20;
            preview();
            mixer_.play(10, world_->medium.sampleVolume << 9, 0);
            return;
        }
        if (action == UI::Play && UI::campaignPlayable(frontend_.bank)) {
            if (frontend_.editing)
                openEditor();
            else
                play();
            return;
        }
        if (action == UI::ToggleEdit) {
            frontend_.editing = !frontend_.editing;
            enter(UI::Page::Banks);
            frontend_.focusAction(UI::ToggleEdit);
            mixer_.play(8, world_->medium.sampleVolume << 8, 0);
            return;
        }
        if (action == UI::IdleCycle) {
            if (idleCreditsNext_) {
                presentation_.stars.reset(world_->random);
                enter(UI::Page::Credits);
            } else
                beginAttract();
            idleCreditsNext_ = !idleCreditsNext_;
            return;
        }
        if (action == UI::Return) {
            if (frontend_.page == UI::Page::Attract) {
                finishAttract();
                return;
            }
            if (frontend_.page == UI::Page::Title)
                enter(UI::Page::Credits);
            else if (frontend_.page == UI::Page::Preview)
                enter(UI::Page::Banks);
            else
                enter(UI::Page::Title);
            return;
        }
        if (action == UI::Resume) {
            togglePause();
            return;
        }
        if (action == UI::Exit) {
            save();
            host_->requestQuit();
            return;
        }
        if (action == UI::Title) {
            save();
            enter(UI::Page::Title);
            return;
        }
        if (action == UI::Tilt && motionAvailable_) {
            frontend_.tiltEnabled = !frontend_.tiltEnabled;
            host_->enableMotion(frontend_.tiltEnabled);
            controls_.cancel();
            mixer_.play(5, world_->medium.sampleVolume << 12, 0);
            save();
            return;
        }
        if (action == UI::Hand) {
            frontend_.touchMode ^= 1;
            controls_.hand = frontend_.touchMode ? goatup2::TouchHand::MovementRight
                                                 : goatup2::TouchHand::MovementLeft;
            mixer_.play(5, world_->medium.sampleVolume << 12, 0);
            save();
            return;
        }
        if (action == UI::Reset && world_->lives > 0) {
            if (frontend_.resetTicks() > 0) {
                mixer_.play(76, world_->medium.sampleVolume << 12, 0);
                requestReset();
            } else {
                frontend_.armReset();
                mixer_.play(75, world_->medium.sampleVolume << 12, 0);
            }
        }
    }
    void step() {
        pollBankFile();
        world_->random.next();
        if (!frontend_.paused) {
            ++frame_;
            world_->sourceTime = float(double(world_->sourceTime) + .04);
        }
        presentation_.stars.advance(frontend_.paused);
        world_->beginLogicalFrame();
        world_->advanceCurrents();
        if (frontend_.page == UI::Page::Editor) {
            editorStep();
            return;
        }
        if (pendingReset_)
            resetStageActors();
        if (pendingTransition_) {
            const int pending = pendingTransition_;
            pendingTransition_ = 0;
            action(pending);
        }
        const int uiAction = frontend_.tick(*world_, frame_);
        // Idle expiry is inside the original outer switch, before actors,
        // unlike button callbacks that switch views at the end of a pass.
        if (uiAction == UI::IdleCycle)
            action(uiAction);
        if (frontend_.page != UI::Page::Playing) {
            frontend_.prepareText(*world_, menuHud_);
            backend_.applyHud(menuHud_);
        }
        const bool playing = frontend_.page == UI::Page::Playing;
        if (frontend_.page == UI::Page::Attract) {
            world_->attractTicks = frontend_.attractRemaining();
            goatup2::updateStagePalette(*world_, frame_);
            world_->runLocomotion(0, 0, frame_, uint8_t(frame_));
            presentation_.requestCamera(*world_); // target written by this goat tick
        } else if (playing) {
            if (!frontend_.paused) {
                const auto phase = session_.phase();
                session_.prepareOuterFrame(*world_, hud_, frame_, controls_.anyReportInput());
                if (session_.phase() != phase)
                    save(); // original record/report boundary
            }
            if (session_.phase() == goatup2::StageSession::Phase::NextLevel &&
                session_.nextOrdinal() < goatup2::BankStageSlots) {
                // 0x42f28 case 1 loads the next source stage inside this outer
                // tick; its actors run in the following live pass of the SAME tick.
                const int index = session_.nextOrdinal();
                const auto &stage = frontend_.banks[size_t(frontend_.bank)].stages[size_t(index)];
                // Case 1 calls 0x48cbc once BEFORE 0x4ca24; that routine calls
                // it again after star creation. Preserve both name reseeds.
                goatup2::generateOriginalName(world_->random, uint32_t(stage.seed),
                                              goatup2::NameMode::Level);
                presentation_.stars.reset(world_->random);
                auto loaded = goatup2::loadOriginalStage(stage, index, backend_.resources(),
                                                         world_->allocation, world_->medium,
                                                         frontend_.bank == 0, world_.get());
                session_.continueRun(*loaded.world, *world_);
                levelName_ = std::move(loaded.name);
                world_ = std::move(loaded.world);
                controls_.beginStage();
                backend_.clearHud();
                hud_.text.clear();
                hud_.clearRows.clear();
                presentation_.attach(*world_, stage.displayWidth, stage.displayHeight);
                host_->log("GoatUp stage %d: %s", index + 1, levelName_.c_str());
            }
            if (!frontend_.paused) {
                backend_.applyHud(hud_);
                presentation_.requestCamera(*world_);
                const auto intent = controls_.tick(*world_, world_->goat->state == 0);
                goatup2::updateStagePalette(*world_, frame_);
                world_->runLocomotion(intent.movement, intent.jump, frame_, uint8_t(frame_));
                backend_.clearHudRows(world_->textRowsToClear);
            }
            if (session_.phase() == goatup2::StageSession::Phase::Done) {
                mixer_.stopAll();
                enter(UI::Page::Title);
            }
        } else
            goatup2::updateStagePalette(*world_, frame_);
        // Source +3d4 controls run after live actors. A Play callback creates
        // actors for drawing now; their first update is the next logical tick.
        // View-switch callbacks set the frontend state for the next outer pass.
        if (uiAction >= UI::Choose0 && uiAction <= UI::Choose3)
            mixer_.play(9, world_->medium.sampleVolume << 8, 0);
        if (uiAction == UI::Return && frontend_.page == UI::Page::Preview)
            mixer_.play(57, world_->medium.sampleVolume << 12, 0);
        if (uiAction == UI::Title || uiAction == UI::Exit) {
            mixer_.stop(63);
            mixer_.play(9, world_->medium.sampleVolume << 12, 0);
        }
        if (uiAction == UI::Begin || (uiAction >= UI::Choose0 && uiAction <= UI::Choose3) ||
            uiAction == UI::Return || uiAction == UI::Title || uiAction == UI::Exit)
            pendingTransition_ = uiAction;
        else if (uiAction != UI::IdleCycle)
            action(uiAction);
        prepareRender();
        for (const auto &sound : world_->sounds) {
            if (sound.kind == goatup2::RuntimeSound::Kind::Stop)
                mixer_.stop(sound.sample);
            else
                mixer_.play(sound.sample, int(sound.gain), sound.sourceOption);
        }
    }
    void prepareRender() {
        if (frontend_.page == UI::Page::Editor) {
            frontend_.prepareDraws(*world_);
            editor_.prepare(*world_, menuHud_);
            if (editor_.mode == 4) {
                presentation_.editor = true;
                presentation_.prepare(*world_);
                presentation_.editor = false;
                presentation_.appendOverlay(editor_.draws());
            } else {
                backend_.applyHud(menuHud_);
                presentation_.prepareEditor(*world_, editor_.draws(), editor_.mode >= 2);
                // The original editor uses the mutating terrain helper too.
                editor_.document.settings().cells = world_->working;
            }
            backend_.execute(presentation_, *world_, mixer_);
            rendered_ = true;
            return;
        }
        frontend_.prepareDraws(*world_);
        if (frontend_.page == UI::Page::Playing || frontend_.page == UI::Page::Attract) {
            presentation_.rightHandedTouch = frontend_.touchMode != 0;
            presentation_.prepare(*world_);
        } else {
            presentation_.prepareMenu(*world_, frontend_.draws(),
                                      frontend_.page == UI::Page::Preview);
        }
        backend_.execute(presentation_, *world_, mixer_);
        rendered_ = true;
    }
    Host *host_ = nullptr;
    GoatAssetHost assets_;
    SurfaceInfo surface_;
    goatup2::StageBackend backend_;
    UI frontend_;
    std::unique_ptr<goatup2::StageRuntime> world_;
    goatup2::StagePresentation presentation_;
    goatup2::StageControls controls_;
    goatup2::StageHud hud_, menuHud_;
    std::shared_ptr<FileSelection> fileSelection_;
    int pendingFileSlot_ = 0;
    goatup2::EditorView editor_;
    std::array<std::vector<uint8_t>, 4> editorBankSource_;
    std::array<bool, 4> editedDirty_{};
    goatup2::BankStorage storage_;
    goatup2::StageSession session_;
    std::string levelName_;
    neon::audio::SampleMixer mixer_{{32, neon::audio::MixArithmetic::ForwardFloatToInteger}};
    uint32_t frame_ = 0;
    double accumulator_ = 0;
    bool motionAvailable_ = false;
    bool suspended_ = false, rendered_ = false, pendingReset_ = false, saving_ = false,
         attractMuted_ = false, idleCreditsNext_ = false;
    int pendingTransition_ = 0;
};
} // namespace
extern const CoreEntry kGoatUp2Core = {&Info, []() -> Core * { return new GoatStageCore; }};
} // namespace mino
