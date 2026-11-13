// neon_device.h - NeonDevice and NeonBaseRender: the engine's thin GL layer.
//
// The engine was written against Direct3D and ported to OpenGL ES 2 with a
// D3D-flavoured device object. This is a re-implementation of that object
// with the original's API and behaviour, plus three port changes:
//   * The "backbuffer" is the host's offscreen surface FBO.
//   * Logical vs pixel size: the game sees its original screen size (e.g.
//     480x320) while the backbuffer and render targets are allocated at
//     pixelScale times that, so everything is drawn at higher resolution.
//   * GL state the platform touches between frames is re-applied each frame.
//
// Faithfully kept quirks (they affect how effects look):
//   * Every texture is bound on texture unit 0; the multi-texture effect
//     shaders therefore read black from units 1-2, as on the original.
//   * Primitive types are raw GL modes (GL_TRIANGLES = 4, ...).
#pragma once

#include "neon/math.h"
#include "neon/render_host.h"

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace neon {
namespace render {

class NeonBaseRender;

// Primitive types: the engine passes these straight to glDrawArrays.
enum _D3DPRIMITIVETYPE {
    D3DPT_POINTLIST = 0,
    D3DPT_LINELIST = 1,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6,
};

// The standard 40-byte vertex (vertex buffer kind 0).
struct NeonVertex {
    float x, y, z;
    float u0, v0;
    float u1, v1;
    float u2, v2;
    uint32_t colour; // R,G,B,A bytes in memory order
};
static_assert(sizeof(NeonVertex) == 40, "vertex layout must match the original");

// Bytes per vertex for each buffer kind (only kind 0 has attribute pointers).
inline int vertexStride(int kind) {
    return kind == 1 ? 0x18 : (kind == 2 ? 0x48 : 0x28);
}

constexpr int kMaxRenderTargets = 64;
constexpr int kMaxVertexBuffers = 32;
constexpr int kMaxLiterals = 32;
constexpr int kNumPrograms = 7;
constexpr int kNumUniforms = 22;

enum UniformSlot {
    U_WorldViewProj,
    U_MatTex0a,
    U_MatTex0b,
    U_MatTex1a,
    U_MatTex1b,
    U_MatTex2a,
    U_MatTex2b,
    U_Tex0,
    U_Tex1,
    U_Tex2,
    U_Tex3,
    U_Tex4,
    U_Tex5,
    U_C0,
    U_C1,
    U_C2,
    U_C5,
    U_C7,
    U_C8,
    U_C9,
    U_C10,
    U_C25,
};

// NeonDevice::deviceFlags
enum DeviceFlags : uint32_t {
    DF_ALPHA_BLEND = 0x80, // blending enabled for draws
    DF_STENCIL = 0x100,    // clears also clear stencil
    DF_DEPTH_TEST = 0x200, // depth test (LEQUAL) for draws
    DF_FIRST_DRAW = 0x400,
};

// SetupDefaultProjection flags
enum ProjectionFlags { PROJ_ORTHO = 1, PROJ_UNSIGNED = 2 };

struct RenderTarget {
    unsigned fbo = 0, tex = 0;
    int width = -1, height = -1; // logical size
    int pixelW = 0, pixelH = 0;
    int state = -1; // -1 never created, 0 free, 1 in use
};

struct VertexBufferSlot {
    unsigned vbo = 0;
    int bytes = 0; // 0 = slot free
    int kind = 0;
    std::vector<uint8_t> contents;
};

struct LiteralTexture {
    unsigned name = 0;
    int width = 0, height = 0;  // texture size (power of two)
    int imageW = 0, imageH = 0; // source image size
};

struct ShaderProgram {
    unsigned id = 0;
    int loc[kNumUniforms];
    float cache[4][4]; // last uploaded c0, c1, c2, c5
};

// Allocation policy is selected by the module; no game-ID branches.

class NeonDevice {
  public:
    NeonDevice();
    NeonDevice(const NeonDevice &) = delete;
    NeonDevice &operator=(const NeonDevice &) = delete;
    ~NeonDevice();
    // All devices share the application's rendering thread/context. Checkpoint
    // before detachment; abandon stale names before allocating in a replacement.
    // Logical slots and all game/effect owners remain alive.
    static bool checkpointAllGraphics();
    static void discardGraphicsCheckpoints();
    static void abandonAllGraphics();
    static bool restoreAllGraphics();

    // ---- port integration
    void setHost(neon::RenderHost *host) {
        host_ = host;
    }
    // Nearest instead of the original's bilinear filtering for literal textures.
    void setNearestTextures(bool nearest) {
        nearestTextures_ = nearest;
    }
    // Rebind the backbuffer (host surface) after it has been recreated.
    void SetBackbuffer(unsigned fbo, int logicalW, int logicalH, int pixelW, int pixelH);
    float pixelScaleX() const {
        return scaleX_;
    }
    float pixelScaleY() const {
        return scaleY_;
    }
    // Byte 0x37f8: set from the game's input-flip flag by the feedback Mixer.
    void setUpsideDown(bool b) {
        upsideDown_ = b;
    }

    // ---- original API
    bool Init(int flags, int logicalW, int logicalH, unsigned fbo, int pixelW, int pixelH,
              int orientation);
    int InitAllShaders();
    int InitLiteralTextures(int count, const std::vector<std::string> &files);
    const LiteralTexture *literal(int i) const;

    void SetupDefaultProjection(int w, int h, int flags);
    void SetupDefaultCamera();
    void UpdateViewport(int w, int h) {
        viewW_ = w;
        viewH_ = h;
    }
    void alphaEnable(bool on);
    void setSamplerTileMode(int kind, int unused, int stage);
    void GetCurrentRtSize(int *w, int *h) const;
    int currentRenderTarget() const {
        return currentRT_;
    }
    void ClearScreen(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    void HandleZBufFlags(uint32_t flags);
    int UpdateBufferFromMem(int vb, const void *data, int count);
    void SetAlphaBlend(unsigned src, unsigned dst);

    int CreateVertexBufferWO(int kind, int count, int usage);
    int DestroyVertexuffer(int vb, int release);

    int FindFreeRT(int w, int h);
    int GetRTTexture(int w, int h) {
        return FindFreeRT(w, h);
    }
    int ReleaseRTTexture(int rt);
    int SetCurrenRT(int rt);
    int FillSurface(int rt, uint32_t colour);

    int RenderBuffer(int vb, int primitive, const matrix4f &world, int count, NeonBaseRender *obj);

    void BeginFrame();
    void PresentFrame();
    void Destroy(bool full);

    // State the render objects write before RenderBuffer (set by NeonBaseRender::Render).
    struct DrawState {
        const float *constants = nullptr; // c0..c31 as vec4s
        const bool *constEnabled = nullptr;
        int renderTarget = -1;
        int srcRT[3] = {0, 0, 0};
        int srcLiteral[3] = {0, 0, 0};
        int srcFont[3] = {0, 0, 0};
        int srcOther[3] = {0, 0, 0};
        int program = 0;
        int programAux = 0;
        float texMat[3][2][4] = {}; // per stage: column 0 and column 1
    } draw;

    uint32_t deviceFlags = DF_ALPHA_BLEND;
    matrix4f view; // camera
    matrix4f projection;

  private:
    static std::vector<NeonDevice *> &devices();
    bool checkpointGraphics();
    void abandonGraphics();
    bool restoreGraphics();
    bool restoring_ = false;
    int restoreUnit0_ = -1;
    std::array<bool, kMaxVertexBuffers> restoreBuffers_{};
    unsigned restoreWrapS_ = 0x2901, restoreWrapT_ = 0x2901;
    std::vector<std::string> literalFiles_;
    std::array<std::vector<uint8_t>, kMaxRenderTargets> targetPixels_;
    std::array<std::vector<uint8_t>, kMaxLiterals> literalPixels_;
    std::array<std::array<float, 16>, kNumPrograms * kNumUniforms> uniformValues_{};
    std::array<bool, kNumPrograms * kNumUniforms> uniformSaved_{};
    void SetD3DState();
    void applyFrameState();
    bool createFBO(RenderTarget &rt, int w, int h);
    unsigned compile(unsigned type, const char *src);
    void bindSource(int stage, unsigned tex, unsigned wrapS, unsigned wrapT);
    int px(float v, float s) const;

    neon::RenderHost *host_ = nullptr;
    bool nearestTextures_ = false;

    // backbuffer (host surface)
    unsigned bbFbo_ = 0;
    int bbW_ = 0, bbH_ = 0; // logical
    int bbPixelW_ = 0, bbPixelH_ = 0;
    float scaleX_ = 1, scaleY_ = 1;

    RenderTarget rts_[kMaxRenderTargets];
    VertexBufferSlot vbs_[kMaxVertexBuffers];
    int vbCount_ = 0;
    LiteralTexture literals_[kMaxLiterals];

    unsigned vertexShaders_[2] = {0, 0};
    unsigned pixelShaders_[6] = {0, 0, 0, 0, 0, 0};
    ShaderProgram programs_[kNumPrograms];

    int viewW_ = 0, viewH_ = 0;
    float zNear_ = 100.0f, zFar_ = 4100.0f;
    int projFlags_ = 1;
    int orientation_ = 0; // 1 = rotate 90 degrees (iOS portrait layer)
    bool upsideDown_ = false;

    unsigned wrapS_[3], wrapT_[3];
    unsigned appliedWrapS_[3], appliedWrapT_[3];
    int boundRT_[3], boundLiteral_[3]; // last texture source per stage (cache)

    int currentProgram_ = -99;
    int currentRT_ = -1;
    unsigned unit0Texture_ = 0; // what the engine last bound (restored each frame)
    unsigned blendSrc_ = 0x0302, blendDst_ = 0x0303; // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    bool fontWarned_ = false;
    int lastVB_ = -1, lastKind_ = -100;
    uint32_t zbufCache_ = 0xffffffff;
    bool frameBegun_ = false;
    bool stateDone_ = false;
};

// A render "object": a vertex buffer plus everything needed to draw it.
// Fields are public because several modules write them directly.
class NeonBaseRender {
  public:
    NeonBaseRender();
    ~NeonBaseRender();

    int Init(NeonDevice *dev) {
        device = dev;
        return 0;
    }
    int SetCurrenRenderTarget(int rt) {
        renderTarget = rt;
        return 0;
    }
    int SetSourceFromRTs(int a, int b, int c);
    int SetSourceFromRTs(int a, int b, int c, int la, int lb, int lc);
    int SetSourceFromLiterals(int a, int b, int c);
    int SetSourceFromLiterals(int a, int b, int c, int fa, int fb, int fc);
    int SetSourceFromFont(int a, int b, int c);
    int SetVertexBufferWO(int vb);
    int GetCurrentBufNum() const {
        return vertexBuffer;
    }
    int ReleaseVertexBuffer(int vb);
    int ReleaseRTTexture(int rt);
    int ClearCurrentRT(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    int GetRTTexture(int w, int h);
    int UpdateBufferFromMem(const void *data, int count);
    // kind: 0 standard vertices; separate: create an extra buffer and return its index
    int GetVertexBufferWO(int kind, int count, int usage, bool separate);
    int Render(int count);

    int vertexBuffer = -1;
    int renderTarget = -1;
    int srcRT[3] = {-1, -1, -1};
    int srcLiteral[3] = {-1, -1, -1};
    int srcFont[3] = {-1, -1, -1};
    int srcOther[3] = {-1, -1, -1};
    NeonDevice *device = nullptr;
    matrix4f world;
    matrix4f matrixB, matrixC; // unused by the renderer itself
    matrix4f texMatrix[3];
    float constants[128]; // c0..c31
    float constants2[80];
    bool constEnabled[32];
    bool constEnabled2[20];
    int shader = -1;
    int shaderAux = -1;
    int clipLeft = -1, clipRight = -1, clipTop = -1, clipBottom = -1; // clipTop < 0: no scissor
    int initialised = 0;
    int primitive = 0;
    int unknown550 = -1;
};

} // namespace render
} // namespace neon
