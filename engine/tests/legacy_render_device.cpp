// Saved reference renderer; namespace/host service names adapted for comparison.
#include "legacy_render_device.h"
#include "legacy_render_shaders.h"

#include <glad/gl.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace frozen {
namespace render {

static constexpr float kUncached = -99999.0f;

// ---------------------------------------------------------------- setup ---

NeonDevice::NeonDevice() {
    D3DXMatrixIdentity(&view);
    D3DXMatrixIdentity(&projection);
    for (int s = 0; s < 3; s++) {
        wrapS_[s] = wrapT_[s] = GL_REPEAT;
        appliedWrapS_[s] = appliedWrapT_[s] = GL_REPEAT;
        boundRT_[s] = boundLiteral_[s] = 0;
    }
    for (auto &p : programs_) {
        p.id = 0;
        for (int &l : p.loc)
            l = -1;
        for (auto &v : p.cache)
            for (float &f : v)
                f = kUncached;
    }
}

NeonDevice::~NeonDevice() {}

int NeonDevice::px(float v, float s) const {
    return int(std::lround(v * s));
}

void NeonDevice::SetBackbuffer(unsigned fbo, int logicalW, int logicalH, int pixelW, int pixelH) {
    bbFbo_ = fbo;
    bbW_ = logicalW;
    bbH_ = logicalH;
    bbPixelW_ = pixelW;
    bbPixelH_ = pixelH;
    scaleX_ = logicalW > 0 ? float(pixelW) / float(logicalW) : 1.0f;
    scaleY_ = logicalH > 0 ? float(pixelH) / float(logicalH) : 1.0f;
    // Render targets keep their logical size; reallocate them at the new scale.
    for (auto &rt : rts_) {
        if (rt.fbo == 0)
            continue;
        int state = rt.state, w = rt.width, h = rt.height;
        glDeleteTextures(1, &rt.tex);
        glDeleteFramebuffers(1, &rt.fbo);
        rt = RenderTarget{};
        if (createFBO(rt, w, h)) {
            rt.width = w;
            rt.height = h;
            rt.state = state;
        }
    }
    if (currentRT_ < 0)
        glBindFramebuffer(GL_FRAMEBUFFER, bbFbo_);
}

// Returns true on failure (as the original).
bool NeonDevice::Init(int flags, int logicalW, int logicalH, unsigned fbo, int pixelW, int pixelH,
                      int orientation) {
    if (flags & 1)
        return true;
    orientation_ = orientation;
    upsideDown_ = false;
    SetBackbuffer(fbo, logicalW, logicalH, pixelW, pixelH);
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    viewW_ = logicalW;
    viewH_ = logicalH;
    zNear_ = 100.0f;
    zFar_ = 4100.0f;
    SetupDefaultProjection(logicalW, logicalH, PROJ_ORTHO);
    return false;
}

unsigned NeonDevice::compile(unsigned type, const char *src) {
    std::string text = host_ ? host_->adaptShader(src, type == GL_VERTEX_SHADER ? 0 : 1) : src;
    GLuint s = glCreateShader(type);
    const char *p = text.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        if (host_)
            host_->log("Neon shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

int NeonDevice::InitAllShaders() {
    for (int i = 0; i < 2; i++)
        vertexShaders_[i] = compile(GL_VERTEX_SHADER, kNeonVertexShaders[i]);
    for (int i = 0; i < 6; i++)
        pixelShaders_[i] = compile(GL_FRAGMENT_SHADER, kNeonPixelShaders[i]);

    static const int vsOf[kNumPrograms] = {0, 0, 0, 0, 0, 0, 1};
    static const int psOf[kNumPrograms] = {0, 1, 2, 3, 4, 5, 0};
    for (int i = 0; i < kNumPrograms; i++) {
        ShaderProgram &p = programs_[i];
        p.id = glCreateProgram();
        glAttachShader(p.id, vertexShaders_[vsOf[i]]);
        glAttachShader(p.id, pixelShaders_[psOf[i]]);
        glBindAttribLocation(p.id, 0, "pos0");
        glBindAttribLocation(p.id, 1, "tex0");
        glBindAttribLocation(p.id, 2, "tex1");
        glBindAttribLocation(p.id, 3, "tex2");
        glBindAttribLocation(p.id, 4, "colour");
        glLinkProgram(p.id);
        GLint ok = 0;
        glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
        if (!ok && host_) {
            char log[2048];
            glGetProgramInfoLog(p.id, sizeof log, nullptr, log);
            host_->log("Neon program %d link failed: %s", i, log);
        }
        for (int u = 0; u < kNumUniforms; u++)
            p.loc[u] = glGetUniformLocation(p.id, kNeonUniformNames[u]);
        for (auto &v : p.cache)
            for (float &f : v)
                f = kUncached;
    }
    for (unsigned &s : vertexShaders_) {
        if (s)
            glDeleteShader(s);
        s = 0;
    }
    for (unsigned &s : pixelShaders_) {
        if (s)
            glDeleteShader(s);
        s = 0;
    }
    return 0;
}

// --------------------------------------------------------------- textures --
// The original loads PNGs through UIImage into a CGBitmapContext:
//   * size padded up to powers of two (images over 1024 were halved),
//   * premultiplied alpha (kCGImageAlphaPremultipliedLast),
//   * drawn through a flipped CTM, so memory row 0 is the image's bottom row
//     and the image occupies the top-left of the padded texture.
// Texture filtering was bilinear with GL_REPEAT wrapping.

static int nextPow2(int v) {
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

int NeonDevice::InitLiteralTextures(int count, const std::vector<std::string> &files) {
    if (count > kMaxLiterals)
        return 1;
    for (auto &t : literals_) {
        if (t.name)
            glDeleteTextures(1, &t.name);
        t = LiteralTexture{};
    }
    for (int i = 0; i < count && i < int(files.size()); i++) {
        std::vector<uint8_t> data;
        if (!host_ || !host_->readAsset(files[size_t(i)], data)) {
            if (host_)
                host_->log("missing texture: %s", files[size_t(i)].c_str());
            continue;
        }
        int w = 0, h = 0, comp = 0;
        unsigned char *img = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &comp, 4);
        if (!img) {
            host_->log("could not decode %s", files[size_t(i)].c_str());
            continue;
        }
        int tw = nextPow2(w), th = nextPow2(h);
        if (tw > 1024 || th > 1024)
            host_->log("texture %s exceeds 1024; not downscaled", files[size_t(i)].c_str());
        std::vector<uint8_t> tex(size_t(tw) * th * 4, 0);
        bool hasAlpha = comp == 2 || comp == 4;
        for (int y = 0; y < h; y++) {
            uint8_t *dst = &tex[size_t(th - 1 - y) * tw * 4]; // memory row (th-1-y) = image row y
            const uint8_t *src = img + size_t(y) * w * 4;
            for (int x = 0; x < w; x++) {
                unsigned a = hasAlpha ? src[x * 4 + 3] : 255;
                dst[x * 4 + 0] = uint8_t((src[x * 4 + 0] * a + 127) / 255);
                dst[x * 4 + 1] = uint8_t((src[x * 4 + 1] * a + 127) / 255);
                dst[x * 4 + 2] = uint8_t((src[x * 4 + 2] * a + 127) / 255);
                dst[x * 4 + 3] = uint8_t(a);
            }
        }
        stbi_image_free(img);

        LiteralTexture &t = literals_[i];
        glGenTextures(1, &t.name);
        glBindTexture(GL_TEXTURE_2D, t.name);
        GLint filter = nearestTextures_ ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data());
        t.width = tw;
        t.height = th;
        t.imageW = w;
        t.imageH = h;
        host_->log("texture %d: %s %dx%d (texture %dx%d)", i, files[size_t(i)].c_str(), w, h, tw,
                   th);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    boundLiteral_[0] = boundLiteral_[1] = boundLiteral_[2] = -100;
    return 0;
}

const LiteralTexture *NeonDevice::literal(int i) const {
    return (i >= 0 && i < kMaxLiterals && literals_[i].name) ? &literals_[i] : nullptr;
}

// ------------------------------------------------------------ projection --

void NeonDevice::SetupDefaultProjection(int w, int h, int flags) {
    if (w < 1 || h < 1) {
        w = viewW_;
        h = viewH_;
    }
    viewW_ = w;
    viewH_ = h;
    projFlags_ = flags;
    if (flags & PROJ_ORTHO)
        D3DXMatrixOrthoLH(&projection, float(w), float(h), zNear_, zFar_);
    else
        D3DXMatrixPerspectiveFovLH(&projection, 0.7853981f, float(w) / float(h), zNear_, zFar_);

    // The original rotated the backbuffer projection by 90 degrees when the
    // GL layer was portrait (orientation == 1). The host surface is already
    // landscape, so orientation is always 0 here; only the upside-down flip
    // of the backbuffer remains.
    if (currentRT_ == -1 && upsideDown_) {
        matrix4f flip;
        D3DXMatrixScaling(&flip, -1.0f, -1.0f, 1.0f);
        projection = flip * projection;
    }
    glViewport(0, 0, px(float(w), scaleX_), px(float(h), scaleY_));
}

void NeonDevice::SetupDefaultCamera() {
    D3DXMatrixIdentity(&view);
}

void NeonDevice::alphaEnable(bool on) {
    if (on)
        deviceFlags |= DF_ALPHA_BLEND;
    else
        deviceFlags &= ~uint32_t(DF_ALPHA_BLEND);
}

void NeonDevice::setSamplerTileMode(int kind, int, int stage) {
    if (stage < 0 || stage > 2)
        return;
    if (kind == 0) {
        wrapS_[stage] = GL_MIRRORED_REPEAT;
        wrapT_[stage] = GL_MIRRORED_REPEAT;
    } else {
        // The original writes T of this stage and the S "already applied"
        // cache entry here (looks like an indexing slip); kept as is.
        wrapT_[stage] = GL_MIRRORED_REPEAT;
        appliedWrapS_[stage] = GL_MIRRORED_REPEAT;
    }
}

void NeonDevice::GetCurrentRtSize(int *w, int *h) const {
    if (currentRT_ < 0) {
        *w = bbW_;
        *h = bbH_;
        return;
    }
    *w = rts_[currentRT_].width;
    *h = rts_[currentRT_].height;
}

void NeonDevice::ClearScreen(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | ((deviceFlags & DF_STENCIL) ? GL_DEPTH_BUFFER_BIT : 0));
}

void NeonDevice::HandleZBufFlags(uint32_t flags) {
    if (zbufCache_ == flags)
        return;
    zbufCache_ = flags;
    if (flags & DF_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    if (flags & DF_ALPHA_BLEND)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

void NeonDevice::SetAlphaBlend(unsigned src, unsigned dst) {
    blendSrc_ = src;
    blendDst_ = dst;
    glBlendFunc(src, dst);
}

// --------------------------------------------------------- vertex buffers --

int NeonDevice::CreateVertexBufferWO(int kind, int count, int usage) {
    if (vbCount_ == kMaxVertexBuffers)
        return -1;
    for (int i = 0; i < kMaxVertexBuffers; i++) {
        VertexBufferSlot &vb = vbs_[i];
        if (vb.bytes != 0)
            continue;
        int bytes = count * vertexStride(kind);
        glGenBuffers(1, &vb.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vb.vbo);
        glBufferData(GL_ARRAY_BUFFER, bytes, nullptr,
                     usage == -1 ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        vb.kind = kind;
        vb.bytes = bytes;
        vbCount_++;
        if (lastVB_ == i)
            lastVB_ = -1;
        return i;
    }
    return -1;
}

int NeonDevice::DestroyVertexuffer(int i, int release) {
    if (i < 0 || i >= kMaxVertexBuffers)
        return -1;
    VertexBufferSlot &vb = vbs_[i];
    if (vb.bytes == 0 || vb.vbo == 0)
        return -1;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vb.vbo);
    vb.vbo = 0;
    if (lastVB_ == i)
        lastVB_ = -1;
    if (release) {
        vb.bytes = 0;
        vbCount_--;
    }
    return i;
}

int NeonDevice::UpdateBufferFromMem(int i, const void *data, int count) {
    if (i < 0 || i >= kMaxVertexBuffers || !vbs_[i].vbo)
        return -1;
    // The original uses 40 bytes for kind 0 and 24 for every other kind here.
    int stride = vbs_[i].kind == 0 ? 0x28 : 0x18;
    int bytes = count * stride;
    if (bytes > vbs_[i].bytes)
        bytes = vbs_[i].bytes; // port safety
    if (bytes <= 0)
        return 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbs_[i].vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data);
    return 0;
}

// --------------------------------------------------------- render targets --

bool NeonDevice::createFBO(RenderTarget &rt, int w, int h) {
    rt.pixelW = std::max(1, px(float(w), scaleX_));
    rt.pixelH = std::max(1, px(float(h), scaleY_));
    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
    glGenTextures(1, &rt.tex);
    glBindTexture(GL_TEXTURE_2D, rt.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rt.pixelW, rt.pixelH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.tex, 0);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!ok && host_)
        host_->log("render target %dx%d incomplete", w, h);
    // Restore the current texture and target (the original bound FBO 0 here,
    // which on iOS silently dropped draws until the next target switch).
    glBindTexture(GL_TEXTURE_2D, 0);
    boundRT_[0] = boundRT_[1] = boundRT_[2] = -100;
    boundLiteral_[0] = boundLiteral_[1] = boundLiteral_[2] = -100;
    glBindFramebuffer(GL_FRAMEBUFFER, currentRT_ >= 0 ? rts_[currentRT_].fbo : bbFbo_);
    return ok;
}

int NeonDevice::FindFreeRT(int w, int h) {
    int freeIdx = -1, emptyIdx = -1;
    for (int i = 0; i < kMaxRenderTargets; i++) {
        RenderTarget &rt = rts_[i];
        if (rt.state == 0)
            freeIdx = i;
        else if (rt.state == -1)
            emptyIdx = i;
        if (rt.width == w && rt.height == h && rt.state == 0) {
            rt.state = 1;
            return i;
        }
    }
    // No free target of this size. Prefer an unused slot; otherwise recycle
    // a free target of another size.
    if (freeIdx >= 0 && emptyIdx == -1) {
        RenderTarget &rt = rts_[freeIdx];
        if (rt.fbo) {
            glDeleteTextures(1, &rt.tex);
            glDeleteFramebuffers(1, &rt.fbo);
        }
        rt = RenderTarget{};
        emptyIdx = freeIdx;
    }
    if (emptyIdx < 0)
        return -1;
    RenderTarget &rt = rts_[emptyIdx];
    if (!createFBO(rt, w, h))
        return -1;
    rt.width = w;
    rt.height = h;
    rt.state = 1;
    return emptyIdx;
}

int NeonDevice::ReleaseRTTexture(int i) {
    if (i < 0 || i >= kMaxRenderTargets || rts_[i].state != 1)
        return -1;
    rts_[i].state = 0;
    return 0;
}

int NeonDevice::SetCurrenRT(int i) {
    if (i >= 0 && i < kMaxRenderTargets) {
        RenderTarget &rt = rts_[i];
        if (rt.state != 1) {
            if (host_)
                host_->log("SetCurrenRT: render target %d not in use", i);
            return -1;
        }
        if (currentRT_ != i) {
            glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            currentRT_ = i;
        }
        SetupDefaultProjection(rt.width, rt.height, projFlags_);
        return 0;
    }
    if (currentRT_ != i)
        glBindFramebuffer(GL_FRAMEBUFFER, bbFbo_);
    currentRT_ = -1;
    SetupDefaultProjection(bbW_, bbH_, projFlags_);
    return 0;
}

int NeonDevice::FillSurface(int i, uint32_t) {
    int prev = currentRT_;
    if (i != prev)
        SetCurrenRT(i);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | ((deviceFlags & DF_STENCIL) ? GL_DEPTH_BUFFER_BIT : 0));
    if (i != prev)
        SetCurrenRT(prev);
    return 0;
}

// ------------------------------------------------------------------ frame --

void NeonDevice::SetD3DState() {
    if (stateDone_)
        return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    SetAlphaBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, bbFbo_);
    D3DXMatrixIdentity(&view);
    SetupDefaultProjection(viewW_, viewH_, PROJ_ORTHO);
    for (int s = 0; s < 3; s++) {
        float a[4] = {1, 0, 0, 0}, b[4] = {0, 1, 0, 0};
        std::memcpy(draw.texMat[s][0], a, sizeof a);
        std::memcpy(draw.texMat[s][1], b, sizeof b);
    }
    stateDone_ = true;
}

// The platform draws its present pass between frames, which changes the
// bound program, texture, buffers, attribute arrays, blend state and target.
// Re-establish exactly what the engine last set, so its caches stay true.
void NeonDevice::applyFrameState() {
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    if (zbufCache_ != 0xffffffff) {
        uint32_t f = zbufCache_;
        zbufCache_ = 0xffffffff;
        HandleZBufFlags(f);
    }
    glBlendFunc(blendSrc_, blendDst_);
    glBindFramebuffer(GL_FRAMEBUFFER, currentRT_ >= 0 ? rts_[currentRT_].fbo : bbFbo_);
    glViewport(0, 0, px(float(viewW_), scaleX_), px(float(viewH_), scaleY_));
    if (currentProgram_ >= 0 && currentProgram_ < kNumPrograms)
        glUseProgram(programs_[currentProgram_].id);
    glBindTexture(GL_TEXTURE_2D, unit0Texture_);
    lastVB_ = -1;
    lastKind_ = -100;
}

void NeonDevice::BeginFrame() {
    if (frameBegun_)
        return;
    applyFrameState();
    SetD3DState();
    frameBegun_ = true;
}

void NeonDevice::PresentFrame() {
    frameBegun_ = false;
}

void NeonDevice::Destroy(bool full) {
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, bbFbo_);
    for (auto &rt : rts_) {
        if (rt.fbo == 0)
            continue;
        glDeleteTextures(1, &rt.tex);
        glDeleteFramebuffers(1, &rt.fbo);
        rt = RenderTarget{};
    }
    for (int i = 0; i < kMaxVertexBuffers; i++)
        DestroyVertexuffer(i, full ? 1 : 0);
    if (full) {
        for (auto &p : programs_) {
            if (p.id)
                glDeleteProgram(p.id);
            p.id = 0;
        }
        for (auto &t : literals_) {
            if (t.name)
                glDeleteTextures(1, &t.name);
            t = LiteralTexture{};
        }
    }
}

// ----------------------------------------------------------------- render --

void NeonDevice::bindSource(int stage, unsigned tex, unsigned wrapS, unsigned wrapT) {
    // Always texture unit 0, as in the original (see header).
    glBindTexture(GL_TEXTURE_2D, tex);
    unit0Texture_ = tex;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(wrapT));
    glUniform1i(programs_[currentProgram_].loc[U_Tex0 + stage], stage);
}

int NeonDevice::RenderBuffer(int i, int primitive, const matrix4f &world, int count,
                             NeonBaseRender *obj) {
    if (i < 0 || i >= kMaxVertexBuffers || vbs_[i].vbo == 0 || vbs_[i].bytes == 0)
        return -1;
    if (!frameBegun_) {
        if (host_)
            host_->log("RenderBuffer called outside BeginFrame/PresentFrame");
        return -1;
    }
    HandleZBufFlags(deviceFlags);
    SetCurrenRT(draw.renderTarget);

    if (obj && obj->clipTop >= 0) {
        int h = currentRT_ >= 0 ? rts_[currentRT_].height : bbH_;
        glScissor(px(float(obj->clipLeft), scaleX_), px(float(h - obj->clipBottom), scaleY_),
                  px(float(obj->clipRight - obj->clipLeft), scaleX_),
                  px(float(obj->clipBottom - obj->clipTop), scaleY_));
        glEnable(GL_SCISSOR_TEST);
    }

    if (lastVB_ != i) {
        glBindBuffer(GL_ARRAY_BUFFER, vbs_[i].vbo);
        lastKind_ = -100;
    }
    lastVB_ = i;
    if (lastKind_ != vbs_[i].kind) {
        lastKind_ = vbs_[i].kind;
        if (lastKind_ == 0) {
            for (GLuint a = 0; a < 5; a++)
                glEnableVertexAttribArray(a);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 40, reinterpret_cast<void *>(0));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 40, reinterpret_cast<void *>(12));
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 40, reinterpret_cast<void *>(20));
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 40, reinterpret_cast<void *>(28));
            glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 40,
                                  reinterpret_cast<void *>(36));
        }
    }

    // World * view * projection, uploaded transposed (D3D row-vector maths in GLSL).
    matrix4f all = world * view * projection, allT;
    D3DXMatrixTranspose(&allT, &all);

    // Program selection: out-of-range requests keep the current program.
    int want = draw.program;
    if (want >= 0 && want < kNumPrograms) {
        if (want != currentProgram_) {
            currentProgram_ = want;
            for (auto &v : programs_[want].cache)
                for (float &f : v)
                    f = kUncached;
            glUseProgram(programs_[want].id);
        }
    } else if (currentProgram_ < -1) {
        currentProgram_ = 0;
        for (auto &v : programs_[0].cache)
            for (float &f : v)
                f = kUncached;
        glUseProgram(programs_[0].id);
    }
    if (currentProgram_ < 0)
        currentProgram_ = 0;
    ShaderProgram &prog = programs_[currentProgram_];

    glUniformMatrix4fv(prog.loc[U_WorldViewProj], 1, GL_FALSE, allT.data());
    if (prog.loc[U_MatTex0a] >= 0) {
        glUniform4fv(prog.loc[U_MatTex0a], 1, draw.texMat[0][0]);
        glUniform4fv(prog.loc[U_MatTex0b], 1, draw.texMat[0][1]);
    }
    if (currentProgram_ != 6) {
        if (prog.loc[U_MatTex1a] >= 0) {
            glUniform4fv(prog.loc[U_MatTex1a], 1, draw.texMat[1][0]);
            glUniform4fv(prog.loc[U_MatTex1b], 1, draw.texMat[1][1]);
        }
        if (prog.loc[U_MatTex2a] >= 0) {
            glUniform4fv(prog.loc[U_MatTex2a], 1, draw.texMat[2][0]);
            glUniform4fv(prog.loc[U_MatTex2b], 1, draw.texMat[2][1]);
        }
    }

    // Shader constants c0, c1, c2, c5 (the only ones the original uploads).
    // Compare all four channels to avoid stale colour state; the original cached only x.
    if (draw.constants && draw.constEnabled) {
        static const struct {
            int flag, slot, cache;
        } kConst[] = {{0, U_C0, 0}, {1, U_C1, 1}, {2, U_C2, 2}, {5, U_C5, 3}};
        for (auto &c : kConst) {
            if (!draw.constEnabled[c.flag] || prog.loc[c.slot] < 0)
                continue;
            const float *v = draw.constants + c.flag * 4;
            if (std::memcmp(prog.cache[c.cache], v, sizeof(float) * 4) == 0)
                continue;
            std::memcpy(prog.cache[c.cache], v, sizeof(float) * 4);
            glUniform4fv(prog.loc[c.slot], 1, v);
        }
    }

    // Texture sources. Render-target and literal bindings share per-stage
    // caches; binding one kind invalidates the other's cache for that stage.
    for (int s = 0; s < 3; s++) {
        int src = draw.srcRT[s];
        if (src == -1)
            continue;
        if (src == boundRT_[s] && appliedWrapS_[s] == wrapS_[s] && appliedWrapT_[s] == wrapT_[s])
            continue;
        boundRT_[s] = src;
        appliedWrapS_[s] = wrapS_[s];
        appliedWrapT_[s] = wrapT_[s];
        boundLiteral_[s] = -100;
        if (prog.loc[U_Tex0 + s] >= 0 && src >= 0 && src < kMaxRenderTargets)
            bindSource(s, rts_[src].tex, wrapS_[s], wrapT_[s]);
    }
    for (int s = 0; s < 3; s++) {
        int src = draw.srcLiteral[s];
        if (src == -1)
            continue;
        if (src == boundLiteral_[s] && appliedWrapS_[s] == wrapS_[s] &&
            appliedWrapT_[s] == wrapT_[s])
            continue;
        boundLiteral_[s] = src;
        appliedWrapS_[s] = wrapS_[s];
        appliedWrapT_[s] = wrapT_[s];
        boundRT_[s] = -100;
        const LiteralTexture *t = literal(src);
        if (prog.loc[U_Tex0 + s] >= 0 && t)
            bindSource(s, t->name, wrapS_[s], wrapT_[s]);
    }
    for (int s = 0; s < 3; s++) {
        if (draw.srcFont[s] != -1 && !fontWarned_) {
            fontWarned_ = true;
            if (host_)
                host_->log("NeonText font textures are not supported (source %d)", draw.srcFont[s]);
        }
    }

    if (count < 1)
        count = vbs_[i].bytes / vertexStride(vbs_[i].kind);
    glDrawArrays(GLenum(primitive), 0, count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    return 0;
}

// ========================================================= NeonBaseRender ==

NeonBaseRender::NeonBaseRender() {
    D3DXMatrixIdentity(&world);
    D3DXMatrixIdentity(&matrixB);
    D3DXMatrixIdentity(&matrixC);
    for (auto &m : texMatrix)
        D3DXMatrixIdentity(&m);
    std::memset(constants, 0, sizeof constants);
    std::memset(constants2, 0, sizeof constants2);
    std::memset(constEnabled, 0, sizeof constEnabled);
    std::memset(constEnabled2, 0, sizeof constEnabled2);
}

NeonBaseRender::~NeonBaseRender() {
    if (!device)
        return;
    if (vertexBuffer != -1)
        device->DestroyVertexuffer(vertexBuffer, 1);
    if (renderTarget != -1)
        device->ReleaseRTTexture(renderTarget);
}

int NeonBaseRender::SetSourceFromRTs(int a, int b, int c) {
    srcRT[0] = a;
    srcRT[1] = b;
    srcRT[2] = c;
    return 0;
}

int NeonBaseRender::SetSourceFromRTs(int a, int b, int c, int la, int lb, int lc) {
    srcRT[0] = a;
    srcRT[1] = b;
    srcRT[2] = c;
    srcLiteral[0] = la;
    srcLiteral[1] = lb;
    srcLiteral[2] = lc;
    return 0;
}

int NeonBaseRender::SetSourceFromLiterals(int a, int b, int c) {
    srcLiteral[0] = a;
    srcLiteral[1] = b;
    srcLiteral[2] = c;
    return 0;
}

int NeonBaseRender::SetSourceFromLiterals(int a, int b, int c, int fa, int fb, int fc) {
    srcLiteral[0] = a;
    srcLiteral[1] = b;
    srcLiteral[2] = c;
    srcFont[0] = fa;
    srcFont[1] = fb;
    srcFont[2] = fc;
    return 0;
}

int NeonBaseRender::SetSourceFromFont(int a, int b, int c) {
    srcFont[0] = a;
    srcFont[1] = b;
    srcFont[2] = c;
    srcOther[0] = srcOther[1] = srcOther[2] = -1;
    return 0;
}

int NeonBaseRender::SetVertexBufferWO(int vb) {
    if (!device)
        return -1;
    vertexBuffer = vb;
    return 0;
}

int NeonBaseRender::ReleaseVertexBuffer(int vb) {
    if (!device)
        return -1;
    if (vb != -1)
        return device->DestroyVertexuffer(vb, 1);
    if (vertexBuffer == -1)
        return -1;
    device->DestroyVertexuffer(vertexBuffer, 1);
    vertexBuffer = -1;
    return 0;
}

int NeonBaseRender::ReleaseRTTexture(int rt) {
    if (device)
        device->ReleaseRTTexture(rt);
    return 0;
}

int NeonBaseRender::ClearCurrentRT(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    int prev = device->currentRenderTarget();
    device->SetCurrenRT(renderTarget);
    device->ClearScreen(a, r, g, b);
    device->SetCurrenRT(prev);
    return 0;
}

int NeonBaseRender::GetRTTexture(int w, int h) {
    return device ? device->GetRTTexture(w, h) : -1;
}

int NeonBaseRender::UpdateBufferFromMem(const void *data, int count) {
    if (!device || vertexBuffer == -1)
        return -1;
    device->UpdateBufferFromMem(vertexBuffer, data, count);
    return 0;
}

int NeonBaseRender::GetVertexBufferWO(int kind, int count, int usage, bool separate) {
    if (!device)
        return -1;
    if (separate)
        return device->CreateVertexBufferWO(kind, count, usage);
    if (vertexBuffer != -1)
        device->DestroyVertexuffer(vertexBuffer, 1);
    vertexBuffer = device->CreateVertexBufferWO(kind, count, usage);
    return vertexBuffer != -1 ? 0 : -1;
}

int NeonBaseRender::Render(int count) {
    if (!device || !initialised || vertexBuffer == -1)
        return -1;
    NeonDevice::DrawState &d = device->draw;
    d.program = shader;
    d.programAux = shaderAux;
    d.constants = constants;
    d.constEnabled = constEnabled;
    d.renderTarget = renderTarget < 0 ? -1 : renderTarget;
    for (int s = 0; s < 3; s++) {
        d.srcRT[s] = srcRT[s];
        d.srcLiteral[s] = srcLiteral[s];
        d.srcFont[s] = srcFont[s];
        d.srcOther[s] = srcOther[s];
        for (int k = 0; k < 4; k++) {
            d.texMat[s][0][k] = texMatrix[s].m[k][0];
            d.texMat[s][1][k] = texMatrix[s].m[k][1];
        }
    }
    device->RenderBuffer(vertexBuffer, primitive, world, count, this);
    return 0;
}

} // namespace render
} // namespace frozen
