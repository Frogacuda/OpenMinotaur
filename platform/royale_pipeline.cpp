#include "royale_pipeline.h"
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include "miniz.h"
#include "stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <map>
namespace mino {
namespace {
enum class Scale { Source, Absolute, Viewport };
struct Spec {
    const char *file;
    bool linear, srgb, floating, mipmap;
    Scale xType, yType;
    float xScale, yScale;
};
const Spec royaleSpecs[] = {
#include "royale_passes.inl"
};
static_assert(sizeof royaleSpecs / sizeof royaleSpecs[0] == 14,
              "This profile implements the supplied 14-pass preset");
const Spec consumerSpecs[] = {
    {"linear.glsl", true, false, false, false, Scale::Source, Scale::Source, 1, 1},
    {"ntsc_module.glsl", true, false, false, false, Scale::Source, Scale::Source, 1, 1},
    {"crt-consumer-1w.glsl", true, false, false, false, Scale::Viewport, Scale::Viewport, 1, 1}};
// RetroArch's parameter UI defaults are authoritative; several upstream static
// fallback macros differ substantially (including a 30-tap vs 3-tap radius).
// Baking the locked configuration also removes composite, curvature and
// vignette branches, without changing the remaining kernels.
std::string lockConsumer(const std::string &source) {
    std::map<std::string, std::string> values;
    std::istringstream lines(source);
    std::string line;
    const std::regex parameter(R"rx(^\s*#pragma parameter\s+(\w+)\s+"[^"]*"\s+([-+.\d]+))rx");
    std::smatch match;
    while (std::getline(lines, line))
        if (std::regex_search(line, match, parameter))
            values[match[1]] = match[2];
    values["u_svideo"] = "1.0";
    values["u_warp"] = "0.0";
    values["u_vignette"] = "0.0";
    lines.clear();
    lines.str(source);
    std::string result;
    const std::regex define(R"(^\s*#define\s+(\w+)\s+.*$)");
    while (std::getline(lines, line)) {
        if (std::regex_match(line, match, define)) {
            auto i = values.find(match[1]);
            if (i != values.end())
                line = "#define " + i->first + " " + i->second;
        }
        result += line + '\n';
    }
    return result;
}
const char *maskNames[] = {"mask_grille_texture_small", "mask_grille_texture_large",
                           "mask_slot_texture_small",   "mask_slot_texture_large",
                           "mask_shadow_texture_small", "mask_shadow_texture_large"};
std::string prepare(const std::string &original, bool es, bool vertex) {
    std::string out = es ? "#version 300 es\nprecision highp float;\nprecision highp "
                           "int;\nprecision highp sampler2D;\n"
                         : "#version 330 core\n";
    if (es)
        out += "vec4 neonTextureLod(sampler2D t,vec2 p,float lod){return "
               "textureLod(t,p,lod);}\nvec4 neonTextureLod(sampler2D t,vec2 p,int lod){return "
               "textureLod(t,p,float(lod));}\n#define textureLod neonTextureLod\n";
    out += vertex ? "#define VERTEX\n" : "#define FRAGMENT\n";
    std::istringstream input(original);
    std::string line;
    int depth = 0;
    bool comment = false, macro = false;
    const std::regex constWord("\\bconst\\b");
    while (std::getline(input, line)) {
        auto first = line.find_first_not_of(" \t");
        auto trimmed = first == std::string::npos ? std::string() : line.substr(first);
        // ES forbids token pasting. Expand the original 33-tap macro without
        // changing its coefficients, sample positions or accumulation order.
        if (es && trimmed.rfind("#define macro_loopz(c)", 0) == 0) {
            while (line.find_last_not_of(" \t\r") != std::string::npos &&
                   line[line.find_last_not_of(" \t\r")] == '\\')
                std::getline(input, line);
            continue;
        }
        if (es && trimmed.rfind("macro_loopz(", 0) == 0) {
            auto n = std::to_string(std::atoi(trimmed.c_str() + 12));
            line = "offset = float(" + n +
                   ") - 1.0; sums = fetch_offset(offset - 32., one_x) + fetch_offset(32. - offset, "
                   "one_x); signal += sums * vec3(luma_filter" +
                   n + ", chroma_filter" + n + ", chroma_filter" + n + ");";
        }
        if (trimmed.rfind("#version", 0) == 0 || trimmed.rfind("#pragma parameter", 0) == 0)
            continue;
        // The converted desktop source erases const, which makes otherwise
        // constant global initializers illegal in GLSL ES. Preserve const there.
        if (es && trimmed.rfind("#define const", 0) == 0)
            continue;
        if (es && trimmed.rfind("#define COMPAT_PRECISION mediump", 0) == 0)
            line = "#define COMPAT_PRECISION highp";
        if (es) {
            const std::string needle = "fmod(frame_count + interlace_bff, modulus)";
            auto at = line.find(needle);
            if (at != std::string::npos)
                line.replace(at, needle.size(),
                             "fmod(float(frame_count) + interlace_bff, modulus)");
        }
        // The preset has no parameter overrides: retain its shipped static
        // defaults, avoiding RetroArch-only uniform initialization. Geometry
        // remains explicitly flat in EVERY pass, not just the final sampler.
        if (trimmed == "#define RUNTIME_SHADER_PARAMS_ENABLE" ||
            trimmed == "#define RUNTIME_SHADER_PARAMS_ENABLE\r")
            continue;
        if (trimmed.find("static const float geom_mode_static =") != std::string::npos)
            line = "    static const float geom_mode_static = 0.0;";
        // A flat full-frame display has no simulated bezel shadow. Bypass the
        // attenuation, rather than setting border_size to zero (its divisor).
        if (trimmed ==
            "const float border_dim_factor = get_border_dim_factor(video_uv, geom_aspect);")
            line = "    const float border_dim_factor = 1.0;";
        // Track only lexical scope, excluding comments and macro bodies. The
        // fixed source bank has balanced scopes in its conditional branches.
        std::string code;
        for (size_t k = 0; k < line.size(); ++k) {
            if (comment) {
                if (k + 1 < line.size() && line[k] == '*' && line[k + 1] == '/') {
                    comment = false;
                    ++k;
                }
                continue;
            }
            if (k + 1 < line.size() && line[k] == '/' && line[k + 1] == '*') {
                comment = true;
                ++k;
                continue;
            }
            if (k + 1 < line.size() && line[k] == '/' && line[k + 1] == '/')
                break;
            code += line[k];
        }
        auto c = code.find_first_not_of(" \t\r");
        const bool directive = macro || (c != std::string::npos && code[c] == '#');
        if (directive) {
            auto last = code.find_last_not_of(" \t\r");
            macro = last != std::string::npos && code[last] == '\\';
            if (es)
                line = std::regex_replace(line, constWord, "");
        } else {
            if (es) {
                // ES requires compile-time const initializers, even for locals.
                // Keep global static defaults constant; local const was erased
                // by the original converted shader and stays mutable here.
                if (depth > 0)
                    line = std::regex_replace(line, constWord, "");
                else if (c != std::string::npos &&
                         code.substr(c).rfind("COMPAT_PRECISION float", 0) == 0 &&
                         code.find('=') != std::string::npos)
                    line = "const " + line;
                if (depth == 0 &&
                    code.find("float bloom_approx_scale_x = OutputSize.x / TextureSize.y;") !=
                        std::string::npos)
                    line = "#define bloom_approx_scale_x (OutputSize.x / TextureSize.y)";
                if (depth == 0 &&
                    code.find("float bloom_approx_scale_x = OutputSize.y / TextureSize.y;") !=
                        std::string::npos)
                    line = "#define bloom_approx_scale_x (OutputSize.y / TextureSize.y)";
            }
            depth += int(std::count(code.begin(), code.end(), '{')) -
                     int(std::count(code.begin(), code.end(), '}'));
        }
        if (es) {
            const std::string needle = ", samples,";
            auto at = line.find(needle);
            if (at != std::string::npos)
                line.replace(at, needle.size(), ", float(samples),");
        }
        if (es)
            for (const auto &pair : {std::pair<std::string, std::string>{
                                         "i * uv_offset_step_y", "float(i) * uv_offset_step_y"},
                                     {"j * uv_offset_step_x", "float(j) * uv_offset_step_x"}}) {
                auto at = line.find(pair.first);
                if (at != std::string::npos)
                    line.replace(at, pair.first.size(), pair.second);
            }
        out += line + '\n';
    }
    return out;
}
int dimension(Scale type, float scale, int source, int viewport) {
    return std::max(1, int(std::lround(scale * (type == Scale::Source     ? source
                                                : type == Scale::Viewport ? viewport
                                                                          : 1))));
}
const char *blitSource = R"(
#if defined(VERTEX)
in vec4 VertexCoord;in vec4 TexCoord;out vec2 uv;uniform int Turn;
void main(){gl_Position=VertexCoord;vec2 p=TexCoord.xy;uv=abs(Turn)==2 ? 1.0-p:Turn>0 ? vec2(1.0-p.y,p.x):Turn<0 ? vec2(p.y,1.0-p.x):p;}
#else
in vec2 uv;out vec4 colour;uniform sampler2D Texture;
void main(){colour=texture(Texture,uv);}
#endif
)";
} // namespace
unsigned RoyalePipeline::program(const std::string &source, bool es, const std::string &label) {
    unsigned stages[2]{};
    for (int i = 0; i < 2; ++i) {
        stages[i] = glCreateShader(i == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        auto text = prepare(source, es, i == 0);
        const char *ptr = text.c_str();
        glShaderSource(stages[i], 1, &ptr, nullptr);
        glCompileShader(stages[i]);
        int ok = 0;
        glGetShaderiv(stages[i], GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[8192]{};
            glGetShaderInfoLog(stages[i], sizeof log, nullptr, log);
            error_ = label + (i == 0 ? " vertex: " : " fragment: ") + log;
            for (auto s : stages)
                if (s)
                    glDeleteShader(s);
            return 0;
        }
    }
    unsigned p = glCreateProgram();
    for (auto s : stages)
        glAttachShader(p, s);
    glBindAttribLocation(p, 0, "VertexCoord");
    glBindAttribLocation(p, 1, "TexCoord");
    glBindAttribLocation(p, 2, "COLOR");
    glLinkProgram(p);
    for (auto s : stages)
        glDeleteShader(s);
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[8192]{};
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        error_ = label + " link: " + log;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
bool RoyalePipeline::initialize(bool es) {
    const auto *specs = profile_ == Profile::Royale ? royaleSpecs : consumerSpecs;
    passCount_ = profile_ == Profile::Royale ? 14 : 3;
    es_ = es;
    std::size_t bytes = 0;
    auto *data = profile_ == Profile::Royale ? royaleArchive(bytes) : consumerArchive(bytes);
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, data, bytes, 0)) {
        error_ = "Embedded CRT archive could not be opened";
        return false;
    }
    auto read = [&](const std::string &name) {
        std::size_t size = 0;
        void *p = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &size, 0);
        if (!p)
            throw std::runtime_error("Missing CRT asset " + name);
        std::string result(static_cast<char *>(p), size);
        mz_free(p);
        return result;
    };
    try {
        blit_ = program(blitSource, es, "tube input");
        if (!blit_)
            throw std::runtime_error(error_);
        turn_ = glGetUniformLocation(blit_, "Turn");
        blitTex_ = glGetUniformLocation(blit_, "Texture");
        for (int i = 0; i < passCount_; ++i) {
            auto &pass = passes_[size_t(i)];
            auto source = read(specs[i].file);
            if (profile_ == Profile::Consumer)
                source = lockConsumer(source);
            pass.program = program(source, es, specs[i].file);
            if (!pass.program)
                throw std::runtime_error(error_);
            auto loc = [&](const char *name) { return glGetUniformLocation(pass.program, name); };
            pass.output = loc("OutputSize");
            pass.input = loc("InputSize");
            pass.texture = loc("TextureSize");
            pass.frame = loc("FrameCount");
            pass.direction = loc("FrameDirection");
            pass.matrix = loc("MVPMatrix");
            int count = 0;
            glGetProgramiv(pass.program, GL_ACTIVE_UNIFORMS, &count);
            for (int u = 0; u < count; ++u) {
                char buffer[256]{};
                int length = 0, size = 0;
                unsigned type = 0;
                glGetActiveUniform(pass.program, unsigned(u), sizeof buffer, &length, &size, &type,
                                   buffer);
                std::string name(buffer);
                int location = loc(buffer);
                auto indexFor = [&](const std::string &stem) {
                    if (stem == "Texture")
                        return -1;
                    if (stem == "OrigTexture" || stem == "OriginalTexture")
                        return -2;
                    if (stem.rfind("PassPrev", 0) == 0)
                        return i - std::atoi(stem.c_str() + 8);
                    for (int m = 0; m < 6; ++m)
                        if (stem == maskNames[m])
                            return 14 + m;
                    throw std::runtime_error(std::string(specs[i].file) + ": unsupported sampler " +
                                             stem);
                };
                if (type == GL_SAMPLER_2D) {
                    int index = indexFor(name);
                    if (index >= i && index < 14)
                        throw std::runtime_error("Forward CRT pass dependency");
                    pass.samplers.push_back({location, index});
                } else if (type == GL_FLOAT_VEC2 && name.rfind("PassPrev", 0) == 0) {
                    auto offset = name.find("Texture");
                    auto inputOffset = name.find("InputSize");
                    if (offset != std::string::npos || inputOffset != std::string::npos) {
                        int index = i - std::atoi(name.c_str() + 8);
                        if (index < 0 || index >= i)
                            throw std::runtime_error("Invalid CRT size dependency");
                        pass.sizes.push_back({location, index});
                    }
                }
            }
            int maxSamplers = 0;
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxSamplers);
            if (int(pass.samplers.size()) > maxSamplers)
                throw std::runtime_error("CRT needs more texture units than this GPU exposes");
            // Linked uniforms identify actual dependencies after static preset
            // branches have been eliminated. Sizes stay fixed until configure.
            pass.constant = pass.frame < 0;
            for (const auto &sampler : pass.samplers) {
                const int index = sampler.index == -1 ? i - 1 : sampler.index;
                if (index < 0 || (index < 14 && !passes_[size_t(index)].constant))
                    pass.constant = false;
            }
        }
        for (int i = 0; i < (profile_ == Profile::Royale ? 6 : 0); ++i) {
            auto file = read(std::string(maskNames[i]) + ".png");
            auto &mask = masks_[size_t(i)];
            auto *pixels =
                stbi_load_from_memory(reinterpret_cast<const unsigned char *>(file.data()),
                                      int(file.size()), &mask.w, &mask.h, nullptr, 4);
            if (!pixels)
                throw std::runtime_error("CRT mask decode failed");
            glGenTextures(1, &mask.texture);
            glBindTexture(GL_TEXTURE_2D, mask.texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mask.w, mask.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels);
            stbi_image_free(pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            i % 2 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            if (i % 2)
                glGenerateMipmap(GL_TEXTURE_2D);
        }
        ready_ = true;
    } catch (const std::exception &e) {
        error_ = e.what();
        mz_zip_reader_end(&zip);
        shutdown();
        return false;
    }
    mz_zip_reader_end(&zip);
    return true;
}
bool RoyalePipeline::target(Target &t, int w, int h, unsigned format) {
    int maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (w > maxSize || h > maxSize) {
        error_ = "CRT intermediate exceeds GPU texture size";
        return false;
    }
    t.w = w;
    t.h = h;
    glGenTextures(1, &t.texture);
    glBindTexture(GL_TEXTURE_2D, t.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, int(format), w, h, 0, GL_RGBA,
                 format == GL_RGBA32F ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        error_ = "CRT needs renderable floating-point and sRGB intermediate targets";
        return false;
    }
    return true;
}
bool RoyalePipeline::configure(bool es, const TubeLayout &layout, Profile profile) {
    if (profile_ != profile) {
        shutdown();
        profile_ = profile;
    }
    if (!ready_ && !initialize(es))
        return false;
    const auto *specs = profile_ == Profile::Royale ? royaleSpecs : consumerSpecs;
    if (layout.width == layout_.width && layout.height == layout_.height &&
        layout.sourceWidth == source_.w && layout.sourceHeight == source_.h && source_.texture) {
        layout_ = layout;
        return true;
    }
    releaseTargets();
    layout_ = layout;
    if (!target(source_, layout.sourceWidth, layout.sourceHeight, GL_RGBA8)) {
        releaseTargets();
        return false;
    }
    int w = source_.w, h = source_.h;
    for (int i = 0; i < passCount_; ++i) {
        auto &spec = specs[i];
        w = dimension(spec.xType, spec.xScale, w, layout.width);
        h = dimension(spec.yType, spec.yScale, h, layout.height);
        if (!target(targets_[size_t(i)], w, h,
                    spec.floating ? GL_RGBA32F
                    : spec.srgb   ? GL_SRGB8_ALPHA8
                                  : GL_RGBA8)) {
            releaseTargets();
            return false;
        }
    }
    error_.clear();
    return true;
}
void RoyalePipeline::draw(unsigned vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glVertexAttrib4f(2, 1, 1, 1, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(8));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
unsigned RoyalePipeline::run(unsigned original, unsigned vbo, const Output *output,
                             bool canonicalRGB8) {
    if (!ready_ || !source_.texture)
        return original;
    const auto *specs = profile_ == Profile::Royale ? royaleSpecs : consumerSpecs;
    const bool alias =
        optimize_ && canonicalRGB8 && profile_ == Profile::Consumer && layout_.inputTurns == 0;
    const Target source = alias ? Target{original, 0, source_.w, source_.h} : source_;
    if (!es_)
        glDisable(GL_FRAMEBUFFER_SRGB);
    if (!alias) {
        glBindFramebuffer(GL_FRAMEBUFFER, source_.fbo);
        glViewport(0, 0, source_.w, source_.h);
        glUseProgram(blit_);
        glUniform1i(turn_, layout_.inputTurns);
        glUniform1i(blitTex_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, original);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        draw(vbo);
    }
    const float identity[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float clockwise[] = {0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < passCount_; ++i) {
        auto &pass = passes_[size_t(i)];
        auto &out = targets_[size_t(i)];
        const auto &input = i ? targets_[size_t(i - 1)] : source;
        if (optimize_ && pass.constant && pass.valid)
            continue;
        const bool final = i == passCount_ - 1 && output;
        glBindFramebuffer(GL_FRAMEBUFFER, final ? output->framebuffer : out.fbo);
        if (final)
            glViewport(output->x, output->y, output->width, output->height);
        else
            glViewport(0, 0, out.w, out.h);
        if (!es_) {
            if (specs[i].srgb)
                glEnable(GL_FRAMEBUFFER_SRGB);
            else
                glDisable(GL_FRAMEBUFFER_SRGB);
        }
        glUseProgram(pass.program);
        glUniformMatrix4fv(pass.matrix, 1, GL_FALSE,
                           final && output->clockwise ? clockwise : identity);
        glUniform2f(pass.output, float(out.w), float(out.h));
        glUniform2f(pass.input, float(input.w), float(input.h));
        glUniform2f(pass.texture, float(input.w), float(input.h));
        glUniform1i(pass.frame, int(i == 0 ? frame_ % 2 : frame_ & 0x7fffffff));
        glUniform1i(pass.direction, 1);
        for (const auto &size : pass.sizes) {
            const auto &t = targets_[size_t(size.index)];
            glUniform2f(size.location, float(t.w), float(t.h));
        }
        // Establish input sampling before binding all aliases (no spare texture
        // unit is assumed, including on GPUs with the minimum sampler count).
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input.texture);
        if (specs[i].mipmap)
            glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        specs[i].mipmap   ? GL_LINEAR_MIPMAP_LINEAR
                        : specs[i].linear ? GL_LINEAR
                                          : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        specs[i].linear ? GL_LINEAR : GL_NEAREST);
        int unit = 0;
        for (const auto &sampler : pass.samplers) {
            const auto &t = sampler.index == -1   ? input
                            : sampler.index == -2 ? source
                            : sampler.index >= 14 ? masks_[size_t(sampler.index - 14)]
                                                  : targets_[size_t(sampler.index)];
            glActiveTexture(GL_TEXTURE0 + unsigned(unit));
            glBindTexture(GL_TEXTURE_2D, t.texture);
            glUniform1i(sampler.location, unit++);
        }
        draw(vbo);
        pass.valid = true;
    }
    if (!es_)
        glDisable(GL_FRAMEBUFFER_SRGB);
    glActiveTexture(GL_TEXTURE0);
    ++frame_;
    return targets_[size_t(passCount_ - 1)].texture;
}
void RoyalePipeline::releaseTargets() {
    for (auto &p : passes_)
        p.valid = false;
    for (auto &t : targets_) {
        if (t.fbo)
            glDeleteFramebuffers(1, &t.fbo);
        if (t.texture)
            glDeleteTextures(1, &t.texture);
        t = {};
    }
    if (source_.fbo)
        glDeleteFramebuffers(1, &source_.fbo);
    if (source_.texture)
        glDeleteTextures(1, &source_.texture);
    source_ = {};
}
void RoyalePipeline::shutdown() {
    releaseTargets();
    for (auto &pass : passes_) {
        if (pass.program)
            glDeleteProgram(pass.program);
        pass = {};
    }
    for (auto &mask : masks_) {
        if (mask.texture)
            glDeleteTextures(1, &mask.texture);
        mask = {};
    }
    if (blit_)
        glDeleteProgram(blit_);
    blit_ = 0;
    ready_ = false;
    frame_ = 0;
}
} // namespace mino
