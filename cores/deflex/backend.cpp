#include "neon/backend_setup.h"
#include "backend.h"
#include "neon_audio.h"
#include <climits>
#include <stdexcept>
namespace deflex {
namespace Neon_Internals {
thread_local neon::DarwinRandom *boundRandom = nullptr;
WaveState *Waves = nullptr;
int neonRand() {
    if (!boundRandom)
        throw std::logic_error("unbound Deflex RNG");
    return int(boundRandom->next());
}
void neonSrand(uint32_t n) {
    if (!boundRandom)
        throw std::logic_error("unbound Deflex RNG");
    boundRandom->seed(n);
}
double oxRNG::rand(double a, double b) {
    if (!boundRandom)
        throw std::logic_error("unbound Deflex RNG");
    return boundRandom->range(a, b);
}
double oxRNG::rand(double b) {
    return rand(0, b);
}
int oxRNG::irand(double b) {
    return int(rand(b));
}
colour getDynamicColour(int t, float a, int f) {
    const auto c =
        neon::paletteColour(t, a, uint32_t(f), {neon::WashRule::TriangularRainbow, 12, 4});
    return {c.r, c.g, c.b, c.a};
}
class NativeGrid final : public NeonModule_Base {
  public:
    NativeGrid() {
        mode = 600;
        kind = 1;
    }
    void setBasics() override {
        kind = 1;
        iconIndex = 4;
    }
    void setDefaults() override {
        setBasics();
        for (int i = 24; i <= 31; ++i)
            p(i) = 1;
    }
    int Init_Module_Resources(NeonDevice *d, int i, NeonStack *s) override {
        device = d;
        index = i;
        stack = s;
        initialised = 1;
        for (int &e : paramEnabled)
            e = 1;
        return 0;
    }
    void Release_Module_Resources() override {
        initialised = 0;
    }
    int Run_Module() override {
        if (!initialised)
            return -1;
        auto *backend = dynamic_cast<Backend *>(stack->effect->gameHost);
        if (!backend)
            return 0;
        auto &r = stack->render;
        matrix4f identity;
        D3DXMatrixIdentity(&identity);
        r.shader = r.shaderAux = 0;
        r.world = identity;
        for (auto &m : r.texMatrix)
            m = identity;
        SetModuleOutputRT(&r);
        SetModuleSourceTextures(&r);
        device->SetupDefaultProjection(stack->rtW, int(float(stack->rtH) * stack->effect->aspect),
                                       0);
        device->SetupDefaultCamera();
        FillShaderConstants(0, false, 0, 0, stack->rtW / 2, stack->rtH / 2, 1, 1, nullptr, nullptr,
                            r.constants, r.constEnabled);
        for (int i = 0; i < 4; ++i)
            r.constants[20 + i] = float(p(24 + i));
        r.constEnabled[5] = true;
        r.primitive = D3DPT_TRIANGLELIST;
        sprite target;
        target.batchStack = stack;
        target.batchPtr = stack->verts;
        target.flags = 3;
        backend->grid(target, double(stack->rtW), double(stack->rtH));
        target.setupPurge();
        return 0;
    }
};
NeonModule_Base *createModule(int type) {
    if (type == 200 || type == 300 || (type >= 400 && type <= 406) || type == 499)
        return nullptr;
    NeonModule_Base *p = type == 500   ? static_cast<NeonModule_Base *>(new showJoy)
                         : type == 600 ? static_cast<NeonModule_Base *>(new NativeGrid)
                                       : new Mixer;
    p->typeId = type;
    return p;
}
} // namespace Neon_Internals
using namespace Neon_Internals;
namespace {
// Binds the original immediate actor and character-grid batches to shared GL.
class Commands final : public ActorRenderer, public GridRenderer {
    sprite &target_;
    int literal_ = -1;
    bool additive_ = false;
    void select(int literal, bool additive) {
        if (literal_ == literal && additive_ == additive)
            return;
        flush();
        literal_ = literal;
        additive_ = additive;
        target_.batchStack->render.SetSourceFromLiterals(literal, literal, literal);
        target_.batchStack->render.device->SetAlphaBlend(additive ? 1 : 0x302, 0x303);
    }

  public:
    explicit Commands(sprite &target) : target_(target) {}
    void flush() {
        target_.setupPurge();
        target_.batchCount = 0;
        target_.batchPtr = target_.batchStack->verts;
    }
    void emit(const ActorSprite &command) override {
        const auto *entry = renderSession().resources->sprite(command.frame);
        select(entry ? entry->page : 0, false);
        target_.frame = command.frame;
        target_.pos = command.position;
        target_.width = command.size.x;
        target_.height = command.size.y;
        target_.col = command.colour;
        target_.angle = command.angle;
        target_.zOffset = command.depth;
        target_.flags = 3 | command.orFlags;
        if (command.region) {
            target_.regionX = command.regionOrigin.x;
            target_.regionY = command.regionOrigin.y;
            target_.regionW = command.regionSize.x;
            target_.regionH = command.regionSize.y;
        }
        target_.corners = command.customCorners ? command.corners.data() : nullptr;
        if (command.customCorners)
            target_.flags |= 0x800;
        target_.setupChecking();
    }
    void quad(const std::array<GridVertex, 6> &q, bool additive, int texture) override {
        select(texture, additive);
        if (target_.batchCount > 0x1fc4)
            flush();
        for (const auto &v : q) {
            auto &n = *target_.batchPtr++;
            n.x = v.x;
            n.y = v.y;
            n.z = v.z;
            n.u0 = n.u1 = n.u2 = v.u;
            n.v0 = n.v1 = n.v2 = v.v;
            n.colour = v.colour;
        }
        target_.batchCount += 6;
    }
};
} // namespace
bool Backend::init(mino::Host &host, const mino::SurfaceInfo &surface, bool startup) {
    auto scope = binding_.bind();
    std::vector<uint8_t> bytes;
    std::string error;
    if (!neon::backend::readCatalog(host, resources_, bytes, error)) {
        host.log("Deflex resources: %s", error.c_str());
        return false;
    }
    if (!neon::backend::initializeDevice(device_, host, surface, 480, 320, [] {}))
        return false;
    binding_.connect(resources_, device_, drawing_);
    if (!neon::backend::initializeDrawing(drawing_, device_, {0, 24000}, {0, 24000},
                                          neon::backend::DrawingOrder::ConfigureEach,
                                          neon::backend::BufferFailure::Stop))
        return false;
    binding_.beginBatch();
    binding_.sourceTexture = -1;
    startupOverlay_ = startup;
    loadingStarted_ = std::chrono::steady_clock::now();
    loadingLiteral_ = 0;
    device_.InitLiteralTextures(1, {"Default-LandscapeLeft.png"});
    if (!device_.literal(0) || !device_.literal(0)->name)
        return false;
    if (startup) {
        presentLoading();
        host.presentStartupFrame();
    }
    auto literals = resources_.literals();
    loadingLiteral_ = int(literals.size());
    literals.push_back("Default-LandscapeLeft.png");
    device_.InitLiteralTextures(int(literals.size()), literals);
    std::vector<superSaver> cells;
    if (!host.readAsset("testGrid.ox", bytes) || !loadGrid(bytes, cells) || !cells[0].hasDNA)
        return false;
    effect_.reset(cells[0].createEffectFromDNA(&device_, 0, this));
    if (!effect_)
        return false;
    auto *last = neon::backend::visitModules(*effect_, [&](auto &stack, auto &module) {
        host.log("Deflex effect: stack %d module %d type %d mode %d", stack.stackIndex,
                 module.index, module.typeId, module.mode);
    });
    drawing_.effect = effect_.get();
    drawing_.effectStack = last ? last->stackIndex : 0;
    return ready_;
}
void Backend::grid(sprite &target, double w, double h) {
    Commands out(target);
    if (gridScene)
        gridScene(out, out, {w, h}, paused_);
    else if (world_)
        world_->drawGrid(out, out, {w, h}, paused_);
    out.flush();
}
void Backend::drawForeground(sprite &target, double w, double h) {
    Commands out(target);
    if (actorsScene)
        actorsScene(out, {w, h}, paused_);
    else if (world_) {
        world_->drawActors(out, {w, h}, false, paused_);
        world_->drawActors(out, {w, h}, true, paused_);
    }
    if (overlay)
        overlay(out, out, {w, h});
    out.flush();
}
void Backend::execute(World *world, bool paused) {
    auto scope = binding_.bind();
    world_ = world;
    paused_ = paused;
    auto *prior = boundRandom;
    neon::DarwinRandom scratch(1);
    boundRandom = world ? &world->random : &scratch;
    try {
        device_.BeginFrame();
        device_.SetCurrenRT(-1);
        device_.alphaEnable(true);
        effect_->aspect = 1;
        effect_->RunEffect();
    } catch (...) {
        boundRandom = prior;
        throw;
    }
    boundRandom = prior;
}
void Backend::present() {
    if (!effect_)
        return;
    auto scope = binding_.bind();
    neon::backend::beginOutput(device_, drawing_, 480, 320);
    binding_.beginBatch();
    sprite output;
    output.frame = INT_MIN;
    output.width = 480;
    output.height = 320;
    output.col.white(1);
    output.draw();
    purge();
    // UIView's original startup overlay fades over 0.8 real seconds, including
    // loading time. This is independent of accelerated simulation/capture ticks.
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - loadingStarted_).count();
    if (startupOverlay_ && elapsed < .8)
        loadingImage(float(1 - elapsed / .8));
    device_.PresentFrame();
}
void Backend::presentLoading() {
    neon::backend::beginOutput(device_, drawing_, 480, 320);
    loadingImage(1);
    device_.PresentFrame();
}
void Backend::loadingImage(float alpha) {
    binding_.beginBatch();
    settex(loadingLiteral_);
    device_.alphaEnable(true);
    device_.SetAlphaBlend(0x302, 0x303);
    sprite image;
    image.frame = -1;
    image.width = 480;
    image.height = 320;
    image.col.white(alpha);
    image.setupSimple(binding_.base);
    const auto *texture = device_.literal(loadingLiteral_);
    float u = float(texture->imageW) / texture->width, v = float(texture->imageH) / texture->height;
    const float uv[6][2] = {{0, 0}, {0, v}, {u, v}, {u, v}, {u, 0}, {0, 0}};
    for (int i = 0; i < 6; ++i) {
        auto &p = binding_.base[i];
        p.u0 = p.u1 = p.u2 = uv[i][0];
        p.v0 = p.v1 = p.v2 = uv[i][1];
    }
    binding_.cursor = binding_.base + 6;
    binding_.count = 6;
    purge();
}
void Backend::resize(const mino::SurfaceInfo &s) {
    auto scope = binding_.bind();
    device_.SetBackbuffer(s.framebuffer, 480, 320, s.width, s.height);
}
void Backend::release() {
    auto scope = binding_.bind();
    effect_.reset();
    drawing_.effect = nullptr;
    device_.Destroy(true);
    binding_.disconnect();
    world_ = nullptr;
    ready_ = false;
}
} // namespace deflex
namespace deflex {
void AudioSystemPlaySample(int, int, int) {
    throw std::logic_error("unexpected legacy Deflex audio callback");
}
} // namespace deflex
