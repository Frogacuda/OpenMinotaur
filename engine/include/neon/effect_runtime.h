// Engine-owned effect lifecycle, ordered execution, history and routing.
// Templates borrow concrete render/module bindings; no concrete game headers,
// platform APIs, game IDs, copies of game state or simulation ticks are used.
#pragma once
#include "neon/effect_dna.h"
#include <cstring>
#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>
namespace neon::effects {
struct TextureRoute {
    int stack = 0, module = 0, num = 0, type = 0;
};
inline constexpr int INPUT_NONE = -1, INPUT_STACK = 3, INPUT_LITERAL = 4, INPUT_FEEDBACK = 5;
template <class Stack> Stack *findById(Stack *s, int id) {
    for (; s; s = s->nextStack)
        if (s->id == id)
            return s;
    return nullptr;
}

template <class Self> void constructModule(Self &self) {

    for (int &e : self.paramEnabled)
        e = 1;
}

template <class Self> void prepareModule(Self &self) {

    self.prepped = 1;
    self.running = 1;
}

template <class Self> void limit(Self &self, int i) {

    double v = self.params[i].value;
    if (v < self.ranges[i].min)
        self.params[i].value = v = self.ranges[i].min;
    if (v > self.ranges[i].max)
        self.params[i].value = self.ranges[i].max;
}

template <class Self> void wrapParameter(Self &self, int i) {

    double v = self.params[i].value;
    double lo = self.ranges[i].min;
    if (v < lo)
        self.params[i].value = v = v + (self.ranges[i].max - lo);
    if (v > self.ranges[i].max)
        self.params[i].value = v - (self.ranges[i].max - self.ranges[i].min);
}

template <class Self> void alignParameter(Self &self, int i, int steps) {

    float s = float(steps);
    int q = int(double(s) * self.params[i].value);
    self.params[i].value = double(float(q) / s);
}

template <class Self> void zeroPair(Self &self, int a, int b) {

    float fa = std::fabs(float(self.params[a].value));
    if (fa > 0.0f) {
        float fb = std::fabs(float(self.params[b].value));
        if (fb > 0.0f) {
            if (fa <= fb) {
                if (fb / fa >= 10.0f) {
                    self.params[a].value = 0;
                    return;
                }
            } else if (fa / fb >= 10.0f) {
                self.params[b].value = 0;
                return;
            }
        }
    }
    self.params[a].value = 0;
    self.params[b].value = 0;
}

template <class Self> int outputTarget(Self &self, typename Self::RenderType *r) {

    r->SetCurrenRenderTarget(self.stack ? self.stack->outputRT : -1);
    return 0;
}

template <class Self> int sourceTextures(Self &self, typename Self::RenderType *r) {

    int ownId = self.stack ? self.stack->id : -1;
    int ownPrev = self.stack ? self.stack->prevRT : -1;
    int rt[6], lit[6];
    self.inputFlags = 0;
    for (int k = 0; k < 6; k++) {
        const typename Self::Input &in = self.inputs[k];
        rt[k] = -1;
        lit[k] = -1;
        if (in.type == INPUT_FEEDBACK) {
            self.inputFlags |= 1u << k;
            rt[k] = ownPrev;
        } else if (in.num != -1) {
            if (in.type == INPUT_STACK) {
                self.inputFlags |= 1u << k;
                if (in.stack == ownId || in.stack == -1)
                    rt[k] = ownPrev;
                else
                    rt[k] = in.rtPtr ? *in.rtPtr : -1;
            } else if (in.type == INPUT_LITERAL) {
                lit[k] = in.num;
            }
        }
    }
    r->SetSourceFromRTs(rt[0], rt[1], rt[2], rt[3], rt[4], rt[5]);
    r->SetSourceFromLiterals(lit[0], lit[1], lit[2], lit[3], lit[4], lit[5]);
    return 0;
}

template <class Self> void applyWaypoint(Self &self, neon::WaypointData *w) {
    using Stack = typename Self::StackType;
    using Effect = typename Self::EffectType;

    if (!w)
        return;
    for (int i = 0; i < w->count; i++) {
        int k = w->index[size_t(i)];
        if (k >= 0 && k < 64)
            self.params[k].value = double(w->value[size_t(i)]);
    }
    self.mode = w->mode;
    self.setBasics();
    self.inputFlags = unsigned(w->inputFlags);
    if (!self.stack || !self.stack->effect)
        return;
    Effect *fx = self.stack->effect;

    static const int kWrapS[3] = {0, 1, 2}, kWrapT[3] = {3, 4, 5};
    for (int k = 0; k < 3; k++) {
        self.wrap[kWrapS[k]] = w->wrap[k * 2];
        self.wrap[kWrapT[k]] = w->wrap[k * 2 + 1];
        self.inputSrc[k] = w->inputSrc[k];
        TextureRoute src{}, dst{};
        if (self.inputFlags & (1u << k)) {
            Stack *s = fx->getTrueStack(w->inputSrc[k]);
            src.stack = s ? s->id : -1;
            src.num = s ? s->outputRT : -1;
            src.type = INPUT_STACK;
        } else {
            src.stack = self.stack->id;
            src.num = w->inputSrc[k];
            src.type = INPUT_LITERAL;
        }
        src.module = self.index;
        dst.stack = self.stack->id;
        dst.module = self.index;
        dst.num = k;
        fx->AssignTextureInput(&src, &dst);
    }
}

template <class Self> void constructStack(Self &self) {
    using Vector3 = typename Self::Vector3Type;

    std::memset(self.waypoints, 0, sizeof self.waypoints);
    std::memset(self.verts, 0, sizeof self.verts);
    for (auto &m : self.matrices)
        D3DXMatrixIdentity(&m);
    Vector3 eye{0, 0, -100}, at{0, 0, 0}, up{0, 1, 0};
    D3DXMatrixLookAtLH(&self.view, &eye, &at, &up);
}

template <class Self> void destroyStack(Self &self) {
    using Module = typename Self::ModuleType;

    for (auto &m : self.waypoints)
        for (auto &b : m)
            for (neon::WaypointData *&w : b) {
                delete w;
                w = nullptr;
            }
    Module *m = self.modules;
    while (m) {
        Module *n = m->next;
        m->Release_Module_Resources();
        delete m;
        m = n;
    }
    self.modules = nullptr;
    if (self.device) {
        if (self.outputRT >= 0)
            self.device->ReleaseRTTexture(self.outputRT);
        if (self.prevRT >= 0)
            self.device->ReleaseRTTexture(self.prevRT);
        if (self.vb0 >= 0)
            self.device->DestroyVertexuffer(self.vb0, 1);
        if (self.vb1 >= 0)
            self.device->DestroyVertexuffer(self.vb1, 1);
    }
}

template <class Self>
int initializeStack(Self &self, typename Self::DeviceType *dev, int stackId, int vertsKind0,
                    int vertsKind1) {
    using Module = typename Self::ModuleType;

    if (self.initialised)
        return 1;
    self.device = dev;
    self.prevRT = self.device->GetRTTexture(self.rtW, self.rtH);
    self.outputRT = self.device->GetRTTexture(self.rtW, self.rtH);
    self.device->SetCurrenRT(self.prevRT);
    self.device->ClearScreen(0, 0, 0, 0);
    self.device->SetCurrenRT(self.outputRT);
    self.device->ClearScreen(0, 0, 0, 0);
    self.device->SetCurrenRT(-1);
    if (vertsKind0 > 0)
        self.vb0 = self.device->CreateVertexBufferWO(0, vertsKind0, -1);
    if (vertsKind1 > 0)
        self.vb1 = self.device->CreateVertexBufferWO(1, vertsKind1, -1);
    self.id = stackId;
    self.moduleCount = 0;
    for (Module *m = self.modules; m; m = m->next)
        m->Init_Module_Resources(self.device, self.moduleCount++, &self);
    self.initialised = 1;
    return 0;
}

template <class Self> int addModule(Self &self, typename Self::ModuleType *m, int pos, bool init) {
    using Module = typename Self::ModuleType;
    constexpr int kMaxModules = Self::kMaxModules;

    if (self.moduleCount >= kMaxModules)
        return -1;
    Module *head = self.modules;
    if (!head || pos == 0) {
        self.modules = m;
        m->next = head;
    } else {
        Module *prev = head;
        int i = 1;
        while (prev->next && i != pos) {
            prev = prev->next;
            i++;
        }
        m->next = prev->next;
        prev->next = m;
    }
    if (init) {
        int idx = self.moduleCount++;
        m->index = idx;
        m->Init_Module_Resources(self.device, idx, &self);
    }
    return m->index;
}

template <class Self> bool waypointExists(const Self &self, int bank, int slot) {
    using Module = typename Self::ModuleType;
    constexpr int kMaxModules = Self::kMaxModules;

    int mi = 0;
    for (Module *m = self.modules; m; m = m->next, mi++)
        if (mi < kMaxModules && self.waypoints[mi][bank - 1][slot])
            return true;
    return false;
}

template <class Self> void selectWaypoint(Self &self, int bank, int slot) {
    using Module = typename Self::ModuleType;
    constexpr int kMaxModules = Self::kMaxModules;

    int mi = 0;
    for (Module *m = self.modules; m; m = m->next, mi++)
        m->getWaypoint(mi < kMaxModules ? self.waypoints[mi][bank - 1][slot] : nullptr);
}

template <class Self> int runStack(Self &self) {
    using Module = typename Self::ModuleType;

    if (!self.initialised)
        return 1;
    self.loopDepth = 0;
    self.device->FillSurface(self.outputRT, 0);
    self.device->SetCurrenRT(self.outputRT);
    self.matrices[0] = self.view;
    for (Module *m = self.modules; m; m = m->next)
        m->runPrep();
    Module *m = self.modules;
    while (m) {
        m->Run_Module();
        m->running = 0;
        if (!m->next)
            self.device->SetupDefaultCamera();
        if (m->kind != -1) {
            m = m->next;
            continue;
        }
        // Loop end (closeBrace): repeat from the matching start until its count runs out.
        if (self.loopDepth < 1 || self.loopDepth >= 16 || !self.loopStart[self.loopDepth])
            return -1;
        int &count = self.loopCount[self.loopDepth];
        if (--count < 1) {
            int depth = 1;
            Module *s = self.loopStart[self.loopDepth];
            do {
                s->prepped = 1;
                s = s->next;
                if (!s)
                    return -1;
                if (s->kind == 3)
                    depth++;
                else if (s->kind == -1)
                    depth--;
            } while (depth > 0);
            m = s->next;
        } else {
            m = self.loopStart[self.loopDepth];
        }
        self.loopDepth--;
    }
    int t = self.prevRT;
    self.prevRT = self.outputRT;
    self.outputRT = t;
    return 0;
}

template <class Self> void destroyEffect(Self &self) {
    using Stack = typename Self::StackType;

    Stack *s = self.stacks;
    while (s) {
        Stack *n = s->nextStack;
        delete s;
        s = n;
    }
}

template <class Self> int initializeEffect(Self &self, typename Self::DeviceType *dev) {
    using Stack = typename Self::StackType;

    if (self.initialised)
        return 1;
    self.device = dev;
    int i = 0;
    for (Stack *s = self.stacks; s; s = s->nextStack)
        s->Init(self.device, i++, 0, 0);
    self.initialised = 1;
    return 0;
}

template <class Self> int runEffect(Self &self) {
    using Stack = typename Self::StackType;

    for (Stack *s = self.stacks; s; s = s->nextStack) {
        int result = s->RunStack();
        if (result)
            return result;
    }
    return 0;
}

template <class Self>
int addStack(Self &self, typename Self::StackType *s, int vertsKind0, int vertsKind1) {
    using Stack = typename Self::StackType;

    int idx;
    s->nextStack = nullptr;
    if (!self.stacks) {
        self.stacks = s;
        self.stackCount = 1;
        idx = 0;
    } else {
        Stack *last = self.stacks;
        while (last->nextStack)
            last = last->nextStack;
        last->nextStack = s;
        idx = self.stackCount++;
    }
    s->Init(self.device, idx, vertsKind0, vertsKind1);
    return idx;
}

template <class Self> typename Self::StackType *stackByIndex(const Self &self, int stackIndex) {
    using Stack = typename Self::StackType;

    for (Stack *s = self.stacks; s; s = s->nextStack)
        if (s->stackIndex == stackIndex)
            return s;
    return nullptr;
}

template <class Self> typename Self::StackType *stackAt(const Self &self, int n) {
    using Stack = typename Self::StackType;

    Stack *s = self.stacks;
    for (int i = 0; s && i < n; i++)
        s = s->nextStack;
    return s;
}

template <class Self> int positionOfStack(const Self &self, int stackIndex) {
    using Stack = typename Self::StackType;

    int i = 0;
    for (Stack *s = self.stacks; s; s = s->nextStack, i++)
        if (s->stackIndex == stackIndex)
            return i;
    return -1;
}

template <class Self>
bool assignTexture(Self &self, const TextureRoute *src, const TextureRoute *dst) {
    using Module = typename Self::ModuleType;
    using Stack = typename Self::StackType;
    constexpr int kMaxLiterals = 32;

    Stack *srcStack = nullptr;
    bool ok = src->type == INPUT_FEEDBACK || src->type == INPUT_NONE ||
              (src->type == INPUT_LITERAL && src->num < kMaxLiterals);
    if (!ok) {
        if (src->stack < 0 || !(srcStack = findById(self.stacks, src->stack)))
            return false;
        if (src->type != INPUT_STACK)
            return false;
    }
    Stack *dstStack = findById(self.stacks, dst->stack);
    if (!dstStack)
        return false;
    Module *m = dstStack->modules;
    while (m && m->index != dst->module)
        m = m->next;
    if (!m || dst->num < 0 || dst->num >= 6)
        return false;
    typename Module::Input &in = m->inputs[dst->num];
    switch (src->type) {
    case INPUT_LITERAL:
        in = {-1, src->num, INPUT_LITERAL, nullptr};
        return true;
    case INPUT_FEEDBACK:
        in = {-1, dstStack->prevRT, INPUT_FEEDBACK, nullptr};
        return true;
    case INPUT_STACK:
        if (dst->stack == srcStack->id)
            in = {-1, -1, INPUT_FEEDBACK, nullptr};
        else
            in = {srcStack->id, srcStack->outputRT, INPUT_STACK, &srcStack->outputRT};
        return true;
    case INPUT_NONE:
        in = {-1, -1, -1, nullptr};
        return true;
    }
    return false;
}

// A loop-start module calls this on each visit. Source replicators initialize
// count on the prepared visit, then re-enter without reinitializing it. The
// geometry/editor replicator capability is separate from this control flow.
template <class Stack, class Module> bool beginLoop(Stack &stack, Module &start, int repeats) {
    const int depth = stack.loopDepth + 1;
    if (depth >= 16 || repeats < 0)
        return false;
    if (start.prepped) {
        stack.loopStart[depth] = &start;
        stack.loopCount[depth] = repeats;
    }
    start.prepped = 0;
    stack.loopDepth = depth;
    return true;
}

template <class Dna, class Factory>
typename Dna::ModuleType *loadModule(Dna &dna, Factory factory) {
    neon::ModuleData record;
    if (!dna.readModule(record))
        return nullptr;
    auto *module = factory(record.type);
    if (!module) {
        dna.error = "Unsupported effect module type " + std::to_string(record.type);
        return nullptr;
    }
    module->setBasics();
    for (int i = 0; i < 64; ++i) {
        module->params[i].value = double(record.values[i]);
        module->params[i].ival = record.integers[i];
    }
    return module;
}
template <class Dna> int loadWaypoint(Dna &dna, neon::WaypointData *value) {
    if (value)
        dna.readWaypoint(*value);
    return 0;
}

template <class Dna, class Device, class Host>
typename Dna::EffectType *buildEffect(Dna &dna, Device *dev, int effectId, Host *host) {
    using Effect = typename Dna::EffectType;
    using Stack = typename Dna::StackType;
    using Module = typename Dna::ModuleType;
    if (!dna.hasDNA || dna.built)
        return nullptr;
    dna.error.clear();
    std::unique_ptr<Effect> fx(new Effect());
    fx->gameHost = host;
    dna.effectSaveReset();
    uint32_t head = uint32_t(dna.s(dna.sc));
    if ((head & 0xffff0000u) == 0xbaaa0000u) {
        dna.rev = int(head & 0xffff);
        ++dna.sc;
    } else
        dna.rev = 0;
    if (dna.rev > 0)
        fx->id = dna.s(dna.sc++);
    int stackNo = 0;
    while (dna.s(dna.sc) == 3) {
        std::unique_ptr<Stack> st(new Stack());
        ++dna.sc;
        st->rtW = st->rtH = 512;
        st->effect = fx.get();
        st->stackIndex = stackNo;
        fx->Init(dev);
        fx->id = effectId;
        std::vector<std::unique_ptr<Module>> modules;
        while (dna.s(dna.sc) == 1) {
            auto *m = dna.loadModule();
            if (!m)
                return nullptr;
            m->stack = st.get();
            modules.emplace_back(m);
        }
        while (dna.s(dna.sc + 3) == 2) {
            int mi = dna.s(dna.sc), bank = dna.s(dna.sc + 1), slot = dna.s(dna.sc + 2);
            dna.sc += 3;
            auto value = std::make_unique<neon::WaypointData>();
            dna.loadWaypoint(value.get());
            if (mi >= 0 && mi < Stack::kMaxModules && bank >= 0 && bank < 3 && slot >= 0 &&
                slot < 64) {
                delete st->waypoints[mi][bank][slot];
                st->waypoints[mi][bank][slot] = value.release();
            }
        }
        if (dna.rev > 0)
            st->stackIndex = dna.s(dna.sc++) & 0xffff;
        if (dna.s(dna.sc) != -1) {
            dna.error = "Malformed effect stack ending";
            return nullptr;
        }
        ++dna.sc;
        for (auto &module : modules) {
            if (st->AddModule(module.get(), -1) < 0) {
                dna.error = "Effect module capacity exceeded";
                return nullptr;
            }
            module.release();
        }
        fx->AddStack(st.release(), Stack::kMaxVerts, 0);
        ++stackNo;
    }
    for (auto *st = fx->stacks; st; st = st->nextStack)
        st->getWaypoint(1, st->waypointExists(1, 63) ? 63 : 0);
    dna.built = true;
    return fx.release();
}
template <class Saver>
bool loadGrid(const std::vector<uint8_t> &bytes, std::vector<Saver> &cells, int maxCells) {
    std::vector<neon::EffectDna> decoded;
    bool ok = neon::readEffectGrid(bytes, decoded, maxCells);
    cells.assign(decoded.size(), Saver{});
    for (size_t i = 0; i < decoded.size(); ++i)
        static_cast<neon::EffectDna &>(cells[i]) = std::move(decoded[i]);
    return ok;
}
} // namespace neon::effects
