#include "effect_runtime.h"
#include "pc_flower.h"
#include "primitive_mesh.h"
#include "recovered_scene.h"
#include "shape_component.h"
#include "title_flight.h"
#include <glad/gl.h>
#include <set>
#include <sstream>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <stb_image.h>
#include <stdexcept>
#include <string>
namespace neon::effects {
namespace {
using namespace pc;
using ClearDepthFunction = void(GLAD_API_PTR *)(GLfloat);
ClearDepthFunction clearDepth = nullptr;
GLuint compile(GLenum type, const std::string &text) {
    GLuint s = glCreateShader(type);
    auto *p = text.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char error[4096]{};
        glGetShaderInfoLog(s, sizeof(error), nullptr, error);
        glDeleteShader(s);
        throw std::runtime_error(error);
    }
    return s;
}
GLuint program(const std::string &vs, const std::string &ps) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = 0, p = 0;
    try {
        f = compile(GL_FRAGMENT_SHADER, ps);
        p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        GLint ok;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char e[4096]{};
            glGetProgramInfoLog(p, sizeof(e), nullptr, e);
            throw std::runtime_error(e);
        }
    } catch (...) {
        glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        if (p)
            glDeleteProgram(p);
        throw;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}
struct State {
    GLint unpackBuffer = 0, unpack[6]{};
    GLboolean stencil = 0, discard = 0, alphaCoverage = 0, sampleCoverage = 0;
    static constexpr GLenum unpackNames[6] = {GL_UNPACK_ALIGNMENT,    GL_UNPACK_ROW_LENGTH,
                                              GL_UNPACK_SKIP_ROWS,    GL_UNPACK_SKIP_PIXELS,
                                              GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES};
    void captureUploadState() {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        for (int i = 0; i < 6; i++)
            glGetIntegerv(unpackNames[i], &unpack[i]);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        discard = glIsEnabled(GL_RASTERIZER_DISCARD);
        alphaCoverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
        sampleCoverage = glIsEnabled(GL_SAMPLE_COVERAGE);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        for (int i = 0; i < 6; i++)
            glPixelStorei(unpackNames[i], i == 0 ? 4 : 0);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_RASTERIZER_DISCARD);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_SAMPLE_COVERAGE);
    }
    void restoreUploadState() {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        for (int i = 0; i < 6; i++)
            glPixelStorei(unpackNames[i], unpack[i]);
        auto set = [](GLenum key, bool on) {
            if (on)
                glEnable(key);
            else
                glDisable(key);
        };
        set(GL_STENCIL_TEST, stencil);
        set(GL_RASTERIZER_DISCARD, discard);
        set(GL_SAMPLE_ALPHA_TO_COVERAGE, alphaCoverage);
        set(GL_SAMPLE_COVERAGE, sampleCoverage);
    }
    GLint depthFunction = GL_LESS, renderbuffer = 0;
    GLboolean depthWrite = 1;
    GLint program, draw, read, vao, vbo, active, view[4], scissor[4], src, dst, srcA, dstA, eq, eqA,
        tex[3]{}, sampler[3]{};
    GLboolean blend, depth, cull, clip, mask[4], dither;
    GLfloat clear[4], savedClearDepth = 1;
    State() {
        captureUploadState();
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_VIEWPORT, view);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &src);
        glGetIntegerv(GL_BLEND_DST_RGB, &dst);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        clip = glIsEnabled(GL_SCISSOR_TEST);
        dither = glIsEnabled(GL_DITHER);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &savedClearDepth);
        for (int i = 0; i < 3; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex[i]);
            // GLES exposes sampler binding through the active texture unit, not an indexed query.
            glGetIntegerv(GL_SAMPLER_BINDING, &sampler[i]);
        }
    }
    ~State() {
        restoreUploadState();
        glDepthFunc(depthFunction);
        glDepthMask(depthWrite);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glUseProgram(program);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glViewport(view[0], view[1], view[2], view[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        glBlendFuncSeparate(src, dst, srcA, dstA);
        glBlendEquationSeparate(eq, eqA);
        auto restore = [](GLenum c, bool on) {
            if (on)
                glEnable(c);
            else
                glDisable(c);
        };
        restore(GL_BLEND, blend);
        restore(GL_DEPTH_TEST, depth);
        restore(GL_CULL_FACE, cull);
        restore(GL_SCISSOR_TEST, clip);
        restore(GL_DITHER, dither);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        clearDepth(savedClearDepth);
        for (int i = 0; i < 3; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, tex[i]);
            glBindSampler(i, sampler[i]);
        }
        glActiveTexture(active);
    }
};
struct Target {
    GLuint texture = 0, fbo = 0;
    int width = 0, height = 0;
    void release() {
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
        if (texture)
            glDeleteTextures(1, &texture);
        texture = fbo = 0;
        width = height = 0;
    }
    void create(int size, int glyphHeight = 0) {
        if (!glyphHeight)
            glyphHeight = size;
        this->width = size;
        this->height = glyphHeight;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, glyphHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("GLES RGBA8 target incomplete");
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
};
struct Vertex {
    float x, y, z, r, g, b, a, u, v, u1, v1;
};
Matrix4 rotation(int axis, float angle) {
    auto m = identityMatrix();
    float c = std::cos(angle), s = std::sin(angle);
    if (axis == 0) {
        m[5] = c;
        m[6] = s;
        m[9] = -s;
        m[10] = c;
    } else if (axis == 1) {
        m[0] = c;
        m[2] = -s;
        m[8] = s;
        m[10] = c;
    } else {
        m[0] = c;
        m[1] = s;
        m[4] = -s;
        m[5] = c;
    }
    return m;
}
const char *flatVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 position;layout(location=1) in vec4 tint;layout(location=2) in vec2 uv0;layout(location=3) in vec2 uv1;
uniform mat4 transform;uniform int spatial;uniform float outputSign;uniform vec2 pixelShift;
out vec4 diffuse;out vec2 t0;out vec2 t1;
void main(){vec4 p=spatial!=0?transform*vec4(position,1.0):vec4(position.xy*2.0-1.0,0.0,1.0);if(spatial!=0)p.z=2.0*p.z-p.w;p.y*=outputSign;p.xy+=pixelShift*p.w;gl_Position=p;diffuse=tint;t0=uv0;t1=uv1;})";
const char *fragmentHeader = R"(#version 300 es
precision highp float;
uniform sampler2D image0;uniform sampler2D image1;uniform sampler2D image2;uniform vec4 c25;uniform vec4 c26;uniform float gain;
in vec4 diffuse;in vec2 t0;in vec2 t1;out vec4 colour;
)";
std::string builtInFragment(int id) {
    std::string body;
    switch (id) {
    case 1000:
        body = "vec4 s=texture(image0,t0);float "
               "l=max(s.r,max(s.g,s.b));colour=vec4(mix(s.rgb,vec3(l),c25.x)*"
               "diffuse.rgb*s.a*"
               "diffuse.a,s.a*diffuse.a);";
        break;
    case 1001:
        body = "colour=diffuse;";
        break;
    case 1002:
        body = "vec3 bloom=vec3(0);for(int j=0;j<8;j++){float "
               "a=float(j)*0.78539816;vec2 "
               "d=vec2(cos(a),sin(a))*c25.xy;bloom+=texture(image0,t0+d).rgb*.075;"
               "bloom+=texture("
               "image0,t0+d*3.0).rgb*.045;}colour=vec4(texture(image0,t0).rgb+"
               "bloom*c25.z,1.0);";
        break;
    case 26:
        body = "colour=texture(image0,t0)*diffuse.a*gain;";
        break;
    default:
        throw std::runtime_error("GLES shader not ported: " + std::to_string(id));
    }
    return std::string(fragmentHeader) + "void main(){" + body + "}";
}
} // namespace
struct Runtime::Impl {
    AssetReader reader;
    std::filesystem::path data;
    std::string read(const std::filesystem::path &p) const {
        return assetText(p, reader);
    }
    RecoveredScene scene;
    std::array<bool, 16> baseReachable{};
    bool allocatedInspection = false;
    TunnelProfiles profiles;
    bool editorAuthoring = false;
    AuthoringModel author;
    std::vector<ShapeSettings> settings;
    ComponentScene components;
    std::map<int, ReadBindings> readBanks;
    ReadReport readReport;
    ComponentScene responseComponents;
    bool useResponseComponents = false;
    std::map<int, ComponentScene> componentDocuments;
    std::array<uint32_t, 256> shapePalette{}, spectrumPalette{};
    struct ComponentTarget {
        Target target;
        uint64_t key = ~uint64_t(0);
        vectors::BeamMesh beam;
        CubeFace shapeVertices;
        uint64_t shapeKey = ~uint64_t(0);
        std::unique_ptr<Runtime> layer;
        std::string layerDocument;
        uint64_t layerHash = 0;
        double layerTime = 0;
    };
    std::map<int, ComponentTarget> componentTargets;
    std::map<int, TunnelPlayback> componentTunnels;
    unsigned baseTargets = 0;
    size_t baseBytes = 0;
    Parameters params;
    OverlayTextLayer overlayText;
    std::unique_ptr<vectors::BeamMesh> overlayOverGameMesh, overlayBezelMesh;
    GLuint overlayProgram = 0;
    struct OverlayCache {
        GLuint vao = 0, vbo = 0;
        size_t vertices = 0;
        uint64_t key = ~uint64_t(0);
    };
    OverlayCache overlayOverGameCache, overlayBezelCache;
    Statistics stats;
    std::vector<vectors::Object> objects;
    uint64_t objectRevision = 0;
    vectors::Canvas canvas;
    vectors::BeamMesh beam;
    std::map<int, GLuint> programs, images;
    std::map<GLuint, std::array<GLint, 31>> uniformLocations;
    enum Uniform {
        U_image0,
        U_image1,
        U_image2,
        U_gain,
        U_materialGain,
        U_pixelShift,
        U_c25,
        U_c26,
        U_viewportSize,
        U_beamStyle,
        U_spatial,
        U_outputSign,
        U_transform,
        U_uvScale0,
        U_uvScale1,
        U_uvScale2,
        U_map0,
        U_map1,
        U_map2,
        U_tone0,
        U_tone1,
        U_tone2,
        U_angle0,
        U_angle1,
        U_angle2,
        U_brightness0,
        U_brightness1,
        U_brightness2,
        U_outTone,
        U_outGain,
        U_unused
    };
    GLint location(GLuint p, Uniform u) {
        auto it = uniformLocations.find(p);
        if (it == uniformLocations.end()) {
            std::array<GLint, 31> values;
            const char *names[] = {"image0",
                                   "image1",
                                   "image2",
                                   "gain",
                                   "materialGain",
                                   "pixelShift",
                                   "c25",
                                   "c26",
                                   "viewportSize",
                                   "beamStyle",
                                   "spatial",
                                   "outputSign",
                                   "transform",
                                   "neonUv0",
                                   "neonUv1",
                                   "neonUv2",
                                   "neonMap0",
                                   "neonMap1",
                                   "neonMap2",
                                   "neonTone0",
                                   "neonTone1",
                                   "neonTone2",
                                   "neonAngle0",
                                   "neonAngle1",
                                   "neonAngle2",
                                   "neonBrightness0",
                                   "neonBrightness1",
                                   "neonBrightness2",
                                   "neonOutput",
                                   "neonOutputGain",
                                   "unused"};
            for (int n = 0; n < 31; n++)
                values[n] = glGetUniformLocation(p, names[n]);
            it = uniformLocations.emplace(p, values).first;
        }
        return it->second[u];
    }
    ScreenLayout screen{};
    Framing allocatedFraming = Framing::LegacyStretch;
    bool producerPass = false, sourcePass = false;
    const Component *materialComponent = nullptr;
    struct MaterialScope {
        const Component *&slot;
        const Component *prior;
        bool &source;
        bool oldSource;
        MaterialScope(const Component *&s, const Component *c, bool &flag, bool f)
            : slot(s), prior(s), source(flag), oldSource(flag) {
            slot = c;
            source = f;
        }
        ~MaterialScope() {
            slot = prior;
            source = oldSource;
        }
    };
    Target scratch, source, studyOutput, geometryFeed;
    std::array<Target, 16> stacks;
    GLuint depthBuffer = 0;
    GLuint beamProgram = 0, vao = 0, vbo = 0, beamVao = 0, beamVbo = 0;
    GeometryStudy geometry;
    std::array<float, 3> fit{};
    std::array<Point, 3> centers{};
    Sequencer studyClock;
    Values studyMixerValues{}, studyOutputValues{};
    int selected = 17;
    unsigned sceneGeneration = 0, graphicsGeneration = ~0u;
    int extent = 0, sourceWidth = 0, sourceHeight = 0;
    double accumulator = 0;
    bool clearPending = false;
    uint64_t renderedKey = ~uint64_t(0);
    explicit Impl(const std::filesystem::path &p, AssetReader read)
        : reader(std::move(read)), data(p), scene(p, "r2-c13", true, reader),
          profiles(TunnelProfiles::load(p / "reference/tunnel", reader)), author(p, reader) {
        baseReachable = scene.reachable;
        auto shapeColours = assetStream(p / "reference/shape-palette.bin", reader);
        if (!shapeColours.read((char *)shapePalette.data(), sizeof shapePalette))
            throw std::runtime_error("Missing original shape palette");
        for (int index = 0; index < 256; index++) {
            float hue = float(index) * 6 / 256, part = hue - std::floor(hue), a = 1 - part;
            float red = 0, green = 0, blue = 0;
            switch (int(hue)) {
            case 0:
                red = 1;
                green = part;
                break;
            case 1:
                red = a;
                green = 1;
                break;
            case 2:
                green = 1;
                blue = part;
                break;
            case 3:
                green = a;
                blue = 1;
                break;
            case 4:
                red = part;
                blue = 1;
                break;
            default:
                red = 1;
                blue = a;
                break;
            }
            spectrumPalette[index] = (uint32_t(red * 255) << 24) | (uint32_t(green * 255) << 16) |
                                     (uint32_t(blue * 255) << 8) | 255;
        }
        componentDocuments.emplace(18, components);
        componentDocuments.emplace(21, ComponentScene::shapeStudy());
        author.bindPlayback(17, &scene.preset);
        std::array<ShapeSettings, 3> originals;
        for (int n = 0; n < 3; n++)
            originals[n] = author.synth.nodes[n + 1].saved;
        geometry.reset(originals);
    }
    ~Impl() {
        release();
    }
    void release(bool keepShared = false) {
        if (!keepShared) {
            for (auto &p : componentTargets)
                p.second.target.release();
            componentTargets.clear();
        }
        if (depthBuffer)
            glDeleteRenderbuffers(1, &depthBuffer);
        depthBuffer = 0;
        studyOutput.release();
        geometryFeed.release();
        scratch.release();
        source.release();
        for (auto &t : stacks)
            t.release();
        if (!keepShared)
            for (auto &p : programs)
                glDeleteProgram(p.second);
        if (!keepShared)
            for (auto &p : images)
                glDeleteTextures(1, &p.second);
        if (!keepShared) {
            programs.clear();
            images.clear();
        }
        uniformLocations.clear();
        if (beamProgram)
            glDeleteProgram(beamProgram);
        if (overlayProgram)
            glDeleteProgram(overlayProgram);
        overlayProgram = 0;
        overlayOverGameMesh.reset();
        overlayBezelMesh.reset();
        for (auto c : {&overlayOverGameCache, &overlayBezelCache}) {
            if (c->vao)
                glDeleteVertexArrays(1, &c->vao);
            if (c->vbo)
                glDeleteBuffers(1, &c->vbo);
            *c = {};
        }
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (beamVao)
            glDeleteVertexArrays(1, &beamVao);
        if (vbo)
            glDeleteBuffers(1, &vbo);
        if (beamVbo)
            glDeleteBuffers(1, &beamVbo);
        beamProgram = vao = vbo = beamVao = beamVbo = 0;
        extent = 0;
        renderedKey = ~uint64_t(0);
        beam.reset();
    }
    void abandon() {
        for (auto &entry : componentTargets) {
            entry.second.target = {};
            entry.second.key = ~uint64_t(0);
            if (entry.second.layer)
                entry.second.layer->abandonGraphics();
        }
        programs.clear();
        images.clear();
        uniformLocations.clear();
        depthBuffer = beamProgram = overlayProgram = vao = vbo = beamVao = beamVbo = 0;
        studyOutput = {};
        geometryFeed = {};
        scratch = {};
        source = {};
        for (auto &target : stacks)
            target = {};
        overlayOverGameCache = {};
        overlayBezelCache = {};
        extent = 0;
        renderedKey = ~uint64_t(0);
    }
    void initialize(int width, int height) {
        scene.reachable = baseReachable;
        bool inspecting = params.layerPreview >= 0;
        // Isolation selects an existing reachable output; keep its original dependency graph.
        int size = std::max(width, height);
        if (extent == size && sourceWidth == width && sourceHeight == height &&
            graphicsGeneration == sceneGeneration && allocatedFraming == params.framing.mode &&
            allocatedInspection == inspecting)
            return;
        release(true);
        GLint maxSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (size < 1 || size > maxSize)
            throw std::runtime_error("Effect target exceeds GLES capability");
        extent = size;
        graphicsGeneration = sceneGeneration;
        allocatedFraming = params.framing.mode;
        allocatedInspection = inspecting;
        sourceWidth = width;
        sourceHeight = height;
        if (selected >= 100)
            studyOutput.create(width, height);
        else
            scratch.create(params.framing.mode == Framing::LegacyStretch ? size : width,
                           params.framing.mode == Framing::LegacyStretch ? size : height);
        source.create(width, height);
        if (selected == 104)
            geometryFeed.create(width, height);
        stats.targets = 0;
        stats.targetBytes = 0;
        for (int i = 0; i < 16; i++)
            if (scene.reachable[i]) {
                stacks[i].create(scratch.width, scratch.height);
            }
        auto account = [&](const Target &t) {
            if (t.texture) {
                ++stats.targets;
                stats.targetBytes += std::size_t(t.width) * t.height * 4;
            }
        };
        account(scratch);
        account(source);
        account(studyOutput);
        account(geometryFeed);
        for (auto &t : stacks)
            account(t);
        if (selected == 1 || selected == 18 || selected == 21) {
            glGenRenderbuffers(1, &depthBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, scratch.width,
                                  scratch.height);
            glBindFramebuffer(GL_FRAMEBUFFER, scratch.fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      depthBuffer);
            stats.targetBytes += std::size_t(scratch.width) * scratch.height * 2;
        }
        baseTargets = stats.targets;
        baseBytes = stats.targetBytes;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        int sizes[] = {3, 4, 2, 2};
        int offsets[] = {0, 12, 28, 36};
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (void *)(intptr_t)offsets[i]);
        }
    }
    void initializeBeam() {
        if (beamProgram)
            return;
        glGenVertexArrays(1, &beamVao);
        glGenBuffers(1, &beamVbo);
        glBindVertexArray(beamVao);
        glBindBuffer(GL_ARRAY_BUFFER, beamVbo);
        int widths[] = {2, 2, 2, 4}, positions[] = {0, 8, 16, 24};
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, widths[i], GL_FLOAT, GL_FALSE, sizeof(vectors::Vertex),
                                  (void *)(intptr_t)positions[i]);
        }
        auto vs = read(data / "vectors/vector-beam.vert.glsl");
        auto pos = vs.find("vec2(2.0,-2.0)");
        if (pos == std::string::npos)
            throw std::runtime_error("Unsupported beam shader vertex convention");
        vs.replace(pos, 14, "vec2(2.0,2.0)");
        pos = vs.find("vec2(-1.0,1.0)");
        vs.replace(pos, 14, "vec2(-1.0,-1.0)");
        beamProgram = program(vs, read(data / "vectors/vector-beam.frag.glsl"));
    }
    GLuint shader(int id) {
        auto &p = programs[id];
        if (!p) {
            auto fragment =
                (id != 26 && id != 1000 &&
                 assetExists(data / "gles/shaders" / (std::to_string(id) + ".frag"), reader))
                    ? read(data / "gles/shaders" / (std::to_string(id) + ".frag"))
                    : builtInFragment(id);
            std::string helpers = "uniform vec2 neonUv0,neonUv1,neonUv2;\n";
            for (int i = 0; i < 3; ++i) {
                auto n = std::to_string(i);
                fragment = std::regex_replace(fragment,
                                              std::regex("texture\\s*\\(\\s*image" + n + "\\s*,"),
                                              "neonSample" + n + "(");
                fragment = std::regex_replace(fragment,
                                              std::regex("sampleLod\\s*\\(\\s*image" + n + "\\s*,"),
                                              "neonLod" + n + "(");
                helpers += "uniform vec4 neonMap" + n + ",neonTone" + n +
                           ";uniform vec2 neonAngle" + n + ";uniform float neonBrightness" + n +
                           ";\n";
                helpers += "vec2 neonCoord" + n + "(vec2 uv){vec2 p=(uv-.5)*neonMap" + n +
                           ".xy;vec2 a=neonAngle" + n + ";p=mat2(a.x,a.y,-a.y,a.x)*p+.5+neonMap" +
                           n + ".zw;return (p-.5)*neonUv" + n + "+.5;}\n";
                helpers += "vec4 neonSample" + n + "(vec2 uv){vec4 t=neonTone" + n +
                           ";return texture(image" + n + ",neonCoord" + n +
                           "(uv))*vec4(t.rgb*neonBrightness" + n + "*t.a,t.a); }\n";
                helpers += "vec4 neonLod" + n + "(vec4 uv){vec4 t=neonTone" + n +
                           ";return textureLod(image" + n + ",neonCoord" + n +
                           "(uv.xy),uv.w)*vec4(t.rgb*neonBrightness" + n + "*t.a,t.a); }\n";
            }
            auto at = fragment.find("void main");
            auto fn = fragment.find("vec4 sampleLod");
            if (fn != std::string::npos)
                at = fn;
            fragment.insert(at, helpers);
            fragment = std::regex_replace(fragment, std::regex("void main\\s*\\("),
                                          "void neonOriginalMain(");
            fragment += "\nuniform vec4 neonOutput;uniform float neonOutputGain;void "
                        "main(){neonOriginalMain();colour*=vec4(neonOutput.rgb*neonOutputGain*"
                        "neonOutput.a,neonOutput.a); }\n";
            p = program(flatVS, fragment);
        }
        glUseProgram(p);
        glUniform1i(location(p, U_image0), 0);
        glUniform1i(location(p, U_image1), 1);
        glUniform1i(location(p, U_image2), 2);
        glUniform1f(location(p, U_gain), 1);
        glUniform1f(location(p, U_materialGain), 1);
        for (auto u : {U_uvScale0, U_uvScale1, U_uvScale2})
            glUniform2f(location(p, u), 1, 1);
        for (int i = 0; i < 3; ++i) {
            glUniform4f(location(p, Uniform(U_map0 + i)), 1, 1, 0, 0);
            glUniform4f(location(p, Uniform(U_tone0 + i)), 1, 1, 1, 1);
            glUniform2f(location(p, Uniform(U_angle0 + i)), 1, 0);
            glUniform1f(location(p, Uniform(U_brightness0 + i)), 1);
        }
        glUniform4f(location(p, U_outTone), 1, 1, 1, 1);
        glUniform1f(location(p, U_outGain), 1);
        // GLES full-screen quads already cover pixel centers. Translating a full
        // target leaves a cleared edge which rotated/feedback layers sample as a seam.
        const float legacyShift = params.framing.mode == Framing::LegacyStretch ? 1.f / 512 : 0;
        glUniform2f(location(p, U_pixelShift), legacyShift, legacyShift);
        return p;
    }
    GLuint texture(int selector) {
        if (selector >= 10000)
            return componentTargets.at(selector - 10000).target.texture;
        if (selector == 201)
            return geometryFeed.texture;
        if (selector == 200)
            return source.texture;
        auto b = originalTextureBinding(selector);
        if (b.kind == TextureSourceKind::Stack) {
            if (!stacks[b.index].texture)
                throw std::runtime_error("Missing GLES layer dependency");
            return stacks[b.index].texture;
        }
        // Legacy studies generate roots on the CPU; recovered scenes use the
        // original game's root images. They share selector numbers, not pixels.
        auto &image =
            images[selector + (selected >= 100 && b.kind == TextureSourceKind::Root ? 1000 : 0)];
        if (image)
            return image;
        int w = 64, h = 64;
        std::vector<unsigned char> pixels;
        if (b.kind == TextureSourceKind::Root) {
            auto root = selected >= 100
                            ? rootTexture(b.index)
                            : loadOriginalRoot(data / "reference/roots", b.index, reader);
            pixels.resize(root.size() * 4);
            for (size_t i = 0; i < root.size(); i++) {
                pixels[i * 4] = uint8_t(root[i] >> 16);
                pixels[i * 4 + 1] = uint8_t(root[i] >> 8);
                pixels[i * 4 + 2] = uint8_t(root[i]);
                pixels[i * 4 + 3] = uint8_t(root[i] >> 24);
            }
        } else if (b.kind == TextureSourceKind::Alternate) {
            auto name = b.index == 100
                            ? std::string("title-flight.png")
                            : (b.index < 10 ? std::string("0") : std::string()) +
                                  std::to_string(b.index) + (b.index >= 31 ? ".jpg" : ".png");
            auto bytes = read(data / "reference/literals" / name);
            int channels;
            auto *rgba = stbi_load_from_memory((const unsigned char *)bytes.data(),
                                               int(bytes.size()), &w, &h, &channels, 4);
            if (!rgba)
                throw std::runtime_error("Cannot decode GLES source image");
            pixels.assign(rgba, rgba + size_t(w) * h * 4);
            stbi_image_free(rgba);
        } else
            throw std::runtime_error("Unported GLES source selector");
        glGenTextures(1, &image);
        glBindTexture(GL_TEXTURE_2D, image);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return image;
    }
    void bind(GLuint program, int shaderId, const ShapeSettings &terminal,
              std::array<int, 3> selectors) {
        for (int i = 0; i < 3; ++i) {
            bool expanded =
                selectors[i] == 200 || selectors[i] == 201 ||
                (selectors[i] >= 10000 &&
                 activeComponents().at(selectors[i] - 10000).kind == ComponentKind::Layer) ||
                (selectors[i] < 10000 &&
                 originalTextureBinding(selectors[i]).kind == TextureSourceKind::Stack);
            bool renderedLayer =
                selectors[i] >= 10000 &&
                activeComponents().at(selectors[i] - 10000).kind == ComponentKind::Layer;
            glUniform2f(location(program, Uniform(U_uvScale0 + i)),
                        expanded ? 1 / screen.extentX : 1,
                        (renderedLayer ? -1.f : 1.f) * (expanded ? 1 / screen.extentY : 1));
        }
        for (int i = 0; i < 3; ++i) {
            auto c = materialComponent;
            int n = Input0ScaleU + i * 10;
            glUniform4f(location(program, Uniform(U_map0 + i)), c ? c->values[n] : 1,
                        c ? c->values[n + 1] : 1, c ? c->values[n + 2] : 0,
                        c ? c->values[n + 3] : 0);
            float angle = c ? c->values[n + 4] : 0;
            glUniform2f(location(program, Uniform(U_angle0 + i)), std::cos(angle), std::sin(angle));
            float r = c ? c->values[n + 5] : 1, g = c ? c->values[n + 6] : 1,
                  b = c ? c->values[n + 7] : 1, gain = c ? c->values[n + 8] : 1,
                  alpha = c ? c->values[n + 9] : 1;
            if (selectors[i] >= 10000) {
                const auto &textureSource = activeComponents().at(selectors[i] - 10000);
                r *= textureSource.values[SourceRed];
                g *= textureSource.values[SourceGreen];
                b *= textureSource.values[SourceBlue];
                gain *= textureSource.values[SourceBrightness];
                alpha *= textureSource.values[SourceOpacity];
            }
            glUniform4f(location(program, Uniform(U_tone0 + i)), r, g, b, alpha);
            glUniform1f(location(program, Uniform(U_brightness0 + i)), gain);
        }
        auto output = materialComponent;
        bool apply = output && !sourcePass && output->kind != ComponentKind::Layer &&
                     output->kind != ComponentKind::Vector;
        glUniform4f(location(program, U_outTone), apply ? output->values[SourceRed] : 1,
                    apply ? output->values[SourceGreen] : 1, apply ? output->values[SourceBlue] : 1,
                    apply ? output->values[SourceOpacity] : 1);
        glUniform1f(location(program, U_outGain), apply ? output->values[SourceBrightness] : 1);
        auto setup = stackRenderSetup(terminal);
        glUniform4fv(location(program, U_c25), 1, setup.constant25.data());
        glUniform4fv(location(program, U_c26), 1, setup.constant26.data());
        // Original 0040a270 initializes MIRROR. TEXCON +b0/+b4 are stored
        // metadata, not sampler writes; 00497610 restores MIRROR after its draw.
        // The explicitly synthetic studies retain their authored REPEAT policy.
        const GLint addressing = selected >= 100 ? GL_REPEAT : GL_MIRRORED_REPEAT;
        for (int i = 0; i < 3; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindSampler(i, 0);
            if ((shaderId >= 1000 ? 1 : scene.masks.at(shaderId)) & (1 << i)) {
                glBindTexture(GL_TEXTURE_2D, texture(selectors[i]));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressing);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressing);
            } else
                glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    void draw(const Vertex *vertices, size_t count, GLenum primitive) {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), vertices, GL_STREAM_DRAW);
        glDrawArrays(primitive, 0, GLsizei(count));
        ++stats.drawCalls;
        stats.triangles += unsigned(primitive == GL_TRIANGLE_STRIP ? count - 2 : count / 3);
        stats.uploadedBytes += count * sizeof(Vertex);
    }
    std::array<Vertex, 4> quad(float sx = 1, float sy = 1) {
        std::array<Vertex, 4> q{};
        for (int k = 0; k < 4; k++) {
            float u = float(k & 1), v = float(k >> 1);
            q[k] = {.5f + (u - .5f) * sx, .5f + (v - .5f) * sy, 0, 1, 1, 1, 1, u, v, u, v};
        }
        return q;
    }
    void unbind() {
        for (int i = 0; i < 3; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    void vectors() {
        initializeBeam();
        canvas.clear();
        if (params.vectorScene == 3) {
            for (auto &o : objects)
                canvas.object(o);
        } else
            vectors::demo(canvas, params.vectorScene, time());
        auto strokes = canvas.strokes();
        for (auto &stroke : strokes)
            for (auto point : {&stroke.a, &stroke.b}) {
                point->x = 320 + (point->x - 320) / screen.extentX;
                point->y = 240 + (point->y - 240) / screen.extentY;
            }
        auto style = params.beam;
        style.width /= screen.extentY;
        auto &vertices = beam.build(strokes, time(), sourceWidth, sourceHeight, style);
        glBindFramebuffer(GL_FRAMEBUFFER, source.fbo);
        glViewport(0, 0, sourceWidth, sourceHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(beamProgram);
        glUniform2f(location(beamProgram, U_viewportSize), float(sourceWidth), float(sourceHeight));
        glUniform4f(location(beamProgram, U_beamStyle), params.beam.intensity, params.beam.halo,
                    params.beam.spread, params.beam.whiteHot);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glColorMask(1, 1, 1, 0);
        glBindVertexArray(beamVao);
        glBindBuffer(GL_ARRAY_BUFFER, beamVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vectors::Vertex), vertices.data(),
                     GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
        ++stats.drawCalls;
        stats.triangles += unsigned(vertices.size() / 3);
        stats.uploadedBytes += vertices.size() * sizeof(vectors::Vertex);
        glColorMask(1, 1, 1, 1);
    }
    float response(const char *key, float baseline, float low, float high) const {
        auto bank = author.responses.find(selected);
        return bank == author.responses.end()
                   ? baseline
                   : bank->second.apply(key, baseline, time(), low, high);
    }
    float exposureValue() const {
        return response("runtime.exposure", params.exposure, 0, 2);
    }
    float yawValue() const {
        return response("runtime.yaw", params.yaw, -3.141593f, 3.141593f);
    }
    float pitchValue() const {
        return response("runtime.pitch", params.pitch, -3.141593f, 3.141593f);
    }
    const ComponentScene &activeComponents() const {
        return useResponseComponents ? responseComponents : components;
    }
    float control(const char *key, float fallback = 0) {
        return selected >= 2 && selected <= 20 && selected != 18
                   ? author.sceneControl(selected, key, fallback)
                   : fallback;
    }
    void evaluateEdits() {
        auto bank = author.responses.find(selected);
        useResponseComponents = (selected == 18 || selected == 21) &&
                                bank != author.responses.end() && !bank->second.sequences.empty();
        if (useResponseComponents) {
            responseComponents = components;
            for (auto &c : responseComponents.items)
                for (const auto &spec : editableComponentFields(c)) {
                    std::string key =
                        "component." + std::to_string(c.id) + "." + std::to_string(int(spec.field));
                    c.values[spec.field] = bank->second.apply(key, c.values[spec.field], time(),
                                                              spec.minimum, spec.maximum);
                }
        }
        settings.clear();
        for (auto &n : scene.preset.nodes)
            settings.push_back(n.effective);
        if (selected >= 7 && selected <= 20 && selected != 18)
            settings = author.authoredScene(selected);
        else if (selected >= 5 && selected <= 6)
            author.applyScene(selected, settings[selected == 5 ? 18 : 17], settings[10],
                              settings[11]);
        else if (selected >= 2 && selected <= 4) {
            int a = selected == 2   ? 13
                    : selected == 3 ? 10
                                    : 7,
                b = selected == 2   ? 16
                    : selected == 3 ? 17
                                    : 14;
            author.apply(selected, settings[a], settings[a + 1], settings[b], settings[b + 1]);
        }
        if (editorAuthoring && selected >= 7 && selected <= 20 && selected != 18) {
            params.titleSize = control("title.size", 1.35f);
            params.titlePeriod = control("title.period", 18);
            params.titleHue = control("title.hue");
            params.titleFilter = control("title.filter", .8f);
            params.titleOpacity = control("title.opacity", 1);
            params.titleSprite = int(control("title.sprite"));
            params.vectorScene = int(control("vector.scene"));
            params.beam.width = control("vector.width", 2);
            params.beam.intensity = control("vector.intensity", 1.5f);
            params.beam.halo = control("vector.halo", .38f);
            params.beam.spread = control("vector.spread", 4);
            params.beam.persistenceMs = control("vector.persistence", 35);
            params.beam.colour = control("vector.colour");
            params.beam.whiteHot = control("vector.hot", .2f);
        }
    }
    ShapeSettings componentTunnelSettings(const Component &c) {
        ShapeSettings n{};
        for (auto &node : author.extraScenes[2].preset.nodes)
            if (node.type == 15) {
                n = node.saved;
                break;
            }
        n.integers[7] = c.profile;
        n.integers[10] = c.profileB;
        n.integers[8] = int(std::lround(c.values[RepeatX]));
        n.integers[9] = int(std::lround(c.values[RepeatY]));
        n.integers[2] =
            int(std::lround((n.integers[2] > 0 ? n.integers[2] : 2097152) * c.values[Speed]));
        n.integers[13] = int(c.values[Opacity] * 255);
        n.integers[14] = int(c.values[Colour] * 255);
        n.floats[2] = c.values[Morph];
        n.floats[4] = c.values[DeformX];
        n.floats[5] = c.values[DeformY];
        n.floats[6] = c.values[DeformZ];
        n.floats[12] = c.values[Roll];
        return n;
    }
    uint64_t layerFingerprint(const Component &c) const {
        auto found = componentTargets.find(c.id);
        if (found != componentTargets.end() && found->second.layerDocument == c.layerScene)
            return found->second.layerHash;
        return std::hash<std::string>{}(c.layerScene);
    }
    uint64_t componentTextureKey(const Component &c) {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](float f) {
            uint32_t bits;
            std::memcpy(&bits, &f, 4);
            h = (h ^ bits) * 1099511628211ull;
        };
        for (int n = Input0ScaleU; n < FieldCount; ++n)
            mix(c.values[n]);
        if (!c.vectorDocument.empty())
            h ^= std::hash<std::string>{}(c.vectorDocument);
        mix(float(c.enabled));
        mix(float(c.resolution));
        mix(float(c.kind));
        if (!c.enabled)
            return h;
        if (c.kind == ComponentKind::Layer) {
            h ^= layerFingerprint(c);
            mix(float(c.layerOutput));
            mix(float(c.layerComponent));
            mix(c.values[Speed]);
        }
        if (c.kind == ComponentKind::Texture) {
            mix(float(c.shader));
            for (auto spec : componentFields(c.kind))
                mix(c.values[spec.field]);
            std::array<int, 3> inputs = {c.source, c.sourceB ? c.sourceB : c.source,
                                         c.sourceC ? c.sourceC : c.source};
            for (int i = 0; i < 3; i++)
                if (componentSamplerMask(c.shader) & (1 << i)) {
                    int input = inputs[i];
                    mix(float(input));
                    if (input >= 10000)
                        h = (h ^ componentTargets.at(input - 10000).key) * 1099511628211ull;
                }
        } else if (c.kind == ComponentKind::Shape) {
            mix(float(c.shader));
            mix(float(c.profile));
            mix(float(c.profileB));
            mix(c.values[Gain]);
            for (int field = ShapeOrderA; field < Input0ScaleU; field++)
                mix(c.values[field]);
            for (int k = 0; k < 8; k++)
                if (componentConstantMask(c.shader) & (1u << k))
                    mix(c.values[C0 + k]);
            std::array<int, 3> inputs = {c.source, c.sourceB ? c.sourceB : c.source,
                                         c.sourceC ? c.sourceC : c.source};
            for (int k = 0; k < 3; k++)
                if (componentSamplerMask(c.shader) & (1 << k)) {
                    mix(float(inputs[k]));
                    if (inputs[k] >= 10000)
                        h = (h ^ componentTargets.at(inputs[k] - 10000).key) * 1099511628211ull;
                }
        } else {
            mix(c.values[BeamWidth]);
            mix(c.values[BeamGlow]);
            mix(params.beam.intensity);
            mix(params.beam.spread);
            mix(params.beam.whiteHot);
            mix(float(c.profile));
            h ^= objectRevision;
            for (unsigned char ch : c.vectorDocument)
                h = (h ^ ch) * 1099511628211ull;
        }
        if (c.animated && c.values[Speed] != 0) {
            mix(float(std::floor(time() * c.updateHz) / c.updateHz));
            mix(c.values[Speed]);
        }
        return h;
    }
    const CubeFace &componentShape(const Component &c, double seconds) {
        auto renderSettings = shapeComponentSettings(c, seconds);
        uint64_t key = 1469598103934665603ull;
        auto mix = [&](const void *data, size_t size) {
            auto p = (const unsigned char *)data;
            for (size_t n = 0; n < size; n++)
                key = (key ^ p[n]) * 1099511628211ull;
        };
        mix(renderSettings.integers.data(), 64);
        mix(renderSettings.floats.data(), 64);
        mix(&c.values[ShapeGateDrive], sizeof(float));
        mix(&c.values[ShapePaletteMode], sizeof(float));
        auto &cache = componentTargets[c.id];
        if (cache.shapeKey != key) {
            auto mesh = generatedShapeMesh(
                renderSettings, c.values[ShapePaletteMode] >= .5f ? spectrumPalette : shapePalette,
                c.values[ShapeGateDrive]);
            CubeFace vertices;
            vertices.reserve(mesh.size());
            size_t corner = 0;
            for (auto &v : mesh) {
                float u = corner % 3 == 2 ? 1.f : 0.f, vv = corner % 3 == 0 ? 0.f : 1.f;
                vertices.push_back(
                    {v.colour, u, vv, u, vv, v.position.x, v.position.y, v.position.z});
                ++corner;
            }
            cache.shapeVertices = std::move(vertices);
            cache.shapeKey = key;
            ++stats.shapeBuilds;
            ++stats.totalShapeBuilds;
        }
        return cache.shapeVertices;
    }
    void componentSource(const Component &c) {
        MaterialScope material(materialComponent, &c, sourcePass, true);
        auto &cache = componentTargets[c.id];
        if (c.kind != ComponentKind::Layer && cache.layer) {
            cache.layer.reset();
            cache.layerDocument.clear();
            cache.layerHash = 0;
        }
        auto revision = componentTextureKey(c);
        int targetWidth = c.resolution, targetHeight = c.resolution;
        if (c.kind == ComponentKind::Layer) {
            float factor = float(c.resolution) / std::max(sourceWidth, sourceHeight);
            targetWidth = std::max(1, int(std::lround(sourceWidth * factor)));
            targetHeight = std::max(1, int(std::lround(sourceHeight * factor)));
        }
        if (cache.target.width != targetWidth || cache.target.height != targetHeight) {
            cache.target.release();
            cache.target.create(targetWidth, targetHeight);
            cache.key = ~uint64_t(0);
            cache.beam.reset();
        }
        if (cache.key == revision) {
            if (c.kind == ComponentKind::Layer && c.animated)
                cache.layerTime = std::floor(time() * c.updateHz) / c.updateHz;
            return;
        }
        if (c.kind == ComponentKind::Layer) {
            if (!cache.layer || cache.layerDocument != c.layerScene) {
                auto pending = std::make_unique<Runtime>(data, reader);
                std::istringstream input(c.layerScene);
                pending->readScene(input);
                pending->parameters().layerPreview = c.layerOutput;
                pending->parameters().componentPreviewId = std::max(0, c.layerComponent);
                pending->parameters().renderScale = 1;
                cache.layer = std::move(pending);
                cache.layerDocument = c.layerScene;
                cache.layerHash = std::hash<std::string>{}(c.layerScene);
                cache.layerTime = 0;
            }
            auto &child = *cache.layer;
            child.parameters().framing = params.framing;
            double seconds = c.animated ? std::floor(time() * c.updateHz) / c.updateHz : 0;
            if (seconds < cache.layerTime) {
                child.reset();
                cache.layerTime = 0;
            }
            Destination output{
                cache.target.fbo, targetWidth, targetHeight, {0, 0, targetWidth, targetHeight}, {}};
            glBindFramebuffer(GL_FRAMEBUFFER, cache.target.fbo);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(1, 1, 1, 1);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            child.frame((seconds - cache.layerTime) * c.values[Speed], output);
            if (child.time() == 0)
                child.frame(1. / 60, output);
            stats.drawCalls += child.statistics().drawCalls;
            stats.triangles += child.statistics().triangles;
            stats.uploadedBytes += child.statistics().uploadedBytes;
            cache.layerTime = seconds;
            cache.key = revision;
            ++stats.textureBuilds;
            ++stats.totalTextureBuilds;
            return;
        }
        unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, cache.target.fbo);
        glViewport(0, 0, c.resolution, c.resolution);
        glColorMask(1, 1, 1, 1);
        glClearColor(0, 0, 0, c.kind == ComponentKind::Texture && c.enabled ? 1.f : 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        double seconds = c.animated ? std::floor(time() * c.updateHz) / c.updateHz : 0;
        if (c.enabled && c.kind == ComponentKind::Shape) {
            ShapeSettings terminal{};
            for (int k = 0; k < 8; k++)
                terminal.floats[k < 4 ? 4 + k : 5 + k] = c.values[C0 + k];
            auto transform = identityMatrix();
            transform[0] = transform[5] = 1.f / 400;
            transform[10] = 0;
            transform[14] = .5f;
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            drawSpatial(
                componentShape(c, seconds), transform, c.shader, terminal,
                {c.source, c.sourceB ? c.sourceB : c.source, c.sourceC ? c.sourceC : c.source},
                c.values[Gain], c.resolution);
        }
        if (c.enabled && c.kind == ComponentKind::Texture) {
            ShapeSettings n{}, terminal{};
            n.integers[2] = c.shader;
            n.integers[1] = 255;
            n.integers[4] = int(c.values[Colour] * 255);
            n.integers[5] = c.source;
            n.integers[3] = c.sourceB ? c.sourceB : c.source;
            n.integers[6] = c.sourceC ? c.sourceC : c.source;
            n.floats[0] = c.values[RepeatX] - 1;
            n.floats[1] = c.values[RepeatY] - 1;
            n.floats[4] = c.values[Roll];
            n.floats[5] = n.floats[0];
            n.floats[6] = n.floats[1];
            n.floats[9] = n.floats[4];
            for (int i = 0; i < 8; i++)
                terminal.floats[i < 4 ? i + 4 : i + 5] = c.values[C0 + i];
            float phase = c.values[Phase] + float(seconds) * c.values[Speed];
            if (componentConstantMask(c.shader) & 2) {
                terminal.floats[5] = c.values[C1] + phase;
                terminal.floats[5] -= std::floor(terminal.floats[5]);
            } else {
                n.floats[4] += phase * 6.2831853f;
                n.floats[9] = n.floats[4];
            }
            glColorMask(1, 1, 1, 0);
            producerPass = true;
            drawMixer(n, terminal, -1, false, c.values[Gain], false);
            producerPass = false;
            glColorMask(1, 1, 1, 1);
        }
        if (c.enabled && c.kind == ComponentKind::Vector) {
            initializeBeam();
            canvas.clear();
            if (c.profile == 4) {
                int columns = int(std::lround(c.values[GridColumns])),
                    rows = int(std::lround(c.values[GridRows]));
                auto energy = [&](float v) {
                    return c.values[GridFade] +
                           (1 - c.values[GridFade]) * std::pow(v, c.values[GridFadePower]);
                };
                // Eight distance segments preserve fading, within the 512-stroke budget.
                for (int x = 0; x <= columns; ++x)
                    for (int j = 0; j < 8; ++j) {
                        float a = j / 8.f, b = (j + 1) / 8.f;
                        canvas.line({x * 640.f / columns, a * 480}, {x * 640.f / columns, b * 480},
                                    {1, 1, 1}, energy((a + b) * .5f), 1);
                    }
                for (int y = 0; y <= rows; ++y) {
                    float v = y / float(rows);
                    canvas.line({0, v * 480}, {640, v * 480}, {1, 1, 1}, energy(v), 1);
                }
            } else if (c.profile == 3) {
                if (c.vectorDocument.empty()) {
                    for (auto &o : objects)
                        canvas.object(o);
                } else {
                    std::istringstream input(c.vectorDocument);
                    for (auto &o : vectors::readObjects(input))
                        canvas.object(o);
                }
            } else
                vectors::demo(canvas, std::clamp(c.profile, 0, 2), seconds * c.values[Speed]);
            auto style = params.beam;
            style.width = c.values[BeamWidth];
            style.halo = c.values[BeamGlow];
            style.colour = 1;
            style.persistenceMs = 0;
            auto &vertices =
                cache.beam.build(canvas.strokes(), seconds, c.resolution, c.resolution, style);
            glUseProgram(beamProgram);
            glUniform2f(location(beamProgram, U_viewportSize), float(c.resolution),
                        float(c.resolution));
            glUniform4f(location(beamProgram, U_beamStyle), style.intensity, style.halo,
                        style.spread, style.whiteHot);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBindVertexArray(beamVao);
            glBindBuffer(GL_ARRAY_BUFFER, beamVbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vectors::Vertex),
                         vertices.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
            ++stats.drawCalls;
            stats.triangles += unsigned(vertices.size() / 3);
            stats.uploadedBytes += vertices.size() * sizeof(vectors::Vertex);
        }
        cache.key = revision;
        ++stats.textureBuilds;
        ++stats.totalTextureBuilds;
    }
    void componentFrame() {
        auto error = components.validate();
        if (!error.empty())
            throw std::runtime_error(error);
        for (auto it = componentTargets.begin(); it != componentTargets.end();) {
            auto found =
                std::find_if(activeComponents().items.begin(), activeComponents().items.end(),
                             [&](const auto &c) {
                                 return c.id == it->first &&
                                        (c.producer() || c.kind == ComponentKind::Primitive);
                             });
            if (found == activeComponents().items.end()) {
                it->second.target.release();
                it = componentTargets.erase(it);
            } else
                ++it;
        }
        for (auto &c : activeComponents().items)
            if (c.producer())
                componentSource(c);
        beginScratch();
        if (params.componentPreviewId &&
            std::none_of(activeComponents().items.begin(), activeComponents().items.end(),
                         [&](const auto &c) { return c.id == params.componentPreviewId; }))
            params.componentPreviewId = 0;
        if (params.componentPreviewId) {
            auto &selectedIndex = components.at(params.componentPreviewId);
            if (selectedIndex.producer()) {
                ShapeSettings n{}, terminal{};
                n.integers[2] = 26;
                n.integers[1] = n.integers[4] = 255;
                drawMixer(n, terminal, 10000 + selectedIndex.id, false, 1, false);
                copyStack(0);
                return;
            }
        }
        for (auto &c : activeComponents().items) {
            MaterialScope material(materialComponent, &c, sourcePass, false);
            if (!c.enabled || !c.visible || c.kind == ComponentKind::Texture ||
                (params.componentPreviewId && params.componentPreviewId != c.id))
                continue;
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, c.blend == 0 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
            ShapeSettings terminal{};
            for (int i = 0; i < 8; i++)
                terminal.floats[i < 4 ? i + 4 : i + 5] = c.values[C0 + i];
            if (c.kind == ComponentKind::Shape || c.kind == ComponentKind::Primitive) {
                double seconds = c.animated ? std::floor(time() * c.updateHz) / c.updateHz : 0;
                const CubeFace *mesh = nullptr;
                if (c.kind == ComponentKind::Primitive) {
                    auto &cache = componentTargets[c.id];
                    uint64_t key = 1469598103934665603ull;
                    auto mix = [&](float value) {
                        uint32_t bits;
                        std::memcpy(&bits, &value, 4);
                        key = (key ^ bits) * 1099511628211ull;
                    };
                    mix(float(c.profile));
                    for (auto f : {PrimitiveWidth, PrimitiveHeight, PrimitiveDepth,
                                   PrimitiveSegments, PrimitiveRings, PrimitiveTube,
                                   PrimitiveDistortion, PrimitiveWaves, Phase, RepeatX, RepeatY})
                        mix(c.values[f]);
                    if (c.animated && c.values[PrimitiveDistortion] != 0) {
                        mix(float(seconds));
                        mix(c.values[Speed]);
                    }
                    if (cache.shapeKey != key) {
                        cache.shapeVertices = primitiveMesh(c, seconds);
                        cache.shapeKey = key;
                        ++stats.shapeBuilds;
                        ++stats.totalShapeBuilds;
                    }
                    mesh = &cache.shapeVertices;
                } else
                    mesh = &componentShape(c, seconds);
                const auto &cachedVertices = *mesh;
                CubeFace faded;
                const CubeFace *vertices = &cachedVertices;
                if (c.values[Opacity] < 1) {
                    faded = cachedVertices;
                    for (auto &v : faded)
                        v.rgba =
                            (v.rgba & 0xffffff00u) | unsigned((v.rgba & 255) * c.values[Opacity]);
                    vertices = &faded;
                }
                auto size = identityMatrix();
                size[0] = size[5] = size[10] = c.values[Scale];
                auto orientation = multiplyMatrix(
                    rotation(2, c.values[Roll] + (c.kind == ComponentKind::Primitive && c.animated
                                                      ? float(seconds) * c.values[Speed]
                                                      : 0)),
                    multiplyMatrix(rotation(1, c.values[Yaw]), rotation(0, c.values[Pitch])));
                auto view = multiplyMatrix(
                    multiplyMatrix(
                        multiplyMatrix(size, orientation),
                        translationMatrix(c.values[X] * 400, c.values[Y] * 400, c.values[Forward])),
                    originalStackView());
                view = multiplyMatrix(
                    view, multiplyMatrix(rotation(1, -yawValue()), rotation(0, -pitchValue())));
                float horizontal = 256.f * (params.framing.mode == Framing::LegacyStretch
                                                ? float(sourceWidth) / sourceHeight
                                                : params.framing.referenceAspect);
                float nearClip =
                    c.kind == ComponentKind::Primitive ? c.values[PrimitiveNearClip] : 400.f;
                float frustumScale = nearClip / 400.f;
                auto projection = originalPerspective(-256 * frustumScale, 256 * frustumScale,
                                                      -horizontal * frustumScale,
                                                      horizontal * frustumScale, nearClip, 14100);
                if (c.kind == ComponentKind::Primitive) {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);
                    glDepthMask(GL_TRUE);
                    clearDepth(1);
                    glClear(GL_DEPTH_BUFFER_BIT);
                    // Resolve the nearest surface before additive/alpha colour draws, so rear
                    // faces cannot leave bright bands through an opaque curved primitive.
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    drawSpatial(*vertices, multiplyMatrix(view, projection), c.shader, terminal,
                                {c.source, c.sourceB ? c.sourceB : c.source,
                                 c.sourceC ? c.sourceC : c.source},
                                c.values[Gain]);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                }
                drawSpatial(
                    *vertices, multiplyMatrix(view, projection), c.shader, terminal,
                    {c.source, c.sourceB ? c.sourceB : c.source, c.sourceC ? c.sourceC : c.source},
                    c.values[Gain]);
                if (c.kind == ComponentKind::Primitive)
                    glDisable(GL_DEPTH_TEST);
            } else if (c.kind == ComponentKind::Flower) {
                ShapeSettings n{};
                n.integers[0] = int(std::lround(c.values[Segments]));
                n.integers[2] = int(c.values[Opacity] * 255);
                n.integers[3] = int(std::lround(c.values[RepeatX]));
                n.integers[7] = int(c.values[Colour] * 255);
                n.integers[9] = int(std::lround(c.values[FlowerMode]));
                n.floats[0] = c.values[InnerRadius] * c.values[Scale];
                n.floats[1] = c.values[MiddleRadius] * c.values[Scale];
                n.floats[2] = c.values[OuterRadius] * c.values[Scale];
                n.floats[3] = c.values[Forward];
                n.floats[4] = c.values[MiddleDepth];
                n.floats[5] = c.values[OuterDepth];
                n.floats[6] = c.values[Yaw];
                n.floats[7] = c.values[Pitch];
                n.floats[8] = c.values[Roll] + (c.animated ? float(time()) * c.values[Speed] : 0);
                n.floats[9] = c.values[DeformX];
                n.floats[10] = c.values[DeformY];
                n.floats[11] = c.values[DeformZ];
                auto parent = multiplyMatrix(translationMatrix(c.values[X], c.values[Y], 0),
                                             originalStackView());
                auto view = multiplyMatrix(
                    flowerModelView(n, parent),
                    multiplyMatrix(rotation(1, -yawValue()), rotation(0, -pitchValue())));
                float horizontal = 256.f * (params.framing.mode == Framing::LegacyStretch
                                                ? float(sourceWidth) / sourceHeight
                                                : params.framing.referenceAspect);
                auto projection =
                    originalPerspective(-256, 256, -horizontal, horizontal, 165, 10024);
                drawSpatial(
                    flowerMesh(n), multiplyMatrix(view, projection), c.shader, terminal,
                    {c.source, c.sourceB ? c.sourceB : c.source, c.sourceC ? c.sourceC : c.source},
                    c.values[Gain]);
            } else if (c.kind == ComponentKind::Tunnel) {
                auto n = componentTunnelSettings(c);
                auto &tunnel = componentTunnels[c.id];
                tunnel.tick(n, profiles, false);
                auto frame = tunnel.frame(n, scene.neutralView);
                ShapeSettings cameraNode{};
                cameraNode.floats[4] = 400;
                cameraNode.floats[5] = 10024;
                for (auto &node : scene.preset.nodes)
                    if (node.type == 12) {
                        cameraNode = node.saved;
                        break;
                    }
                auto camera =
                    multiplyMatrix(translationMatrix(-c.values[X], -c.values[Y], 0),
                                   multiplyMatrix(rotation(1, -c.values[Yaw] - yawValue()),
                                                  rotation(0, -c.values[Pitch] - pitchValue())));
                float vertical = (cameraNode.floats[3] + 1) * 256;
                float horizontal = vertical * (params.framing.mode == Framing::LegacyStretch
                                                   ? float(sourceWidth) / sourceHeight
                                                   : params.framing.referenceAspect);
                auto projection = originalPerspective(-vertical, vertical, -horizontal, horizontal,
                                                      cameraNode.floats[4], cameraNode.floats[5]);
                drawSpatial(
                    frame.vertices,
                    multiplyMatrix(multiplyMatrix(frame.modelView, camera), projection), c.shader,
                    terminal,
                    {c.source, c.sourceB ? c.sourceB : c.source, c.sourceC ? c.sourceC : c.source},
                    c.values[Gain]);
            } else {
                int input = c.kind == ComponentKind::Vector || c.kind == ComponentKind::Layer
                                ? 10000 + c.id
                                : c.source;
                auto p = shader(c.shader);
                bind(p, c.shader, terminal,
                     {input, c.sourceB ? c.sourceB : input, c.sourceC ? c.sourceC : input});
                glUniform1f(location(p, U_materialGain), c.values[Gain]);
                if (c.shader == 26)
                    glUniform1f(location(p, U_gain), c.values[Gain]);
                glUniform1i(location(p, U_spatial), 0);
                glUniform1f(location(p, U_outputSign), 1);
                auto q = quad();
                float angle = c.values[Roll] + (c.animated && c.kind != ComponentKind::Layer
                                                    ? float(time()) * c.values[Speed]
                                                    : 0);
                for (auto &v : q) {
                    float x = (v.x - .5f) * c.values[Scale], y = (v.y - .5f) * c.values[Scale];
                    float z = -x * std::sin(c.values[Yaw]);
                    x *= std::cos(c.values[Yaw]);
                    y = y * std::cos(c.values[Pitch]) - z * std::sin(c.values[Pitch]);
                    v.x = .5f + x * std::cos(angle) - y * std::sin(angle) + c.values[X];
                    v.y = .5f + x * std::sin(angle) + y * std::cos(angle) + c.values[Y];
                    if (c.kind != ComponentKind::Layer) {
                        v.x = .5f + (v.x - .5f) / screen.extentX;
                        v.y = .5f + (v.y - .5f) / screen.extentY;
                    } else {
                        v.u = .5f + (v.u - .5f) * screen.extentX;
                        v.v = .5f + (v.v - .5f) * screen.extentY;
                    }
                    v.a = c.values[Opacity];
                    v.b = c.values[Colour];
                    v.u *= c.kind == ComponentKind::Plane ? c.values[RepeatX] : 1;
                    v.v *= c.kind == ComponentKind::Plane ? c.values[RepeatY] : 1;
                    v.u1 = v.u;
                    v.v1 = v.v;
                }
                draw(q.data(), q.size(), GL_TRIANGLE_STRIP);
            }
        }
        copyStack(0);
    }
    void tickCpu() {
        scene.preset.tick();
        evaluateEdits();
        if ((selected == 18 || selected == 21))
            for (auto &c : activeComponents().items)
                if (c.enabled && c.kind == ComponentKind::Tunnel) {
                    auto n = componentTunnelSettings(c);
                    componentTunnels[c.id].tick(n, profiles, c.animated);
                }
        if (selected >= 100) {
            studyClock.tick(140);
            studyMixerValues =
                evaluate(scene.preset.nodes[9].channels, studyClock.clocks(), studyMixerValues);
            studyOutputValues =
                evaluate(scene.preset.nodes[10].channels, studyClock.clocks(), studyOutputValues);
        }
        for (auto &t : scene.tunnels)
            t.second.tick(settings[t.first], profiles);
        for (auto &p : scene.particles)
            p.second.tick(settings[p.first], scene.palette);
    }
    double time() const {
        return scene.preset.ticks / 60.;
    }
    bool usesVectorTexture() const {
        for (auto &n : settings)
            if (std::find(n.integers.begin(), n.integers.end(), 200) != n.integers.end())
                return true;
        return false;
    }
    uint64_t key() const {
        bool vectorSource = usesVectorTexture();
        uint64_t hash = scene.preset.ticks ^ (vectorSource ? objectRevision : 0);
        auto bank = author.responses.find(selected);
        if (bank != author.responses.end())
            hash ^= bank->second.revision * 1099511628211ull;
        auto mix = [&](float f) {
            uint32_t b;
            std::memcpy(&b, &f, 4);
            hash = (hash ^ b) * 1099511628211ull;
        };
        for (float v :
             {yawValue(), pitchValue(), params.titleSize, params.titlePeriod, params.titleHue,
              params.titleFilter, params.titleOpacity, float(params.titleSprite)})
            mix(v);
        if (vectorSource)
            for (float v : {params.beam.width, params.beam.intensity, params.beam.halo,
                            params.beam.spread, params.beam.persistenceMs, params.beam.colour,
                            params.beam.whiteHot, float(params.vectorScene)})
                mix(v);
        if (selected >= 2 && selected <= 20 && selected != 18)
            for (auto &e : author.edits[selected - 2]) {
                if (!vectorSource && e.key.rfind("vector.", 0) == 0)
                    continue;
                mix(float(e.enabled));
                mix(e.value);
                mix(float(e.motion));
                mix(e.period);
                mix(e.depth);
            }
        if ((selected == 18 || selected == 21))
            mix(float(params.componentPreviewId));
        if ((selected == 18 || selected == 21))
            for (auto &c : activeComponents().items) {
                mix(float(c.id));
                mix(float(c.kind));
                mix(float(c.enabled));
                mix(float(c.visible));
                mix(float(c.animated));
                mix(float(c.source));
                mix(float(c.sourceB));
                mix(float(c.sourceC));
                mix(float(c.shader));
                mix(float(c.blend));
                mix(float(c.resolution));
                mix(float(c.profile));
                mix(float(c.profileB));
                mix(float(c.updateHz));
                if (c.kind == ComponentKind::Layer) {
                    hash ^= layerFingerprint(c);
                    mix(float(c.layerOutput));
                    mix(float(c.layerComponent));
                }
                if (!c.vectorDocument.empty())
                    hash ^= std::hash<std::string>{}(c.vectorDocument);
                for (auto v : c.values)
                    mix(v);
            }
        if (selected == 104) {
            for (float v : {float(geometry.enabled), float(geometry.routing),
                            float(geometry.placement), float(geometry.copies), geometry.size,
                            geometry.spread, geometry.spin, geometry.opacity, geometry.glow})
                mix(v);
            for (int layer = 0; layer < 3; layer++) {
                mix(float(geometry.visible[layer]));
                mix(geometry.layerOpacity[layer]);
                for (auto value : geometry.shapes[layer].integers)
                    mix(float(value));
                for (auto value : geometry.shapes[layer].floats)
                    mix(value);
                for (int c = 0; c < 2; c++) {
                    mix(float(geometry.motion[layer][c]));
                    mix(geometry.rate[layer][c]);
                }
            }
        }
        return hash;
    }

    void beginScratch(bool depth = false) {
        unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, scratch.fbo);
        glViewport(0, 0, scratch.width, scratch.height);
        glClearColor(0, 0, 0, 0);
        if (depth) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            clearDepth(1);
        } else
            glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0));
    }
    void drawMixer(ShapeSettings n, const ShapeSettings &terminal, int overrideSource = -1,
                   bool legacyCoordinates = false, float materialGain = 1, bool additive = true) {
        int id = n.integers[2];
        auto p = shader(id);
        bind(p, id, terminal,
             {overrideSource >= 0 ? overrideSource : n.integers[5], n.integers[3], n.integers[6]});
        glUniform1i(location(p, U_spatial), 0);
        glUniform1f(location(p, U_outputSign), 1);
        glUniform1f(location(p, U_materialGain), materialGain);
        if (id == 26)
            glUniform1f(location(p, U_gain), materialGain);
        if (additive)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        auto q = quad();
        for (auto &v : q) {
            if (legacyCoordinates && params.framing.mode == Framing::LegacyStretch) {
                v.x -= .5f / (512 * (producerPass ? 1 : screen.extentX));
                v.y -= .5f / (512 * (producerPass ? 1 : screen.extentY));
            }
            if (!producerPass) {
                v.u = .5f + (v.u - .5f) * screen.extentX;
                v.v = .5f + (v.v - .5f) * screen.extentY;
                v.u1 = .5f + (v.u1 - .5f) * screen.extentX;
                v.v1 = .5f + (v.v1 - .5f) * screen.extentY;
            }
            auto uv = [&](float &u, float &vv, int base) {
                float sx = std::max(.0001f, n.floats[base] + 1),
                      sy = std::max(.0001f, n.floats[base + 1] + 1),
                      x = u - .5f + n.floats[base + 2] / sx, y = vv - .5f + n.floats[base + 3] / sy,
                      a = n.floats[base + 4];
                u = (x * std::cos(a) - y * std::sin(a)) * sx + .5f;
                vv = (x * std::sin(a) + y * std::cos(a)) * sy + .5f;
            };
            uv(v.u, v.v, 0);
            uv(v.u1, v.v1, 5);
            v.a = n.integers[1] / 255.f;
            v.b = n.integers[4] / 255.f;
        }
        draw(q.data(), q.size(), GL_TRIANGLE_STRIP);
    }
    void copyStack(int stack) {
        unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, stacks[stack].fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        auto p = shader(26);
        glUniform1i(location(p, U_spatial), 0);
        glUniform1f(location(p, U_outputSign), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scratch.texture);
        glDisable(GL_BLEND);
        auto q =
            params.framing.mode == Framing::LegacyStretch ? quad(332.f / 330, 262.f / 260) : quad();
        draw(q.data(), q.size(), GL_TRIANGLE_STRIP);
    }
    void drawSpatial(const CubeFace &mesh, const Matrix4 &matrix, int id,
                     const ShapeSettings &terminal, std::array<int, 3> inputs,
                     float materialGain = 1, int targetExtent = 0) {
        auto p = shader(id);
        bind(p, id, terminal, inputs);
        glUniform1f(location(p, U_materialGain), materialGain);
        if (id == 26)
            glUniform1f(location(p, U_gain), materialGain);
        if (id == 1000)
            glUniform4f(location(p, U_c25), params.titleFilter, 0, 0, 0);
        auto adjusted = matrix;
        if (!targetExtent) {
            for (int i = 0; i < 4; ++i) {
                adjusted[i * 4] /= screen.extentX;
                adjusted[i * 4 + 1] /= screen.extentY;
            }
        }
        glUniformMatrix4fv(location(p, U_transform), 1, GL_FALSE, adjusted.data());
        glUniform1i(location(p, U_spatial), 1);
        float pixelExtent = float(targetExtent ? targetExtent : extent);
        glUniform2f(location(p, U_pixelShift),
                    params.framing.mode == Framing::LegacyStretch
                        ? 1.f / (targetExtent ? pixelExtent : scratch.width)
                        : 0,
                    params.framing.mode == Framing::LegacyStretch
                        ? 1.f / (targetExtent ? pixelExtent : scratch.height)
                        : 0);
        glUniform1f(location(p, U_outputSign), -1);
        std::vector<Vertex> vertices;
        vertices.reserve(mesh.size());
        for (auto &v : mesh)
            vertices.push_back({v.x, v.y, v.z, ((v.rgba >> 24) & 255) / 255.f,
                                ((v.rgba >> 16) & 255) / 255.f, ((v.rgba >> 8) & 255) / 255.f,
                                (v.rgba & 255) / 255.f, v.u0, v.v0, v.u1, v.v1});
        draw(vertices.data(), vertices.size(), GL_TRIANGLES);
    }
    void studyFrame() {
        auto &graph = scene.preset;
        beginScratch(selected == 1);
        if (selected == 1) {
            auto n = graph.nodes[24].effective;
            auto camera = multiplyMatrix(
                translationMatrix(-control("camera.x"), -control("camera.y"), -control("camera.z")),
                multiplyMatrix(rotation(1, -yawValue() - control("camera.yaw")),
                               multiplyMatrix(rotation(0, -pitchValue() - control("camera.pitch")),
                                              rotation(2, -control("camera.roll")))));
            auto transform = multiplyMatrix(
                multiplyMatrix(originalCubeModelView(n, originalStackView()), camera),
                originalStackProjection(512, 512));
            auto field = cubeField(n);
            uint32_t rgba = 0xffff0000u | (uint32_t(n.integers[9]) << 8) | uint32_t(n.integers[6]);
            ShapeSettings terminal{};
            glDisable(GL_BLEND);
            for (int face = 0; face < 6; face++)
                if (n.integers[face])
                    drawSpatial(cubeFace(n, field, rgba, face), transform, n.integers[7], terminal,
                                {n.integers[face], 0, 0});
            glDisable(GL_DEPTH_TEST);
            return;
        }
        int sourceNode = selected == 0 ? 9 : selected == 2 ? 13 : selected == 3 ? 10 : 7;
        auto sourceNodeSettings = settings[sourceNode];
        auto terminal = settings[sourceNode + 1];
        drawMixer(sourceNodeSettings, terminal, -1, selected == 0);
        if (selected == 0)
            return;
        int sourceStack = graph.nodes[sourceNode].stack;
        copyStack(sourceStack);
        beginScratch();
        int finalNode = selected == 2 ? 16 : selected == 3 ? 17 : 14;
        auto finalTerminal = settings[finalNode + 1];
        finalTerminal.integers[7] = 3;
        drawMixer(settings[finalNode], finalTerminal, 31 + sourceStack);
        copyStack(graph.nodes[finalNode].stack);
    }
    std::array<uint32_t, 4> palette(int layer) const {
        std::array<uint32_t, 4> colours{};
        for (int i = 0; i < 4; i++) {
            float h = std::fmod(layer * .19f + i * .17f + 1.f, 1.f) * 6, c = .95f;
            float x = c * (1 - std::abs(std::fmod(h, 2.f) - 1)), r = 0, g = 0, b = 0;
            if (h < 1) {
                r = c;
                g = x;
            } else if (h < 2) {
                r = x;
                g = c;
            } else if (h < 3) {
                g = c;
                b = x;
            } else if (h < 4) {
                g = x;
                b = c;
            } else if (h < 5) {
                r = x;
                b = c;
            } else {
                r = c;
                b = x;
            }
            colours[i] = 0xff000000u | (uint32_t((r + .04f) * 255) << 16) |
                         (uint32_t((g + .04f) * 255) << 8) | uint32_t((b + .04f) * 255);
        }
        return colours;
    }
    void drawStudyShapes(const Mesh &mesh) {
        auto p = shader(1001);
        glUniform1i(location(p, U_spatial), 0);
        glUniform1f(location(p, U_outputSign), 1);
        glUniform2f(location(p, U_pixelShift),
                    params.framing.mode == Framing::LegacyStretch ? 1.f / sourceWidth : 0,
                    params.framing.mode == Framing::LegacyStretch ? 1.f / sourceHeight : 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        std::vector<Vertex> vertices;
        vertices.reserve(mesh.size());
        for (auto &v : mesh) {
            auto c = v.colour;
            vertices.push_back({v.position.x / sourceWidth, v.position.y / sourceHeight, 0,
                                ((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f,
                                (c & 255) / 255.f, (c >> 24) / 255.f, 0, 0, 0, 0});
        }
        draw(vertices.data(), vertices.size(), GL_TRIANGLES);
    }
    void syntheticStudyFrame() {
        auto n = scene.preset.nodes[9].saved, terminal = scene.preset.nodes[10].saved;
        float phase = studyOutputValues[7] ? mapFloat(*studyOutputValues[7], 0, 1) : 0;
        terminal.floats[5] = std::clamp(phase, 0.f, 1.f);
        n.floats[4] = studyMixerValues[3] ? mapFloat(*studyMixerValues[3], -6.2831855f, 6.2831855f)
                                          : n.floats[4];
        n.integers[2] = selected == 103 ? 80 : 1079;
        int root = selected == 101 || selected == 103 ? 2 : selected == 102 ? 3 : 4;
        Mesh shapes;
        if (selected == 104 && geometry.enabled)
            shapes = composeGeometry(geometry, studyClock.clocks()[3], sourceWidth, sourceHeight,
                                     fit, centers, {palette(0), palette(1), palette(2)});
        bool feeds = selected == 104 && geometry.enabled && geometry.routing != 0;
        if (feeds) {
            unbind();
            glBindFramebuffer(GL_FRAMEBUFFER, geometryFeed.fbo);
            glViewport(0, 0, sourceWidth, sourceHeight);
            glClearColor(2.f / 255, 3.f / 255, 9.f / 255, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            auto p = shader(26);
            glUniform1i(location(p, U_spatial), 0);
            glUniform1f(location(p, U_outputSign), 1);
            glUniform2f(location(p, U_pixelShift), 0, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture(root + 1));
            glBindSampler(0, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glDisable(GL_BLEND);
            auto q = quad();
            draw(q.data(), q.size(), GL_TRIANGLE_STRIP);
            drawStudyShapes(shapes);
        }
        unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, studyOutput.fbo);
        glViewport(0, 0, sourceWidth, sourceHeight);
        glClearColor(2.f / 255, 3.f / 255, 9.f / 255, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        n.integers[5] = feeds ? 201 : root + 1;
        drawMixer(n, terminal, -1, true, (selected == 103 ? 2.f : 32.f) * exposureValue(), false);
        if (selected == 104 && geometry.enabled && geometry.routing != 1)
            drawStudyShapes(shapes);
    }
    void sceneFrame() {
        if (clearPending) {
            unbind();
            glClearColor(0, 0, 0, 0);
            for (auto &t : stacks)
                if (t.fbo) {
                    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            clearPending = false;
        }
        auto revision = key() ^ (uint64_t(params.layerPreview + 1) * 0x9e3779b97f4a7c15ull);
        if (renderedKey == revision)
            return;
        if ((selected == 18 || selected == 21)) {
            componentFrame();
            renderedKey = revision;
            return;
        }
        if (selected >= 100) {
            syntheticStudyFrame();
            renderedKey = revision;
            return;
        }
        if (selected < 5) {
            studyFrame();
            renderedKey = revision;
            return;
        }
        auto camera = multiplyMatrix(
            translationMatrix(-control("camera.x"), -control("camera.y"), -control("camera.z")),
            multiplyMatrix(rotation(1, -yawValue() - control("camera.yaw")),
                           multiplyMatrix(rotation(0, -pitchValue() - control("camera.pitch")),
                                          rotation(2, -control("camera.roll")))));
        for (int stack = 0; stack < 16; stack++)
            if (scene.reachable[stack]) {
                unbind();
                glBindFramebuffer(GL_FRAMEBUFFER, scratch.fbo);
                glViewport(0, 0, scratch.width, scratch.height);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
                auto &indices = scene.preset.stacks[stack].nodes;
                auto terminal = settings[indices.back()];
                if (control(("layer." + std::to_string(stack)).c_str(), 1) < .5f) {
                    copyStack(stack);
                    continue;
                }
                ShapeSettings cameraNode{};
                cameraNode.floats[4] = 400;
                cameraNode.floats[5] = 10024;
                bool hasCamera = false;
                for (auto index : indices) {
                    auto &node = scene.preset.nodes[index];
                    auto n = settings[index];
                    if (node.type == 12) {
                        cameraNode = n;
                        hasCamera = true;
                        continue;
                    }
                    if (node.type == 0 || node.type == 7 || node.type == 14)
                        continue;
                    if (node.type == 9 || node.type == 6) {
                        if (node.type == 6)
                            n.integers[5] = 31 + stack;
                        drawMixer(n, terminal);
                    } else if (node.type == 15 || node.type == 1 || node.type == 19) {
                        TunnelFrame frame;
                        if (node.type == 19) {
                            frame.vertices = flowerMesh(n);
                            frame.modelView = flowerModelView(n, hasCamera ? scene.neutralView
                                                                           : originalStackView());
                        } else if (node.type == 15) {
                            auto &tunnel = scene.tunnels.at(index);
                            tunnel.tick(n, profiles, false);
                            frame = tunnel.frame(n, scene.neutralView);
                        } else {
                            auto &particles = scene.particles.at(index);
                            if (!particles.ready())
                                particles.tick(n, scene.palette);
                            frame.modelView = scene.neutralView;
                            frame.vertices =
                                selected == 16 && index == 19
                                    ? titleFlightBillboards(n, particles.particles(), time(),
                                                            params.titleSize, params.titlePeriod,
                                                            params.titleHue, params.titleOpacity,
                                                            params.titleSprite)
                                    : particleBillboards(n, particles.particles());
                        }
                        auto projection = originalPerspective(
                            -(cameraNode.floats[3] + 1) * 256 / control("camera.zoom", 1),
                            (cameraNode.floats[3] + 1) * 256 / control("camera.zoom", 1),
                            -(cameraNode.floats[2] + 1) * 256 / control("camera.zoom", 1),
                            (cameraNode.floats[2] + 1) * 256 / control("camera.zoom", 1),
                            cameraNode.floats[4], cameraNode.floats[5]);
                        auto matrix =
                            multiplyMatrix(multiplyMatrix(frame.modelView, camera), projection);
                        int id = node.type == 15 ? n.integers[15]
                                 : node.type == 19
                                     ? n.integers[6]
                                     : (selected == 16 && index == 19 ? 1000 : n.integers[9]);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_ONE, GL_ONE);
                        drawSpatial(
                            frame.vertices, matrix, id, terminal,
                            node.type == 15
                                ? std::array<int, 3>{n.integers[3], n.integers[4], n.integers[12]}
                            : node.type == 19
                                ? std::array<int, 3>{n.integers[4], n.integers[5], n.integers[8]}
                                : std::array<int, 3>{id == 1000 ? 147 : n.integers[6],
                                                     n.integers[7], 0});
                    } else
                        throw std::runtime_error("Unported module in GLES effect");
                }
                copyStack(stack);
            }
        renderedKey = revision;
    }
    void renderOverlays(const Destination &d) {
        const auto layout = screenLayout(d.viewport.width, d.viewport.height, d.clockwiseTurns);
        if (d.width < 1 || d.height < 1 || d.viewport.x < 0 || d.viewport.y < 0 ||
            d.viewport.x > d.width || d.viewport.y > d.height ||
            d.viewport.width > d.width - d.viewport.x ||
            d.viewport.height > d.height - d.viewport.y)
            throw std::runtime_error("Invalid overlay destination");
        const auto &frame = overlayText.prepare(layout.width, layout.height);
        if (frame.overGame.empty() && frame.bezel.empty())
            return;
        if (!glad_glBindSampler || !clearDepth)
            throw std::runtime_error("Host must load GLES 3 before rendering overlays");
        State state;
        initializeBeam();
        if (!overlayProgram)
            overlayProgram = program(read(data / "vectors/vector-beam.vert.glsl"),
                                     read(data / "vectors/vector-beam.frag.glsl"));
        glBindFramebuffer(GL_FRAMEBUFFER, d.framebuffer);
        auto r = d.viewport;
        glViewport(r.x, d.height - r.y - r.height, r.width, r.height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DITHER);
        glColorMask(1, 1, 1, 0);
        glBlendEquation(GL_FUNC_ADD);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(overlayProgram);
        glUniform2f(location(overlayProgram, U_viewportSize), float(r.width), float(r.height));
        auto style = overlayText.beamStyle();
        glUniform4f(location(overlayProgram, U_beamStyle), style.intensity, style.halo,
                    style.spread, style.whiteHot);
        auto drawText = [&](const auto &input, std::unique_ptr<vectors::BeamMesh> &mesh,
                            OverlayCache &cache, bool bezel) {
            if (input.empty())
                return;
            uint64_t key = 1469598103934665603ull;
            auto mix = [&](float f) {
                uint32_t bits;
                std::memcpy(&bits, &f, 4);
                key = (key ^ bits) * 1099511628211ull;
            };
            for (float f : {float(layout.width), float(layout.height), float(layout.turns),
                            style.width, style.intensity, style.halo, style.spread,
                            style.persistenceMs, style.colour, style.whiteHot})
                mix(f);
            for (auto &s : input)
                for (float f : {s.a.x, s.a.y, s.b.x, s.b.y, s.colour.r, s.colour.g, s.colour.b,
                                s.energy, s.width})
                    mix(f);
            if (style.persistenceMs > 0)
                mix(float(time()));
            if (cache.key != key) {
                auto strokes = input;
                for (auto &stroke : strokes)
                    for (auto p : {&stroke.a, &stroke.b}) {
                        p->x *= 640.f / layout.width;
                        p->y *= 480.f / layout.height;
                    }
                if (!mesh)
                    mesh = std::make_unique<vectors::BeamMesh>();
                auto vertices = mesh->build(strokes, time(), layout.width, layout.height, style);
                for (auto &v : vertices) {
                    float x = v.x / layout.width, y = v.y / layout.height;
                    rotateScreenPoint(x, y, layout.turns);
                    v.x = x * r.width;
                    v.y = y * r.height;
                }
                if (!cache.vao) {
                    glGenVertexArrays(1, &cache.vao);
                    glGenBuffers(1, &cache.vbo);
                }
                glBindVertexArray(cache.vao);
                glBindBuffer(GL_ARRAY_BUFFER, cache.vbo);
                int widths[] = {2, 2, 2, 4}, offsets[] = {0, 8, 16, 24};
                for (int i = 0; i < 4; ++i) {
                    glEnableVertexAttribArray(i);
                    glVertexAttribPointer(i, widths[i], GL_FLOAT, GL_FALSE, sizeof(vectors::Vertex),
                                          (void *)(intptr_t)offsets[i]);
                }
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vectors::Vertex),
                             vertices.data(), GL_STATIC_DRAW);
                stats.uploadedBytes += vertices.size() * sizeof(vectors::Vertex);
                cache.vertices = vertices.size();
                cache.key = key;
            }
            if (!cache.vertices)
                return;
            glBindVertexArray(cache.vao);
            auto paint = [&](Rect clip) {
                if (clip.width <= 0 || clip.height <= 0)
                    return;
                glEnable(GL_SCISSOR_TEST);
                glScissor(clip.x, d.height - clip.y - clip.height, clip.width, clip.height);
                glDrawArrays(GL_TRIANGLES, 0, GLsizei(cache.vertices));
                ++stats.drawCalls;
                stats.triangles += unsigned(cache.vertices / 3);
            };
            auto h = d.exclude;
            int x = std::clamp(h.x, r.x, r.x + r.width), y = std::clamp(h.y, r.y, r.y + r.height),
                right = std::clamp(h.x + h.width, r.x, r.x + r.width),
                bottom = std::clamp(h.y + h.height, r.y, r.y + r.height);
            if (!bezel || h.width <= 0 || h.height <= 0 || right <= x || bottom <= y)
                paint(r);
            else {
                paint({r.x, r.y, r.width, y - r.y});
                paint({r.x, bottom, r.width, r.y + r.height - bottom});
                paint({r.x, y, x - r.x, bottom - y});
                paint({right, y, r.x + r.width - right, bottom - y});
            }
        };
        drawText(frame.bezel, overlayBezelMesh, overlayBezelCache, true);
        drawText(frame.overGame, overlayOverGameMesh, overlayOverGameCache, false);
    }
    void render(const Destination &destination) {
        if (!glad_glBindSampler || !clearDepth)
            throw std::runtime_error("Host must load GLES 3 sampler entry points before rendering");
        if (destination.width < 1 || destination.height < 1 || destination.viewport.width < 1 ||
            destination.viewport.height < 1 || destination.viewport.x < 0 ||
            destination.viewport.y < 0 || destination.viewport.x > destination.width ||
            destination.viewport.y > destination.height ||
            destination.viewport.width > destination.width - destination.viewport.x ||
            destination.viewport.height > destination.height - destination.viewport.y)
            throw std::runtime_error("Invalid effect destination");
        if (!std::isfinite(yawValue()) || !std::isfinite(pitchValue()) ||
            !std::isfinite(exposureValue()) || !std::isfinite(params.renderScale) ||
            params.renderScale < .25f || params.renderScale > 1 || params.vectorScene < 0 ||
            params.vectorScene > 3 || params.layerPreview < -1 || params.layerPreview > 15)
            throw std::runtime_error("Invalid effect parameters");
        State state;
        for (int unit = 0; unit < 3; unit++)
            glBindSampler(unit, 0);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DITHER);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glColorMask(1, 1, 1, 1);
        stats.drawCalls = stats.triangles = 0;
        stats.uploadedBytes = 0;
        stats.textureBuilds = 0;
        stats.shapeBuilds = 0;
        auto layout = screenLayout(destination.viewport.width, destination.viewport.height,
                                   destination.clockwiseTurns, params.framing);
        if (layout.extentX != screen.extentX || layout.extentY != screen.extentY) {
            renderedKey = ~uint64_t(0);
            clearPending = true;
        }
        screen = layout;
        initialize(std::max(1, int(std::lround(screen.width * params.renderScale))),
                   std::max(1, int(std::lround(screen.height * params.renderScale))));
        evaluateEdits();
        if (params.sourceOnly || control("vector.enabled") > .5f)
            vectors();
        else {
            glBindFramebuffer(GL_FRAMEBUFFER, source.fbo);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        sceneFrame();
        stats.targets = baseTargets;
        stats.targetBytes = baseBytes;
        for (auto &t : componentTargets)
            if (t.second.target.texture) {
                ++stats.targets;
                stats.targetBytes += size_t(t.second.target.width) * t.second.target.height * 4;
                if (t.second.layer) {
                    stats.targets += t.second.layer->statistics().targets;
                    stats.targetBytes += t.second.layer->statistics().targetBytes;
                }
            }
        unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, destination.framebuffer);
        auto r = destination.viewport;
        glViewport(r.x, destination.height - r.y - r.height, r.width, r.height);
        auto p = shader(selected == 104 && geometry.glow > 0 ? 1002 : 26);
        glUniform4f(location(p, U_c25), 2.f / sourceWidth, 2.f / sourceHeight, geometry.glow, 0);
        glUniform1f(location(p, U_gain),
                    selected >= 100 ? 1 : std::clamp(exposureValue(), 0.f, 4.f));
        glUniform2f(location(p, U_pixelShift), 1.f / r.width, -1.f / r.height);
        glUniform1i(location(p, U_spatial), 0);
        glUniform1f(location(p, U_outputSign), -1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(
            GL_TEXTURE_2D,
            (params.sourceOnly || int(control("scene.output", -1)) == 16) ? source.texture
            : selected >= 100                                             ? studyOutput.texture
            : selected < 2
                ? scratch.texture
                : stacks[params.layerPreview >= 0 ? params.layerPreview
                         : params.previewStage
                             ? scene.stages[params.previewStage]
                             : int(control("scene.output", float(scene.preset.outputStack)))]
                      .texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glDisable(GL_BLEND);
        glEnable(GL_SCISSOR_TEST);
        glColorMask(1, 1, 1, 0);
        auto q = params.framing.mode != Framing::LegacyStretch || selected < 2 || selected >= 100
                     ? quad()
                     : quad(320.f / 330, 240.f / 260);
        if (selected >= 2 && selected <= 4) {
            float yaw = control("camera.yaw"), pitch = control("camera.pitch"),
                  roll = control("camera.roll"), zoom = control("camera.zoom", 1);
            for (auto &v : q) {
                float x = v.x - .5f, y = v.y - .5f, z = -x * std::sin(yaw);
                x *= std::cos(yaw);
                float py = y * std::cos(pitch) - z * std::sin(pitch);
                z = y * std::sin(pitch) + z * std::cos(pitch);
                y = py;
                float px = x * std::cos(roll) - y * std::sin(roll);
                y = x * std::sin(roll) + y * std::cos(roll);
                x = px;
                float perspective = 2 / (2 + z);
                v.x = .5f + (x + control("camera.x")) * zoom * perspective;
                v.y = .5f + (y + control("camera.y")) * zoom * perspective;
            }
        }
        for (auto &v : q)
            rotateScreenPoint(v.x, v.y, screen.turns);
        if (selected < 2 || selected >= 100)
            glUniform2f(location(p, U_pixelShift), 0, 0);
        GLint backgroundTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &backgroundTexture);
        auto paint = [&](Rect c) {
            if (c.width <= 0 || c.height <= 0)
                return;
            glScissor(c.x, destination.height - c.y - c.height, c.width, c.height);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(p);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, backgroundTexture);
            glDisable(GL_BLEND);
            glUniform1f(location(p, U_gain),
                        selected >= 100 ? 1 : std::clamp(exposureValue(), 0.f, 4.f));
            glUniform2f(
                location(p, U_pixelShift),
                params.framing.mode != Framing::LegacyStretch || selected < 2 || selected >= 100
                    ? 0
                    : 1.f / r.width,
                params.framing.mode != Framing::LegacyStretch || selected < 2 || selected >= 100
                    ? 0
                    : -1.f / r.height);
            glUniform1i(location(p, U_spatial), 0);
            glUniform1f(location(p, U_outputSign), -1);
            if (control("vector.background", 1) > .5f || params.sourceOnly)
                draw(q.data(), q.size(), GL_TRIANGLE_STRIP);
            if (params.layerPreview < 0 && !params.componentPreviewId && !params.sourceOnly &&
                control("vector.enabled") > .5f && int(control("vector.placement")) != 1) {
                auto overlay = shader(26);
                glUniform1f(location(overlay, U_gain), 1);
                glUniform1i(location(overlay, U_spatial), 0);
                glUniform1f(location(overlay, U_outputSign), -1);
                glUniform2f(location(overlay, U_pixelShift), 0, 0);
                glBindTexture(GL_TEXTURE_2D, source.texture);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                auto full = quad();
                for (auto &v : full)
                    rotateScreenPoint(v.x, v.y, screen.turns);
                draw(full.data(), full.size(), GL_TRIANGLE_STRIP);
            }
        };
        auto hole = destination.exclude;
        int x = std::clamp(hole.x, r.x, r.x + r.width), y = std::clamp(hole.y, r.y, r.y + r.height),
            right = std::clamp(hole.x + hole.width, r.x, r.x + r.width),
            bottom = std::clamp(hole.y + hole.height, r.y, r.y + r.height);
        if (hole.width <= 0 || hole.height <= 0 || right <= x || bottom <= y)
            paint(r);
        else {
            paint({r.x, r.y, r.width, y - r.y});
            paint({r.x, bottom, r.width, r.y + r.height - bottom});
            paint({r.x, y, x - r.x, bottom - y});
            paint({right, y, r.x + r.width - right, bottom - y});
        }
    }
};
Runtime::Runtime(const std::filesystem::path &p, pc::AssetReader reader)
    : impl_(std::make_unique<Impl>(p, std::move(reader))) {}
Runtime::~Runtime() = default;
void Runtime::abandonGraphics() {
    impl_->abandon();
}
void Runtime::advance(double elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::runtime_error("Invalid effect time step");
    auto &i = *impl_;
    i.accumulator += std::min(elapsed, .25);
    int count = 0;
    while (i.accumulator >= 1. / 60 && count < 4) {
        i.tickCpu();
        i.accumulator -= 1. / 60;
        ++count;
    }
    if (i.accumulator >= 1. / 60) {
        i.stats.droppedTicks += unsigned(i.accumulator * 60);
        i.accumulator = std::fmod(i.accumulator, 1. / 60);
    }
}
void Runtime::frame(double elapsed, const Destination &destination) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::runtime_error("Invalid effect frame time");
    auto &i = *impl_;
    i.accumulator += std::min(elapsed, .25);
    unsigned steps = 0, draws = 0, triangles = 0;
    size_t uploaded = 0;
    while (i.accumulator >= 1. / 60 && steps < 4) {
        i.tickCpu();
        i.accumulator -= 1. / 60;
        ++steps;
        i.render(destination);
        draws += i.stats.drawCalls;
        triangles += i.stats.triangles;
        uploaded += i.stats.uploadedBytes;
    }
    if (i.accumulator >= 1. / 60) {
        i.stats.droppedTicks += unsigned(i.accumulator * 60);
        i.accumulator = std::fmod(i.accumulator, 1. / 60);
    }
    if (!steps && i.scene.preset.ticks) {
        i.render(destination);
        draws = i.stats.drawCalls;
        triangles = i.stats.triangles;
        uploaded = i.stats.uploadedBytes;
    }
    i.stats.drawCalls = draws;
    i.stats.triangles = triangles;
    i.stats.uploadedBytes = uploaded;
}
void Runtime::reset() {
    auto reads = impl_->readBanks.find(impl_->selected);
    if (reads != impl_->readBanks.end())
        reads->second.reset();
    impl_->readReport = {};
    auto bank = impl_->author.responses.find(impl_->selected);
    if (bank != impl_->author.responses.end())
        bank->second.reset();
    impl_->scene.reset();
    impl_->componentTunnels.clear();
    impl_->studyClock = {};
    impl_->studyMixerValues = {};
    impl_->studyOutputValues = {};
    impl_->accumulator = 0;
    impl_->renderedKey = ~uint64_t(0);
    impl_->beam.reset();
    impl_->clearPending = true;
}
void Runtime::render(const Destination &d) {
    impl_->render(d);
}
void Runtime::renderOverlays(const Destination &d) {
    impl_->renderOverlays(d);
}
OverlayTextLayer &Runtime::overlays() {
    return impl_->overlayText;
}
void Runtime::releaseGraphics() {
    impl_->release();
}
const std::vector<vectors::Object> &Runtime::vectorObjects() const {
    return impl_->objects;
}
void Runtime::setVectorObjects(std::vector<vectors::Object> objects) {
    if (objects.size() > 64)
        throw std::runtime_error("Too many vector objects");
    size_t count = 0;
    for (auto &o : objects)
        count += o.strokes.size();
    if (count > vectors::BeamMesh::MaxStrokes)
        throw std::runtime_error("Vector objects exceed runtime stroke budget");
    impl_->objects = std::move(objects);
    ++impl_->objectRevision;
    impl_->beam.reset();
    impl_->renderedKey = ~uint64_t(0);
}
pc::GeometryStudy &Runtime::geometryStudy() {
    return impl_->geometry;
}
Parameters &Runtime::parameters() {
    return impl_->params;
}
const Statistics &Runtime::statistics() const {
    return impl_->stats;
}
double Runtime::time() const {
    return impl_->time();
}
bool Runtime::loadGlesEntryPoints(void *(*resolve)(const char *)) {
    glad_glGenSamplers = reinterpret_cast<PFNGLGENSAMPLERSPROC>(resolve("glGenSamplers"));
    glad_glSamplerParameteri =
        reinterpret_cast<PFNGLSAMPLERPARAMETERIPROC>(resolve("glSamplerParameteri"));
    glad_glDeleteSamplers = reinterpret_cast<PFNGLDELETESAMPLERSPROC>(resolve("glDeleteSamplers"));
    glad_glBindSampler = reinterpret_cast<PFNGLBINDSAMPLERPROC>(resolve("glBindSampler"));
    clearDepth = reinterpret_cast<ClearDepthFunction>(resolve("glClearDepthf"));
    return glad_glBindSampler && clearDepth && glad_glGenSamplers && glad_glSamplerParameteri &&
           glad_glDeleteSamplers;
}
const std::filesystem::path &Runtime::dataPath() const {
    return impl_->data;
}
int Runtime::selectedPreset() const {
    return impl_->selected;
}
void Runtime::selectPreset(int id) {
    if (std::none_of(presets().begin(), presets().end(), [&](const auto &p) { return p.id == id; }))
        throw std::runtime_error("Unknown GLES preset");
    static const char *names[] = {"r2-c8",  "r6-c15", "r6-c11", "r4-c3",  "r5-c13", "r1-c6",
                                  "r1-c4",  "r1-c8",  "r4-c10", "r2-c13", "r6-c1",  "r1-c10",
                                  "r5-c8",  "r1-c0",  "r3-c1",  "r5-c0",  "r1-c8",  "r2-c13",
                                  "r2-c13", "r1-c12", "r2-c14", "r2-c13"};
    RecoveredScene pending(impl_->data, names[id >= 100 ? 0 : id], id >= 5 && id < 100,
                           impl_->reader);
    if (impl_->selected == 18 || impl_->selected == 21)
        impl_->componentDocuments[impl_->selected] = impl_->components;
    if (id == 18 || id == 21)
        impl_->components = impl_->componentDocuments.at(id);
    if (id >= 100) {
        pending.reachable.fill(false);
        pending.stages.fill(pending.preset.outputStack);
    }
    if (id < 5) {
        pending.reachable.fill(false);
        if (id < 2)
            pending.reachable[pending.preset.outputStack] = true;
        else {
            int source = id == 2 ? 13 : id == 3 ? 10 : 7, final = id == 2 ? 16 : id == 3 ? 17 : 14;
            pending.reachable[pending.preset.nodes[source].stack] = true;
            pending.reachable[pending.preset.nodes[final].stack] = true;
        }
        pending.stages.fill(pending.preset.outputStack);
        if (id >= 2)
            pending.stages[1] = pending.stages[2] =
                pending.preset.nodes[id == 2   ? 13
                                     : id == 3 ? 10
                                               : 7]
                    .stack;
    }
    if (id == 18 || id == 21) {
        pending.reachable.fill(false);
        pending.reachable[0] = true;
        pending.preset.outputStack = 0;
        pending.stages.fill(0);
    }
    impl_->scene = std::move(pending);
    impl_->baseReachable = impl_->scene.reachable;
    impl_->selected = id;
    auto reads = impl_->readBanks.find(id);
    if (reads != impl_->readBanks.end())
        reads->second.reset();
    impl_->readReport = {};
    auto bank = impl_->author.responses.find(id);
    if (bank != impl_->author.responses.end())
        bank->second.reset();
    impl_->author.bindPlayback(id <= 20 && id != 18 ? id : -1, &impl_->scene.preset);
    ++impl_->sceneGeneration;
    impl_->studyClock = {};
    impl_->studyMixerValues = {};
    impl_->studyOutputValues = {};
    if (id >= 100) {
        std::array<ShapeSettings, 3> originals;
        for (int i = 0; i < 3; i++) {
            originals[i] = impl_->scene.preset.nodes[i + 1].saved;
            auto mesh = generateShapes(originals[i], impl_->palette(i));
            float left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
            for (auto &v : mesh) {
                left = std::min(left, v.position.x);
                right = std::max(right, v.position.x);
                top = std::min(top, v.position.y);
                bottom = std::max(bottom, v.position.y);
            }
            impl_->centers[i] = {(left + right) * .5f, (top + bottom) * .5f, 0};
            impl_->fit[i] = 1 / std::max(1.f, std::max(right - left, bottom - top));
        }
        impl_->geometry.reset(originals);
        impl_->geometry.enabled = id == 104;
    }
    impl_->params = Parameters{};
    if (id >= 100)
        impl_->params.exposure = .65f;
    impl_->params.beam.colour = 1;
    impl_->accumulator = 0;
    impl_->beam.reset();
    impl_->renderedKey = ~uint64_t(0);
    impl_->clearPending = true;
}
bool Runtime::validateShaderLibrary(std::ostream &log) {
    bool passed = true;
    for (auto &entry : std::filesystem::directory_iterator(impl_->data / "gles/shaders")) {
        if (entry.path().extension() != ".frag")
            continue;
        try {
            auto p = program(flatVS, impl_->read(entry.path()));
            glDeleteProgram(p);
            log << entry.path().filename().string() << ": compiled\n";
        } catch (const std::exception &e) {
            passed = false;
            log << entry.path().filename().string() << ": FAILED " << e.what() << '\n';
        }
    }
    return passed;
}
} // namespace neon::effects

namespace neon::effects {
pc::AuthoringModel &Runtime::authoring() {
    impl_->editorAuthoring = true;
    return impl_->author;
}
void Runtime::writeScene(std::ostream &out, double rate) const {
    if (!std::isfinite(rate) || rate < 0 || rate > 4)
        throw std::runtime_error("Invalid playback rate");
    screenLayout(4, 3, 0, impl_->params.framing);
    out.precision(9);
    out << "NEONGLES 8\n"
        << impl_->selected << ' ' << impl_->params.exposure << ' ' << rate << ' '
        << impl_->params.renderScale << '\n'
        << int(impl_->params.framing.mode) << ' ' << impl_->params.framing.referenceAspect << '\n';
    impl_->author.writeEdits(out);
    pc::writeGeometry(out, impl_->geometry);
    impl_->components.write(out);
    writeResponseBanks(out, impl_->author.responses);
    writeReadBanks(out, impl_->readBanks);
    vectors::writeObjects(out, impl_->objects);
    if (!out)
        throw std::runtime_error("Could not write GLES scene");
}
double Runtime::readScene(std::istream &in) {
    std::string magic;
    int version, id;
    float gain, tempo, scale;
    if (!(in >> magic >> version >> id >> gain >> tempo >> scale) || magic != "NEONGLES" ||
        (version < 1 || version > 8) || !std::isfinite(gain) || gain < 0 || gain > 2 ||
        !std::isfinite(tempo) || tempo < 0 || tempo > 4 || !std::isfinite(scale) || scale < .25f ||
        scale > 1)
        throw std::runtime_error("Invalid GLES scene");
    ScreenFraming framing{Framing::LegacyStretch, 4.f / 3};
    if (version >= 5) {
        int mode;
        if (!(in >> mode >> framing.referenceAspect))
            throw std::runtime_error("Invalid screen framing");
        framing.mode = Framing(mode);
        screenLayout(4, 3, 0, framing);
    }
    auto pending = std::make_unique<pc::AuthoringModel>(impl_->data, impl_->reader);
    pending->readEdits(in, id);
    auto geometry = pc::readGeometry(in);
    auto components = version >= 2 ? ComponentScene::read(in) : ComponentScene{};
    auto responses = version >= 3 ? readResponseBanks(in) : std::map<int, ResponseBank>{};
    auto reads = version >= 4 ? readReadBanks(in) : std::map<int, ReadBindings>{};
    auto objects = vectors::readObjects(in);
    in >> std::ws;
    if (!in.eof())
        throw std::runtime_error("Unexpected scene data");
    struct ParseScope {
        int &depth;
        bool root;
        ParseScope(int &d, int &remaining) : depth(d), root(d == 0) {
            if (root)
                remaining = 4;
            if (++depth > 4) {
                --depth;
                throw std::runtime_error("Saved layer nesting exceeds three levels");
            }
        }
        ~ParseScope() {
            --depth;
        }
    };
    static thread_local int depth = 0, remaining = 4;
    ParseScope scope(depth, remaining);
    for (auto &c : components.items)
        if (!c.vectorDocument.empty()) {
            std::istringstream vectors(c.vectorDocument);
            vectors::readObjects(vectors);
        }
    for (auto &c : components.items)
        if (c.kind == ComponentKind::Layer) {
            if (--remaining < 0)
                throw std::runtime_error("Scene exceeds four embedded layer sources");
            Runtime child(impl_->data, impl_->reader);
            std::istringstream payload(c.layerScene);
            child.readScene(payload);
        }
    selectPreset(id); // validates and constructs before replacing active state
    impl_->author.edits = std::move(pending->edits);
    impl_->author.sceneNames = std::move(pending->sceneNames);
    impl_->author.responses = std::move(responses);
    impl_->readBanks = std::move(reads);
    impl_->readReport = {};
    impl_->editorAuthoring = true;
    impl_->components = std::move(components);
    impl_->componentDocuments[id == 21 ? 21 : 18] = impl_->components;
    impl_->componentTunnels.clear();
    for (auto &t : impl_->componentTargets)
        t.second.key = ~uint64_t(0);
    impl_->geometry = std::move(geometry);
    setVectorObjects(std::move(objects));
    impl_->params.exposure = gain;
    impl_->params.renderScale = scale;
    impl_->params.framing = framing;
    return tempo;
}
} // namespace neon::effects

namespace neon::effects {
ComponentScene &Runtime::components() {
    return impl_->components;
}
ResponseBank &Runtime::responses() {
    return impl_->author.responses[impl_->selected];
}
bool Runtime::triggerEvent(const std::string &event, float strength) {
    return responses().trigger(event, time(), strength);
}
void Runtime::setInput(const std::string &name, float value) {
    responses().setInput(name, value);
}
void Runtime::clearInput(const std::string &name) {
    responses().clearInput(name);
}
ReadBindings &Runtime::readBindings() {
    return impl_->readBanks[impl_->selected];
}
ReadReport Runtime::observe(const ReadSnapshot &snapshot) {
    impl_->readReport = readBindings().observe(snapshot, responses(), time());
    return impl_->readReport;
}
ReadReport Runtime::inspect(const ReadSource &source) {
    return observe(source.read());
}
const ReadReport &Runtime::lastReadReport() const {
    return impl_->readReport;
}
void Runtime::resetReads() {
    readBindings().reset();
    responses().reset();
    impl_->readReport = {};
}
std::vector<ResponseParameter> Runtime::responseParameters() {
    auto &i = *impl_;
    std::vector<ResponseParameter> result = {
        {"runtime.exposure", "Overall exposure", 0, 2, i.params.exposure},
        {"runtime.yaw", "Camera yaw", -3.141593f, 3.141593f, i.params.yaw},
        {"runtime.pitch", "Camera pitch", -3.141593f, 3.141593f, i.params.pitch}};
    if (i.selected == 18 || i.selected == 21) {
        for (const auto &c : i.components.items)
            for (const auto &f : editableComponentFields(c))
                result.push_back(
                    {"component." + std::to_string(c.id) + "." + std::to_string(int(f.field)),
                     c.name + " / " + f.label, f.minimum, f.maximum, c.values[f.field]});
    } else if (i.selected >= 2 && i.selected <= 20) {
        for (const auto &e : i.author.controls(i.selected))
            if (e.applicable && e.choices.empty())
                result.push_back({e.key, e.label, e.minimum, e.maximum, e.current});
    }
    return result;
}
} // namespace neon::effects
