#include "widget_renderer.h"
#include "vector/vector_scene.h"
#include <glad/gl.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace neon::widgets {
namespace {
void need(bool v, const std::string &s) {
    if (!v)
        throw std::runtime_error(s);
}
std::string read(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    need(bool(in), "Cannot read widget asset: " + path.u8string());
    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    need(size >= 0 && size <= 16 * 1024 * 1024, "Widget asset exceeds 16 MiB");
    in.seekg(0);
    return {std::istreambuf_iterator<char>(in), {}};
}
std::string resource(const ResourceReader &reader, const std::string &name) {
    auto path = std::filesystem::u8path(name);
    need(!path.is_absolute() && !path.has_root_name(), "Widget asset must be bundle-relative");
    for (const auto &part : path)
        need(part != "..", "Widget asset cannot leave its bundle");
    std::vector<uint8_t> bytes;
    need(reader && reader(name, bytes), "Cannot read widget asset: " + name);
    need(bytes.size() <= 16 * 1024 * 1024, "Widget asset exceeds 16 MiB");
    return std::string(bytes.begin(), bytes.end());
}
GLuint shader(GLenum type, const char *source) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]{};
        glGetShaderInfoLog(s, 4096, nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(log);
    }
    return s;
}
GLuint program(const char *v, const char *f) {
    GLuint vs = shader(GL_VERTEX_SHADER, v), ps = 0, p = 0;
    try {
        ps = shader(GL_FRAGMENT_SHADER, f);
        p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, ps);
        glLinkProgram(p);
        GLint ok;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            glGetProgramInfoLog(p, 4096, nullptr, log);
            throw std::runtime_error(log);
        }
    } catch (...) {
        if (p)
            glDeleteProgram(p);
        if (ps)
            glDeleteShader(ps);
        glDeleteShader(vs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(ps);
    return p;
}
struct State {
    GLint program, vao, buffer, draw, read, active, texture, sampler, unpackBuffer, unpack[4],
        viewport[4], scissor[4], srcRGB, dstRGB, srcA, dstA, eqRGB, eqA;
    GLboolean mask[4], blend, depth, cull, stencil, discard, coverage, alphaCoverage, dither;
    State() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        GLenum keys[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS,
                         GL_UNPACK_SKIP_ROWS};
        for (int i = 0; i < 4; ++i)
            glGetIntegerv(keys[i], &unpack[i]);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        discard = glIsEnabled(GL_RASTERIZER_DISCARD);
        scissorOn = glIsEnabled(GL_SCISSOR_TEST);
        coverage = glIsEnabled(GL_SAMPLE_COVERAGE);
        alphaCoverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
        dither = glIsEnabled(GL_DITHER);
    }
    GLboolean scissorOn;
    static void enable(GLenum key, GLboolean v) {
        if (v)
            glEnable(key);
        else
            glDisable(key);
    }
    ~State() {
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(0, sampler);
        glActiveTexture(active);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        GLenum keys[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS,
                         GL_UNPACK_SKIP_ROWS};
        for (int i = 0; i < 4; ++i)
            glPixelStorei(keys[i], unpack[i]);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
        glBlendEquationSeparate(eqRGB, eqA);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        enable(GL_BLEND, blend);
        enable(GL_DEPTH_TEST, depth);
        enable(GL_CULL_FACE, cull);
        enable(GL_STENCIL_TEST, stencil);
        enable(GL_RASTERIZER_DISCARD, discard);
        enable(GL_SCISSOR_TEST, scissorOn);
        enable(GL_SAMPLE_COVERAGE, coverage);
        enable(GL_SAMPLE_ALPHA_TO_COVERAGE, alphaCoverage);
        enable(GL_DITHER, dither);
    }
};
struct Vertex {
    float x, y, u, v, r, g, b, a;
};
Rect intersection(Rect a, Rect b) {
    double x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0., std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0., std::min(a.y + a.height, b.y + b.height) - y)};
}
bool lineClip(vectors::Point &a, vectors::Point &b, Rect r) {
    double dx = b.x - a.x, dy = b.y - a.y, t0 = 0, t1 = 1;
    double p[] = {-dx, dx, -dy, dy},
           q[] = {a.x - r.x, r.x + r.width - a.x, a.y - r.y, r.y + r.height - a.y};
    for (int i = 0; i < 4; ++i) {
        if (std::abs(p[i]) < 1e-12) {
            if (q[i] < 0)
                return false;
        } else {
            double t = q[i] / p[i];
            if (p[i] < 0)
                t0 = std::max(t0, t);
            else
                t1 = std::min(t1, t);
            if (t0 > t1)
                return false;
        }
    }
    b = {float(a.x + dx * t1), float(a.y + dy * t1)};
    a = {float(a.x + dx * t0), float(a.y + dy * t0)};
    return true;
}
const char *spriteVS = R"(#version 300 es
precision highp float;layout(location=0)in vec2 position;layout(location=1)in vec2 uv;layout(location=2)in vec4 tint;uniform vec2 viewport;out vec2 t;out vec4 c;void main(){gl_Position=vec4(position/viewport*vec2(2,-2)+vec2(-1,1),0,1);t=uv;c=tint;})";
const char *spritePS = R"(#version 300 es
precision highp float;
uniform sampler2D image;
uniform bool textured;
uniform bool emissive;
uniform float radialFill;
uniform vec4 radialTrack;
in vec2 t;
in vec4 c;
out vec4 colour;
void main(){
    vec4 p=textured?texture(image,t):vec4(1);
    if(emissive){colour=vec4(p.rgb*c.rgb*c.a,0);return;}
    if(radialFill>=0.0){
        vec2 point=t-vec2(0.5);
        float turn=fract(atan(point.x,-point.y)/6.28318530718+1.0);
        float edge=clamp(fwidth(turn)*0.5,0.00001,0.02);
        float lit=radialFill>=1.0?1.0:radialFill<=0.0?0.0:
            1.0-smoothstep(radialFill-edge,radialFill+edge,turn);
        colour=vec4(mix(radialTrack.rgb,c.rgb,lit),mix(radialTrack.a,1.0,lit)*c.a*p.a);
    }else{
        colour=p*c;
    }
    colour.rgb*=colour.a;
})";
} // namespace
namespace {
const char *fixedBeamVS = R"WIDGET(#version 300 es
precision highp float;
layout(location=0) in vec2 position;
layout(location=1) in vec2 localPosition;
layout(location=2) in vec2 beam;
layout(location=3) in vec4 tint;
uniform vec2 viewportSize;
out vec2 strokeLocal;out vec2 strokeBeam;out vec4 strokeTint;
void main(){gl_Position=vec4(position*vec2(2.0,-2.0)/viewportSize+vec2(-1.0,1.0),0.0,1.0);strokeLocal=localPosition;strokeBeam=beam;strokeTint=tint;}
)WIDGET";
const char *fixedBeamPS = R"WIDGET(#version 300 es
precision highp float;
in vec2 strokeLocal;in vec2 strokeBeam;in vec4 strokeTint;
uniform vec4 beamStyle; // intensity, halo, spread, white-hot
out vec4 colour;
void main(){
 float endpoint=max(max(-strokeLocal.x,strokeLocal.x-strokeBeam.x),0.0);
 float d2=endpoint*endpoint+strokeLocal.y*strokeLocal.y;
 float w=strokeBeam.y,aa2=w*w+0.46209812;
 float core=exp2(-4.0*d2/aa2)*w*inversesqrt(aa2);
 float halo2=w*w*beamStyle.z*beamStyle.z;
 float nearHalo=0.0,farHalo=0.0;if(beamStyle.y>0.0){nearHalo=exp2(-4.0*d2/halo2);farHalo=exp2(-4.0*d2/(halo2*5.29));float reach=min(64.0,max(2.0,w*beamStyle.z*2.0));float fade=clamp((reach*reach-d2)/(reach*reach*0.4375),0.0,1.0);fade=fade*fade*(3.0-2.0*fade);nearHalo*=fade;farHalo*=fade;}
 vec3 hot=mix(strokeTint.rgb,vec3(1.0),clamp(beamStyle.w,0.0,1.0)*core);
 vec3 light=(hot*core+strokeTint.rgb*beamStyle.y*(0.22*nearHalo+0.018*farHalo))*strokeTint.a*beamStyle.x;
 colour=vec4(light,clamp(max(light.r,max(light.g,light.b)),0.0,1.0));
}
)WIDGET";
} // namespace
struct Renderer::Impl {
    Value manifest;
    ResourceReader reader;
    vectors::FontRegistry fonts;
    RenderStatistics stats;
    GLuint sprite = 0, beam = 0, vao = 0, vbo = 0;
    GLint spriteRadialFill = -1, spriteRadialTrack = -1;
    std::string beamVS = fixedBeamVS, beamPS = fixedBeamPS;
    struct Image {
        GLuint texture = 0;
        int width = 0, height = 0;
    };
    std::map<std::string, Image> images;
    std::map<std::pair<int, int>, GLuint> radialMasks;
    std::map<std::string, std::vector<vectors::Stroke>> vectorsCache, textCache;
    vectors::BeamMesh mesh;
    std::vector<vectors::Stroke> batch;
    Rect batchClip{};
    bool batchBezel = false;
    struct RasterTile {
        Rect bounds, uv;
        GLuint texture = 0;
    };
    int atlasSize = 1024;
    GLuint atlas = 0, atlasFbo = 0;
    std::vector<GLuint> atlasPages;
    int atlasX = 0, atlasY = 0, atlasRowHeight = 0;
    int cacheW = 0, cacheH = 0;
    std::map<std::string, RasterTile> tiles;
    std::set<std::string> warmedFonts;
    bool cacheDraw = false;
    bool warmOnly = false;
    std::vector<Vertex> quads;
    GLuint quadTexture = 0;
    bool quadEmissive = false;
    Rect quadClip{};
    bool quadBezel = false;
    RenderTarget target;
    int nativeW = 0, nativeH = 0;
    void forgetAtlas() {
        atlas = atlasFbo = 0;
        atlasPages.clear();
        atlasX = atlasY = atlasRowHeight = 0;
        tiles.clear();
        warmedFonts.clear();
        quads.clear();
    }
    void releaseAtlas() {
        if (!atlasPages.empty())
            glDeleteTextures(GLsizei(atlasPages.size()), atlasPages.data());
        if (atlasFbo)
            glDeleteFramebuffers(1, &atlasFbo);
        forgetAtlas();
    }
    void releaseRadials() {
        for (const auto &entry : radialMasks)
            glDeleteTextures(1, &entry.second);
        radialMasks.clear();
    }
    void release() {
        releaseAtlas();
        releaseRadials();
        for (auto &p : images)
            if (p.second.texture)
                glDeleteTextures(1, &p.second.texture);
        images.clear();
        if (sprite)
            glDeleteProgram(sprite);
        if (beam)
            glDeleteProgram(beam);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (vbo)
            glDeleteBuffers(1, &vbo);
        sprite = beam = vao = vbo = 0;
    }
    Image &image(const std::string &id) {
        auto it = images.find(id);
        if (it != images.end())
            return it->second;
        need(images.size() < 32, "Widget image cache budget exceeded");
        auto asset = manifest.at("assets").at(id);
        auto file = asset.string(id);
        if (!asset.at("path").missing())
            file = asset.at("path").string();
        auto bytes = resource(reader, file);
        int w, h, channels;
        int iw = 0, ih = 0;
        if (!stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                                   int(bytes.size()), &iw, &ih, &channels) ||
            iw < 1 || ih < 1 || iw > 2048 || ih > 2048)
            throw std::runtime_error("Widget image invalid or larger than 2048: " + file);
        auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                                            int(bytes.size()), &w, &h, &channels, 4);
        need(pixels != nullptr, "Cannot decode widget image: " + file);
        size_t textureBytes = size_t(w) * h * 4;
        for (auto &entry : images)
            textureBytes += size_t(entry.second.width) * entry.second.height * 4;
        if (textureBytes > 32 * 1024 * 1024) {
            stbi_image_free(pixels);
            throw std::runtime_error("Widget texture cache exceeds 32 MiB");
        }
        Image result;
        result.width = w;
        result.height = h;
        glGenTextures(1, &result.texture);
        glBindTexture(GL_TEXTURE_2D, result.texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stats.uploads += size_t(w) * h * 4;
        ++stats.textureUploads;
        return images.emplace(id, result).first->second;
    }
    std::array<double, 2> turn(double x, double y) {
        double a = x / nativeW, b = y / nativeH;
        switch ((target.turns % 4 + 4) % 4) {
        case 1:
            x = 1 - b;
            y = a;
            break;
        case 2:
            x = 1 - a;
            y = 1 - b;
            break;
        case 3:
            x = b;
            y = 1 - a;
            break;
        default:
            x = a;
            y = b;
        }
        return {x * target.viewport.width, y * target.viewport.height};
    }
    Rect displayClip(Rect clip) {
        auto a = turn(clip.x, clip.y), b = turn(clip.x + clip.width, clip.y + clip.height);
        return {target.viewport.x + std::min(a[0], b[0]), target.viewport.y + std::min(a[1], b[1]),
                std::abs(a[0] - b[0]), std::abs(a[1] - b[1])};
    }
    template <class F> void paint(Rect clip, bool bezel, F draw) {
        clip = intersection(displayClip(clip), target.viewport);
        auto scissor = [&](Rect r) {
            if (r.width <= 0 || r.height <= 0)
                return;
            int x = int(std::ceil(r.x)), y = int(std::ceil(r.y)),
                right = int(std::floor(r.x + r.width)), bottom = int(std::floor(r.y + r.height));
            if (right <= x || bottom <= y)
                return;
            glScissor(x, target.height - bottom, right - x, bottom - y);
            draw();
            ++stats.draws;
        };
        if (!bezel || target.gameplay.width <= 0) {
            scissor(clip);
            return;
        }
        auto hole = intersection(clip, target.gameplay);
        if (hole.width <= 0 || hole.height <= 0) {
            scissor(clip);
            return;
        }
        scissor({clip.x, clip.y, clip.width, hole.y - clip.y});
        scissor({clip.x, hole.y + hole.height, clip.width,
                 clip.y + clip.height - hole.y - hole.height});
        scissor({clip.x, hole.y, hole.x - clip.x, hole.height});
        scissor(
            {hole.x + hole.width, hole.y, clip.x + clip.width - hole.x - hole.width, hole.height});
    }
    void upload(const void *p, size_t size) {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, size, p, GL_STREAM_DRAW);
        stats.uploads += size;
    }
    void flush(float widthScale = 1) {
        if (batch.empty())
            return;
        glUseProgram(beam);
        glUniform2f(glGetUniformLocation(beam, "viewportSize"), float(target.viewport.width),
                    float(target.viewport.height));
        glUniform4f(glGetUniformLocation(beam, "beamStyle"), 1.25f, .25f, 3, .1f);
        auto strokes = batch;
        for (auto &s : strokes)
            for (auto p : {&s.a, &s.b}) {
                p->x *= 640.f / nativeW;
                p->y *= 480.f / nativeH;
            }
        vectors::BeamStyle style;
        style.width = 1.4f * widthScale;
        style.intensity = 1.25f;
        style.halo = .25f;
        style.spread = 3;
        style.persistenceMs = 0;
        style.colour = 1;
        style.whiteHot = .1f;
        auto vertices = mesh.build(strokes, 0, nativeW, nativeH, style);
        for (auto &v : vertices) {
            auto p = turn(v.x, v.y);
            v.x = float(p[0]);
            v.y = float(p[1]);
        }
        upload(vertices.data(), vertices.size() * sizeof(vectors::Vertex));
        int sizes[] = {2, 2, 2, 4}, offsets[] = {0, 8, 16, 24};
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(vectors::Vertex),
                                  (void *)(intptr_t)offsets[i]);
        }
        glBlendFunc(GL_ONE, GL_ONE);
        paint(batchClip, batchBezel, [&] {
            glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
            stats.triangles += vertices.size() / 3;
        });
        stats.strokes += batch.size();
        batch.clear();
    }
    Rect fillRect(const Draw &d) {
        auto r = d.bounds;
        if (d.fillDirection == "left") {
            r.x += r.width * (1 - d.fill);
            r.width *= d.fill;
        } else if (d.fillDirection == "up") {
            r.y += r.height * (1 - d.fill);
            r.height *= d.fill;
        } else if (d.fillDirection == "down")
            r.height *= d.fill;
        else
            r.width *= d.fill;
        return r;
    }
    void stroke(const Draw &d, vectors::Stroke s) {
        if (!lineClip(s.a, s.b, fillRect(d)))
            return;
        auto a = d.transform.point(s.a.x, s.a.y), b = d.transform.point(s.b.x, s.b.y);
        s.a = {float(a[0]), float(a[1])};
        s.b = {float(b[0]), float(b[1])};
        if (!lineClip(s.a, s.b, d.clip))
            return;
        s.colour.r *= float(d.color[0]);
        s.colour.g *= float(d.color[1]);
        s.colour.b *= float(d.color[2]);
        s.energy *= float(d.color[3]);
        s.width *= float(d.lineWidth * std::hypot(d.transform.a, d.transform.b));
        if (stats.strokes + batch.size() >= 2048 || batch.size() >= 512) {
            ++stats.dropped;
            return;
        }
        batch.push_back(s);
    }
    void quad(const Draw &d, Rect local, Rect uv, GLuint texture, bool emissive = false,
              const Draw *radial = nullptr) {
        if (warmOnly)
            return;
        auto cropped = intersection(local, fillRect(d));
        if (cropped.width <= 0 || cropped.height <= 0)
            return;
        uv = {uv.x + (cropped.x - local.x) / local.width * uv.width,
              uv.y + (cropped.y - local.y) / local.height * uv.height,
              uv.width * cropped.width / local.width, uv.height * cropped.height / local.height};
        local = cropped;
        std::array<Vertex, 6> vertices;
        int corner[] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; ++i) {
            int c = corner[i];
            double x = c == 1 || c == 2 ? 1 : 0, y = c >= 2 ? 1 : 0;
            auto p = d.transform.point(local.x + x * local.width, local.y + y * local.height);
            auto q = turn(p[0], p[1]);
            vertices[i] = {float(q[0]),
                           float(q[1]),
                           float(uv.x + x * uv.width),
                           float(uv.y + y * uv.height),
                           float(d.color[0]),
                           float(d.color[1]),
                           float(d.color[2]),
                           float(d.color[3])};
        }
        if (!cacheDraw || radial) {
            flushQuads();
            drawQuads(vertices.data(), vertices.size(), d.clip, d.bezelOnly, texture, emissive,
                      radial);
            return;
        }
        if (!quads.empty() && (quadTexture != texture || quadEmissive != emissive ||
                              quadClip.x != d.clip.x || quadClip.y != d.clip.y ||
                              quadClip.width != d.clip.width || quadClip.height != d.clip.height ||
                              quadBezel != d.bezelOnly))
            flushQuads();
        quadTexture = texture;
        quadEmissive = emissive;
        quadClip = d.clip;
        quadBezel = d.bezelOnly;
        quads.insert(quads.end(), vertices.begin(), vertices.end());
    }
    void flushQuads() {
        if (quads.empty())
            return;
        drawQuads(quads.data(), quads.size(), quadClip, quadBezel, quadTexture, quadEmissive);
        quads.clear();
    }
    void drawQuads(const Vertex *vertices, size_t count, Rect clip, bool bezel, GLuint texture,
                   bool emissive, const Draw *radial = nullptr) {
        glUseProgram(sprite);
        glUniform2f(glGetUniformLocation(sprite, "viewport"), float(target.viewport.width),
                    float(target.viewport.height));
        glUniform1i(glGetUniformLocation(sprite, "image"), 0);
        glUniform1i(glGetUniformLocation(sprite, "textured"), texture != 0);
        glUniform1i(glGetUniformLocation(sprite, "emissive"), emissive);
        glUniform1f(spriteRadialFill, radial ? float(radial->fill) : -1.f);
        if (radial) {
            const auto &track = radial->trackColor;
            glUniform4f(spriteRadialTrack, float(track[0]), float(track[1]), float(track[2]),
                        float(track[3]));
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(0, 0);
        upload(vertices, count * sizeof(Vertex));
        for (int i = 0; i < 3; ++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, i == 2 ? 4 : 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (void *)(intptr_t)(i == 0   ? 0
                                                     : i == 1 ? 8
                                                              : 16));
        }
        glDisableVertexAttribArray(3);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        paint(clip, bezel, [&] {
            glDrawArrays(GL_TRIANGLES, 0, GLsizei(count));
            stats.triangles += count / 3;
        });
    }
    GLuint radialMask(const Draw &d) {
        need(d.segments >= 1 && d.segments <= 64, "Invalid widget radial segment count");
        const auto pixelSize = std::min(d.bounds.width * std::hypot(d.transform.a, d.transform.b),
                                        d.bounds.height * std::hypot(d.transform.c, d.transform.d));
        const int size = int(std::clamp(std::round(pixelSize), 8., 512.));
        const auto key = std::make_pair(size, d.segments);
        const auto found = radialMasks.find(key);
        if (found != radialMasks.end())
            return found->second;
        need(radialMasks.size() < 16, "Widget radial mask budget exceeded");
        constexpr double tau = 6.283185307179586;
        const double step = tau / d.segments;
        const double halfWidth = std::min(8. / 132 * 16 / d.segments, .22);
        std::vector<uint8_t> pixels(size_t(size) * size * 4, 0);
        const auto covered = [&](double x, double y) {
            const double radiusSq = x * x + y * y;
            if (radiusSq < (47. / 132) * (47. / 132) || radiusSq > .24)
                return false;
            const double angle = std::atan2(x, -y);
            const double offset = std::remainder(angle - step / 2, step);
            const double r = std::sqrt(radiusSq);
            const double radial = r * std::cos(offset);
            return radial >= 47. / 132 && radial <= 54. / 132 &&
                   std::abs(r * std::sin(offset)) <= halfWidth;
        };
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                int coverage = 0;
                for (double sy : {.125, .375, .625, .875})
                    for (double sx : {.125, .375, .625, .875})
                        coverage += covered((x + sx) / size - .5, (y + sy) / size - .5);
                pixels[(size_t(y) * size + x) * 4 + 3] = uint8_t((coverage * 255 + 8) / 16);
            }
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        radialMasks.emplace(key, texture);
        ++stats.textureUploads;
        stats.uploads += pixels.size();
        return texture;
    }
    void radialDraw(const Draw &d) {
        const auto texture = radialMask(d);
        Draw quadDraw = d;
        quadDraw.fill = 1;
        // Keep this small draw in its own bezel strip.
        auto a = d.transform.point(0, 0);
        double left = a[0], right = a[0], top = a[1], bottom = a[1];
        for (double x : {0., d.bounds.width})
            for (double y : {0., d.bounds.height}) {
                auto p = d.transform.point(x, y);
                left = std::min(left, p[0]);
                right = std::max(right, p[0]);
                top = std::min(top, p[1]);
                bottom = std::max(bottom, p[1]);
            }
        quadDraw.clip = intersection(d.clip, {std::floor(left), std::floor(top),
                                           std::ceil(right) - std::floor(left),
                                           std::ceil(bottom) - std::floor(top)});
        quad(quadDraw, d.bounds, {0, 0, 1, 1}, texture, false, &d);
    }
    void addAtlasPage() {
        need(atlasPages.size() < 4, "Widget raster cache page budget exceeded");
        glGenTextures(1, &atlas);
        atlasPages.push_back(atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasSize, atlasSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!atlasFbo)
            glGenFramebuffers(1, &atlasFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, atlasFbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atlas, 0);
        need(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
             "Widget raster atlas framebuffer unavailable");
    }
    RasterTile bake(const Draw &draw, const std::vector<vectors::Stroke> &strokes) {
        if (strokes.empty())
            return {};
        Draw local = draw;
        local.transform.x = local.transform.y = 0;
        double left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
        for (const auto &stroke : strokes) {
            for (auto p : {stroke.a, stroke.b}) {
                auto q = local.transform.point(p.x, p.y);
                left = std::min(left, q[0]);
                right = std::max(right, q[0]);
                top = std::min(top, q[1]);
                bottom = std::max(bottom, q[1]);
            }
        }
        const double beamWidth = 1.4 * draw.lineWidth * std::hypot(draw.transform.a, draw.transform.b) *
                                 nativeH / 720;
        const double padding = std::ceil(std::clamp(beamWidth * 6, 2., 64.)) + 1;
        left = std::floor(left - padding);
        top = std::floor(top - padding);
        const int w = int(std::ceil(right + padding) - left);
        const int h = int(std::ceil(bottom + padding) - top);
        need(w <= atlasSize && h <= atlasSize, "Widget raster tile too large");
        if (atlasX + w > atlasSize) {
            atlasY += atlasRowHeight;
            atlasX = atlasRowHeight = 0;
        }
        flushQuads();
        if (!atlas || atlasY + h > atlasSize) {
            addAtlasPage();
            atlasX = atlasY = atlasRowHeight = 0;
        }
        const auto previousTarget = target;
        const auto previousW = nativeW, previousH = nativeH;
        const int y = atlasSize - atlasY - h;
        target = {atlasFbo, atlasSize, atlasSize, {double(atlasX), double(atlasY), double(w), double(h)}};
        nativeW = w;
        nativeH = h;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, atlasFbo);
        glViewport(atlasX, y, w, h);
        glScissor(atlasX, y, w, h);
        constexpr GLfloat transparent[] = {0, 0, 0, 0};
        glColorMask(1, 1, 1, 1);
        glClearBufferfv(GL_COLOR, 0, transparent);
        glColorMask(1, 1, 1, 0);
        local.transform.x = -left;
        local.transform.y = -top;
        local.clip = batchClip = {0, 0, double(w), double(h)};
        batchBezel = false;
        const auto previousStrokes = stats.strokes;
        stats.strokes = 0;
        for (const auto &line : strokes)
            stroke(local, line);
        // A tile is a crop of the final view. Its height must not make the
        // vector beam thinner than it would be on the gameplay display.
        flush(float(previousH) / h);
        stats.strokes += previousStrokes;
        ++stats.rasterBakes;
        target = previousTarget;
        nativeW = previousW;
        nativeH = previousH;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.framebuffer);
        glViewport(int(target.viewport.x), target.height - int(target.viewport.y + target.viewport.height),
                   int(target.viewport.width), int(target.viewport.height));
        const RasterTile tile{
            {left, top, double(w), double(h)},
            {double(atlasX) / atlasSize, 1 - double(atlasY) / atlasSize,
             double(w) / atlasSize, -double(h) / atlasSize}, atlas};
        atlasX += w;
        atlasRowHeight = std::max(atlasRowHeight, h);
        return tile;
    }
    std::string rasterStyle(const Draw &d) const {
        std::ostringstream key;
        key.precision(12);
        for (double n : {d.transform.a, d.transform.b, d.transform.c, d.transform.d,
                         d.color[0], d.color[1], d.color[2], d.color[3], d.lineWidth})
            key << n << '/';
        return key.str();
    }
    void placeTile(const RasterTile &tile, const Draw &d, double x = 0) {
        if (tile.bounds.width == 0)
            return;
        Draw spriteDraw;
        spriteDraw.bounds = tile.bounds;
        spriteDraw.clip = d.clip;
        spriteDraw.bezelOnly = d.bezelOnly;
        auto origin = d.transform.point(x, 0);
        spriteDraw.transform.x = origin[0];
        spriteDraw.transform.y = origin[1];
        quad(spriteDraw, tile.bounds, tile.uv, tile.texture, true);
    }
    void cachedVector(const Draw &d) {
        std::string key = rasterStyle(d) + d.points.json() + (d.closed ? "closed" : "open");
        key += std::to_string(d.bounds.width) + "/" + std::to_string(d.bounds.height);
        auto found = tiles.find(key);
        if (found == tiles.end()) {
            std::vector<vectors::Stroke> strokes;
            const auto &points = d.points.array();
            for (size_t n = 1; n < points.size() + (d.closed && points.size() > 1); ++n) {
                const auto &a = points[n - 1];
                const auto &b = points[n % points.size()];
                strokes.push_back({{float(a.at(0).number()), float(a.at(1).number())},
                                   {float(b.at(0).number()), float(b.at(1).number())},
                                   {1, 1, 1}, 1, 1});
            }
            found = tiles.emplace(key, bake(d, strokes)).first;
        }
        placeTile(found->second, d);
    }
    void cachedText(const Draw &d) {
        if (d.text.empty())
            return;
        const auto *font = fonts.find(d.font);
        need(font, "Unknown rasterized widget font");
        const auto styleKey = rasterStyle(d) + d.font + "/" + std::to_string(d.fontSize) + "/";
        const auto tileFor = [&](char c) -> const RasterTile & {
            auto key = styleKey + c;
            const auto found = tiles.find(key);
            if (found != tiles.end())
                return found->second;
            vectors::TextStyle style;
            style.height = float(d.fontSize);
            Draw glyph = d;
            glyph.bounds = {-128, -128, 256, 256};
            const auto strokes = vectors::layoutText(*font, std::string_view(&c, 1), style).strokes;
            return tiles.emplace(std::move(key), bake(glyph, strokes)).first->second;
        };
        if (warmedFonts.insert(styleKey).second)
            for (char c : std::string_view("0123456789,-./"))
                tileFor(c);
        const auto advance = [&](unsigned char c) {
            const auto *g = font->find(c);
            return (g ? g->advance : font->defaultAdvance) * d.fontSize / font->capHeight;
        };
        double width = 0;
        for (unsigned char c : d.text)
            width += advance(c);
        double x = d.align == "right" ? d.bounds.width - width :
                   d.align == "center" ? (d.bounds.width - width) / 2 : 0;
        for (unsigned char c : d.text) {
            char glyph = font->uppercase && c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : char(c);
            placeTile(tileFor(glyph), d, x);
            x += advance(c);
        }
    }
    void spriteDraw(const Draw &d) {
        auto &img = image(d.asset);
        Rect uv{0, 0, 1, 1};
        double iw = img.width, ih = img.height;
        if (d.frame.array().size() == 4) {
            iw = d.frame.at(2).number();
            ih = d.frame.at(3).number();
            need(iw > 0 && ih > 0, "Invalid sprite frame size");
            uv = {d.frame.at(0).number() / img.width, d.frame.at(1).number() / img.height,
                  iw / img.width, ih / img.height};
        }
        Rect r = d.bounds;
        if (d.fit != "stretch") {
            double scale = d.fit == "cover" ? std::max(r.width / iw, r.height / ih)
                                            : std::min(r.width / iw, r.height / ih);
            r = {r.x + (r.width - iw * scale) * .5, r.y + (r.height - ih * scale) * .5, iw * scale,
                 ih * scale};
        }
        quad(d, r, uv, img.texture);
    }
    void rasterText(const Draw &d, const Value &font) {
        auto &img = image(font.at("atlas").string());
        double cw = font.at("cell").at(0).number(), ch = font.at("cell").at(1).number();
        need(cw > 0 && ch > 0, "Raster font requires cell metrics");
        int columns = int(font.at("columns").number()), first = int(font.at("first").number(32)),
            count = int(font.at("count").number(96));
        need(columns > 0 && count > 0, "Invalid raster font range");
        const double em = font.at("em").number(ch);
        need(em > 0, "Raster font requires a positive em size");
        const double scale = d.fontSize / em, defaultAdvance = font.at("advance").number(cw);
        const double offsetX = font.at("offset").at(0).number() * scale;
        const double offsetY = font.at("offset").at(1).number() * scale;
        std::vector<uint32_t> codepoints;
        for (size_t p = 0; p < d.text.size();) {
            uint32_t c = (unsigned char)d.text[p++];
            if (c >= 128) {
                int extra = c >= 240 ? 3 : c >= 224 ? 2 : c >= 192 ? 1 : 0;
                uint32_t v = c & ((1 << (6 - extra)) - 1);
                bool ok = extra > 0 && p + extra <= d.text.size();
                for (int k = 0; k < extra && ok; ++k) {
                    unsigned char next = d.text[p + k];
                    if ((next & 0xc0) != 0x80)
                        ok = false;
                    else
                        v = (v << 6) | (next & 63);
                }
                if (ok) {
                    p += extra;
                    c = v;
                } else
                    c = '?';
            }
            codepoints.push_back(c);
        }
        auto glyphAt = [&](size_t p) {
            int glyph = int(codepoints[p]) - first;
            return glyph >= 0 && glyph < count ? glyph : '?' - first;
        };
        const bool kerned = !font.at("kerning").object().empty();
        auto step = [&](size_t p) {
            const int glyph = glyphAt(p);
            double width = glyph >= 0 ? font.at("advances").at(size_t(glyph)).number(defaultAdvance)
                                      : defaultAdvance;
            if (kerned && p + 1 < codepoints.size() && codepoints[p + 1] != '\n')
                width += font.at("kerning")
                             .at(std::to_string(glyph) + "," + std::to_string(glyphAt(p + 1)))
                             .number();
            return width * scale;
        };
        auto aligned = [&](size_t p) {
            double width = 0;
            for (; p < codepoints.size() && codepoints[p] != '\n'; ++p)
                width += step(p);
            return d.align == "center"  ? (d.bounds.width - width) * .5
                   : d.align == "right" ? d.bounds.width - width
                                        : 0.;
        };
        double x = aligned(0), y = 0;
        for (size_t p = 0; p < codepoints.size(); ++p) {
            uint32_t c = codepoints[p];
            if (c == '\n') {
                y += font.at("lineHeight").number(ch) * scale;
                x = aligned(p + 1);
                continue;
            }
            const int glyph = glyphAt(p);
            if (glyph >= 0 && glyph < count)
                quad(d, {x - offsetX, y - offsetY, cw * scale, ch * scale},
                     {double(glyph % columns) * cw / img.width,
                      double(glyph / columns) * ch / img.height, cw / img.width, ch / img.height},
                     img.texture);
            x += step(p);
        }
    }
};
Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() {
    impl_->release();
}
void Renderer::assets(const Value &d, const std::filesystem::path &folder) {
    assets(d, [folder](const std::string &name, std::vector<uint8_t> &out) {
        auto data = read(folder / std::filesystem::u8path(name));
        out.assign(data.begin(), data.end());
        return true;
    });
}
void Renderer::assets(const Value &d, ResourceReader reader) {
    need(bool(reader), "Widget resource reader is required");
    vectors::FontRegistry fonts;
    for (auto &entry : d.at("fonts").object())
        if (entry.second.at("type").string() == "vector") {
            std::istringstream input(resource(reader, entry.second.at("path").string()));
            auto font = vectors::VectorFont::read(input);
            font.id = entry.first;
            fonts.add(std::move(font), true);
        }
    if (impl_->sprite) {
        State guard;
        impl_->release();
    }
    impl_->manifest = d;
    impl_->reader = std::move(reader);
    impl_->fonts = std::move(fonts);
    impl_->textCache.clear();
    impl_->vectorsCache.clear();
}
void Renderer::abandonGraphics() {
    impl_->images.clear();
    impl_->forgetAtlas();
    impl_->radialMasks.clear();
    impl_->sprite = impl_->beam = impl_->vao = impl_->vbo = 0;
    impl_->stats = {};
}

bool Renderer::loadGlesEntryPoints(void *(*resolve)(const char *)) {
    glad_glBindSampler = reinterpret_cast<PFNGLBINDSAMPLERPROC>(resolve("glBindSampler"));
    return glad_glBindSampler != nullptr;
}
void Renderer::render(const Frame &frame, const RenderTarget &target) {
    renderFrame(frame, target, false);
}
void Renderer::renderCached(const Frame &frame, const RenderTarget &target) {
    renderFrame(frame, target, true);
}
void Renderer::warmCached(const Frame &frame, const RenderTarget &target) {
    renderFrame(frame, target, true, true);
}
void Renderer::renderFrame(const Frame &frame, const RenderTarget &target, bool cached, bool warm) {
    auto &i = *impl_;
    i.stats = {};
    if (frame.draws.empty())
        return;
    need(target.width > 0 && target.height > 0 && target.viewport.width > 0 &&
             target.viewport.height > 0,
         "Invalid widget render target");
    need(glad_glBindSampler != nullptr, "Host must load GLES 3 sampler entry points");
    need(target.viewport.x >= 0 && target.viewport.y >= 0 &&
             target.viewport.x + target.viewport.width <= target.width &&
             target.viewport.y + target.viewport.height <= target.height,
         "Widget viewport is outside its render target");
    State state;
    if (!i.sprite) {
        i.sprite = program(spriteVS, spritePS);
        i.spriteRadialFill = glGetUniformLocation(i.sprite, "radialFill");
        i.spriteRadialTrack = glGetUniformLocation(i.sprite, "radialTrack");
        i.beam = program(i.beamVS.c_str(), i.beamPS.c_str());
        glGenVertexArrays(1, &i.vao);
        glGenBuffers(1, &i.vbo);
    }
    i.target = target;
    int turns = (target.turns % 4 + 4) % 4;
    i.nativeW = int(turns % 2 ? target.viewport.height : target.viewport.width);
    i.nativeH = int(turns % 2 ? target.viewport.width : target.viewport.height);
    i.cacheDraw = cached;
    i.warmOnly = warm;
    if (cached && (i.cacheW != i.nativeW || i.cacheH != i.nativeH)) {
        i.releaseAtlas();
        i.releaseRadials();
        i.cacheW = i.nativeW;
        i.cacheH = i.nativeH;
        i.atlasSize = i.nativeH > 900 ? 2048 : 1024;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(int(target.viewport.x),
               target.height - int(target.viewport.y + target.viewport.height),
               int(target.viewport.width), int(target.viewport.height));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_SAMPLE_COVERAGE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_DITHER);
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glBlendEquation(GL_FUNC_ADD);
    glColorMask(1, 1, 1, 0);
    for (auto &d : frame.draws) {
        if (d.kind == "sprite" || d.kind == "rect" || d.kind == "radial" ||
            (d.kind == "text" &&
             i.manifest.at("fonts").at(d.font).at("type").string() == "raster")) {
            i.flush();
            if (d.kind == "sprite")
                i.spriteDraw(d);
            else if (d.kind == "radial")
                i.radialDraw(d);
            else if (d.kind == "rect")
                i.quad(d, d.bounds, {0, 0, 1, 1}, 0);
            else
                i.rasterText(d, i.manifest.at("fonts").at(d.font));
            continue;
        }
        if (cached && (d.kind == "text" || d.asset.empty())) {
            i.flush();
            if (d.kind == "text")
                i.cachedText(d);
            else
                i.cachedVector(d);
            continue;
        }
        i.flushQuads();
        if (!i.batch.empty() &&
            (d.clip.x != i.batchClip.x || d.clip.y != i.batchClip.y ||
             d.clip.width != i.batchClip.width || d.clip.height != i.batchClip.height ||
             d.bezelOnly != i.batchBezel))
            i.flush();
        i.batchClip = d.clip;
        i.batchBezel = d.bezelOnly;
        if (d.kind == "text") {
            auto key = d.font + "\n" + std::to_string(d.fontSize) + "\n" + d.align + "\n" + d.text;
            auto found = i.textCache.find(key);
            if (found == i.textCache.end()) {
                if (i.textCache.size() >= 64)
                    i.textCache.erase(i.textCache.begin());
                vectors::TextStyle style;
                style.font = d.font;
                style.height = float(d.fontSize);
                style.align = d.align == "center"  ? vectors::TextAlign::Center
                              : d.align == "right" ? vectors::TextAlign::Right
                                                   : vectors::TextAlign::Left;
                auto layout = vectors::layoutText(i.fonts, d.text, style, 512);
                found = i.textCache.emplace(key, std::move(layout.strokes)).first;
                ++i.stats.textLayouts;
            }
            double x = d.align == "center"  ? d.bounds.width * .5
                       : d.align == "right" ? d.bounds.width
                                            : 0;
            for (auto s : found->second) {
                s.a.x += float(x);
                s.b.x += float(x);
                i.stroke(d, s);
            }
        } else if (!d.asset.empty()) {
            auto found = i.vectorsCache.find(d.asset);
            if (found == i.vectorsCache.end()) {
                auto name = i.manifest.at("assets").at(d.asset).string(d.asset);
                std::istringstream file(resource(i.reader, name));
                std::vector<vectors::Stroke> strokes;
                vectors::Canvas canvas;
                for (auto &o : vectors::readObjects(file))
                    canvas.object(o);
                found = i.vectorsCache.emplace(d.asset, canvas.strokes()).first;
            }
            for (auto s : found->second) {
                s.a.x *= float(d.bounds.width / 640);
                s.b.x *= float(d.bounds.width / 640);
                s.a.y *= float(d.bounds.height / 480);
                s.b.y *= float(d.bounds.height / 480);
                i.stroke(d, s);
            }
        } else {
            auto &points = d.points.array();
            for (size_t n = 1; n < points.size() + (d.closed ? 1 : 0); ++n) {
                auto &a = points[(n - 1) % points.size()];
                auto &b = points[n % points.size()];
                i.stroke(d, {{float(a.at(0).number()), float(a.at(1).number())},
                             {float(b.at(0).number()), float(b.at(1).number())},
                             {1, 1, 1},
                             1,
                             1});
            }
        }
    }
    i.flush();
    i.flushQuads();
}
void Renderer::release() {
    impl_->release();
}
const RenderStatistics &Renderer::statistics() const {
    return impl_->stats;
}
} // namespace neon::widgets
