#pragma once
#include "neon/math.h"
#include <string>

namespace neon::backend {
// These explicit host-binding operations borrow the existing services. They do
// not own a game loop, introduce destructor cleanup or choose history lifetimes.
struct BufferSpec {
    int kind, count;
};
enum class DrawingOrder { AllocateBothThenConfigure, ConfigureEach };
enum class BufferFailure { IgnoreReturn, Stop };

template <class Render> void configureRenderer(Render &render) {
    render.initialised = 1;
    render.shader = 0;
    render.primitive = static_cast<decltype(render.primitive)>(4); // triangle list
    for (auto &matrix : render.texMatrix)
        neon::D3DXMatrixIdentity(&matrix);
    neon::D3DXMatrixIdentity(&render.world);
}

template <class Drawing, class Device>
bool initializeDrawing(Drawing &drawing, Device &device, BufferSpec main, BufferSpec aux,
                       DrawingOrder order, BufferFailure failure) {
    auto allocate = [&](auto &render, BufferSpec spec) {
        render.Init(&device);
        const int result = render.GetVertexBufferWO(spec.kind, spec.count, -1, false);
        return failure == BufferFailure::IgnoreReturn || result == 0;
    };
    if (!allocate(drawing.main, main))
        return false;
    if (order == DrawingOrder::ConfigureEach)
        configureRenderer(drawing.main);
    if (!allocate(drawing.aux, aux))
        return false;
    if (order == DrawingOrder::AllocateBothThenConfigure)
        configureRenderer(drawing.main);
    configureRenderer(drawing.aux);
    return true;
}

// markReady runs at its original point, BEFORE shader initialization. If shader
// setup throws, the owner must still know it has a device to release.
template <class Device, class Host, class Surface, class MarkReady>
bool initializeDevice(Device &device, Host &host, const Surface &surface, int width, int height,
                      MarkReady markReady) {
    device.setHost(&host);
    if (device.Init(0, width, height, surface.framebuffer, surface.width, surface.height, 0))
        return false;
    markReady();
    device.InitAllShaders();
    device.setNearestTextures(false);
    return true;
}

template <class Host, class Resources, class Bytes>
bool readCatalog(Host &host, Resources &resources, Bytes &bytes, std::string &error) {
    return host.readAsset("NeonResources.xml", bytes) &&
           resources.load(std::string(bytes.begin(), bytes.end()), &error);
}

template <class Device> bool validateLiterals(Device &device, int count) {
    for (int i = 0; i < count; ++i)
        if (!device.literal(i) || !device.literal(i)->name)
            return false;
    return true;
}

template <class Effect> auto lastStack(Effect &effect) {
    auto *last = effect.stacks;
    while (last && last->nextStack)
        last = last->nextStack;
    return last;
}

template <class Effect, class Visit> auto visitModules(Effect &effect, Visit visit) {
    decltype(effect.stacks) last = nullptr;
    for (auto *stack = effect.stacks; stack; stack = stack->nextStack) {
        last = stack;
        for (auto *module = stack->modules; module; module = module->next)
            visit(*stack, *module);
    }
    return last;
}

// Final-surface setup includes the explicit opaque-black
// frame clear. Render-target allocation does not perform this clear.
// Logical dimensions, image submission and startup overlay timing stay local.
template <class Device, class Drawing>
void beginOutput(Device &device, Drawing &drawing, int width, int height) {
    device.BeginFrame();
    device.SetCurrenRT(-1);
    drawing.main.SetCurrenRenderTarget(-1);
    drawing.aux.SetCurrenRenderTarget(-1);
    device.alphaEnable(false);
    device.SetupDefaultProjection(width, height, 1); // PROJ_ORTHO
    device.ClearScreen(255, 0, 0, 0);
    neon::D3DXMatrixTranslation(&drawing.main.world, width * -.5f, height * .5f, 0);
}

// Only the three replaceable-device backends use this release contract. The
// caller owns callbacks, services and the factory; value-device/early cores
// retain their distinct release order. No batch/history/effectStack reset added.
template <class EffectOwner, class Drawing, class DeviceOwner, class Binding, class Clear,
          class Factory>
void releaseReplacingDevice(EffectOwner &effect, Drawing &drawing, DeviceOwner &device,
                            bool &deviceReady, bool &gameReady, Binding &binding, Clear clear,
                            Factory factory) {
    effect.reset();
    drawing.effect = nullptr;
    clear();
    if (deviceReady)
        device->Destroy(true);
    drawing.main.device = drawing.aux.device = nullptr;
    drawing.main.vertexBuffer = drawing.aux.vertexBuffer = -1;
    device = factory();
    deviceReady = gameReady = false;
    binding.disconnect();
}
} // namespace neon::backend
