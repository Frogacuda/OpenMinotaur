#include "neon/backend_setup.h"
#include "stage_backend.h"
#include <climits>
#include <stdexcept>

namespace goatup2 {
namespace Neon_Internals {
extern thread_local neon::DarwinRandom *boundRandom;
extern thread_local neon::audio::SampleMixer *boundMixer;
} // namespace Neon_Internals
using namespace Neon_Internals;
namespace {
constexpr int Width = 320, Height = 480;
struct BoundEngine {
    BoundEngine(StageRuntime &world, neon::audio::SampleMixer &mixer)
        : priorRandom(boundRandom), priorMixer(boundMixer) {
        boundRandom = &world.random;
        boundMixer = &mixer;
    }
    ~BoundEngine() {
        boundRandom = priorRandom;
        boundMixer = priorMixer;
    }
    neon::DarwinRandom *priorRandom;
    neon::audio::SampleMixer *priorMixer;
};
} // namespace
bool StageBackend::init(mino::Host &host, const mino::SurfaceInfo &surface, bool startup) {
    startupPresented_ = false;
    auto binding = state_.bind();
    std::vector<uint8_t> data;
    std::string error;
    if (!neon::backend::readCatalog(host, resources_, data, error)) {
        host.log("GoatUp 2 resources: %s", error.c_str());
        return false;
    }
    if (!neon::backend::initializeDevice(device_, host, surface, Width, Height,
                                         [&] { deviceReady_ = true; }))
        return false;
    state_.connect(resources_, device_, context_);
    if (!neon::backend::initializeDrawing(context_, device_, {0, 24000}, {0, 24000},
                                          neon::backend::DrawingOrder::ConfigureEach,
                                          neon::backend::BufferFailure::Stop))
        return false;
    state_.beginBatch();
    state_.sourceTexture = -1;
    if (startup) {
        loadingLiteral_ = 0;
        device_.InitLiteralTextures(1, {"__goatup2_launch.png"});
        if (!device_.literal(0) || !device_.literal(0)->name)
            return false;
        presentLoading();
        startupPresented_ = host.presentStartupFrame();
    }
    auto literalFiles = resources_.literals();
    loadingLiteral_ = int(literalFiles.size());
    literalFiles.push_back("__goatup2_launch.png");
    device_.InitLiteralTextures(int(literalFiles.size()), literalFiles);
    if (!neon::backend::validateLiterals(device_, int(literalFiles.size())))
        return false;
    std::vector<superSaver> cells;
    if (!host.readAsset("testGrid.ox", data) || !loadGrid(data, cells) || cells.empty() ||
        !cells[0].hasDNA)
        return false;
    effect_.reset(cells[0].createEffectFromDNA(&device_, 0, this));
    if (!effect_) {
        host.log("GoatUp 2 effect: %s", cells[0].error.c_str());
        return false;
    }
    auto *last = neon::backend::visitModules(*effect_, [&](auto &, auto &module) {
        if (auto *text = dynamic_cast<cellGrid *>(&module)) {
            grid = text;
            grid->setSize(42, 46);
            grid->fontY = 162;
            grid->cls();
            grid->clattr();
        }
    });
    context_.effect = effect_.get();
    context_.effectStack = last ? last->stackIndex : 0;
    return prepared_;
}
void StageBackend::applyHud(const StageHud &hud) {
    if (!grid)
        throw std::logic_error("original HUD character grid missing");
    for (int row : hud.clearRows)
        grid->cll(row);
    for (const auto &line : hud.text) {
        grid->printColour = line.colour;
        grid->printAt(line.column, line.row, line.text.c_str());
    }
}
void StageBackend::clearHudRows(const std::vector<int32_t> &rows) {
    if (!grid)
        throw std::logic_error("original HUD character grid missing");
    for (int row : rows)
        grid->cll(row);
}
void StageBackend::clearHud() {
    if (!grid)
        throw std::logic_error("original HUD character grid missing");
    grid->cls();
}
void StageBackend::resize(const mino::SurfaceInfo &surface) {
    auto binding = state_.bind();
    device_.SetBackbuffer(surface.framebuffer, Width, Height, surface.width, surface.height);
}
void StageBackend::execute(const StagePresentation &frame, StageRuntime &world,
                           neon::audio::SampleMixer &mixer) {
    if (!effect_)
        throw std::logic_error("GoatUp stage effect not initialized");
    auto binding = state_.bind();
    BoundEngine services(world, mixer);
    auto *priorWaves = Waves;
    Waves = &waves_;
    frame_ = &frame;
    try {
        device_.BeginFrame();
        device_.SetCurrenRT(-1);
        device_.alphaEnable(true);
        effect_->aspect = 1;
        effect_->RunEffect();
    } catch (...) {
        frame_ = nullptr;
        Waves = priorWaves;
        throw;
    }
    frame_ = nullptr;
    Waves = priorWaves;
}
void StageBackend::drawForeground(sprite &target, double w, double h) {
    // The outer session owns its original UI list. This resource/effect adapter
    // supplies no substitute HUD, buttons or menus.
    if (foreground)
        foreground(target, w, h);
}
void StageBackend::drawBackground(sprite &target, double w, double h) {
    if (!frame_)
        throw std::logic_error("effect callback outside prepared GoatUp frame");
    drawOverlay(target, w, h, frame_->draws());
}
void StageBackend::drawOverlay(sprite &target, double w, double h,
                               const std::vector<StageDraw> &draws) {
    auto &render = target.batchStack->render;
    const int priorRT = render.srcRT[0], priorLiteral = render.srcLiteral[0],
              priorFont = render.srcFont[0], priorOther = render.srcOther[0];
    bool localPage = false;
    auto selectPage = [&](bool local) {
        if (local == localPage)
            return;
        target.setupPurge();
        target.batchPtr = target.batchStack->verts;
        // Original packets retain their stack route. The added UX atlas opts
        // into primary texture 2 and restores the incoming route afterwards.
        render.srcRT[0] = local ? -1 : priorRT;
        render.srcLiteral[0] = local ? 2 : priorLiteral;
        render.srcFont[0] = local ? -1 : priorFont;
        render.srcOther[0] = local ? -1 : priorOther;
        localPage = local;
    };
    for (const auto &d : draws) {
        const auto *entry = resources_.sprite(d.frame);
        selectPage(entry && entry->page == 2);
        target.pos = {d.position.x * w, d.position.y * h};
        target.width = d.size.x * w;
        target.height = d.size.y * h;
        target.col = d.colour;
        target.angle = d.angle;
        target.flags = d.flags;
        target.frame = d.frame;
        if (d.tile) {
            // StageTemplate's 0x1000 path supplies an explicit UV rectangle.
            // Use the shared geometry emitter, then its six original UV pairs.
            // This bypasses sprite-map UVs, not stack routing or submission.
            if (target.batchCount > NeonStack::kSpriteFlushAfter) {
                target.setupPurge();
                target.batchPtr = target.batchStack->verts;
            }
            target.setupSimple(target.batchPtr);
            const double uv[6][2] = {{d.u, d.v},
                                     {d.u, d.v + d.dv},
                                     {d.u + d.du, d.v + d.dv},
                                     {d.u + d.du, d.v + d.dv},
                                     {d.u + d.du, d.v},
                                     {d.u, d.v}};
            for (int i = 0; i < 6; ++i) {
                auto &v = target.batchPtr[i];
                v.u0 = v.u1 = v.u2 = float(uv[i][0]);
                v.v0 = v.v1 = v.v2 = float(uv[i][1]);
            }
            target.batchPtr += 6;
            target.batchCount += 6;
        } else
            target.setupChecking();
    }
    selectPage(false);
}
void StageBackend::present() {
    if (!effect_)
        return;
    auto binding = state_.bind();
    neon::backend::beginOutput(device_, context_, Width, Height);
    state_.beginBatch();
    sprite output;
    output.frame = INT_MIN;
    output.width = Width;
    output.height = Height;
    output.col.white(1);
    output.draw();
    purge();
    device_.PresentFrame();
}
void StageBackend::presentLoading() {
    if (!deviceReady_)
        return;
    auto binding = state_.bind();
    neon::backend::beginOutput(device_, context_, Width, Height);
    state_.beginBatch();
    settex(loadingLiteral_);
    sprite image;
    image.frame = -1;
    image.width = Width;
    image.height = Height;
    image.col.white(1);
    image.setupSimple(state_.base);
    const auto *texture = device_.literal(loadingLiteral_);
    if (!texture || !texture->name)
        return;
    const float u = float(texture->imageW) / texture->width,
                v = float(texture->imageH) / texture->height;
    const float uv[6][2] = {{0, 0}, {0, v}, {u, v}, {u, v}, {u, 0}, {0, 0}};
    for (int i = 0; i < 6; ++i) {
        auto &vertex = state_.base[i];
        vertex.u0 = vertex.u1 = vertex.u2 = uv[i][0];
        vertex.v0 = vertex.v1 = vertex.v2 = uv[i][1];
    }
    state_.cursor = state_.base + 6;
    state_.count = 6;
    purge();
    device_.PresentFrame();
}
void StageBackend::release() {
    auto binding = state_.bind();
    effect_.reset();
    context_.effect = nullptr;
    frame_ = nullptr;
    foreground = {};
    if (deviceReady_)
        device_.Destroy(true);
    deviceReady_ = false;
    startupPresented_ = false;
    state_.disconnect();
    state_.beginBatch();
}
} // namespace goatup2
