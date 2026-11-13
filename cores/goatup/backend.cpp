#include "neon/backend_setup.h"
#include "neon/sprite_commands.h"
#include "backend.h"
#include "neon_audio.h"
#include <climits>
#include <stdexcept>

namespace goatup {
namespace Neon_Internals {
thread_local neon::DarwinRandom *boundRandom = nullptr;
WaveState *Waves = nullptr;
void neonSrand(uint32_t seed) {
    if (!boundRandom)
        throw std::logic_error("Unbound GoatUp random stream");
    boundRandom->seed(seed);
}
int neonRand() {
    if (!boundRandom)
        throw std::logic_error("Unbound GoatUp random stream");
    return int(boundRandom->next());
}
double oxRNG::rand(double low, double high) {
    if (!boundRandom)
        throw std::logic_error("Unbound GoatUp random stream");
    return boundRandom->range(low, high);
}
double oxRNG::rand(double high) {
    return rand(0, high);
}
int oxRNG::irand(double high) {
    return int(rand(high));
}
colour getDynamicColour(int type, float alpha, int frame) {
    const auto c = neon::paletteColour(type, alpha, uint32_t(frame),
                                       {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
NeonModule_Base *createModule(int type) {
    if (type == 200 || type == 300 || (type >= 400 && type <= 406) || type == 499)
        return nullptr;
    NeonModule_Base *p = type == 500   ? static_cast<NeonModule_Base *>(new showJoy)
                         : type == 600 ? static_cast<NeonModule_Base *>(new cellGrid)
                                       : new Mixer;
    p->typeId = type;
    return p;
}
} // namespace Neon_Internals
using namespace Neon_Internals;
namespace {
constexpr int Width = 320, Height = 480;
}
bool Backend::init(mino::Host &host, const mino::SurfaceInfo &surface, bool startup) {
    auto binding = binding_.bind();
    std::vector<uint8_t> data;
    std::string error;
    if (!neon::backend::readCatalog(host, resources_, data, error)) {
        host.log("GoatUp resources: %s", error.c_str());
        return false;
    }
    if (!neon::backend::initializeDevice(*device_, host, surface, Width, Height,
                                         [&] { deviceReady_ = true; }))
        return false;
    binding_.connect(resources_, *device_, context_);
    if (!neon::backend::initializeDrawing(context_, *device_, {0, 24000}, {0, 24000},
                                          neon::backend::DrawingOrder::ConfigureEach,
                                          neon::backend::BufferFailure::Stop))
        return false;
    binding_.beginBatch();
    binding_.sourceTexture = -1;
    // The original UIKit launch image precedes the effect and sample load.
    device_->InitLiteralTextures(1, {"Default@2x.png"});
    if (startup) {
        presentLoading();
        host.presentStartupFrame();
    }
    auto literals = resources_.literals();
    literals.push_back("Default@2x.png");
    device_->InitLiteralTextures(int(literals.size()), literals);
    if (!neon::backend::validateLiterals(*device_, int(resources_.literals().size())))
        return false;
    std::vector<superSaver> cells;
    if (!host.readAsset("testGrid.ox", data) || !loadGrid(data, cells) || cells.empty() ||
        !cells[0].hasDNA)
        return false;
    effect_.reset(cells[0].createEffectFromDNA(device_.get(), 0, this));
    if (!effect_) {
        host.log("GoatUp effect: %s", cells[0].error.c_str());
        return false;
    }
    auto *last = neon::backend::visitModules(*effect_, [&](auto &, auto &value) {
        auto *module = &value;
        host.log("GoatUp effect module: type %d, mode %d", module->typeId, module->mode);
        if (auto *grid = dynamic_cast<cellGrid *>(module)) {
            grid->cls();
            grid->clattr();
        }
    });
    context_.effect = effect_.get();
    context_.effectStack = last ? last->stackIndex : 0;
    return ready_;
}
void Backend::resize(const mino::SurfaceInfo &surface) {
    auto scope = binding_.bind();
    device_->SetBackbuffer(surface.framebuffer, Width, Height, surface.width, surface.height);
}
void Backend::execute(neon::DarwinRandom &random) {
    if (!effect_)
        throw std::logic_error("GoatUp effect is not initialized");
    auto scope = binding_.bind();
    auto *prior = boundRandom;
    boundRandom = &random;
    try {
        device_->BeginFrame();
        device_->SetCurrenRT(-1);
        device_->alphaEnable(true);
        effect_->aspect = 1;
        effect_->RunEffect();
    } catch (...) {
        boundRandom = prior;
        throw;
    }
    boundRandom = prior;
}
void Backend::drawBackground(sprite &target, double width, double height) {
    if (!background)
        throw std::logic_error("GoatUp background is not connected");
    background(target, width, height);
}
void Backend::drawForeground(sprite &target, double width, double height) {
    if (!foreground)
        throw std::logic_error("GoatUp foreground is not connected");
    foreground(target, width, height);
}
void Backend::drawSurface(sprite &target, double width, double height, const TowerSurface &tower) {
    for (const auto &q : tower.quads) {
        std::array<double, 8> corners;
        for (int n = 0; n < 4; ++n) {
            corners[size_t(n * 2)] = q.corners[size_t(n)].x * width;
            corners[size_t(n * 2 + 1)] = q.corners[size_t(n)].y * height;
        }
        target.flags = 3 | 0x800;
        target.frame = q.frame;
        target.angle = 0;
        target.col.white(q.alpha);
        target.corners = corners.data();
        target.setupChecking();
    }
    target.corners = nullptr;
    target.flags = 3;
}
void Backend::drawActors(sprite &target, double width, double height,
                         const std::vector<ActorDraw> &actors) {
    neon::sprites::executeLiteralCommands(
        target, resources_, actors, [](const ActorDraw &actor) { return actor.frame + 2; },
        [&](sprite &target, const ActorDraw &actor, int frame) {
            const float extent = actor.particle ? actor.radius : float(actor.radius + actor.radius);
            target.flags = 3;
            target.frame = frame;
            target.angle = actor.angle;
            target.col = actor.colour;
            neon::sprites::normalizedActorRectangle(target, actor, width, height, extent);
            target.setupChecking();
        },
        [](int) {
            throw std::logic_error("Original GoatUp actor frame is not in its resource catalog");
        });
}
void Backend::present() {
    if (!effect_)
        return;
    auto scope = binding_.bind();
    neon::backend::beginOutput(*device_, context_, Width, Height);
    binding_.beginBatch();
    sprite image;
    image.frame = INT_MIN;
    image.width = Width;
    image.height = Height;
    image.col.white(1);
    image.draw();
    purge();
    device_->PresentFrame();
}
void Backend::presentLoading() {
    if (!deviceReady_)
        return;
    auto scope = binding_.bind();
    neon::backend::beginOutput(*device_, context_, Width, Height);
    binding_.beginBatch();
    // Before the catalog literals load, the startup image is the sole literal.
    const int loading = int(resources_.literals().size());
    const int index = device_->literal(loading) && device_->literal(loading)->name ? loading : 0;
    settex(index);
    sprite image;
    image.frame = -1;
    image.width = Width;
    image.height = Height;
    image.col.white(1);
    image.setupSimple(binding_.base);
    const auto *texture = device_->literal(index);
    if (!texture || !texture->name)
        return;
    const float u = float(texture->imageW) / texture->width,
                v = float(texture->imageH) / texture->height;
    const float uv[6][2] = {{0, 0}, {0, v}, {u, v}, {u, v}, {u, 0}, {0, 0}};
    for (int n = 0; n < 6; ++n) {
        auto &vertex = binding_.base[n];
        vertex.u0 = vertex.u1 = vertex.u2 = uv[n][0];
        vertex.v0 = vertex.v1 = vertex.v2 = uv[n][1];
    }
    binding_.cursor = binding_.base + 6;
    binding_.count = 6;
    purge();
    device_->PresentFrame();
}
void Backend::release() {
    auto scope = binding_.bind();
    neon::backend::releaseReplacingDevice(
        effect_, context_, device_, deviceReady_, ready_, binding_,
        [&] {
            background = {};
            foreground = {};
            text = {};
        },
        [] { return std::make_unique<NeonDevice>(); });
}
void AudioSystemPlaySample(int, int, int) {
    throw std::logic_error("Unbound GoatUp audio callback");
}
} // namespace goatup
