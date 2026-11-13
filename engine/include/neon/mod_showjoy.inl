// Common effect implementation. Included inside a module binding namespace.
// Game access is exclusively through GamePresentationHost.

showJoy::showJoy() {
    D3DXMatrixIdentity(&ident);
    mode = 500;
    kind = 2;
}

showJoy::~showJoy() {
    initialised = 0;
    if (stack && stack->effect) {
        if (stack->effect->gameHost)
            stack->effect->gameHost->releaseGame();
    }
}

void showJoy::setBasics() {
    kind = 1;
    if (mode == 500)
        iconIndex = kPresentationIcon;
}

void showJoy::setDefaults() {
    setBasics();
    for (int i = 28; i <= 31; i++)
        p(i) = 1.0;
}

void showJoy::Release_Module_Resources() {
    initialised = 0;
    if (stack && stack->effect) {
        if (stack->effect->gameHost)
            stack->effect->gameHost->releaseGame();
    }
}

int showJoy::Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) {
    index = idx;
    stack = st;
    device = dev;
    if (st->effect && st->effect->gameHost)
        st->effect->gameHost->prepareGame();
    if (stack && stack->vb0 >= 0) {
        initialised = 1;
        if (!stack->render.initialised) {
            stack->render.Init(device);
            stack->render.initialised = 1;
            stack->render.shader = 0;
        }
        for (int &e : paramEnabled)
            e = 1;
    }
    return 0;
}

int showJoy::Run_Module() {
    sprite spr;
    if (!initialised)
        return -1;
    GamePresentationHost *game = stack->effect->gameHost;
    if (!game || !game->isRunning())
        return 0;

    const auto rules = game->presenterRules();
    if (rules.purgeBefore)
        purge();
    D3DXMatrixIdentity(&ident);
    NeonBaseRender &r = stack->render;
    r.shader = 0;
    r.shaderAux = 0;
    r.texMatrix[0] = ident;
    for (int i = 0; i < rules.blendCalls; ++i)
        r.device->SetAlphaBlend(0x302, 0x303);
    float hw = float(stack->rtW) * 0.5f, hh = float(stack->rtH) * 0.5f;
    SetModuleOutputRT(&r);
    SetModuleSourceTextures(&r);
    device->SetupDefaultProjection(int(hw + hw), int((hh + hh) * stack->effect->aspect),
                                   rules.projectionFlags);
    device->SetupDefaultCamera();

    spr.frame = rules.sampleFrame;
    spr.col = colour(double(int(p(28) * 255.0)), double(int(p(29) * 255.0)),
                     double(int(p(30) * 255.0)), double(int(p(31) * 255.0)));
    spr.col.white(1.0);
    spr.batchCount = 0;
    spr.flags = SPR_CENTRE_X | SPR_CENTRE_Y;
    spr.batchStack = stack;
    spr.batchPtr = stack->verts;
    _input_info src{stack->id, index, 0, INPUT_LITERAL}, dst{stack->id, index, 0, 0};
    stack->effect->AssignTextureInput(&src, &dst);
    r.primitive = D3DPT_TRIANGLELIST;
    r.world = ident;
    FillShaderConstants(r.shader, false, 0.0f, 0, int(hw), int(hh), 1.0f, 1.0f, nullptr, nullptr,
                        r.constants, r.constEnabled);
    for (int i = 0; i < 4; i++)
        r.constants[20 + i] = float(p(24 + i));
    r.constEnabled[5] = true;

    double w = double(stack->rtW), h = double(stack->rtH);
    game->drawForeground(spr, w, h);

    game->finishForeground(spr, w, h);
    spr.setupPurge();
    return 0;
}
