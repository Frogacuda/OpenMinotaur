// Saved reference source blocks. Only owner/type names and the allocation seam
// are adapted. Reference implementations never call backend_setup.h operations.
template <class Drawing, class Device> bool oldEarlyDrawing(Drawing &drawing, Device &device) {
    drawing.main.Init(&device);
    drawing.main.GetVertexBufferWO(0, 20000, -1, false);
    drawing.aux.Init(&device);
    drawing.aux.GetVertexBufferWO(1, 24000, -1, false);
    for (auto *r : {&drawing.main, &drawing.aux}) {
        r->initialised = 1;
        r->shader = 0;
        r->primitive = 4;
        for (auto &m : r->texMatrix)
            neon::D3DXMatrixIdentity(&m);
        neon::D3DXMatrixIdentity(&r->world);
    }
    return true;
}
template <class Drawing, class Device> bool oldLateDrawing(Drawing &drawing, Device &device) {
    for (auto *r : {&drawing.main, &drawing.aux}) {
        r->Init(&device);
        if (r->GetVertexBufferWO(0, 24000, -1, false))
            return false;
        r->initialised = 1;
        r->shader = 0;
        r->primitive = 4;
        for (auto &matrix : r->texMatrix)
            neon::D3DXMatrixIdentity(&matrix);
        neon::D3DXMatrixIdentity(&r->world);
    }
    return true;
}
template <class Effect, class Drawing, class Device, class Binding, class Clear, class Factory>
void oldRelease(Effect &effect, Drawing &drawing, Device &device, bool &deviceReady,
                bool &gameReady, Binding &binding, Clear clear, Factory factory) {
    effect.reset();
    drawing.effect = nullptr;
    clear();
    if (deviceReady)
        device->Destroy(true);
    // A new startup can draw before effect targets exist. Discard the released
    // device's binding caches rather than restoring deleted GL names on reopen.
    drawing.main.device = drawing.aux.device = nullptr;
    drawing.main.vertexBuffer = drawing.aux.vertexBuffer = -1;
    device = factory();
    deviceReady = gameReady = false;
    binding.disconnect();
}
template <class Device, class Host, class Surface>
bool oldDevice(Device &device, Host &host, const Surface &surface, int width, int height,
               bool &ready) {
    device.setHost(&host);
    if (device.Init(0, width, height, surface.framebuffer, surface.width, surface.height, 0))
        return false;
    ready = true;
    device.InitAllShaders();
    device.setNearestTextures(false);
    return true;
}
template <class Device, class Drawing>
void oldOutput(Device &device, Drawing &drawing, int Width, int Height) {
    device.BeginFrame();
    device.SetCurrenRT(-1);
    drawing.main.SetCurrenRenderTarget(-1);
    drawing.aux.SetCurrenRenderTarget(-1);
    device.alphaEnable(false);
    device.SetupDefaultProjection(Width, Height, 1);
    device.ClearScreen(255, 0, 0, 0);
    neon::D3DXMatrixTranslation(&drawing.main.world, Width * -.5f, Height * .5f, 0);
}
template <class Device> bool oldValidate(Device &device, int count) {
    for (int n = 0; n < count; ++n)
        if (!device.literal(n) || !device.literal(n)->name)
            return false;
    return true;
}
template <class Host, class Resources, class Bytes>
bool oldCatalog(Host &host, Resources &resources, Bytes &data, std::string &error) {
    return host.readAsset("NeonResources.xml", data) &&
           resources.load(std::string(data.begin(), data.end()), &error);
}
template <class Effect, class Visit> auto oldVisit(Effect &effect, Visit visit) {
    decltype(effect.stacks) last = nullptr;
    for (auto *stack = effect.stacks; stack; stack = stack->nextStack) {
        last = stack;
        for (auto *module = stack->modules; module; module = module->next)
            visit(*stack, *module);
    }
    return last;
}

template <class Drawing, class Device> bool oldGridDrawing(Drawing &drawing, Device &device) {
    for (auto *render : {&drawing.main, &drawing.aux}) {
        render->Init(&device);
        render->GetVertexBufferWO(0, 24000, -1, false);
        render->initialised = 1;
        render->shader = 0;
        render->primitive = 4;
        for (auto &matrix : render->texMatrix)
            neon::D3DXMatrixIdentity(&matrix);
        neon::D3DXMatrixIdentity(&render->world);
    }
    return true;
}
