#include "neon/backend_setup.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(#x);                                                          \
    } while (false)
namespace frozen {
#include "fixtures/backend-reference/legacy-setup.h"
}
struct Trace {
    std::vector<std::string> events;
    int fail = -1, mode = 0, step = 0;
    std::function<std::string()> snapshot;
    int hit(const std::string &name) {
        events.push_back(name + (snapshot ? "|" + snapshot() : ""));
        const bool at = step++ == fail;
        if (at && mode == 1)
            throw std::runtime_error("dependency");
        return at && mode == 2 ? -1 : 0;
    }
};
struct Device {
    Trace &trace;
    int generation = 1;
    bool destroyed = false;
    void *host = nullptr;
    int logicalW = 0, logicalH = 0, framebuffer = -1;
    Device(Trace &t, int gen = 1) : trace(t), generation(gen) {}
    ~Device() {
        trace.events.push_back("owner-delete:" + std::to_string(generation));
    }
    void setHost(void *h) {
        trace.hit("host");
        host = h;
    }
    int Init(int zero, int w, int h, int f, int pw, int ph, int flags) {
        CHECK(zero == 0 && flags == 0 && pw == 641 && ph == 479);
        int result = trace.hit("device-init");
        logicalW = w;
        logicalH = h;
        framebuffer = f;
        return result;
    }
    void InitAllShaders() {
        trace.hit("shaders");
    }
    void setNearestTextures(bool v) {
        CHECK(!v);
        trace.hit("filter");
    }
    void Destroy(bool full) {
        CHECK(full);
        trace.hit("gpu-destroy:" + std::to_string(generation));
        destroyed = true;
    }
    void BeginFrame() {
        trace.hit("begin");
    }
    void SetCurrenRT(int rt) {
        CHECK(rt == -1);
        trace.hit("device-rt");
    }
    void alphaEnable(bool a) {
        CHECK(!a);
        trace.hit("alpha");
    }
    void SetupDefaultProjection(int w, int h, int flags) {
        CHECK(flags == 1);
        trace.hit("projection:" + std::to_string(w) + "," + std::to_string(h));
    }
    void ClearScreen(int a, int r, int g, int b) {
        CHECK(a == 255 && r == 0 && g == 0 && b == 0);
        trace.hit("explicit-clear");
    }
    struct Texture {
        unsigned name = 7;
    } textures[3];
    int missing = -1, unnamed = -1;
    Texture *literal(int i) {
        trace.hit("literal:" + std::to_string(i));
        if (i == missing)
            return nullptr;
        textures[i].name = i == unnamed ? 0 : 7;
        return &textures[i];
    }
};
struct Render {
    Trace &trace;
    std::string id;
    Device *device = nullptr;
    int vertexBuffer = 71, initialised = 7, shader = 13, primitive = 9;
    std::array<neon::matrix4f, 6> texMatrix;
    neon::matrix4f world;
    Render(Trace &t, std::string name) : trace(t), id(name) {
        std::memset(texMatrix.data(), 0xa5, sizeof(texMatrix));
        std::memset(&world, 0x5a, sizeof(world));
    }
    void Init(Device *d) {
        trace.hit(id + "-init");
        device = d;
    }
    int GetVertexBufferWO(int kind, int count, int usage, bool separate) {
        CHECK(usage == -1 && !separate);
        int result =
            trace.hit(id + "-buffer:" + std::to_string(kind) + "," + std::to_string(count));
        vertexBuffer = result ? -1 : kind + 30;
        return result;
    }
    void SetCurrenRenderTarget(int rt) {
        CHECK(rt == -1);
        trace.hit(id + "-rt");
    }
};
struct Drawing {
    Render main, aux;
    void *effect = reinterpret_cast<void *>(1);
    int effectStack = 29;
    Drawing(Trace &t) : main(t, "main"), aux(t, "aux") {}
};
struct Binding {
    Trace &t;
    bool connected = true;
    int history = 177, batch = 17;
    void disconnect() {
        t.hit("disconnect");
        connected = false;
    }
};
struct EffectOwner {
    Trace &t;
    bool present = true;
    std::function<void()> release;
    void reset() {
        t.hit("effect-reset");
        if (present && release)
            release();
        present = false;
    }
};
struct Surface {
    int framebuffer = 73, width = 641, height = 479;
};
struct Model {
    Trace &trace;
    Drawing drawing;
    Binding binding;
    bool ready = false, gameReady = true, callbacks = true;
    EffectOwner effect;
    std::unique_ptr<Device> device;
    Model(Trace &t)
        : trace(t), drawing(t), binding{t}, effect{t}, device(std::make_unique<Device>(t)) {
        effect.release = [&] { gameReady = false; };
        drawing.main.device = drawing.aux.device = device.get();
        t.snapshot = [&] {
            return std::to_string(drawing.main.initialised) + "," +
                   std::to_string(drawing.aux.initialised) + "," + std::to_string(ready) + "," +
                   std::to_string(gameReady) + "," + std::to_string(binding.connected) + "," +
                   std::to_string(callbacks) + "," + std::to_string(drawing.effect != nullptr) +
                   "," + std::to_string(drawing.main.device != nullptr) + "," +
                   std::to_string(drawing.main.vertexBuffer);
        };
    }
    void clear() {
        trace.hit("clear-background");
        trace.hit("clear-foreground");
        trace.hit("clear-text");
        callbacks = false;
    }
    std::unique_ptr<Device> factory() {
        trace.hit("factory");
        return std::make_unique<Device>(trace, device->generation + 1);
    }
};
void same(const Model &a, const Model &b) {
    CHECK(a.trace.events == b.trace.events && a.ready == b.ready && a.gameReady == b.gameReady &&
          a.callbacks == b.callbacks && a.effect.present == b.effect.present);
    CHECK(a.binding.connected == b.binding.connected && a.binding.history == b.binding.history &&
          a.binding.batch == b.binding.batch);
    CHECK(a.drawing.effect == b.drawing.effect && a.drawing.effectStack == b.drawing.effectStack);
    for (auto pair : {std::pair<const Render *, const Render *>{&a.drawing.main, &b.drawing.main},
                      {&a.drawing.aux, &b.drawing.aux}}) {
        const auto &x = *pair.first;
        const auto &y = *pair.second;
        CHECK(bool(x.device) == bool(y.device) && x.vertexBuffer == y.vertexBuffer &&
              x.initialised == y.initialised && x.shader == y.shader && x.primitive == y.primitive);
        CHECK(std::memcmp(x.texMatrix.data(), y.texMatrix.data(), sizeof(x.texMatrix)) == 0 &&
              std::memcmp(&x.world, &y.world, sizeof(x.world)) == 0);
    }
    CHECK(a.device->generation == b.device->generation &&
          a.device->destroyed == b.device->destroyed && a.device->host == b.device->host &&
          a.device->logicalW == b.device->logicalW && a.device->logicalH == b.device->logicalH &&
          a.device->framebuffer == b.device->framebuffer);
}
template <class F> bool raised(F f) {
    try {
        f();
        return false;
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()) == "dependency");
        return true;
    }
}
struct CatalogHost {
    Trace &trace;
    bool available = true;
    bool readAsset(const std::string &name, std::vector<unsigned char> &bytes) {
        CHECK(name == "NeonResources.xml");
        trace.hit("read");
        bytes = {'x', 'm', 'l'};
        return available;
    }
};
struct Catalog {
    Trace &trace;
    bool valid = true;
    bool load(const std::string &bytes, std::string *error) {
        CHECK(bytes == "xml");
        trace.hit("parse");
        *error = valid ? "" : "bad";
        return valid;
    }
};
struct Module {
    int id;
    Module *next = nullptr;
};
struct Stack {
    int id;
    Stack *nextStack = nullptr;
    Module *modules = nullptr;
};
struct Effect {
    Stack *stacks = nullptr;
};
int main() {
    try {
        int cases = 0;
        for (int profile : {0, 1, 2})
            for (int mode : {0, 1, 2})
                for (int fail = -1; fail < 8; ++fail) {
                    const bool early = profile == 0, stop = profile == 1;
                    Trace at, bt;
                    at.fail = bt.fail = fail;
                    at.mode = bt.mode = mode;
                    Model a(at), b(bt);
                    bool ar = false, br = false;
                    bool ax = raised([&] {
                        ar = neon::backend::initializeDrawing(
                            a.drawing, *a.device,
                            early ? neon::backend::BufferSpec{0, 20000}
                                  : neon::backend::BufferSpec{0, 24000},
                            early ? neon::backend::BufferSpec{1, 24000}
                                  : neon::backend::BufferSpec{0, 24000},
                            early ? neon::backend::DrawingOrder::AllocateBothThenConfigure
                                  : neon::backend::DrawingOrder::ConfigureEach,
                            stop ? neon::backend::BufferFailure::Stop
                                 : neon::backend::BufferFailure::IgnoreReturn);
                    });
                    bool bx = raised([&] {
                        br = early  ? frozen::oldEarlyDrawing(b.drawing, *b.device)
                             : stop ? frozen::oldLateDrawing(b.drawing, *b.device)
                                    : frozen::oldGridDrawing(b.drawing, *b.device);
                    });
                    CHECK(ax == bx && ar == br);
                    same(a, b);
                    ++cases;
                }
        for (bool ready : {false, true})
            for (int mode : {0, 1, 2})
                for (int fail = -1; fail < 7; ++fail) {
                    Trace at, bt;
                    at.fail = bt.fail = fail;
                    at.mode = bt.mode = mode;
                    Model a(at), b(bt);
                    a.ready = b.ready = ready;
                    Surface surface;
                    int host = 0;
                    bool ar = false, br = false;
                    bool ax = raised([&] {
                        ar = neon::backend::initializeDevice(*a.device, host, surface, 320, 480,
                                                             [&] { a.ready = true; });
                    });
                    bool bx = raised([&] {
                        br = frozen::oldDevice(*b.device, host, surface, 320, 480, b.ready);
                    });
                    CHECK(ax == bx && ar == br);
                    same(a, b);
                    ++cases;
                }
        for (bool ready : {false, true})
            for (bool present : {false, true})
                for (int fail = -1; fail < 11; ++fail) {
                    Trace at, bt;
                    at.fail = bt.fail = fail;
                    at.mode = bt.mode = 1;
                    Model a(at), b(bt);
                    a.ready = b.ready = ready;
                    a.effect.present = b.effect.present = present;
                    bool ax = raised([&] {
                        neon::backend::releaseReplacingDevice(
                            a.effect, a.drawing, a.device, a.ready, a.gameReady, a.binding,
                            [&] { a.clear(); }, [&] { return a.factory(); });
                    });
                    bool bx = raised([&] {
                        frozen::oldRelease(
                            b.effect, b.drawing, b.device, b.ready, b.gameReady, b.binding,
                            [&] { b.clear(); }, [&] { return b.factory(); });
                    });
                    CHECK(ax == bx);
                    same(a, b);
                    at.fail = bt.fail = -1;
                    // Recovery after an injected dependency/allocation throw, then repeated
                    // release.
                    for (int repeat = 0; repeat < 2; ++repeat) {
                        neon::backend::releaseReplacingDevice(
                            a.effect, a.drawing, a.device, a.ready, a.gameReady, a.binding,
                            [&] { a.clear(); }, [&] { return a.factory(); });
                        frozen::oldRelease(
                            b.effect, b.drawing, b.device, b.ready, b.gameReady, b.binding,
                            [&] { b.clear(); }, [&] { return b.factory(); });
                        same(a, b);
                    }
                    ++cases;
                }
        for (int width : {320, 480})
            for (int fail = -1; fail < 9; ++fail) {
                Trace at, bt;
                at.fail = bt.fail = fail;
                at.mode = bt.mode = 1;
                Model a(at), b(bt);
                CHECK(raised([&] {
                          neon::backend::beginOutput(*a.device, a.drawing, width,
                                                     width == 320 ? 480 : 320);
                      }) == raised([&] {
                          frozen::oldOutput(*b.device, b.drawing, width, width == 320 ? 480 : 320);
                      }));
                same(a, b);
                ++cases;
            }
        for (int count : {0, 1, 3})
            for (int missing : {-1, 0, 2})
                for (int unnamed : {-1, 0, 2}) {
                    Trace at, bt;
                    Model a(at), b(bt);
                    a.device->missing = b.device->missing = missing;
                    a.device->unnamed = b.device->unnamed = unnamed;
                    CHECK(neon::backend::validateLiterals(*a.device, count) ==
                          frozen::oldValidate(*b.device, count));
                    same(a, b);
                    ++cases;
                }
        for (bool available : {false, true})
            for (bool valid : {false, true})
                for (int fail = -1; fail < 3; ++fail) {
                    Trace at, bt;
                    at.fail = bt.fail = fail;
                    at.mode = bt.mode = 1;
                    CatalogHost ah{at, available}, bh{bt, available};
                    Catalog ac{at, valid}, bc{bt, valid};
                    std::vector<unsigned char> ab{1}, bb{1};
                    std::string ae = "old", be = "old";
                    bool ar = false, br = false;
                    CHECK(raised([&] { ar = neon::backend::readCatalog(ah, ac, ab, ae); }) ==
                          raised([&] { br = frozen::oldCatalog(bh, bc, bb, be); }));
                    CHECK(ar == br && ae == be && ab == bb && at.events == bt.events);
                    ++cases;
                }
        for (int stacks : {0, 1, 3})
            for (int modules : {0, 1, 3})
                for (int cut : {-1, 0, 1})
                    for (int fail : {-1, 0, 4}) {
                        std::array<Stack, 3> as{}, bs{};
                        std::array<std::array<Module, 3>, 3> am{}, bm{};
                        Effect a, b;
                        for (int i = 0; i < stacks; ++i) {
                            as[i].id = bs[i].id = i;
                            as[i].nextStack = i + 1 < stacks ? &as[i + 1] : nullptr;
                            bs[i].nextStack = i + 1 < stacks ? &bs[i + 1] : nullptr;
                            for (int j = 0; j < modules; ++j) {
                                am[i][j].id = bm[i][j].id = i * 10 + j;
                                am[i][j].next = j + 1 < modules ? &am[i][j + 1] : nullptr;
                                bm[i][j].next = j + 1 < modules ? &bm[i][j + 1] : nullptr;
                            }
                            as[i].modules = modules ? am[i].data() : nullptr;
                            bs[i].modules = modules ? bm[i].data() : nullptr;
                        }
                        a.stacks = stacks ? as.data() : nullptr;
                        b.stacks = stacks ? bs.data() : nullptr;
                        CHECK((neon::backend::lastStack(a) ? neon::backend::lastStack(a)->id
                                                           : -1) == stacks - 1);
                        Trace at, bt;
                        at.fail = bt.fail = fail;
                        at.mode = bt.mode = 1;
                        Stack *al = nullptr, *bl = nullptr;
                        CHECK(raised([&] {
                                  al = neon::backend::visitModules(
                                      a, [&](auto &stack, auto &module) {
                                          at.hit(std::to_string(stack.id) + ":" +
                                                 std::to_string(module.id));
                                          if (cut == 0)
                                              module.next = nullptr;
                                          if (cut == 1)
                                              stack.nextStack = nullptr;
                                      });
                              }) == raised([&] {
                                  bl = frozen::oldVisit(b, [&](auto &stack, auto &module) {
                                      bt.hit(std::to_string(stack.id) + ":" +
                                             std::to_string(module.id));
                                      if (cut == 0)
                                          module.next = nullptr;
                                      if (cut == 1)
                                          stack.nextStack = nullptr;
                                  });
                              }));
                        CHECK(at.events == bt.events && (al ? al->id : -1) == (bl ? bl->id : -1));
                        ++cases;
                    }
        // Independent policies, not just preservation against a saved implementation.
        {
            Trace t;
            Model a(t);
            t.fail = 1;
            t.mode = 2;
            CHECK(!neon::backend::initializeDrawing(a.drawing, *a.device, {0, 24000}, {0, 24000},
                                                    neon::backend::DrawingOrder::ConfigureEach,
                                                    neon::backend::BufferFailure::Stop));
            CHECK(a.drawing.main.initialised == 7 && a.drawing.aux.initialised == 7 && t.step == 2);
        }
        {
            Trace t;
            Model a(t);
            t.fail = 2;
            t.mode = 1;
            int host;
            Surface sf;
            CHECK(raised([&] {
                neon::backend::initializeDevice(*a.device, host, sf, 320, 480,
                                                [&] { a.ready = true; });
            }));
            CHECK(a.ready);
        }
        {
            Trace t;
            Model a(t);
            neon::backend::beginOutput(*a.device, a.drawing, 320, 480);
            CHECK(a.drawing.main.world.m[3][0] == -160 && a.drawing.main.world.m[3][1] == 240 &&
                  t.events.back().find("explicit-clear") == 0);
        }
        std::cout
            << "Backend lifecycle operations: " << cases
            << " ordered state/failure/recovery comparisons and independent contracts passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
