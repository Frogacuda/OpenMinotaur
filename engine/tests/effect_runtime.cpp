// Event expectations follow NeonStack::RunStack (Minotron ARM 0xc8dc,
// Rescue ARM 0xc6a0). No renderer or concrete game is needed to run this test.
#include "neon/effect_runtime.h"
#include "neon/immediate_texture.h"
#include "neon/math.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
namespace {
std::vector<std::string> events;
int liveModules = 0;
using namespace neon::effects;
struct Device {
    int next = 10, liveTargets = 0, liveBuffers = 0;
    int GetRTTexture(int, int) {
        ++liveTargets;
        return next++;
    }
    void ReleaseRTTexture(int) {
        --liveTargets;
    }
    int CreateVertexBufferWO(int, int, int) {
        ++liveBuffers;
        return next++;
    }
    void DestroyVertexuffer(int, int) {
        --liveBuffers;
    }
    void SetCurrenRT(int n) {
        events.push_back("target" + std::to_string(n));
    }
    void ClearScreen(int, int, int, int) {
        events.push_back("clear");
    }
    void FillSurface(int n, int) {
        events.push_back("fill" + std::to_string(n));
    }
    void SetupDefaultCamera() {
        events.push_back("camera");
    }
};
struct Render {
    std::array<int, 6> rt{}, literal{};
    int target = -1;
    void SetCurrenRenderTarget(int n) {
        target = n;
    }
    void SetSourceFromRTs(int a, int b, int c, int d, int e, int f) {
        rt = {a, b, c, d, e, f};
    }
    void SetSourceFromLiterals(int a, int b, int c, int d, int e, int f) {
        literal = {a, b, c, d, e, f};
    }
};
struct Module;
struct Stack;
struct Effect;
struct Binding {
    using ModuleType = Module;
    using StackType = Stack;
    using EffectType = Effect;
    using DeviceType = Device;
    using RenderType = Render;
    using Vector3Type = neon::D3DXVECTOR3;
};
struct Module : Binding {
    struct Param {
        double value = 0;
        int ival = 0;
    };
    struct Range {
        double min = 0, max = 1;
    };
    struct Input {
        int stack = -1, num = -1, type = -1;
        int *rtPtr = nullptr;
    };
    Param params[64];
    Range ranges[64];
    Input inputs[6];
    int paramEnabled[64];
    int typeId = 0, mode = 0, index = -1, kind = 0, prepped = 0, running = 0, wrap[6]{},
        inputSrc[3]{}, visits = 0, repeats = 0;
    unsigned inputFlags = 0;
    Module *next = nullptr;
    Stack *stack = nullptr;
    std::string label;
    Module() {
        ++liveModules;
        neon::effects::constructModule(*this);
    }
    ~Module() {
        --liveModules;
    }
    void setBasics() {}
    void Init_Module_Resources(Device *, int i, Stack *s) {
        index = i;
        stack = s;
        events.push_back("init" + label);
    }
    void Release_Module_Resources() {
        events.push_back("release" + label);
    }
    void runPrep() {
        events.push_back("prep" + label);
        neon::effects::prepareModule(*this);
    }
    int Run_Module();
    void getWaypoint(neon::WaypointData *w) {
        neon::effects::applyWaypoint(*this, w);
    }
};
struct Stack : Binding {
    static constexpr int kMaxModules = 32, kMaxVerts = 8192;
    int initialised = 0, id = -1, rtW = 512, rtH = 512, moduleCount = 0, outputRT = -1, prevRT = -1,
        vb0 = -1, vb1 = -1;
    int loopDepth = 0, loopCount[16]{}, stackIndex = 0, verts[1]{};
    Module *loopStart[16]{};
    Module *modules = nullptr;
    Stack *nextStack = nullptr;
    Device *device = nullptr;
    Effect *effect = nullptr;
    neon::matrix4f matrices[16], view;
    neon::WaypointData *waypoints[32][3][64];
    Stack() {
        neon::effects::constructStack(*this);
    }
    ~Stack() {
        neon::effects::destroyStack(*this);
    }
    int Init(Device *d, int stackId, int a, int b) {
        return neon::effects::initializeStack(*this, d, stackId, a, b);
    }
    int AddModule(Module *m, int pos) {
        return neon::effects::addModule(*this, m, pos, true);
    }
    int RunStack() {
        return neon::effects::runStack(*this);
    }
    bool waypointExists(int b, int s) const {
        return neon::effects::waypointExists(*this, b, s);
    }
    void getWaypoint(int b, int s) {
        neon::effects::selectWaypoint(*this, b, s);
    }
};
struct Effect : Binding {
    Device *device = nullptr;
    int initialised = 0, id = 0, stackCount = 0;
    void *gameHost = nullptr;
    Stack *stacks = nullptr;
    ~Effect() {
        neon::effects::destroyEffect(*this);
    }
    int Init(Device *d) {
        return neon::effects::initializeEffect(*this, d);
    }
    int AddStack(Stack *s, int a, int b) {
        return neon::effects::addStack(*this, s, a, b);
    }
    Stack *getStack(int i) {
        return neon::effects::stackByIndex(*this, i);
    }
    Stack *getTrueStack(int i) {
        return neon::effects::stackAt(*this, i);
    }
    bool AssignTextureInput(const neon::effects::TextureRoute *a,
                            const neon::effects::TextureRoute *b) {
        return neon::effects::assignTexture(*this, a, b);
    }
};
int Module::Run_Module() {
    events.push_back("run" + label);
    ++visits;
    if (kind == 3)
        REQUIRE(neon::effects::beginLoop(*stack, *this, repeats));
    return 0;
}
struct Dna : neon::EffectDna, Binding {
    Module *loadModule() {
        return neon::effects::loadModule(*this, [](int type) -> Module * {
            if (type == 200)
                return nullptr;
            auto *m = new Module;
            m->typeId = type;
            return m;
        });
    }
    int loadWaypoint(neon::WaypointData *w) {
        return neon::effects::loadWaypoint(*this, w);
    }
};
Module *append(Stack &s, const char *label, int kind = 0, int repeats = 0) {
    auto *m = new Module;
    m->label = label;
    m->kind = kind;
    m->repeats = repeats;
    REQUIRE(s.AddModule(m, -1) >= 0);
    return m;
}
void orderedAndNestedExecution() {
    Device device;
    {
        Effect effect;
        effect.Init(&device);
        auto *s = new Stack;
        effect.AddStack(s, 8, 0);
        auto *outer = append(*s, "outer", 3, 2);
        auto *inner = append(*s, "inner", 3, 3);
        auto *body = append(*s, "body");
        append(*s, "innerEnd", -1);
        append(*s, "outerEnd", -1);
        auto *tail = append(*s, "tail");
        events.clear();
        REQUIRE(s->RunStack() == 0);
        const std::vector<std::string> expected = {
            "fill11",       "target11",     "prepouter",   "prepinner",   "prepbody",
            "prepinnerEnd", "prepouterEnd", "preptail",    "runouter",    "runinner",
            "runbody",      "runinnerEnd",  "runinner",    "runbody",     "runinnerEnd",
            "runinner",     "runbody",      "runinnerEnd", "runouterEnd", "runouter",
            "runinner",     "runbody",      "runinnerEnd", "runinner",    "runbody",
            "runinnerEnd",  "runinner",     "runbody",     "runinnerEnd", "runouterEnd",
            "runtail",      "camera"};
        REQUIRE(events == expected);
        REQUIRE(body->visits == 6 && inner->visits == 6 && outer->visits == 2 && tail->visits == 1);
        REQUIRE(s->outputRT == 10 && s->prevRT == 11 && s->loopDepth == 0);
        events.clear();
        REQUIRE(s->RunStack() == 0);
        REQUIRE(body->visits == 12 && tail->visits == 2);
        REQUIRE(events[0] == "fill10" && s->prevRT == 10 && s->outputRT == 11);
    }
    REQUIRE(device.liveTargets == 0 && device.liveBuffers == 0 && liveModules == 0);
    // A malformed loop never swaps feedback or walks off the list.
    {
        Stack s;
        s.Init(&device, 0, 0, 0);
        append(s, "orphan", -1);
        int old = s.prevRT;
        REQUIRE(s.RunStack() == -1 && s.prevRT == old);
    }
    {
        Stack s;
        s.Init(&device, 0, 0, 0);
        auto *m = append(s, "start", 3, 1);
        s.loopDepth = 15;
        REQUIRE(!neon::effects::beginLoop(s, *m, 1));
    }
}
void routingAndWaypoints() {
    Device device;
    Effect effect;
    effect.Init(&device);
    auto *a = new Stack;
    auto *b = new Stack;
    a->stackIndex = 7;
    b->stackIndex = 11;
    a->effect = b->effect = &effect;
    effect.AddStack(a, 0, 0);
    effect.AddStack(b, 0, 0);
    auto *m = append(*b, "sink");
    using namespace neon::effects;
    TextureRoute dst{b->id, m->index, 0, 0};
    for (int slot = 0; slot < 6; ++slot) {
        TextureRoute src{a->id, 0, a->outputRT, INPUT_STACK};
        dst.num = slot;
        REQUIRE(effect.AssignTextureInput(&src, &dst));
    }
    Render render;
    sourceTextures(*m, &render);
    for (int x : render.rt)
        REQUIRE(x == a->outputRT);
    REQUIRE(m->inputFlags == 63);
    const int before = a->outputRT;
    REQUIRE(a->RunStack() == 0);
    sourceTextures(*m, &render);
    for (int x : render.rt)
        REQUIRE(x == a->outputRT &&
                x != before); // pointers track the swap, not a stale target number
    TextureRoute src{b->id, 0, b->outputRT, INPUT_STACK};
    dst.num = 0;
    REQUIRE(effect.AssignTextureInput(&src, &dst));
    src.type = INPUT_LITERAL;
    src.num = 9;
    dst.num = 1;
    REQUIRE(effect.AssignTextureInput(&src, &dst));
    src.type = INPUT_NONE;
    dst.num = 5;
    REQUIRE(effect.AssignTextureInput(&src, &dst));
    sourceTextures(*m, &render);
    REQUIRE(render.rt[0] == b->prevRT && render.rt[1] == -1 && render.literal[1] == 9 &&
            render.rt[5] == -1);
    REQUIRE(m->inputFlags == 29);
    dst.num = 6;
    REQUIRE(!effect.AssignTextureInput(&src, &dst));
    dst.num = 0;
    src.type = INPUT_STACK;
    src.stack = 99;
    REQUIRE(!effect.AssignTextureInput(&src, &dst));
    REQUIRE(stackByIndex(effect, 7) == a && stackByIndex(effect, 11) == b &&
            positionOfStack(effect, 11) == 1);
    neon::WaypointData w;
    w.count = 2;
    w.index = {2, 63};
    w.value = {.125f, .75f};
    w.mode = 5;
    w.inputFlags = 1;
    w.inputSrc[0] = 0;
    w.inputSrc[1] = 9;
    w.inputSrc[2] = 10;
    for (int i = 0; i < 6; ++i)
        w.wrap[i] = i;
    b->waypoints[0][0][63] = new neon::WaypointData(w);
    REQUIRE(b->waypointExists(1, 63) && !b->waypointExists(1, 0));
    b->getWaypoint(1, 63);
    REQUIRE(m->params[2].value == .125 && m->params[63].value == .75 && m->mode == 5);
    REQUIRE(m->wrap[0] == 0 && m->wrap[3] == 1 && m->wrap[2] == 4 && m->wrap[5] == 5);
    sourceTextures(*m, &render);
    REQUIRE(render.rt[0] == a->outputRT && render.literal[1] == 9 && render.literal[2] == 10);
}
void failuresAndLifetime() {
    Device device;
    Dna valid;
    valid.hasDNA = true;
    valid.S = {3, 1, 500, -1};
    valid.F.resize(64);
    valid.P.resize(64);
    valid.F[63] = .5f;
    valid.P[63] = 71;
    {
        std::unique_ptr<Effect> e(buildEffect(valid, &device, 6, static_cast<void *>(nullptr)));
        REQUIRE(e && valid.built && e->id == 6 && e->stacks->modules->params[63].ival == 71);
    }
    REQUIRE(liveModules == 0 && device.liveTargets == 0 && device.liveBuffers == 0);
    // Fail after a fully allocated first stack. Both its resources and pending modules must be
    // released.
    Dna bad;
    bad.hasDNA = true;
    bad.S = {3, 1, 500, -1, 3, 1, 500, 1, 200, -1};
    REQUIRE(!buildEffect(bad, &device, 0, static_cast<void *>(nullptr)));
    REQUIRE(bad.error == "Unsupported effect module type 200");
    REQUIRE(liveModules == 0 && device.liveTargets == 0 && device.liveBuffers == 0 && !bad.built);
    bad.S = {3, 1, 500, 42};
    REQUIRE(!buildEffect(bad, &device, 0, static_cast<void *>(nullptr)));
    REQUIRE(bad.error == "Malformed effect stack ending" && liveModules == 0);
    bad.S = {3};
    for (int i = 0; i < 33; ++i) {
        bad.S.push_back(1);
        bad.S.push_back(500);
    }
    bad.S.push_back(-1);
    REQUIRE(!buildEffect(bad, &device, 0, static_cast<void *>(nullptr)));
    REQUIRE(bad.error == "Effect module capacity exceeded" && liveModules == 0);
}
void immediateRouting() {
    struct Context {
        Render main;
        Effect *effect = nullptr;
        int effectStack = 0;
    } context;
    struct Session {
        Context *context = nullptr;
        int sourceTexture = -1;
    } session;
    using namespace neon::sprites;
    int flushes = 0, binds = 0;
    ImmediateSource chosen;
    auto flush = [&] { ++flushes; };
    auto bind = [&](auto &, ImmediateSource source) {
        ++binds;
        chosen = source;
    };
    selectImmediateTexture(session, 2, LiteralSelectionRule::AlwaysSelect, flush, bind);
    REQUIRE(flushes == 1 && binds == 0);
    session.context = &context;
    selectImmediateTexture(session, 2, LiteralSelectionRule::AlwaysSelect, flush, bind);
    REQUIRE(chosen.literal == 2 && session.sourceTexture == 2);
    selectImmediateTexture(session, 2, LiteralSelectionRule::ReuseSelectedLiteral, flush, bind);
    REQUIRE(flushes == 2 && binds == 1);
    selectImmediateTexture(session, 2, LiteralSelectionRule::AlwaysSelect, flush, bind);
    REQUIRE(flushes == 3 && binds == 2);
    selectImmediateTexture(session, -1, LiteralSelectionRule::ReuseSelectedLiteral, flush, bind);
    REQUIRE(chosen.literal == 4 && session.sourceTexture == 4);
    Device device;
    Effect effect;
    effect.Init(&device);
    auto *stack = new Stack;
    stack->stackIndex = 7;
    effect.AddStack(stack, 0, 0);
    context.effect = &effect;
    context.effectStack = 7;
    selectImmediateTexture(session, -1, LiteralSelectionRule::ReuseSelectedLiteral, flush, bind);
    REQUIRE(chosen.history == stack->prevRT && chosen.literal == -1 &&
            session.sourceTexture == kNoTexture);
    stack->RunStack();
    selectImmediateTexture(session, -1, LiteralSelectionRule::ReuseSelectedLiteral, flush, bind);
    REQUIRE(chosen.history == stack->prevRT && flushes == 6 && binds == 5);
}
void fixture(const char *path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    std::vector<Dna> cells;
    REQUIRE(neon::effects::loadGrid(bytes, cells, 256));
    Device device;
    int count = 0;
    for (auto &dna : cells)
        if (dna.hasDNA) {
            // The fixture exercise validates graph construction/routing, not availability of future
            // games' draw modules. Type 200 is reserved for the explicit failure test above;
            // fixtures in this body of work do not instantiate it.
            auto source = static_cast<neon::EffectDna>(dna);
            source.effectSaveReset();
            const int revision =
                (uint32_t(source.s(0)) & 0xffff0000u) == 0xbaaa0000u ? source.s(0) & 0xffff : 0;
            if (revision) {
                source.sc = 2;
            }
            std::vector<std::vector<int>> types;
            std::vector<int> indices;
            int expectedWaypoints = 0;
            while (source.s(source.sc) == 3) {
                ++source.sc;
                types.emplace_back();
                while (source.s(source.sc) == 1) {
                    neon::ModuleData record;
                    REQUIRE(source.readModule(record));
                    types.back().push_back(record.type);
                }
                while (source.s(source.sc + 3) == 2) {
                    source.sc += 3;
                    neon::WaypointData w;
                    REQUIRE(source.readWaypoint(w));
                    ++expectedWaypoints;
                }
                indices.push_back(revision ? source.s(source.sc++) & 0xffff : int(indices.size()));
                REQUIRE(source.s(source.sc++) == -1);
            }
            std::unique_ptr<Effect> fx(buildEffect(dna, &device, 0, static_cast<void *>(nullptr)));
            REQUIRE(fx);
            ++count;
            REQUIRE(dna.sc == source.sc && dna.pc == source.pc && dna.fc == source.fc);
            size_t si = 0;
            int actualWaypoints = 0;
            for (auto *s = fx->stacks; s; s = s->nextStack, ++si) {
                REQUIRE(si < types.size() && s->stackIndex == indices[si] && s->effect == fx.get());
                size_t mi = 0;
                for (auto *m = s->modules; m; m = m->next, ++mi)
                    REQUIRE(mi < types[si].size() && m->typeId == types[si][mi] && m->stack == s &&
                            m->index == int(mi));
                REQUIRE(mi == types[si].size() && s->moduleCount == int(mi));
                for (auto &module : s->waypoints)
                    for (auto &bank : module)
                        for (auto *value : bank)
                            if (value)
                                ++actualWaypoints;
            }
            REQUIRE(si == types.size() && fx->stackCount == int(si) &&
                    actualWaypoints == expectedWaypoints);
        }
    REQUIRE(count > 0 && liveModules == 0 && device.liveTargets == 0 && device.liveBuffers == 0);
    std::cout << path << ": " << count << " effect graphs constructed and released\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        orderedAndNestedExecution();
        routingAndWaypoints();
        failuresAndLifetime();
        immediateRouting();
        if (argc == 2)
            fixture(argv[1]);
        std::cout
            << "Ordered prep/run, nested loops, six texture inputs, feedback swaps, immediate routing, waypoints and failure cleanup passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
