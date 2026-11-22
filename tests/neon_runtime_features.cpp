// Scene interchange, image ownership and context-loss checks for the optional renderer.
#include "../platform/audio_level.h"
#include "effect_runtime.h"
#include "layer_domain_validation.h"
#include "scene_coverage_validation.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <fstream>
#include <glad/gl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace neon::effects;
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void initialize() {
    require(gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) != 0,
            "GL loader failed");
    require(Runtime::loadGlesEntryPoints([](const char *name) -> void * {
                return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            }),
            "GLES loader failed");
}
} // namespace
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Usage: neon-runtime-features data");
        mino::AudioLevel level;
        float samples[] = {1, -1, 1, -1};
        level.submit(samples, 4);
        require(level.value() == 1, "Audio RMS peak calibration");
        float half[] = {.5f, -.5f};
        level.submit(half, 2);
        require(level.value() == .5f, "Audio RMS half calibration");
        level.submit(nullptr, 0);
        require(level.value() == 0, "Audio RMS silence");

        require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        auto *window = SDL_CreateWindow("Neon feature checks", 128, 128,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(window != nullptr, SDL_GetError());
        auto context = SDL_GL_CreateContext(window);
        require(context != nullptr, SDL_GetError());
        initialize();
        if (argc == 2)
            validateSceneCoverage(argv[1]);
        {
            Runtime source(argv[1]);
            source.selectPreset(7);
            ImageAssets images;
            std::vector<uint8_t> pixels(64 * 64 * 4, 255);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    auto n = (y * 64 + x) * 4;
                    int cell = x / 32 + (y / 32) * 2;
                    pixels[n] = cell == 0 || cell == 3 ? 255 : 0;
                    pixels[n + 1] = cell == 1 || cell == 3 ? 255 : 0;
                    pixels[n + 2] = cell == 2 || cell == 3 ? 255 : 0;
                }
            int texture = images.addPixels("Sprite sequence test", 64, 64, pixels);
            source.setImageAssets(images);
            auto set = [&](const std::string &key, float value) {
                for (auto &e : source.authoring().controls(7))
                    if (e.key == key) {
                        e.enabled = true;
                        e.value = value;
                        return;
                    }
                throw std::runtime_error("Missing sprite test control " + key);
            };
            set("input.19.0", float(texture));
            for (auto field : {std::pair<const char *, float>{"enabled", 1},
                               {"columns", 2},
                               {"rows", 2},
                               {"baseCount", 2},
                               {"rareFirst", 2},
                               {"rareCount", 2},
                               {"rareEvery", 10},
                               {"paletteTint", 0}})
                set(std::string("sprite.19.") + field.first, field.second);
            std::ostringstream text;
            source.writeScene(text);
            Runtime editable(argv[1]);
            std::istringstream scene(text.str());
            editable.readScene(scene);
            std::ostringstream compiled;
            source.writeBaked(compiled);
            std::istringstream input(compiled.str());
            auto prepared = Runtime::loadBaked(input, argv[1]);
            require(editable.insertSprite(19, 3, 3, 2, 1) && prepared->insertSprite(19, 3, 3, 2, 1),
                    "Sprite insertion API rejected active emitter");
            require(!prepared->insertSprite(19, 4, 1) && !prepared->insertSprite(-1, 1, 1),
                    "Invalid sprite insertion accepted");
            Destination target{0, 128, 128, {0, 0, 128, 128}, {}};
            for (int frame = 0; frame < 60; ++frame) {
                std::map<int, std::array<float, 3>> colours{
                    {1, {frame % 2 ? 1.f : 0.f, frame % 2 ? 0.f : 1.f, .25f}}};
                editable.setSpriteColours(colours);
                prepared->setSpriteColours(colours);
                std::vector<uint8_t> a(128 * 128 * 4), b(a.size());
                editable.frame(1. / 60, target);
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
                prepared->frame(1. / 60, target);
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
                require(a == b, "Prepared sprite sequence differs from saved editable scene");
                require(glGetError() == GL_NO_ERROR, "Sprite sequence GLES error");
            }
            prepared->releaseGraphics();
            editable.releaseGraphics();
            source.releaseGraphics();
            std::cout
                << "PASS: imported sprite atlas, saved scene, prepared playback and queued insertion match pixel-for-pixel.\n";
            if (std::getenv("MINO_SPRITE_SEQUENCE_CHECK"))
                return 0;
        }

        {
            auto scenePath =
                std::filesystem::path(argv[1]).parent_path() / "bezels/SuperOxWars.neongles";
            for (auto size :
                 {std::pair{128, 72}, std::pair{128, 96}, std::pair{72, 128}, std::pair{96, 128}}) {
                for (float scale : {1.f, .5f}) {
                    std::vector<unsigned char> first;
                    double error = 0;
                    for (float endpoint : {-1.f, 1.f}) {
                        Runtime effect(argv[1]);
                        effect.loadSceneFile(scenePath);
                        effect.parameters().renderScale = scale;
                        for (int tick = 0; tick < 60; ++tick)
                            effect.frame(1. / 60,
                                         {0, 128, 128, {0, 0, size.first, size.second}, {}});
                        for (auto &field : effect.authoring().controls(effect.selectedPreset()))
                            if (field.key == "node.20.f.7") {
                                field.enabled = true;
                                field.value = endpoint;
                            }
                        effect.frame(1. / 60, {0, 128, 128, {0, 0, size.first, size.second}, {}});
                        std::vector<unsigned char> image(size.first * size.second * 4);
                        glReadPixels(0, 128 - size.second, size.first, size.second, GL_RGBA,
                                     GL_UNSIGNED_BYTE, image.data());
                        require(glGetError() == GL_NO_ERROR, "Super Ox Wars loop GLES error");
                        if (first.empty())
                            first = image;
                        else
                            for (size_t n = 0; n < image.size(); ++n)
                                if (n % 4 != 3)
                                    error += std::abs(int(first[n]) - int(image[n]));
                        effect.releaseGraphics();
                    }
                    error /= size.first * size.second * 3;
                    std::cout << "Super Ox Wars loop " << size.first << "x" << size.second
                              << " scale=" << scale << " endpoint RGB error=" << error << std::endl;
                    require(error < .05, "Super Ox Wars scroll wrap changes the image");
                }
            }
        }
        if (std::getenv("MINO_OX_LOOP_CHECK"))
            return 0;
        validateLayerDomains(argv[1]);
        {
            Runtime effect(argv[1]);
            Destination target{0, 128, 128, {0, 0, 128, 128}, {}};
            for (int shader : {94, 95}) {
                Runtime surface(argv[1]);
                surface.selectPreset(18);
                surface.components().items.clear();
                surface.components().nextId = 1;
                int grid = surface.components().add(ComponentKind::Vector);
                surface.components().at(grid).profile = 5;
                surface.components().at(grid).visible = false;
                int plane = surface.components().add(ComponentKind::Plane);
                auto &c = surface.components().at(plane);
                c.source = 10000 + grid;
                c.shader = shader;
                c.animated = false;
                c.values[RepeatX] = 3;
                c.values[RepeatY] = 2;
                c.values[C0] = .35f;
                c.values[C1] = 3.5f;
                c.values[C2] = c.values[C3] = .5f;
                c.values[C4] = c.values[C5] = 0;
                const auto resources = surface.bakedResources();
                require(std::find(resources.begin(), resources.end(),
                                  "gles/shaders/" + std::to_string(shader) + ".frag") !=
                            resources.end(),
                        "Surface barrel missing from prepared resource closure");
                std::ostringstream saved;
                surface.writeBaked(saved);
                std::istringstream input(saved.str());
                auto restored = Runtime::loadBaked(input, argv[1]);
                for (int frame = 0; frame < 4; ++frame) {
                    std::vector<unsigned char> a(128 * 128 * 4), b(a.size());
                    surface.frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
                    restored->frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
                    require(a == b, "Prepared surface barrel differs from editable playback");
                    require(glGetError() == GL_NO_ERROR, "Surface barrel GLES error");
                }
                restored->releaseGraphics();
                surface.releaseGraphics();
            }
            std::cout
                << "PASS: single-surface barrels, tiled material, resource closure and prepared playback parity.\n";
            Runtime reference(argv[1]);
            reference.parameters().copyElision = false;
            size_t count = 0;
            for (const auto &preset : Runtime::presets()) {
                effect.selectPreset(preset.id);
                reference.selectPreset(preset.id);
                for (int tick = 0; tick < 4; ++tick) {
                    std::vector<unsigned char> fast(128 * 128 * 4), slow(fast.size());
                    effect.frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, fast.data());
                    reference.frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, slow.data());
                    for (size_t n = 0; n < fast.size(); ++n)
                        if (n % 4 != 3 && fast[n] != slow[n])
                            throw std::runtime_error("Copy-elision image mismatch in preset " +
                                                     std::to_string(preset.id));
                }
                require(glGetError() == GL_NO_ERROR, "Preset GLES error");
                ++count;
            }
            std::cout << "PASS: " << count
                      << " preset graphs match conservative copies pixel-for-pixel.\n";
            {
                auto presentation = std::filesystem::path(argv[1]).parent_path();
                for (const auto &path :
                     std::filesystem::recursive_directory_iterator(presentation)) {
                    if (path.path().extension() != ".neongles")
                        continue;
                    Runtime authored(argv[1]);
                    std::ifstream source(path.path());
                    double rate = authored.readScene(source);
                    std::ostringstream snapshot;
                    authored.writeBaked(snapshot, rate);
                    size_t templateReads = 0;
                    auto read = [&](const auto &name, auto &bytes) {
                        if (name.extension() == ".grpreset" || name.extension() == ".tsv" ||
                            name.extension() == ".constants")
                            ++templateReads;
                        std::ifstream file(std::filesystem::path(argv[1]) / name, std::ios::binary);
                        if (!file)
                            return false;
                        bytes.assign(std::istreambuf_iterator<char>(file), {});
                        return true;
                    };
                    std::istringstream binary(snapshot.str());
                    double restoredRate = 0;
                    auto playback = Runtime::loadBaked(binary, {}, read, &restoredRate);
                    const auto document = snapshot.str();
                    auto rejects = [&](std::string invalid) {
                        try {
                            std::istringstream input(invalid);
                            auto unused = Runtime::loadBaked(input);
                        } catch (const std::exception &) {
                            return true;
                        }
                        return false;
                    };
                    require(rejects(document.substr(0, document.size() / 2)),
                            "Truncated baked scene was accepted");
                    auto version = document;
                    version[8] = '\xff';
                    require(rejects(version), "Unknown baked version was accepted");
                    require(rejects(document + "trailing"), "Trailing baked data was accepted");
                    require(rate == restoredRate, "Baked playback rate differs");
                    require(templateReads == 0, "Baked load read editor templates");
                    for (int tick = 0; tick < 12; ++tick) {
                        std::vector<unsigned char> a(128 * 128 * 4), b(a.size());
                        authored.frame(rate / 60, target);
                        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
                        playback->frame(rate / 60, target);
                        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
                        for (size_t n = 0; n < a.size(); ++n)
                            if (n % 4 != 3 && a[n] != b[n])
                                throw std::runtime_error(
                                    "Baked pixel mismatch: " + path.path().filename().string() +
                                    " frame " + std::to_string(tick));
                    }
                    require(templateReads == 0, "Baked playback read editor templates");
                    require(glGetError() == GL_NO_ERROR, "Baked playback GLES error");
                    std::cout << "PASS: baked " << path.path().filename().string()
                              << " matches editable playback; zero template reads.\n";
                    playback->releaseGraphics();
                    authored.releaseGraphics();
                }
            }
            {
                auto source =
                    std::filesystem::path(argv[1]).parent_path() / "bezels/Gridrunner.neongles";
                Runtime full(argv[1]), sampled(argv[1]);
                std::ifstream a(source), b(source);
                full.readScene(a);
                sampled.readScene(b);
                full.frame(1. / 60, target);
                sampled.frame(1. / 60, target);
                const auto memory = sampled.statistics().targetBytes;
                std::vector<unsigned char> before(128 * 128 * 4), after(before.size());
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
                const double time = sampled.time();
                require(sampled.presentCached(target), "Completed output not reusable");
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
                require(before == after && sampled.time() == time,
                        "Cached presentation changed pixels or clocks");
                require(sampled.statistics().targetBytes == memory, "Cache added render targets");
                unsigned referenceDraws = 0;
                for (int n = 0; n < 2; ++n) {
                    full.frame(1. / 60, target);
                    referenceDraws += full.statistics().drawCalls;
                }
                sampled.frameSampled(1. / 30, target);
                require(std::abs(full.time() - sampled.time()) < 1e-7,
                        "Reduced cadence slowed the animation clock");
                require(sampled.statistics().drawCalls < referenceDraws * .75,
                        "Nested graphs rendered catch-up passes");
                require(glGetError() == GL_NO_ERROR, "Sampled playback GLES error");
                std::cout
                    << "PASS: cached pixels/clock/memory unchanged; sampled nested playback draws "
                    << sampled.statistics().drawCalls << " versus " << referenceDraws << ".\n";
            }
            {
                Runtime grid(argv[1]);
                std::ifstream scene(std::filesystem::path(argv[1]).parent_path() /
                                    "bezels/Gridrunner.neongles");
                grid.readScene(scene);
                auto original = grid.components().at(3).layerScene;
                ResponseSequence clock;
                clock.name = "Nested clock";
                clock.source = "speed";
                clock.mode = ResponseMode::Input;
                clock.inputLow = 0;
                clock.inputHigh = 8;
                clock.targets = {{"layer.3/clock.8.6.speed", ResponseWave::Constant,
                                  ResponseBlend::Replace, 1, 0, 0, 8}};
                ResponseSequence shader;
                shader.name = "Nested shader";
                shader.source = "shader";
                shader.mode = ResponseMode::Input;
                shader.inputLow = 0;
                shader.inputHigh = 91;
                shader.targets = {{"layer.3/node.18.i.2", ResponseWave::Constant,
                                   ResponseBlend::Replace, 1, 0, 0, 91}};
                grid.responses().sequences = {clock, shader};
                grid.responses().rebuild();
                grid.setInput("speed", .5f);
                grid.setInput("shader", 13);
                for (int n = 0; n < 4; ++n)
                    grid.frame(1. / 60, target);
                auto *source = grid.layerRuntime(3);
                require(source != nullptr, "Nested source not rendered");
                require(std::abs(source->authoring().composition(38).nodes[8].channels[6].speed -
                                 .375f) < .00001f,
                        "Nested speed did not reach controller");
                require(source->authoring().authoredScene(38)[18].integers[2] == 13,
                        "Nested shader did not reach renderer");
                require(grid.components().at(3).layerScene == original,
                        "Live rig changed saved source graph");
                for (auto &sequence : grid.responses().sequences)
                    sequence.enabled = false;
                grid.responses().rebuild();
                grid.frame(1. / 60, target);
                require(source->authoring().authoredScene(38)[18].integers[2] == 14,
                        "Nested shader did not restore saved baseline");
                grid.components().at(3).animated = false;
                grid.components().at(4).animated = false;
                grid.frame(1. / 60, target);
                grid.frame(1. / 60, target);
                auto cachedDraws = grid.statistics().drawCalls;
                grid.setInput("unrelated.value", .5f);
                grid.frame(1. / 60, target);
                require(grid.statistics().drawCalls == cachedDraws,
                        "Unrelated input invalidated a static layer source");
                require(glGetError() == GL_NO_ERROR, "Nested source GLES error");
                grid.releaseGraphics();
                std::cout << "PASS: nested live inputs, continuous source clocks, shader switching "
                             "and disconnect restoration without authored graph changes.\n";
            }
            {
                Runtime cached(argv[1]), redrawn(argv[1]);
                auto scenePath =
                    std::filesystem::path(argv[1]).parent_path() / "bezels/Gridrunner.neongles";
                cached.loadSceneFile(scenePath);
                redrawn.loadSceneFile(scenePath);
                cached.components().at(3).visible = false;
                redrawn.components().at(3).visible = false;
                std::vector<unsigned char> first, a(128 * 128 * 4), b(a.size());
                bool changed = false;
                for (int frame = 0; frame < 45; ++frame) {
                    cached.frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, a.data());
                    if (frame == 0)
                        first = a;
                    else
                        changed |= a != first;
                    if (auto source = redrawn.layerRuntime(4))
                        source->setVectorObjects(source->vectorObjects());
                    redrawn.frame(1. / 60, target);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, b.data());
                    require(a == b, "Cached vector source changes animated blend output");
                    if (frame > 0)
                        require(redrawn.statistics().drawCalls == cached.statistics().drawCalls + 1,
                                "Static beams were not cached independently of the animated blend");
                }
                require(changed, "Grid composite froze while reusing the vector source");
                cached.releaseGraphics();
                redrawn.releaseGraphics();
                std::cout << "PASS: cached beam source matches forced redraw while grid blend "
                             "keeps animating.\n";
            }
            {
                Runtime isolated(argv[1]);
                isolated.selectPreset(18);
                isolated.components().items.clear();
                int beam = isolated.components().add(ComponentKind::Vector);
                isolated.components().at(beam).profile = 5;
                isolated.components().at(beam).animated = false;
                isolated.parameters().componentPreviewId = beam;
                std::vector<unsigned char> fast(128 * 128 * 4), slow(fast.size());
                isolated.frame(1. / 60, target);
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, fast.data());
                isolated.parameters().copyElision = false;
                isolated.frame(1. / 60, target);
                glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, slow.data());
                require(fast == slow && glGetError() == GL_NO_ERROR,
                        "Producer Solo differs under copy elision");
                isolated.releaseGraphics();
                std::cout << "PASS: producer Solo keeps its direct component output.\n";
            }
            effect.selectPreset(18);
            effect.components().items.clear();
            ImageAssets images;
            std::vector<uint8_t> pixels(32 * 16 * 4, 255);
            for (size_t n = 0; n < pixels.size(); n += 4) {
                pixels[n] = 40;
                pixels[n + 1] = 120;
                pixels[n + 2] = 210;
            }
            int image = images.addPixels("Colour field", 32, 16, pixels);
            auto &asset = images.at(image);
            asset.wrapU = 1;
            asset.wrapV = 2;
            effect.setImageAssets(images);
            int plane = effect.components().add(ComponentKind::Plane);
            effect.components().at(plane).source = image;
            effect.components().at(plane).animated = false;
            std::ostringstream saved;
            effect.writeScene(saved);
            Runtime restored(argv[1]);
            std::istringstream input(saved.str());
            restored.readScene(input);
            require(restored.imageAssets().at(image).pixels() == pixels,
                    "Embedded image changed on scene roundtrip");
            restored.frame(1. / 60, target);
            std::cout << "Image uploads " << restored.statistics().imageUploads << " total "
                      << restored.statistics().totalImageUploads << " draws "
                      << restored.statistics().drawCalls << " source "
                      << restored.components().at(plane).source << '\n';
            require(restored.statistics().imageUploads == 1, "Image first upload missing");
            restored.frame(0, target);
            require(restored.statistics().imageUploads == 0, "Static image uploaded again");
            auto before = restored.statistics().totalImageUploads;
            restored.abandonGraphics();
            effect.abandonGraphics();
            SDL_GL_DestroyContext(context);
            context = SDL_GL_CreateContext(window);
            require(context != nullptr, SDL_GetError());
            initialize();
            restored.frame(0, target);
            require(restored.statistics().totalImageUploads == before + 1,
                    "Image not rebuilt after context loss");
            require(glGetError() == GL_NO_ERROR, "Context recovery GLES error");
            restored.releaseGraphics();
            effect.releaseGraphics();
            std::cout << "PASS: v15 embedded images, sampler settings, static upload cache and "
                         "GLES context recovery.\n";
        }
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
