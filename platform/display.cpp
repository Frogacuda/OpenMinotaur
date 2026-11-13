#include "display.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace mino {

// ----------------------------------------------------------- shaders ------
// Written once in GLSL ES 1.00 style and passed through adaptShader(), the
// same path the original engine shaders take.

static const char *kPresentVS = R"(
attribute vec2 aPos;
attribute vec2 aUV;
uniform float uClockwise;
varying vec2 vUV;
varying vec2 vScreenUV;
void main() {
    vScreenUV=aUV;
    vUV = uClockwise > 0.5 ? vec2(1.0-aUV.y,aUV.x):uClockwise<-.5 ? vec2(aUV.y,1.0-aUV.x):aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// Sharp bilinear: emulates an integer nearest-neighbour prescale followed by
// a bilinear downscale to the output size, in one pass. Texels stay crisp and
// only the seams between them are blended, so non-integer scales stay even.
#ifdef MINO_NO_PRESENTATION_FILTERS
static const char *kPresentFS = R"(
precision highp float;
uniform sampler2D uTex;
uniform vec2 uTexSize;
uniform vec2 uOutSize;
uniform float uSharp;
varying vec2 vUV;
void main() {
    vec2 uv=vUV;
    if(uSharp>0.5) {
        vec2 texel=uv*uTexSize,scale=max(floor(uOutSize/uTexSize),vec2(1.0));
        vec2 base=floor(texel),fraction=texel-base-0.5,region=0.5-0.5/scale;
        vec2 f=(fraction-clamp(fraction,-region,region))*scale+0.5;
        uv=(base+f)/uTexSize;
    }
    gl_FragColor=vec4(texture2D(uTex,uv).rgb,1.0);
}
)";
#else
static const char *kPresentFS = R"(
precision highp float;
uniform sampler2D uTex;
uniform vec2 uTexSize;
uniform vec2 uOutSize;
uniform float uSharp;
uniform float uMode;
uniform bool uScanlines;
uniform float uPortraitScreen;
uniform vec2 uRasterSize;
uniform float uPass;
uniform float uTime;
varying vec2 vUV;
varying vec2 vScreenUV;
// Integral of a quarter-row gap: averaging its coverage avoids moire when
// native rows fall between output pixels, and gives a steady mean below 1x.
float rowGaps(float row) {
    return floor(row)*.25+max(fract(row)-.75,0.0);
}
void main() {
    vec2 uv = vUV;
    if(uPass>0.5){
        vec2 p=vUV;
        if(uPass<1.5){
            p=.5+(p-.5)*.7+vec2(sin(p.y*4.0+uTime*.2),cos(p.x*3.0-uTime*.17))*.12;
            vec3 c=vec3(0.0);for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)c+=texture2D(uTex,clamp(p+vec2(float(x),float(y))*.045,vec2(.02),vec2(.98)),4.5).rgb;
            c=c/25.0;float light=.65+.3*sin((p.x+p.y)*5.0);gl_FragColor=vec4(c*light,1.0);return;
        }
        if(uPass>2.5){
            float imageAspect=uTexSize.x/uTexSize.y,screenAspect=uOutSize.x/uOutSize.y;
            vec2 scale=vec2(min(1.0,screenAspect/imageAspect),min(1.0,imageAspect/screenAspect));p=.5+(p-.5)*scale;
            gl_FragColor=vec4(texture2D(uTex,vec2(p.x,1.0-p.y)).rgb,1.0);return;
        }
        vec2 tile=fract(p*vec2(3.0,2.0));vec3 art=texture2D(uTex,vec2(tile.x,1.0-tile.y)).rgb;
        float line=pow(max(0.0,sin(p.x*80.0+p.y*25.0)),18.0);
        gl_FragColor=vec4(art*.32+vec3(.025,.035,.07)+line*vec3(.04,.09,.13),1.0);return;
    }
    if (uSharp > 0.5) {
        vec2 texel = uv * uTexSize;
        vec2 scale = uMode>1.5 && uMode<2.5 ? vec2(3.0):max(floor(uOutSize / uTexSize), vec2(1.0));
        vec2 base = floor(texel);
        vec2 frac = texel - base - 0.5;
        vec2 region = 0.5 - 0.5 / scale;
        vec2 f = (frac - clamp(frac, -region, region)) * scale + 0.5;
        uv = (base + f) / uTexSize;
    }
    vec3 colour=texture2D(uTex,uv).rgb;
    if(uScanlines){
        // One gap per native raster row, in the same oriented tube axes used
        // by the CRT pipelines. This pass is exclusive to Scanlines mode.
        float row=(uPortraitScreen>.5 ? vScreenUV.x:vScreenUV.y)*uRasterSize.x;
        float footprint=uRasterSize.x/uRasterSize.y;
        float gap=footprint>=1.0 ? .25:(rowGaps(row+.5*footprint)-rowGaps(row-.5*footprint))/footprint;
        colour*=1.0-.55*gap;
    }
    gl_FragColor=vec4(colour,1.0);
}
)";

#endif
std::string Display::adaptShader(const std::string &src, int stage) const {
    if (es_) {
        // OpenGL ES 3 accepts GLSL ES 1.00 as-is; the original shaders need no change.
        return src.find("precision") == std::string::npos && stage == 1
                   ? "precision mediump float;\n" + src
                   : src;
    }
    // Desktop GL 3.3 core: rewrite the ES 1.00 dialect.
    std::string out;
    out.reserve(src.size() + 256);
    size_t pos = 0;
    while (pos < src.size()) {
        size_t eol = src.find('\n', pos);
        if (eol == std::string::npos)
            eol = src.size();
        std::string line = src.substr(pos, eol - pos);
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 9, "precision") != 0)
            out += line + '\n';
        pos = eol + 1;
    }
    auto replaceAll = [](std::string &s, const std::string &a, const std::string &b) {
        for (size_t i = s.find(a); i != std::string::npos; i = s.find(a, i + b.size()))
            s.replace(i, a.size(), b);
    };
    replaceAll(out, "gl_FragColor", "mino_FragColor");
    std::string head = "#version 330 core\n#define lowp\n#define mediump\n#define highp\n";
    if (stage == 0) {
        head += "#define attribute in\n#define varying out\n";
    } else {
        head += "#define varying in\n#define texture2D texture\nout vec4 mino_FragColor;\n";
    }
    return head + out;
}

static unsigned compileStage(GLenum type, const std::string &src) {
    GLuint s = glCreateShader(type);
    const char *p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        SDL_Log("shader compile failed: %s\n%s", log, src.c_str());
    }
    return s;
}

unsigned Display::compile(const char *vs, const char *fs) {
    GLuint p = glCreateProgram();
    GLuint v = compileStage(GL_VERTEX_SHADER, adaptShader(vs, 0));
    GLuint f = compileStage(GL_FRAGMENT_SHADER, adaptShader(fs, 1));
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aUV");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        SDL_Log("present program link failed: %s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// -------------------------------------------------------------- setup -----

bool Display::init(bool glES) {
    es_ = glES;
    // Direct CRT output can replace the RGB8 intermediate only when the window
    // has the same quantization and no automatic sRGB encoding. Other formats
    // retain the intermediate and ordinary presentation copy.
    int previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const unsigned attachment = es_ ? GL_BACK : GL_BACK_LEFT;
    int red = 0, green = 0, blue = 0, encoding = 0, type = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &green);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blue);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &type);
    directCRT_ = red == 8 && green == 8 && blue == 8 && encoding == GL_LINEAR &&
                 type == GL_UNSIGNED_NORMALIZED;
    glBindFramebuffer(GL_FRAMEBUFFER, unsigned(previous));
    surface_.glES = glES;
    presentProg_ = compile(kPresentVS, kPresentFS);
    uTex_ = glGetUniformLocation(presentProg_, "uTex");
    uTexSize_ = glGetUniformLocation(presentProg_, "uTexSize");
    uOutSize_ = glGetUniformLocation(presentProg_, "uOutSize");
    uSharp_ = glGetUniformLocation(presentProg_, "uSharp");
    uClockwise_ = glGetUniformLocation(presentProg_, "uClockwise");

    // Core profile requires a bound VAO; one shared VAO is enough because the
    // engine re-specifies its attribute pointers every draw, as on ES 2.
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Full-screen triangle strip: pos.xy, uv.xy (uv.y = 0 at bottom = FBO row 0).
    const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return presentProg_ != 0;
}

void Display::shutdown() {
#ifndef MINO_NO_PRESENTATION_FILTERS
    royale_.shutdown();
#endif
    destroySurface();
    if (bezelTex_)
        glDeleteTextures(1, &bezelTex_);
    bezelTex_ = 0;
    bezelKey_.clear();
    if (presentProg_)
        glDeleteProgram(presentProg_);
    if (quadVbo_)
        glDeleteBuffers(1, &quadVbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    presentProg_ = quadVbo_ = vao_ = 0;
}
void Display::abandonGraphics() {
    // Names from a lost context must never be deleted in its replacement.
    surface_.framebuffer = 0;
    surface_.width = surface_.height = 0;
    colorTex_ = depthRb_ = presentProg_ = quadVbo_ = vao_ = bezelTex_ = 0;
    bezelKey_.clear();
#ifndef MINO_NO_PRESENTATION_FILTERS
    royale_.abandonGraphics();
#endif
}
bool Display::graphicsValid() const {
    return presentProg_ && glIsProgram(presentProg_) && surface_.framebuffer &&
           glIsFramebuffer(surface_.framebuffer);
}

void Display::createSurface(int w, int h) {
    destroySurface();
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

    glGenRenderbuffers(1, &depthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthRb_);

    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE)
        SDL_Log("surface FBO incomplete: 0x%x", st);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    surface_.width = w;
    surface_.height = h;
    surface_.framebuffer = fbo;
}

void Display::destroySurface() {
    if (surface_.framebuffer) {
        GLuint fbo = surface_.framebuffer;
        glDeleteFramebuffers(1, &fbo);
    }
    if (colorTex_)
        glDeleteTextures(1, &colorTex_);
    if (depthRb_)
        glDeleteRenderbuffers(1, &depthRb_);
    surface_.framebuffer = 0;
    colorTex_ = depthRb_ = 0;
    surface_.width = surface_.height = 0;
}

// ------------------------------------------------------------- layout -----

bool Display::configure(int windowW, int windowH, NativeFormat native, const DisplaySettings &s) {
    winW_ = std::max(windowW, 1);
    winH_ = std::max(windowH, 1);
    filter_ = s.filter;
    backdrop_ = s.backdropColour;
    backdropMode_ = s.backdrop;
    screenMode_ = s.screenMode;
    backdropTurns_ = winW_ >= winH_ ? (s.orientation == Orientation::Portrait        ? 1
                                       : s.orientation == Orientation::PortraitRight ? -1
                                                                                     : 0)
                                    : 0;
    if (screenMode_ != ScreenMode::HiRes)
        filter_ = screenMode_ == ScreenMode::Sharp ? Filter::SharpBilinear : Filter::Bilinear;

    const auto previous = layout_;
    layout_ = displayLayout(winW_, winH_, native, s);
    view_ = layout_.view;
    surface_.displayAspect =
        (layout_.turns & 1) ? float(view_.h) / view_.w : float(view_.w) / view_.h;
    const int sw = layout_.width, sh = layout_.height;
    error_.clear();
#ifndef MINO_NO_PRESENTATION_FILTERS
    tube_ = tubeLayout(view_.w, view_.h, layout_.turns, sw, sh);
    if (isCRT(screenMode_)) {
        const auto profile = screenMode_ == ScreenMode::CRT2 ? RoyalePipeline::Profile::Consumer
                                                             : RoyalePipeline::Profile::Royale;
        if (!royale_.configure(es_, tube_, profile)) {
            error_ = royale_.error();
            SDL_Log("CRT filter unavailable: %s", error_.c_str());
            screenMode_ = ScreenMode::Sharp;
            filter_ = Filter::SharpBilinear;
        }
    } else
        royale_.releaseTargets();
#endif

    if (sw != surface_.width || sh != surface_.height) {
        createSurface(std::max(sw, 2), std::max(sh, 2));
        return true;
    }
    // Touches and pad axes must be cancelled/rebased even when only the
    // presentation changed and the native render target can be reused.
    return previous.turns != layout_.turns || previous.view.x != view_.x ||
           previous.view.y != view_.y || previous.view.w != view_.w || previous.view.h != view_.h;
}

void Display::beginFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, surface_.framebuffer);
    glViewport(0, 0, surface_.width, surface_.height);
    glBindVertexArray(vao_);
}

void Display::present(unsigned framebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindVertexArray(vao_);
    // Core/startup draws use colour and extra texture attributes. Presentation
    // uses only position/UV; don't retain enabled arrays from a released VBO.
    for (unsigned attribute = 2; attribute < 5; ++attribute)
        glDisableVertexAttribArray(attribute);
    glViewport(0, 0, winW_, winH_);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(float((backdrop_ >> 16) & 0xff) / 255.0f, float((backdrop_ >> 8) & 0xff) / 255.0f,
                 float(backdrop_ & 0xff) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

#ifndef MINO_NO_PRESENTATION_FILTERS
    if (backdropMode_ == Backdrop::Neon || (backdropMode_ == Backdrop::Image && bezelTex_)) {
        glBindVertexArray(vao_);
        glUseProgram(presentProg_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backdropMode_ == Backdrop::Neon ? colorTex_ : bezelTex_);
        if (backdropMode_ == Backdrop::Neon)
            glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        backdropMode_ == Backdrop::Neon ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(uTex_, 0);
        glUniform1f(uClockwise_,
                    float(backdropMode_ == Backdrop::Neon ? layout_.turns : backdropTurns_));
        glUniform2f(uTexSize_, float(bezelW_), float(bezelH_));
        glUniform2f(uOutSize_, float(backdropTurns_ ? winH_ : winW_),
                    float(backdropTurns_ ? winW_ : winH_));
        glUniform1f(glGetUniformLocation(presentProg_, "uTime"), float(SDL_GetTicks() / 1000.0));
        glUniform1f(glGetUniformLocation(presentProg_, "uPass"), backdropMode_ == Backdrop::Neon
                                                                     ? 1.f
                                                                 : bezelTiled_ ? 2.f
                                                                               : 3.f);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(0));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(8));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
#endif
    unsigned output = colorTex_;
    int rotation = layout_.turns;
    int textureW = surface_.width, textureH = surface_.height;
#ifndef MINO_NO_PRESENTATION_FILTERS
    if (isCRT(screenMode_)) {
        if (directCRT_ && framebuffer == 0 && screenMode_ == ScreenMode::CRT) {
            const RoyalePipeline::Output destination{
                framebuffer, view_.x, winH_ - (view_.y + view_.h),
                view_.w,     view_.h, tube_.clockwise};
            royale_.run(colorTex_, quadVbo_, &destination);
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            return;
        }
        output = royale_.run(colorTex_, quadVbo_, nullptr,
                             surface_.width == tube_.sourceWidth &&
                                 surface_.height == tube_.sourceHeight);
        rotation = tube_.clockwise;
        textureW = tube_.width;
        textureH = tube_.height;
    }
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(view_.x, winH_ - (view_.y + view_.h), view_.w, view_.h);
    glBindVertexArray(vao_);
    glUseProgram(presentProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, output);
    GLint f = filter_ == Filter::Nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glUniform1i(uTex_, 0);
    glUniform1f(glGetUniformLocation(presentProg_, "uPass"), 0.f);
    glUniform1f(glGetUniformLocation(presentProg_, "uMode"),
                screenMode_ == ScreenMode::Sharp ? 2.f : 0.f);
    glUniform1i(glGetUniformLocation(presentProg_, "uScanlines"),
                screenMode_ == ScreenMode::Scanlines);
    glUniform1f(glGetUniformLocation(presentProg_, "uPortraitScreen"),
                view_.h > view_.w ? 1.f : 0.f);
#ifndef MINO_NO_PRESENTATION_FILTERS
    glUniform2f(glGetUniformLocation(presentProg_, "uRasterSize"), float(tube_.sourceHeight),
                float(tube_.height));
#endif
    glUniform2f(uTexSize_, float(textureW), float(textureH));
    glUniform2f(uOutSize_, float(layout_.turns ? view_.h : view_.w),
                float(layout_.turns ? view_.w : view_.h));
    glUniform1f(uSharp_, !isCRT(screenMode_) && filter_ == Filter::SharpBilinear ? 1.0f : 0.0f);
    glUniform1f(uClockwise_, float(rotation));

    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void *>(8));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Display::setBezel(int width, int height, const unsigned char *pixels, bool tiled) {
    bezelW_ = width;
    bezelH_ = height;
    bezelTiled_ = tiled;
    if (bezelTex_)
        glDeleteTextures(1, &bezelTex_);
    bezelTex_ = 0;
    if (pixels) {
        glGenTextures(1, &bezelTex_);
        glBindTexture(GL_TEXTURE_2D, bezelTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void Display::windowToSurface(float wx, float wy, float *sx, float *sy) const {
    // Deliberately unclamped: a finger in the letterbox bars reports a position
    // outside the surface (negative or beyond width/height). The games' touch
    // controls are relative/joystick-like, so the bars simply extend the touch
    // area; menu buttons are unaffected because such touches miss them.
    layout_.windowToSurface(wx, wy, sx, sy);
}

} // namespace mino
