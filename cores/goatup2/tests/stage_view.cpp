// Real-renderer integration of the reconstructed stage family. Not a substitute
// frontend or campaign. Uses imported data; captures/save fixtures stay in scratch.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"
#include "stage_backend.h"
#include "stage_setup.h"
#include "stage_controls.h"
#include "stage_session.h"
#include "loading_art.h"
#include "stage_frontend.h"
#include "bank_storage.h"
#include "name_generator.h"
#include "display.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace mino {
extern const CoreEntry kGoatUp2Core;
}

namespace {
struct Host final : mino::Host {
    std::filesystem::path assets, scratch;
    mino::Display display;
    std::string pickerPath;
    mutable bool gameTextureRead = false;
    bool touchscreen = true, sensor = true, sensorEnabled = false;
    int quits = 0;
    bool touchControlsAvailable() const override {
        return touchscreen;
    }
    bool enableMotion(bool value) override {
        sensorEnabled = value && sensor;
        return sensorEnabled;
    }
    mino::MotionState motion() const override {
        return {sensorEnabled, .1, -.95, 0};
    }
    int startupFrames = 0;
    uint64_t startupTicks = 0;
    std::function<void()> startupHook;
    bool presentStartupFrame() override {
        if (!startupHook)
            return false;
        if (gameTextureRead)
            throw std::runtime_error("startup presented after game textures loaded");
        startupHook();
        ++startupFrames;
        startupTicks = SDL_GetTicks();
        return true;
    }
    std::shared_ptr<mino::FileSelection> chooseDataFile() override {
        auto result = std::make_shared<mino::FileSelection>();
        result->path = pickerPath;
        result->done = true;
        return result;
    }
    bool readDataFile(const std::string &path, std::vector<uint8_t> &bytes,
                      size_t limit) const override {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        return bytes.size() <= limit;
    }
    bool readAsset(const std::string &name, std::vector<uint8_t> &out) const override {
        if (name == "Launch.png" || name == "NeonResources.xml")
            gameTextureRead = false;
        if (name == "grunt.png")
            gameTextureRead = true;
        if (name == "__goatup2_launch.png") {
            goatup2::loadingArt(out);
            return true;
        }
        std::ifstream in(assets / name, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), {});
        return true;
    }
    std::string saveDir() const override {
        return scratch.generic_string() + "/";
    }
    void log(const char *format, ...) override {
        va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
        std::fputc('\n', stderr);
    }
    std::string adaptShader(const std::string &text, int stage) const override {
        return display.adaptShader(text, stage);
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {
        ++quits;
    }
};
void require(bool value, const char *reason) {
    if (!value)
        throw std::runtime_error(reason);
}
std::vector<uint8_t> pixels(const mino::SurfaceInfo &s) {
    std::vector<uint8_t> out(size_t(s.width) * size_t(s.height) * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
    glReadPixels(0, 0, s.width, s.height, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    return out;
}
void capture(const std::filesystem::path &path, const mino::SurfaceInfo &s) {
    const auto image = pixels(s);
    stbi_flip_vertically_on_write(1);
    require(
        stbi_write_png(path.string().c_str(), s.width, s.height, 4, image.data(), s.width * 4) != 0,
        "write capture");
}
} // namespace
int main(int argc, char **argv) {
    try {
        require(argc == 3, "usage: mino_goatup2_view_checks ASSETS SCRATCH");
        Host host;
        host.assets = std::filesystem::absolute(argv[1]);
        host.scratch = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(host.scratch);
        require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        auto *window = SDL_CreateWindow("GoatUp 2 stage rendering", 480, 640,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(window != nullptr, "window");
        auto context = SDL_GL_CreateContext(window);
        require(context != nullptr, "context");
        require(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) != 0,
                "GL loader");
        require(host.display.init(false), "display");
        mino::DisplaySettings settings;
        host.display.configure(480, 640, {320, 480, .75f, .75f}, settings);
        goatup2::StageBackend backend;
        require(backend.init(host, host.display.surface()), "stage backend");
        std::vector<uint8_t> bytes;
        require(host.readAsset("DEFAULT_0", bytes), "original stage bank");
        goatup2::Bank bank;
        std::string error;
        require(goatup2::readBank(bytes, bank, error), error.c_str());
        const auto &stage = bank.stages[0];
        auto loaded = goatup2::loadOriginalStage(stage, 0, backend.resources(), {0, 0, 0, false, 0},
                                                 {0, 0, 0, 8, false}, true);
        auto world = std::move(loaded.world);
        goatup2::StageSession session;
        session.setBank(bank);
        session.beginRun(*world);
        require(loaded.name == "Tiny Fursuit of Wales", "original stage name");
        goatup2::StagePresentation presentation;
        presentation.attach(*world, stage.displayWidth, stage.displayHeight);
        neon::DarwinRandom incoming(bank.decoderRandomState);
        presentation.stars.reset(incoming);
        goatup2::StageHud hud;
        neon::audio::SampleMixer mixer({32, neon::audio::MixArithmetic::ForwardFloatToInteger});
        std::vector<neon::audio::Sample> samples;
        for (const auto &entry : backend.resources().samples()) {
            neon::audio::Sample sample;
            sample.maxInstances = entry.maxInstances;
            sample.priority = entry.priority;
            require(host.readAsset(entry.file, bytes) && neon::audio::parseWav(bytes, sample),
                    "original sample asset");
            samples.push_back(std::move(sample));
        }
        mixer.setSamples(std::move(samples));
        goatup2::StageControls controls;
        neon::input::ControllerState pad;
        pad.connected = true;
        std::vector<float> audio(735 * 2);
        bool heardSound = false;
        world->preparePresentation();
        for (uint32_t frame = 0; frame < 360; ++frame) {
            // The original outer update consumes a discarded rand before its
            // gates. This runner drives source intent, not simulated touches.
            world->random.next();
            world->sourceTime = float(double(world->sourceTime) + .04);
            presentation.stars.advance(world->originalPauseFlag);
            world->beginLogicalFrame();
            world->advanceCurrents();
            session.prepareOuterFrame(*world, hud, frame + 1);
            backend.applyHud(hud);
            presentation.requestCamera(*world);
            pad.lx = frame >= 120 && frame < 210 ? 1.f : 0;
            pad.buttons = frame >= 140 && frame < 150 ? neon::input::South : 0;
            controls.pads(&pad, 1);
            const auto intent = controls.tick(*world, world->goat->state == 0);
            goatup2::updateStagePalette(*world, frame + 1);
            world->runLocomotion(intent.movement, intent.jump, frame + 1, uint8_t(frame + 1));
            backend.clearHudRows(world->textRowsToClear);
            presentation.prepare(*world);
            for (const auto &event : world->sounds) {
                if (event.kind == goatup2::RuntimeSound::Kind::Stop)
                    mixer.stop(event.sample);
                else
                    mixer.play(event.sample, int(event.gain), event.sourceOption);
            }
            mixer.mix(audio.data(), 735);
            for (float value : audio) {
                require(std::isfinite(value), "nonfinite sample output");
                heardSound = heardSound || value != 0;
            }
            host.display.beginFrame();
            backend.execute(presentation, *world, mixer);
            backend.present();
            require(glGetError() == GL_NO_ERROR, "stage GL error");
            if (frame == 60 || frame == 180 || frame == 359)
                capture(host.scratch / ("stage-" + std::to_string(frame) + ".png"),
                        host.display.surface());
        }
        const auto original = pixels(host.display.surface());
        const auto random = world->random.draws();
        const auto cursor = world->goat->followerCursor;
        for (int i = 0; i < 3; ++i) {
            host.display.beginFrame();
            backend.present();
        }
        require(pixels(host.display.surface()) == original, "redraw changed retained frame");
        require(world->random.draws() == random && world->goat->followerCursor == cursor,
                "redraw advanced simulation");
        require(heardSound, "sound events never reached sample output");
        // Render a source-derived completion state. This is a report layout /
        // effect-stack integration check, not evidence of completing the level
        // through player input or an original-execution comparison.
        world->goat->state = 4;
        world->sceneFade = .09f;
        world->exitFadeTicks = 51;
        world->activeFollowers = 0;
        world->stageTicks = 123;
        world->bestStageTime = 123;
        world->score = 321;
        world->stageTotal = 432;
        world->newRecord = true;
        world->newBestStageTime = true;
        for (uint32_t tick = 0; tick < 132; ++tick) {
            world->beginLogicalFrame();
            world->advanceCurrents();
            presentation.stars.advance(false);
            session.prepareOuterFrame(*world, hud, 361 + tick);
            backend.applyHud(hud);
            world->runLocomotion(0, 0, 361 + tick, uint8_t(361 + tick));
            backend.clearHudRows(world->textRowsToClear);
            presentation.prepare(*world);
            host.display.beginFrame();
            backend.execute(presentation, *world, mixer);
            backend.present();
            require(glGetError() == GL_NO_ERROR, "report GL error");
        }
        require(session.phase() == goatup2::StageSession::Phase::BetweenLevels &&
                    session.reportTicks() > 120,
                "report reveal");
        capture(host.scratch / "between-level-report.png", host.display.surface());
        for (int index = 1; index < 20; ++index) {
            while (session.phase() != goatup2::StageSession::Phase::NextLevel) {
                world->beginLogicalFrame();
                world->advanceCurrents();
                session.prepareOuterFrame(*world, hud, 600);
                world->runLocomotion(0, 0, 600, 88);
            }
            const int lives = world->lives, total = world->stageTotal;
            const auto glyphs = world->glyphLookup;
            const auto offset = world->gridOffset;
            const auto &nextStage = bank.stages[size_t(index)];
            goatup2::generateOriginalName(world->random, uint32_t(nextStage.seed),
                                          goatup2::NameMode::Level);
            presentation.stars.reset(world->random);
            auto next =
                goatup2::loadOriginalStage(nextStage, index, backend.resources(), world->allocation,
                                           world->medium, true, world.get());
            session.continueRun(*next.world, *world);
            world = std::move(next.world);
            backend.clearHud();
            controls.beginStage();
            presentation.attach(*world, nextStage.displayWidth, nextStage.displayHeight);
            require(world->lives == lives && world->stageTotal == total && world->score == 0 &&
                        world->stageTicks == 0,
                    "run totals across stage load");
            require(world->glyphLookup == glyphs && world->gridOffset.x == offset.x &&
                        world->gridOffset.y == offset.y,
                    "template/global state across stage load");
            int placedEnemies = 0, placedContacts = 0, liveEnemies = 0, liveContacts = 0;
            for (const auto &o : nextStage.objects) {
                placedEnemies += o.type >= 4 && o.type <= 8;
                placedContacts += o.type == 1 || o.type == 2 || o.type == 3 || o.type == 9;
            }
            for (auto *a = world->enemies; a; a = a->next)
                ++liveEnemies;
            for (auto *a = world->contactObjects; a; a = a->next)
                ++liveContacts;
            require(placedEnemies == liveEnemies && placedContacts == liveContacts,
                    "omitted stage placements");
            for (uint32_t tick = 0; tick < 600; ++tick) {
                const uint32_t frame = 1000U * unsigned(index) + tick;
                world->random.next();
                world->sourceTime = float(double(world->sourceTime) + .04);
                presentation.stars.advance(false);
                world->beginLogicalFrame();
                world->advanceCurrents();
                session.prepareOuterFrame(*world, hud, frame);
                backend.applyHud(hud);
                presentation.requestCamera(*world);
                pad.lx = tick >= 90 && tick < 180 ? 1.f : tick >= 240 && tick < 300 ? -.5f : 0.f;
                pad.buttons = tick >= 100 && tick < 112 ? neon::input::South : 0;
                controls.pads(&pad, 1);
                const auto intent = controls.tick(*world, world->goat->state == 0);
                goatup2::updateStagePalette(*world, frame);
                world->runLocomotion(intent.movement, intent.jump, frame, uint8_t(frame));
                backend.clearHudRows(world->textRowsToClear);
                presentation.prepare(*world);
                for (auto *a = world->enemies; a; a = a->next)
                    require(std::isfinite(a->stagePosition.x) && std::isfinite(a->stagePosition.y),
                            "nonfinite enemy position");
                host.display.beginFrame();
                backend.execute(presentation, *world, mixer);
                backend.present();
                require(glGetError() == GL_NO_ERROR, "later stage GL error");
                for (const auto &event : world->sounds) {
                    require(event.sample >= 0 &&
                                size_t(event.sample) < backend.resources().samples().size(),
                            "later stage sample index");
                    if (event.kind == goatup2::RuntimeSound::Kind::Stop)
                        mixer.stop(event.sample);
                    else
                        mixer.play(event.sample, int(event.gain), event.sourceOption);
                }
                mixer.mix(audio.data(), 735);
                for (float sample : audio)
                    require(std::isfinite(sample), "later stage nonfinite audio");
                if (tick == 60 || tick == 300)
                    capture(host.scratch / ("level-" + std::to_string(index + 1) + "-" +
                                            std::to_string(tick) + ".png"),
                            host.display.surface());
            }
            // Supplied completion state also exercises the second report/load
            // boundary, without claiming these scripted inputs beat the level.
            world->goat->state = 4;
            world->exitFadeTicks = 51;
            world->sceneFade = .09f;
            world->activeFollowers = 0;
        }
        // Source-derived completion state for level 20, through its actual
        // life bonus and timed report. This still is not a played-through route.
        world->beginLogicalFrame();
        world->advanceCurrents();
        const int spares = world->lives;
        const int totalBefore = world->stageTotal;
        session.prepareOuterFrame(*world, hud, 21000);
        require(session.phase() == goatup2::StageSession::Phase::GameComplete,
                "campaign completion phase");
        require(session.livesBonus() == spares * 20000 &&
                    world->stageTotal == totalBefore + session.livesBonus(),
                "campaign life bonus");
        for (uint32_t tick = 0; tick < 310; ++tick) {
            world->beginLogicalFrame();
            world->advanceCurrents();
            presentation.stars.advance(false);
            session.prepareOuterFrame(*world, hud, 21001 + tick);
            backend.applyHud(hud);
            world->runLocomotion(0, 0, 21001 + tick, uint8_t(tick));
            backend.clearHudRows(world->textRowsToClear);
            presentation.prepare(*world);
            host.display.beginFrame();
            backend.execute(presentation, *world, mixer);
            backend.present();
        }
        capture(host.scratch / "campaign-complete.png", host.display.surface());
        while (session.phase() != goatup2::StageSession::Phase::Done)
            session.prepareOuterFrame(*world, hud, 22000, true);
        // Campaigns 2 and 4 share the newly translated machine families.
        // Suppress goat death ONLY in this render fixture, so each stage can
        // exercise 600 updates; completion states below are supplied, not played.
        for (int bankIndex : {1, 2, 3}) {
            goatup2::Bank extraBank;
            require(host.readAsset("DEFAULT_" + std::to_string(bankIndex), bytes) &&
                        goatup2::readBank(bytes, extraBank, error),
                    "additional campaign data");
            goatup2::StageSession run;
            run.setBank(extraBank);
            for (int level = 0; level < 20; ++level) {
                const auto &testStage = extraBank.stages[size_t(level)];
                goatup2::generateOriginalName(world->random, uint32_t(testStage.seed),
                                              goatup2::NameMode::Level);
                presentation.stars.reset(world->random);
                auto reloadedBank = goatup2::loadOriginalStage(
                    testStage, level, backend.resources(), world->allocation, world->medium, false,
                    world.get());
                if (level == 0)
                    run.beginRun(*reloadedBank.world);
                else
                    run.continueRun(*reloadedBank.world, *world);
                world = std::move(reloadedBank.world);
                world->deathDisabled = true;
                world->preparePresentation();
                require(world->initialWorking == testStage.cells,
                        "tutorial labels leaked into another campaign");
                presentation.attach(*world, testStage.displayWidth, testStage.displayHeight);
                backend.clearHud();
                controls.cancel();
                controls.beginStage();
                goatup2::PlacementTable placements;
                placements.load(testStage);
                int expectedEnemies = 0, expectedContacts = 0, enemies = 0, contacts = 0;
                for (const auto &placement : placements.entries()) {
                    expectedEnemies += placement.type >= 4 && placement.type <= 8;
                    expectedContacts += placement.type == 1 || placement.type == 2 ||
                                        placement.type == 3 || placement.type == 9;
                }
                for (auto *actor = world->enemies; actor; actor = actor->next)
                    ++enemies;
                for (auto *actor = world->contactObjects; actor; actor = actor->next)
                    ++contacts;
                require(enemies == expectedEnemies && contacts == expectedContacts,
                        "additional campaign omitted placement");
                for (uint32_t tick = 0;
                     tick < 600 && run.phase() == goatup2::StageSession::Phase::Playing; ++tick) {
                    const uint32_t frame = 30000 + uint32_t(bankIndex * 20000 + level * 600) + tick;
                    world->random.next();
                    world->sourceTime = float(double(world->sourceTime) + .04);
                    presentation.stars.advance(false);
                    world->beginLogicalFrame();
                    world->advanceCurrents();
                    run.prepareOuterFrame(*world, hud, frame);
                    backend.applyHud(hud);
                    presentation.requestCamera(*world);
                    pad.lx = tick >= 90 && tick < 180    ? 1.f
                             : tick >= 240 && tick < 300 ? -.5f
                                                         : 0.f;
                    pad.buttons = tick >= 100 && tick < 112 ? neon::input::South : 0;
                    controls.pads(&pad, 1);
                    const auto intent = controls.tick(*world, world->goat->state == 0);
                    goatup2::updateStagePalette(*world, frame);
                    world->runLocomotion(intent.movement, intent.jump, frame, uint8_t(frame));
                    backend.clearHudRows(world->textRowsToClear);
                    presentation.prepare(*world);
                    for (auto *actor = world->enemies; actor; actor = actor->next)
                        require(std::isfinite(actor->stagePosition.x) &&
                                    std::isfinite(actor->stagePosition.y),
                                "additional campaign nonfinite actor");
                    for (const auto &event : world->sounds) {
                        require(event.sample >= 0 &&
                                    size_t(event.sample) < backend.resources().samples().size(),
                                "additional campaign sample index");
                        if (event.kind == goatup2::RuntimeSound::Kind::Stop)
                            mixer.stop(event.sample);
                        else
                            mixer.play(event.sample, int(event.gain), event.sourceOption);
                    }
                    mixer.mix(audio.data(), 735);
                    for (float sample : audio)
                        require(std::isfinite(sample), "additional campaign finite audio");
                    host.display.beginFrame();
                    backend.execute(presentation, *world, mixer);
                    backend.present();
                    require(glGetError() == GL_NO_ERROR, "additional campaign GL error");
                    if (tick == 240)
                        capture(host.scratch / ("campaign-" + std::to_string(bankIndex + 1) +
                                                "-level-" + std::to_string(level + 1) + ".png"),
                                host.display.surface());
                }
                if (run.phase() == goatup2::StageSession::Phase::Playing) {
                    world->goat->state = 4;
                    world->exitFadeTicks = 51;
                    world->sceneFade = .09f;
                    world->activeFollowers = 0;
                    world->beginLogicalFrame();
                    world->advanceCurrents();
                    run.prepareOuterFrame(*world, hud, 0);
                }
                if (level < 19) {
                    while (run.phase() == goatup2::StageSession::Phase::BetweenLevels)
                        run.prepareOuterFrame(*world, hud, 0);
                    require(run.phase() == goatup2::StageSession::Phase::NextLevel &&
                                run.nextOrdinal() == level + 1,
                            "additional campaign stage transition");
                } else
                    require(run.phase() == goatup2::StageSession::Phase::GameComplete,
                            "additional campaign completion");
            }
            require(extraBank.fur == 19 && run.livesBonus() == 180000,
                    "additional campaign final records and life bonus");
        }
        // All four banks are exposed for browsing; exercise every reachable
        // template without invoking any untranslated actor constructor.
        goatup2::StageFrontend browser;
        for (int testBank = 0; testBank < 4; ++testBank)
            require(host.readAsset("DEFAULT_" + std::to_string(testBank), bytes) &&
                        goatup2::readBank(bytes, browser.banks[size_t(testBank)], error),
                    "preview bank data");
        browser.initialize(*world);
        backend.foreground = [&](goatup2::Neon_Internals::sprite &target, double w, double h) {
            backend.drawOverlay(target, w, h, browser.foregroundDraws());
        };
        for (int testBank = 0; testBank < 4; ++testBank)
            for (int level = 0; level < 20; ++level) {
                const auto &testStage = browser.banks[size_t(testBank)].stages[size_t(level)];
                auto preview = goatup2::createOriginalStage(testStage, level, backend.resources(),
                                                            world->allocation, world->medium, 1,
                                                            false, nullptr, false);
                require(!preview->goat && !preview->enemies && !preview->contactObjects,
                        "preview creates no actors");
                browser.bank = testBank;
                browser.level = level;
                browser.enter(goatup2::StageFrontend::Page::Preview, *preview);
                browser.previewName = goatup2::generateOriginalName(
                    preview->random, uint32_t(testStage.seed), goatup2::NameMode::Level);
                presentation.attach(*preview, testStage.displayWidth, testStage.displayHeight);
                goatup2::updateStagePalette(*preview, uint32_t(testBank * 20 + level));
                browser.prepare(*preview, hud);
                backend.clearHud();
                backend.applyHud(hud);
                presentation.prepareMenu(*preview, browser.draws(), true);
                host.display.beginFrame();
                backend.execute(presentation, *preview, mixer);
                backend.present();
                require(glGetError() == GL_NO_ERROR, "bank preview GL error");
                if (level == 0)
                    capture(host.scratch /
                                ("bank-" + std::to_string(testBank + 1) + "-preview.png"),
                            host.display.surface());
            }
        backend.release();
        backend.release();
        // A real encoded save is loaded by Core before preview/play. The
        // independent bank roundtrip checks cover every geometry/coordinate.
        goatup2::BankStorage saved(host.scratch);
        goatup2::Bank recordBank;
        require(host.readAsset("DEFAULT_0", bytes) && goatup2::readBank(bytes, recordBank, error),
                "save source");
        saved.source(0, bytes, recordBank);
        recordBank.stages[0].highScore = 271828;
        recordBank.stages[0].bestTime = 1234;
        require(saved.save(0, recordBank, error), "seed encoded record fixture");
        auto core = std::unique_ptr<mino::Core>(mino::kGoatUp2Core.create());
        host.startupHook = [&] { capture(host.scratch / "loading.png", host.display.surface()); };
        require(core->init(host, host.display.surface()), "stage Core init");
        require(host.startupFrames == 1, "startup presented during initialization");
        mino::PadState state;
        state.connected = true;
        int splashChecked = 0;
        auto tickCore = [&](int count = 1) {
            if (splashChecked != host.startupFrames) {
                // A large simulation delta must not shorten the real-time splash.
                const auto launchPixels = pixels(host.display.surface());
                core->frame(10);
                require(pixels(host.display.surface()) == launchPixels,
                        "splash ended before one real second");
                const auto elapsed = SDL_GetTicks() - host.startupTicks;
                if (elapsed < 1010)
                    SDL_Delay(Uint32(1010 - elapsed));
                splashChecked = host.startupFrames;
            }
            for (int i = 0; i < count; ++i) {
                core->pads(&state, 1);
                host.display.beginFrame();
                core->frame(1. / 60);
                core->mixAudio(audio.data(), 735);
                for (float sample : audio)
                    require(std::isfinite(sample), "frontend finite audio");
                state.pressed = 0;
            }
        };
        auto press = [&](uint32_t button) {
            state.buttons = state.pressed = button;
            tickCore();
            state.buttons = 0;
            tickCore(2);
        };
        tickCore();
        tickCore(120);
        capture(host.scratch / "title.png", host.display.surface());
        press(mino::PAD_EAST);
        require(host.quits == 1, "title B exits to iOX");
        press(mino::PAD_SOUTH);
        tickCore(5);
        capture(host.scratch / "banks.png", host.display.surface());
        press(mino::PAD_SOUTH);
        tickCore(5);
        capture(host.scratch / "preview.png", host.display.surface());
        // Same source actions through touch: next, previous, then play. Touch
        // routing never manufactures controller input or advances live actors.
        auto touchButton = [&](float x, float y) {
            auto surface = host.display.surface();
            core->touch(
                {mino::TouchPhase::Down, 7, (x + .5f) * surface.width, (y + .5f) * surface.height});
            tickCore();
            core->touch(
                {mino::TouchPhase::Up, 7, (x + .5f) * surface.width, (y + .5f) * surface.height});
            tickCore(2);
        };
        touchButton(.42f, .39f);
        tickCore(3);
        capture(host.scratch / "preview-next.png", host.display.surface());
        touchButton(-.42f, .39f);
        tickCore(3);
        touchButton(0, .39f);
        tickCore(60);
        capture(host.scratch / "play.png", host.display.surface());
        press(mino::PAD_START);
        tickCore(65);
        capture(host.scratch / "pause.png", host.display.surface());
        capture(host.scratch / "pause-tilt-off.png", host.display.surface());
        touchButton(0, .23f);
        tickCore(16);
        require(host.sensorEnabled, "tilt enabled");
        capture(host.scratch / "pause-tilt-on.png", host.display.surface());
        touchButton(0, .23f);
        tickCore(16);
        require(!host.sensorEnabled, "tilt disabled");
        host.touchscreen = false;
        tickCore(16);
        capture(host.scratch / "pause-no-touch.png", host.display.surface());
        host.touchscreen = true;
        tickCore(16);
        touchButton(0, .12f); // original touch-layout callback persists directly
        goatup2::BankStorage reloaded(host.scratch);
        int savedHand = 0;
        require(reloaded.loadTouchMode(savedHand, error) && savedHand == 1,
                "Core touch preference saved");
        // Reset's two presses are the source confirmation, spending one spare
        // and reusing the existing explosion/factory paths rather than death.
        touchButton(0, -.17f);
        touchButton(0, -.17f);
        tickCore(6);
        capture(host.scratch / "reset.png", host.display.surface());
        require(core->back(), "native Back pauses");
        tickCore(15);
        // Return through the original quit icon, not a host-only quit panel.
        touchButton(0, -.025f);
        tickCore(3);
        capture(host.scratch / "returned-title.png", host.display.surface());
        // Original idle alternation: first demo, then credits. Input dismisses
        // the demo to title; it is not forwarded as gameplay/menu acceptance.
        tickCore(520);
        capture(host.scratch / "attract-start.png", host.display.surface());
        tickCore(210);
        capture(host.scratch / "attract-camera.png", host.display.surface());
        press(mino::PAD_SOUTH);
        tickCore(6);
        capture(host.scratch / "attract-dismissed-title.png", host.display.surface());
        tickCore(820);
        capture(host.scratch / "credits.png", host.display.surface());
        require(core->back(), "credits Back returns to title");
        tickCore(2);
        press(mino::PAD_SOUTH);
        press(mino::PAD_DOWN);
        press(mino::PAD_SOUTH);
        capture(host.scratch / "campaign-2-core-preview.png", host.display.surface());
        press(mino::PAD_SOUTH);
        tickCore(90);
        capture(host.scratch / "campaign-2-core-play.png", host.display.surface());
        require(core->back(), "campaign 2 native pause");
        tickCore(13);
        touchButton(0, -.025f);
        tickCore(3);
        press(mino::PAD_SOUTH);
        for (int i = 0; i < 3; ++i)
            press(mino::PAD_DOWN);
        press(mino::PAD_SOUTH);
        for (int i = 0; i < 16; ++i)
            touchButton(.42f, .39f);
        capture(host.scratch / "campaign-4-core-preview-17.png", host.display.surface());
        touchButton(0, .39f);
        tickCore(240);
        capture(host.scratch / "campaign-4-core-play-17.png", host.display.surface());
        require(core->back(), "campaign 4 native pause");
        tickCore(13);
        touchButton(0, -.025f);
        tickCore(3);
        press(mino::PAD_SOUTH);
        press(mino::PAD_DOWN);
        press(mino::PAD_DOWN);
        press(mino::PAD_SOUTH);
        for (int i = 0; i < 12; ++i)
            touchButton(.42f, .39f);
        capture(host.scratch / "campaign-3-core-preview-13.png", host.display.surface());
        touchButton(0, .39f);
        tickCore(240);
        capture(host.scratch / "campaign-3-core-play-13.png", host.display.surface());
        const auto corePixels = pixels(host.display.surface());
        host.display.beginFrame();
        core->frame(0);
        require(pixels(host.display.surface()) == corePixels, "Core redraw");
        core->pause(true);
        host.display.beginFrame();
        core->frame(1. / 60);
        require(pixels(host.display.surface()) == corePixels, "Core suspended frame");
        core->pause(false);
        settings.internalScale = 2;
        host.display.configure(600, 800, core->nativeFormat(), settings);
        core->resize(host.display.surface());
        core->pads(nullptr, 0);
        host.display.beginFrame();
        core->frame(1. / 60);
        require(glGetError() == GL_NO_ERROR, "Core resize");
        core->back();
        tickCore(16);
        touchButton(0, .355f);
        tickCore(3);
        require(host.quits == 2, "pause Exit exits to iOX");
        core->save();
        core->shutdown();
        core->shutdown();
        core.reset();
        // Host lifecycle saves preserve loaded records and reload in a fresh Core.
        core.reset(mino::kGoatUp2Core.create());
        require(core->init(host, host.display.surface()), "saved Core reinit");
        tickCore(72);
        press(mino::PAD_SOUTH);
        press(mino::PAD_SOUTH);
        capture(host.scratch / "saved-preview-reopened.png", host.display.surface());
        require(core->back(), "saved preview Back");
        tickCore(3);
        touchButton(.43f, -.41f);
        touchButton(32.5f / 42 - .5f, 20.5f / 46 - .5f);
        tickCore(4);
        touchButton(0, .39f);
        tickCore(6);
        capture(host.scratch / "editor-terrain.png", host.display.surface());
        touchButton(0, 0);
        tickCore(3);
        capture(host.scratch / "editor-painted.png", host.display.surface());
        state.lx = .6f;
        tickCore(3);
        state.lx = 0;
        tickCore(2);
        press(mino::PAD_SOUTH);
        tickCore(3);
        touchButton(.43f, .41f);
        tickCore(3);
        capture(host.scratch / "editor-objects.png", host.display.surface());
        touchButton(0, 0);
        tickCore(3);
        touchButton(.31f, .41f);
        tickCore(3);
        touchButton(.43f, -.41f);
        tickCore(90);
        capture(host.scratch / "editor-test-play.png", host.display.surface());
        touchButton(.43f, .41f);
        tickCore(3);
        capture(host.scratch / "editor-return-from-test.png", host.display.surface());
        touchButton(.43f, .41f);
        touchButton(.43f, .41f);
        tickCore(3);
        capture(host.scratch / "editor-global-tools.png", host.display.surface());
        touchButton(-.43f, .41f);
        tickCore(2);
        {
            goatup2::Bank empty;
            std::vector<uint8_t> raw;
            require(saved.loadEdited(1, empty, raw, error) && empty.stages[0].seed == 0 &&
                        empty.stages[0].objects.empty(),
                    "Clear saves empty level slot");
        }
        touchButton(.31f, .41f);
        tickCore(2);
        touchButton(-.3f, .41f);
        touchButton(0, .41f);
        touchButton(-.2f, .41f);
        touchButton(-.1f, .41f);
        tickCore(2);
        require(core->back(), "editor native Back returns to preview");
        tickCore(3);
        capture(host.scratch / "editor-saved-preview.png", host.display.surface());
        std::vector<uint8_t> editedBytes;
        goatup2::Bank editedBank;
        require(saved.loadEdited(1, editedBank, editedBytes, error), "editor bank persisted");
        require(!editedBytes.empty() && (editedBank.stages[0].cells[5850] & 255U) == 19U,
                "edited terrain saved in original bank format");
        require(core->back(), "editor preview back to banks");
        tickCore(3);
        capture(host.scratch / "editor-bank-tools.png", host.display.surface());
        touchButton(28.5f / 42 - .5f, 20.5f / 46 - .5f);
        tickCore(3);
        {
            goatup2::Bank named;
            std::vector<uint8_t> raw;
            require(saved.loadEdited(1, named, raw, error) && named.gid != editedBank.gid,
                    "bank name regenerated and saved");
        }
        touchButton(19.5f / 42 - .5f, 20.5f / 46 - .5f);
        tickCore(3);
        for (const auto &entry : std::filesystem::directory_iterator(host.scratch))
            if (entry.path().extension() == ".gu2")
                host.pickerPath = entry.path().string();
        require(!host.pickerPath.empty(), "named bank exported");
        // Load the exported bank into another editable slot through Host's
        // completed picker callback; OS dialog interaction remains manual QA.
        touchButton(23.5f / 42 - .5f, 29.5f / 46 - .5f);
        tickCore(4);
        goatup2::Bank copied;
        std::vector<uint8_t> copiedBytes;
        require(saved.loadEdited(2, copied, copiedBytes, error) &&
                    copied.stages[0].cells == editedBank.stages[0].cells,
                "named bank loaded into selected slot");
        const auto validCopy = copiedBytes;
        auto plain = goatup2::transformBank(copiedBytes);
        std::string invalid(plain.begin(), plain.end());
        const auto dimension = invalid.find("(XCL)100");
        require(dimension != std::string::npos, "import fixture grid tag");
        invalid.replace(dimension, 8, "(XCL)101");
        const auto invalidBytes = goatup2::transformBank({invalid.begin(), invalid.end()});
        host.pickerPath = (host.scratch / "invalid-grid.gu2").string();
        {
            std::ofstream out(host.pickerPath, std::ios::binary);
            out.write(reinterpret_cast<const char *>(invalidBytes.data()), invalidBytes.size());
        }
        touchButton(23.5f / 42 - .5f, 29.5f / 46 - .5f);
        tickCore(4);
        require(saved.loadEdited(2, copied, copiedBytes, error) && copiedBytes == validCopy,
                "unsafe bank import retained previous bank");
        core->shutdown();
        core.reset();
        core.reset(mino::kGoatUp2Core.create());
        require(core->init(host, host.display.surface()), "edited bank Core reopen");
        tickCore(72);
        press(mino::PAD_SOUTH);
        press(mino::PAD_DOWN);
        press(mino::PAD_SOUTH);
        tickCore(3);
        capture(host.scratch / "editor-reopened-preview.png", host.display.surface());
        require(core->back(), "preview to banks before idle test");
        tickCore(3);
        require(core->back(), "banks to title before idle test");
        tickCore(3);
        core->save();
        std::vector<std::pair<std::filesystem::path, std::vector<uint8_t>>> recordsBeforeDemo;
        for (const auto &entry : std::filesystem::directory_iterator(host.scratch))
            if (entry.path().extension() == ".dat") {
                std::ifstream in(entry.path(), std::ios::binary);
                recordsBeforeDemo.push_back(
                    {entry.path(), {std::istreambuf_iterator<char>(in), {}}});
            }
        tickCore(520);
        capture(host.scratch / "attract-reopened-core.png", host.display.surface());
        tickCore(1500);
        capture(host.scratch / "attract-timeout-title.png", host.display.surface());
        core->save();
        for (const auto &before : recordsBeforeDemo) {
            std::ifstream in(before.first, std::ios::binary);
            const std::vector<uint8_t> after{std::istreambuf_iterator<char>(in), {}};
            require(after == before.second, "idle demonstration changed saved record/editor bank");
        }
        core->shutdown();
        core.reset();
        const auto mainScratch = host.scratch;
        for (int heldFrames : {1, 2, 8, 30}) {
            host.scratch = mainScratch / ("held-mode-" + std::to_string(heldFrames));
            std::filesystem::create_directories(host.scratch);
            state = {};
            state.connected = true;
            core.reset(mino::kGoatUp2Core.create());
            require(core->init(host, host.display.surface()), "held-input Core init");
            tickCore(72);
            press(mino::PAD_SOUTH);
            press(mino::PAD_UP);
            auto hostFrames = [&](int count) {
                for (int i = 0; i < count; ++i) {
                    core->pads(&state, 1);
                    host.display.beginFrame();
                    core->frame(1. / 240);
                    state.pressed = 0;
                }
            };
            auto hold = [&](int frames) {
                state.buttons = state.pressed = mino::PAD_SOUTH;
                hostFrames(frames);
                state.buttons = 0;
                hostFrames(8);
            };
            // Mode must remain focused across edit/play rebuilds. Include taps
            // released between simulation ticks and long held buttons.
            hold(heldFrames);
            hold(12);
            hold(12);
            capture(host.scratch / "edit-banks.png", host.display.surface());
            touchButton(32.5f / 42 - .5f, 20.5f / 46 - .5f);
            touchButton(0, .39f);
            tickCore(5);
            touchButton(0, 0);
            tickCore(3);
            require(core->back(), "held-input editor exit");
            core->save();
            goatup2::BankStorage check(host.scratch.generic_string());
            goatup2::Bank edited;
            std::vector<uint8_t> savedBytes;
            require(check.loadEdited(1, edited, savedBytes, error) && !savedBytes.empty() &&
                        (edited.stages[0].cells[5850] & 255) == 19,
                    "held Mode did not reach editable bank");
            require(core->back(), "held-input preview to banks");
            tickCore(2);
            for (uint32_t backButton : {uint32_t(mino::PAD_BACK), uint32_t(mino::PAD_EAST)}) {
                state.buttons = state.pressed = backButton;
                core->pads(&state, 1);
                host.display.beginFrame();
                core->frame(1. / 30);
                state.pressed = 0;
                // Two logical ticks in the transition frame must not observe a
                // false neutral snapshot before the next held host snapshot.
                tickCore(20);
                state.buttons = 0;
                tickCore(2);
                require(core->back(), "title Back provides launcher exit");
                press(mino::PAD_SOUTH); // re-enter Banks for the other Back path
            }
            core->shutdown();
            core.reset();
        }
        host.scratch = mainScratch;
        host.display.shutdown();
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "Eighty original stages across all four campaigns, 80 template previews and "
                     "frontend flow render through shared effect stack; report/load, input and "
                     "retained redraw checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
