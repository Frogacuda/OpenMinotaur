#include "live_bezel.h"
#include "effect_runtime.h"
#include "live_rig.h"
#include "neon_performance.h"
#include "presentation_assets.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>
namespace mino {
namespace {
bool effectRead(const std::filesystem::path &name, std::vector<uint8_t> &bytes) {
    return readPresentationAsset("neon/" + name.generic_string(), bytes);
}
std::string asset(const std::string &name) {
    std::vector<uint8_t> bytes;
    if (!readPresentationAsset(name, bytes))
        throw std::runtime_error(std::string("Missing live bezel asset: ") + name);
    return {bytes.begin(), bytes.end()};
}
} // namespace
struct LiveBezel::Impl {
    std::string game;
    NeonCadence cadence;
    NeonPacer pacer;
    bool assessing = false;
    bool lowQuality = false;
    int lastWidth = 0, lastHeight = 0, lastTurns = -1;
    std::filesystem::path shaderCache;
    std::unique_ptr<neon::effects::Runtime> effect;
    std::unique_ptr<neon::effects::RigPlayer> rig;
    double rate = 1, pending = 0;
    float baselineHue = 0;
    uint64_t sequence = 0;
    bool failed = false, primed = false, shadersReady = false, refreshed = false;
    std::vector<std::pair<std::string, std::string>> bindings;
};
LiveBezel::LiveBezel() : impl_(std::make_unique<Impl>()) {}
LiveBezel::~LiveBezel() = default;
void LiveBezel::setShaderCacheDirectory(const std::filesystem::path &path) {
    impl_->shaderCache = path;
    if (impl_->effect)
        impl_->effect->setShaderCacheDirectory(path);
}
void LiveBezel::setGameplayTexture(unsigned texture, int width, int height) {
    if (impl_->effect)
        impl_->effect->setGameplayTexture(texture, width, height);
}
void LiveBezel::beginCadenceAssessment() {
    impl_->cadence.reset();
    impl_->pacer.reset();
    impl_->assessing = true;
}
void LiveBezel::finishCadenceAssessment() {
    if (!impl_->assessing)
        return;
    impl_->cadence.finish();
    impl_->pacer.reset();
    impl_->assessing = false;
}
bool LiveBezel::assessingCadence() const {
    return impl_->assessing;
}
void LiveBezel::discardPendingFrame() {
    impl_->pending = 0;
    impl_->pacer.reset();
    impl_->refreshed = false;
}
void LiveBezel::observeFrame(double workMs, double intervalMs) {
    if (available() && impl_->assessing)
        impl_->cadence.observe(workMs, intervalMs);
}
unsigned LiveBezel::cadenceDivisor() const {
    return impl_->cadence.divisor();
}
bool LiveBezel::refreshedFrame() const {
    return impl_->refreshed;
}
bool LiveBezel::available() const {
    return impl_->effect && !impl_->failed;
}
void LiveBezel::reset() {
    auto &i = *impl_;
    if (i.effect)
        i.effect->releaseGraphics();
    auto cache = std::move(i.shaderCache);
    i = Impl{};
    i.shaderCache = std::move(cache);
}
void LiveBezel::abandonGraphics() {
    if (impl_->effect)
        impl_->effect->abandonGraphics();
    impl_->shadersReady = false;
}
void LiveBezel::update(const std::string &game, PresentationState state, double dt,
                       bool lowQuality) {
    auto &i = *impl_;
    if (i.game != game) {
        reset();
        i.game = game;
    }
    const char *stem = game == "goatup"               ? "GoatUp"
                       : game == "goatup2"            ? "GoatUp2"
                       : game == "minorescue"         ? "MinotaurRescue"
                       : game == "minotron"           ? "Minotron"
                       : game == "superoxwars"        ? "SuperOxWars"
                       : game == "fiveaday"           ? "FiveADay"
                       : game == "deflex"             ? "Deflex"
                       : game == "minos"              ? "Minos"
                       : game == "gridrunner"         ? "Gridrunner"
                       : game == "gridrunner-classic" ? "GridrunnerClassic"
                                                      : nullptr;
    if (!stem || i.failed)
        return;
    try {
        if (!i.effect) {
            auto resolve = [](const char *name) -> void * {
                return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            };
            if (!neon::effects::Runtime::loadGlesEntryPoints(resolve))
                throw std::runtime_error("GLES entry points unavailable");
            auto path = std::string("bezels/") + stem;
            std::istringstream scene(asset(path + ".neonb"));
            i.effect = neon::effects::Runtime::loadBaked(scene, {}, effectRead, &i.rate);
            i.effect->parameters().skipTransparentMixerPasses = game == "minotron";
            i.effect->setShaderCacheDirectory(i.shaderCache);
            i.baselineHue = i.effect->parameters().hueShift;
            std::istringstream definitions(asset(path + ".nhml"));
            i.rig = std::make_unique<neon::effects::RigPlayer>(
                neon::effects::RigProgram::read(definitions));
            const auto parameters = i.effect->responseParameters();
            std::set<std::string> installed;
            auto install = [&](const std::string &target, float low, float high) {
                auto found = std::find_if(parameters.begin(), parameters.end(),
                                          [&](const auto &p) { return p.key == target; });
                if (found == parameters.end() || low < found->low || high > found->high ||
                    low >= high)
                    throw std::runtime_error("Live rig target/range unavailable: " + target);
                if (target == "runtime.hue")
                    return;
                if (!installed.insert(target).second) {
                    for (auto &sequence : i.effect->responses().sequences)
                        if (sequence.name == target) {
                            sequence.inputLow = std::min(sequence.inputLow, low);
                            sequence.inputHigh = std::max(sequence.inputHigh, high);
                            sequence.targets.front().low = sequence.inputLow;
                            sequence.targets.front().high = sequence.inputHigh;
                        }
                    return;
                }
                neon::effects::ResponseSequence s;
                s.name = target;
                s.mode = neon::effects::ResponseMode::Input;
                s.source = "live.value." + std::to_string(i.bindings.size());
                s.inputLow = low;
                s.inputHigh = high;
                s.targets = {{target, neon::effects::ResponseWave::Constant,
                              neon::effects::ResponseBlend::Replace, 1, 0, low, high}};
                i.bindings.push_back({target, s.source});
                i.effect->responses().sequences.push_back(s);
            };
            for (const auto &r : i.rig->program().indexed)
                install(r.target, float(r.low), float(r.high));
            for (const auto &r : i.rig->program().values)
                install(r.target, float(r.low), float(r.high));
            for (const auto &r : i.rig->program().envelopes) {
                auto found = std::find_if(parameters.begin(), parameters.end(),
                                          [&](const auto &p) { return p.key == r.target; });
                if (found == parameters.end())
                    throw std::runtime_error("Live state envelope target unavailable: " + r.target);
                install(r.target, found->low, found->high);
            }
            for (const auto &r : i.rig->program().pulses) {
                if (r.target == "runtime.hue")
                    continue;
                auto found = std::find_if(parameters.begin(), parameters.end(),
                                          [&](const auto &p) { return p.key == r.target; });
                if (found == parameters.end())
                    throw std::runtime_error("Live pulse target unavailable: " + r.target);
                install(r.target, found->low, found->high);
            }
            for (const auto &r : i.rig->program().counters) {
                install(r.target, float(r.low), float(r.high));
                if (r.target == "runtime.hue")
                    i.baselineHue = float(r.baseline);
            }
            for (const auto &r : i.rig->program().cycles) {
                if (r.target == "runtime.hue")
                    continue;
                auto found = std::find_if(parameters.begin(), parameters.end(),
                                          [&](const auto &p) { return p.key == r.target; });
                if (found == parameters.end())
                    throw std::runtime_error("Live cycle target unavailable: " + r.target);
                install(r.target, found->low, found->high);
            }
            for (const auto &r : i.rig->program().states) {
                float low = r.baseline, high = r.baseline;
                for (const auto &c : r.choices) {
                    low = std::min(low, c.value);
                    high = std::max(high, c.value);
                }
                install(r.target, low, high);
            }
            for (const auto &r : i.rig->program().steps)
                install(r.target, float(r.low), float(r.high));
            for (const auto &r : i.rig->program().fadeIndices) {
                auto choices = r.entries;
                choices.push_back(r.baseline);
                install(r.target, float(*std::min_element(choices.begin(), choices.end())),
                        float(*std::max_element(choices.begin(), choices.end())));
                auto mask = std::find_if(parameters.begin(), parameters.end(),
                                         [&](const auto &p) { return p.key == r.mask; });
                if (mask == parameters.end())
                    throw std::runtime_error("Live fade mask unavailable: " + r.mask);
                install(r.mask, mask->low, mask->high);
            }
            i.effect->responses().rebuild();
        }
        if (!i.shadersReady) {
            for (const auto &rule : i.rig->program().states) {
                std::vector<float> choices{rule.baseline};
                for (const auto &option : rule.choices)
                    choices.push_back(option.value);
                i.effect->prepareShaderChoices(rule.target, choices);
            }
            for (const auto &rule : i.rig->program().fadeIndices) {
                std::vector<float> choices{float(rule.baseline)};
                for (double value : rule.entries)
                    choices.push_back(float(value));
                i.effect->prepareShaderChoices(rule.target, choices);
            }
            i.shadersReady = true;
        }
        if (i.lowQuality != lowQuality) {
            i.lowQuality = lowQuality;
            i.pacer.reset();
        }
        // LQ retains the established mobile fill-rate budget; HQ sharpens both
        // final composition and the visible Gridrunner base producer.
        if (game == "gridrunner" || game == "gridrunner-classic")
            i.effect->components().at(3).resolution = lowQuality ? 512 : 1024;
        i.effect->parameters().renderScale = lowQuality ? .5f : 1.f;
        neon::effects::ReadSnapshot sample;
        sample.game = game;
        sample.session = state.session;
        sample.sequence = ++i.sequence;
        sample.values = std::move(state.values);
        for (const auto &[name, items] : state.collections) {
            auto &output = sample.collections[name];
            for (const auto &item : items)
                output.push_back({item.id, item.value, item.colourSource});
        }
        const auto &values = i.rig->sample(sample, dt, state.gameplay, state.paused);
        i.effect->setSpriteColours(state.colours);
        for (const auto &action : i.rig->insertions())
            i.effect->insertSprite(action.node, action.frame, action.count, action.speed,
                                   action.colourSource);
        bool routesChanged = false;
        for (const auto &binding : i.bindings) {
            auto value = values.find(binding.first);
            for (auto &sequence : i.effect->responses().sequences)
                if (sequence.source == binding.second &&
                    sequence.enabled != (value != values.end())) {
                    sequence.enabled = value != values.end();
                    routesChanged = true;
                }
            if (value == values.end())
                i.effect->clearInput(binding.second);
            else
                i.effect->setInput(binding.second, value->second);
        }
        if (routesChanged)
            i.effect->responses().rebuild();
        auto hue = values.find("runtime.hue");
        float wrapped = std::fmod(hue == values.end() ? i.baselineHue : hue->second, 1.f);
        i.effect->parameters().hueShift = wrapped < 0 ? wrapped + 1 : wrapped;
        i.pending = state.gameplay && state.paused ? 0 : i.pending + dt;
        // The loading presentation may arrive before any simulation time has elapsed.
        if (!i.primed) {
            i.pending = std::max(i.pending, 1. / 60.);
            i.primed = true;
        }
    } catch (const std::exception &e) {
        SDL_Log("Live bezel unavailable: %s", e.what());
        reset();
        i.game = game;
        i.failed = true;
    }
}
bool LiveBezel::draw(unsigned framebuffer, int width, int height, int x, int y, int w, int h,
                     int turns, bool exclusivePass) {
    auto &i = *impl_;
    i.refreshed = false;
    if (!i.effect || i.failed)
        return false;
    try {
        if (w >= width && h >= height) {
            discardPendingFrame();
            return true;
        }
        if (i.lastWidth != width || i.lastHeight != height || i.lastTurns != turns) {
            i.pacer.reset();
            i.lastWidth = width;
            i.lastHeight = height;
            i.lastTurns = turns;
        }
        const neon::effects::Destination destination{
            framebuffer,           width,         height,
            {0, 0, width, height}, {x, y, w, h},  turns,
            !exclusivePass,        exclusivePass, i.game == "minos" || i.game == "minotron"};
        const bool due = i.pacer.due(cadenceDivisor());
        if (!due) {
            if (i.effect->presentCached(destination)) {
                return true;
            }
            i.pacer.reset();
        }
        const double elapsed = std::exchange(i.pending, 0.) * i.rate;
        // Advance animation clocks, then render each effect graph once. Late game
        // frames must not cause multiple GPU passes to recover elapsed time.
        i.effect->frameSampled(elapsed, destination);
        i.refreshed = true;
        return true;
    } catch (const std::exception &e) {
        SDL_Log("Live bezel rendering failed: %s", e.what());
        auto game = i.game;
        reset();
        i.game = game;
        i.failed = true;
        return false;
    }
}
} // namespace mino
