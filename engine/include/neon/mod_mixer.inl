// Common effect implementation. Included inside a module binding namespace.
// Game access is exclusively through GamePresentationHost.

Mixer::Mixer() {
    D3DXMatrixIdentity(&ident);
    mode = 1;
    kind = 0;
}

void Mixer::setBasics() {
    kind = 0;
    static const struct {
        int mode, shader, icon;
    } kTab[] = {
        {2, 0, 0x7e},   {3, 2, 0x7f},   {4, 3, 0x71},   {5, 1, 0x80},   {6, 4, 0xb},
        {7, 5, 0xc},    {8, 6, 0xd},    {9, 12, 0xe},   {10, 17, 0x27}, {11, 10, 0x29},
        {12, 20, 0x28}, {13, 21, 0x14}, {14, 7, 0x15},  {15, 8, 0x10},  {16, 9, 0x11},
        {17, 60, 0x17}, {18, 62, 0x18}, {19, 63, 0x19}, {20, 50, 0x1a}, {21, 51, 0x1b},
        {22, 53, 0x1c}, {23, 55, 0x2b}, {24, 33, 0x2c},
    };
    for (const auto &e : kTab)
        if (e.mode == mode) {
            params[0].ival = e.shader;
            iconIndex = e.icon;
            return;
        }
}

void Mixer::setDefaults() {
    setBasics();
    for (int i = 28; i <= 31; i++)
        p(i) = 1.0;
    _input_info src{stack->id, index, 9, INPUT_LITERAL}, dst{stack->id, index, 0, 0};
    stack->effect->AssignTextureInput(&src, &dst);
    src.num = 4;
    dst.num = 1;
    stack->effect->AssignTextureInput(&src, &dst);
    src.num = 8;
    dst.num = 2;
    stack->effect->AssignTextureInput(&src, &dst);
    if (mode == 3)
        for (int i = 24; i <= 27; i++)
            p(i) = 0.5;
    else if (mode == 4)
        for (int i = 24; i <= 27; i++)
            p(i) = 1.0;
    else if (mode == 5) {
        _input_info fb{stack->id, index, stack->stackIndex, INPUT_STACK},
            d0{stack->id, index, 0, 0};
        inputSrc[0] = stack->stackIndex;
        stack->effect->AssignTextureInput(&fb, &d0);
        p(28) = 0.8;
        p(29) = p(30) = p(31) = 0.95;
        p(5) = p(6) = 0.98;
    }
}

void Mixer::Release_Module_Resources() {
    initialised = 0;
    if (stack)
        stack->render.ReleaseVertexBuffer(vb);
}

int Mixer::Init_Module_Resources(NeonDevice *dev, int idx, NeonStack *st) {
    index = idx;
    device = dev;
    stack = st;
    // (Called once from AddModule before the stack has a device, then again
    // from NeonStack::Init; only the second call does any work.)
    if (!stack || stack->vb0 < 0)
        return 0;
    if (!stack->render.initialised) {
        stack->render.Init(device);
        stack->render.shader = 0;
        stack->render.initialised = 1;
    }
    // The original also allocated this on the first call (leaking one buffer).
    if (vb < 0)
        vb = st->render.GetVertexBufferWO(0, 8, -1, true);
    if (initialised)
        return 0;

    params[0].ival = 0;
    initialised = 1;
    for (int &e : paramEnabled)
        e = 1;
    auto range = [&](int i, double lo, double hi) {
        ranges[i].min = lo;
        ranges[i].max = hi;
    };
    // Per input k (params 6k+2 .. 6k+7): rotation, x, y, scale x, scale y, (spare).
    for (int k = 0; k < 3; k++) {
        int b = 6 * k;
        p(b + 2) = 0;
        p(b + 3) = 0;
        p(b + 4) = 0;
        p(b + 5) = 1;
        p(b + 6) = 1;
        p(b + 7) = 1;
        range(b + 2, -1, 1);
        range(b + 3, -8, 8);
        range(b + 4, -8, 8);
        range(b + 5, 0, 8);
        range(b + 6, 0, 8);
        range(b + 7, 0, 1);
    }
    for (int i = 24; i <= 27; i++) {
        p(i) = 0.5;
        range(i, 0, 1);
    } // c5 mix
    params[0].ival = 0;
    for (int i = 28; i <= 31; i++) {
        p(i) = 1.0;
        range(i, 0, 1);
    } // vertex colour
    range(22, -1, 1);
    range(23, -1, 1);
    range(32, 0, 1);
    range(33, 0, 1);
    p(22) = 0;
    p(23) = 0;
    for (int i = 34; i <= 37; i++)
        range(i, 0, 1);
    for (int i = 44; i <= 52; i++) {
        p(i) = 0;
        range(i, -1, 1);
    } // drift speeds
    p(32) = 1;
    p(33) = 1;
    p(34) = 0.5;
    p(35) = 0.5;
    p(36) = 1;
    p(37) = 0;
    p(38) = 1;
    p(39) = 1;
    p(40) = 1;
    p(41) = 4;
    p(42) = 1;
    for (int i = 0; i < 64; i++)
        ranges[i].saved = p(i);
    return 0;
}

int Mixer::Run_Module() {
    if (!initialised)
        return -1;
    NeonBaseRender &r = stack->render;
    D3DXMatrixIdentity(&ident);
    r.shader = params[0].ival;
    r.shaderAux = 0;

    // Shader 1 (the game's feedback mixer): take transform and tint from the game.
    GamePresentationHost *host = stack->effect ? stack->effect->gameHost : nullptr;
    if (r.shader == 1 && host && host->hasGame()) {
        const auto feedback = host->feedback();
        r.device->setUpsideDown(feedback.inputFlipped);
        p(2) = double(feedback.rotation);
        p(3) = double(feedback.x);
        p(4) = double(feedback.y);
        p(5) = double(feedback.scaleX);
        p(6) = double(feedback.scaleY);
        p(28) = double(feedback.colour[0]);
        p(29) = double(feedback.colour[1]);
        p(30) = double(feedback.colour[2]);
        p(31) = double(feedback.colour[3]);
    }

    for (int s = 0; s < 3; s++) {
        int b = 6 * s, q = 46 + 3 * s;
        bool special = unsigned(mode - 17) <= 2u && s == 0;
        if (std::fabs(p(q)) > 1e-9) {
            p(b + 2) += p(q);
            floatWrap(b + 2);
        }
        if (std::fabs(p(q - 1)) > 1e-9) {
            if (special) {
                p(22) += p(45);
                floatWrap(22);
            } else {
                p(b + 4) += p(q - 1);
                floatWrap(b + 4);
            }
        }
        if (std::fabs(p(q - 2)) > 1e-9) {
            if (special) {
                p(23) += p(44);
                floatWrap(23);
            } else {
                p(b + 3) += p(q - 2);
                floatWrap(b + 3);
            }
        }
        float tx = float(p(b + 3) * double(float(1.0 / p(b + 5))));
        float ty = float(p(b + 4) * double(float(1.0 / p(b + 6))));
        matrix4f t0, t1, sc, rz, t2;
        D3DXMatrixTranslation(&t0, -0.5f, -0.5f, 0.0f);
        D3DXMatrixTranslation(&t1, tx, ty, 0.0f);
        D3DXMatrixScaling(&sc, float(p(b + 5)), float(p(b + 6)), 1.0f);
        D3DXMatrixRotationZ(&rz, float(p(b + 2) * 3.141592502593994));
        D3DXMatrixTranslation(&t2, 0.5f, 0.5f, 0.0f);
        r.texMatrix[s] = t0 * rz * t1 * sc * t2;
    }

    float hw = float(stack->rtW) * 0.5f, hh = float(stack->rtH) * 0.5f;
    SetModuleOutputRT(&r);
    SetModuleSourceTextures(&r);
    r.device->SetAlphaBlend(1, 1);
    device->SetupDefaultProjection(int(hw + hw), int(hh + hh), PROJ_ORTHO);
    device->SetupDefaultCamera();
    device->setSamplerTileMode(0, wrap[0], 0);
    device->setSamplerTileMode(1, wrap[3], 0);
    device->setSamplerTileMode(0, wrap[1], 1);
    device->setSamplerTileMode(1, wrap[4], 1);
    device->setSamplerTileMode(0, wrap[2], 2);
    device->setSamplerTileMode(1, wrap[5], 2);
    vptr = quad;

    // (Mode 9 picks shader 12 or 15 from the order of params 24-26 here;
    // Minotron never uses mode 9, so that is not ported.)

    uint32_t c = uint32_t(int(p(28) * 255.0)) | (uint32_t(int(p(29) * 255.0)) << 8) |
                 (uint32_t(int(p(30) * 255.0)) << 16) | (uint32_t(int(p(31) * 255.0)) << 24);
    int savedVB = r.GetCurrentBufNum();
    auto V = [&](float x, float y, float u, float v) {
        NeonVertex &n = *vptr++;
        n.x = x;
        n.y = y;
        n.z = 100.0f;
        n.u0 = n.u1 = n.u2 = u;
        n.v0 = n.v1 = n.v2 = v;
        n.colour = c;
    };
    V(-hw, -hh, 0, 1);
    V(-hw, hh, 0, 0);
    V(hw, -hh, 1, 1);
    V(hw, hh, 1, 0);
    r.SetVertexBufferWO(vb);
    r.UpdateBufferFromMem(quad, 4);
    r.primitive = D3DPT_TRIANGLESTRIP;
    r.world = ident;

    if (Waves)
        FillShaderConstants(r.shader, params[2].ival != 0, 0.0f, Waves->beat, int(hw), int(hh),
                            1.0f, 1.0f, nullptr, nullptr, r.constants, r.constEnabled);
    for (int i = 0; i < 4; i++)
        r.constants[20 + i] = float(p(24 + i));
    r.constEnabled[5] = true;
    r.constants[0] = float(p(22));
    r.constants[1] = float(p(23));
    r.constants[2] = float(p(32));
    r.constants[3] = float(p(33));
    r.constants[4] = 0.0f; // transition value (always 0)
    r.constEnabled[0] = true;
    r.constEnabled[1] = true;
    if (r.shader == 25) {
        for (int i = 24; i <= 27; i++)
            r.constants[i] = 0.0f;
        for (int i = 0; i < 9; i++)
            r.constants[28 + i] = float(p(32 + i));
        r.constants[37] = r.constants[38] = r.constants[39] = 0.0f;
        for (int i = 6; i <= 9; i++)
            r.constEnabled[i] = true;
        r.constants[40] = float(p(41) / double(hw));
        r.constants[41] = float(p(42) / double(hh));
        r.constants[42] = r.constants[40] * 0.5f;
        r.constants[43] = r.constants[41] * 0.5f;
        r.constEnabled[10] = true;
    } else {
        for (int i = 0; i < 4; i++)
            r.constants[24 + i] = float(p(34 + i));
        r.constEnabled[6] = true;
    }
    r.Render(4);
    r.SetVertexBufferWO(savedVB);
    return 0;
}
