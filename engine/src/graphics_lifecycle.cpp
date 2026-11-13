#include "neon/render_device.h"
#include <glad/gl.h>
#include <algorithm>

namespace neon::render {
std::vector<NeonDevice *> &NeonDevice::devices() {
    static std::vector<NeonDevice *> list;
    return list;
}
bool NeonDevice::checkpointAllGraphics() {
    bool ok = true;
    for (auto *d : devices())
        if (d->programs_[0].id)
            ok = d->checkpointGraphics() && ok;
    return ok;
}
void NeonDevice::discardGraphicsCheckpoints() {
    for (auto *d : devices()) {
        for (auto &p : d->targetPixels_)
            std::vector<uint8_t>().swap(p);
        for (auto &p : d->literalPixels_)
            std::vector<uint8_t>().swap(p);
        d->uniformSaved_.fill(false);
    }
}
void NeonDevice::abandonAllGraphics() {
    for (auto *d : devices())
        d->abandonGraphics();
}
bool NeonDevice::restoreAllGraphics() {
    bool ok = true;
    for (auto *d : devices())
        if (d->restoring_)
            ok = d->restoreGraphics() && ok;
    return ok;
}

bool NeonDevice::checkpointGraphics() {
    uniformSaved_.fill(false);
    for (int p = 0; p < kNumPrograms; ++p)
        for (int u = 0; u < kNumUniforms; ++u)
            if (programs_[p].id && programs_[p].loc[u] >= 0) {
                const int slot = p * kNumUniforms + u;
                glGetUniformfv(programs_[p].id, programs_[p].loc[u], uniformValues_[slot].data());
                uniformSaved_[slot] = true;
            }
    GLint read = 0, drawFramebuffer = 0, pack = 0, pbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (unit0Texture_) {
        GLint active = 0, bound = 0, s = 0, t = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glBindTexture(GL_TEXTURE_2D, unit0Texture_);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &s);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &t);
        restoreWrapS_ = unsigned(s);
        restoreWrapT_ = unsigned(t);
        glBindTexture(GL_TEXTURE_2D, unsigned(bound));
        glActiveTexture(unsigned(active));
    }
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    bool ok = true;
    const auto capture = [&](unsigned texture, int w, int h, std::vector<uint8_t> &pixels) {
        pixels.clear();
        if (!texture || w <= 0 || h <= 0)
            return;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ok = false;
            return;
        }
        pixels.resize(size_t(w) * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    };
    for (int i = 0; i < kMaxRenderTargets; ++i)
        capture(rts_[i].tex, rts_[i].pixelW, rts_[i].pixelH, targetPixels_[i]);
    for (int i = 0; i < kMaxLiterals; ++i)
        capture(literals_[i].name, literals_[i].width, literals_[i].height, literalPixels_[i]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, unsigned(read));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, unsigned(drawFramebuffer));
    glDeleteFramebuffers(1, &fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, pack);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, unsigned(pbo));
    return ok;
}
void NeonDevice::abandonGraphics() {
    if (restoring_)
        return;
    restoring_ = programs_[0].id != 0;
    restoreUnit0_ = -1;
    for (int i = 0; i < kMaxLiterals; ++i)
        if (literals_[i].name && literals_[i].name == unit0Texture_)
            restoreUnit0_ = i;
    for (int i = 0; i < kMaxRenderTargets; ++i)
        if (rts_[i].tex && rts_[i].tex == unit0Texture_)
            restoreUnit0_ = kMaxLiterals + i;
    for (auto &rt : rts_)
        rt.fbo = rt.tex = 0;
    for (int i = 0; i < kMaxVertexBuffers; ++i) {
        restoreBuffers_[i] = vbs_[i].vbo != 0;
        vbs_[i].vbo = 0;
    }
    for (auto &literal : literals_)
        literal.name = 0;
    for (auto &program : programs_)
        program.id = 0;
    std::fill(std::begin(vertexShaders_), std::end(vertexShaders_), 0);
    std::fill(std::begin(pixelShaders_), std::end(pixelShaders_), 0);
    unit0Texture_ = 0;
    bbFbo_ = 0;
}
bool NeonDevice::restoreGraphics() {
    InitAllShaders();
    for (auto &p : programs_) {
        GLint linked = 0;
        glGetProgramiv(p.id, GL_LINK_STATUS, &linked);
        if (!linked)
            return false;
    }
    // Disabled constants retain their last GPU value, not the shader default.
    for (int p = 0; p < kNumPrograms; ++p) {
        glUseProgram(programs_[p].id);
        for (int u = 0; u < kNumUniforms; ++u)
            if (uniformSaved_[p * kNumUniforms + u] && programs_[p].loc[u] >= 0) {
                const float *value = uniformValues_[p * kNumUniforms + u].data();
                const int location = programs_[p].loc[u];
                if (u == U_WorldViewProj)
                    glUniformMatrix4fv(location, 1, GL_FALSE, value);
                else if (u >= U_Tex0 && u <= U_Tex5)
                    glUniform1i(location, int(value[0]));
                else
                    glUniform4fv(location, 1, value);
            }
    }
    glActiveTexture(GL_TEXTURE0);
    const auto files = literalFiles_;
    InitLiteralTextures(int(files.size()), files);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < kMaxLiterals; ++i)
        if (literals_[i].name && !literalPixels_[i].empty()) {
            glBindTexture(GL_TEXTURE_2D, literals_[i].name);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, literals_[i].width, literals_[i].height,
                            GL_RGBA, GL_UNSIGNED_BYTE, literalPixels_[i].data());
        }
    for (int i = 0; i < kMaxVertexBuffers; ++i)
        if (restoreBuffers_[i]) {
            auto &vb = vbs_[i];
            glGenBuffers(1, &vb.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vb.vbo);
            glBufferData(GL_ARRAY_BUFFER, vb.bytes, vb.contents.data(), GL_DYNAMIC_DRAW);
        }
    for (int i = 0; i < kMaxRenderTargets; ++i) {
        auto &rt = rts_[i];
        if (rt.width <= 0 || rt.height <= 0)
            continue;
        if (!createFBO(rt, rt.width, rt.height))
            return false;
        if (targetPixels_[i].size() == size_t(rt.pixelW) * rt.pixelH * 4) {
            glBindTexture(GL_TEXTURE_2D, rt.tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rt.pixelW, rt.pixelH, GL_RGBA, GL_UNSIGNED_BYTE,
                            targetPixels_[i].data());
        } else {
            // Lost history without a checkpoint starts transparent, never with
            // undefined driver contents. Normal target allocation is unchanged.
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    if (restoreUnit0_ >= kMaxLiterals)
        unit0Texture_ = rts_[restoreUnit0_ - kMaxLiterals].tex;
    else if (restoreUnit0_ >= 0)
        unit0Texture_ = literals_[restoreUnit0_].name;
    // These indices also carry logical state: invalid shader selections retain
    // the last valid program, and GetCurrentRtSize observes the selected target.
    // BeginFrame rebinds them; only hardware binding/uniform caches are invalid.
    if (unit0Texture_) {
        glBindTexture(GL_TEXTURE_2D, unit0Texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(restoreWrapS_));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(restoreWrapT_));
    }
    lastVB_ = -1;
    lastKind_ = -100;
    zbufCache_ = 0xffffffff;
    for (int i = 0; i < 3; ++i) {
        boundRT_[i] = boundLiteral_[i] = -100;
        appliedWrapS_[i] = appliedWrapT_[i] = 0;
    }
    frameBegun_ = false;
    restoring_ = false;
    return true;
}
} // namespace neon::render
