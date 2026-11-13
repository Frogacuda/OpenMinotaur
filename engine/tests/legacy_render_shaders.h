// Saved reference renderer; namespace/host service names adapted for comparison.
// neon_shaders.h - the engine's GLSL ES 1.00 shaders, as embedded in the
// original binary (identical in Minotron and Minotaur Rescue).
//
// Programs (NeonDevice::InitAllShaders):
//   0..5 : vertex shader 0 + pixel shader 0..5
//   6    : vertex shader 1 + pixel shader 0
// Pixel shaders 0 and 1 are the same source in the original.
#pragma once

namespace frozen {
namespace render {

// Full vertex shader: three texture-coordinate sets, each transformed by two
// rows of a texture matrix, with t flipped (textures are stored bottom-up).
static const char *const kNeonVertexShader0 = R"(
attribute vec4 pos0;
attribute vec2 tex0;
attribute vec2 tex1;
attribute vec2 tex2;
attribute vec4 colour;

varying lowp vec4 colorVarying;
varying lowp vec2 TexCoord[3];

uniform mat4 WorldViewProj;
uniform vec4 matTex0a;
uniform vec4 matTex0b;
uniform vec4 matTex1a;
uniform vec4 matTex1b;
uniform vec4 matTex2a;
uniform vec4 matTex2b;

void main()
{
    vec4 tmp;
    colorVarying = colour;

    tmp.xy = tex0.st;
    tmp.z  = 0.0;
    tmp.w  = 1.0;
    TexCoord[0].s = dot(tmp, matTex0a);
    TexCoord[0].t = 1.0 - dot(tmp, matTex0b);

    tmp.xy = tex1.st;
    TexCoord[1].s = dot(tmp, matTex1a);
    TexCoord[1].t = 1.0 - dot(tmp, matTex1b);

    tmp.xy = tex2.st;
    TexCoord[2].s = dot(tmp, matTex2a);
    TexCoord[2].t = 1.0 - dot(tmp, matTex2b);

    gl_Position = pos0 * WorldViewProj;
}
)";

// Simple vertex shader: texture coordinate 0 passed through untransformed.
static const char *const kNeonVertexShader1 = R"(
attribute vec4 pos0;
attribute vec2 tex0;
attribute vec2 tex1;
attribute vec2 tex2;
attribute vec4 colour;

varying lowp vec4 colorVarying;
varying vec2 TexCoord[3];

uniform mat4 WorldViewProj;

void main()
{
    colorVarying = colour;
    TexCoord[0] = tex0;
    gl_Position = pos0 * WorldViewProj;
}
)";

// 0/1: textured, vertex-coloured.
static const char *const kNeonPixelShader0 = R"(
uniform lowp sampler2D s_tex0;
varying lowp vec4 colorVarying;
varying mediump vec2 TexCoord[3];

void main()
{
    gl_FragColor = colorVarying * texture2D(s_tex0, TexCoord[0]);
}
)";

// 2: two cross-fades (c1) blended by c5.
static const char *const kNeonPixelShader2 = R"(
uniform lowp sampler2D s_tex0;
uniform lowp sampler2D s_tex1;
uniform lowp sampler2D s_tex3;
uniform lowp sampler2D s_tex4;
uniform mediump vec4 c1;
uniform mediump vec4 c5;
varying lowp vec4 colorVarying;
varying mediump vec2 TexCoord[3];

void main()
{
    lowp vec4 col, col2;
    col  = mix(texture2D(s_tex0, TexCoord[0]), texture2D(s_tex3, TexCoord[0]), c1);
    col2 = mix(texture2D(s_tex1, TexCoord[0]), texture2D(s_tex4, TexCoord[0]), c1);
    gl_FragColor = colorVarying * mix(col, col2, c5);
}
)";

// 3: max of two cross-fades (second uses texture coordinate 1).
static const char *const kNeonPixelShader3 = R"(
uniform lowp sampler2D s_tex0;
uniform lowp sampler2D s_tex1;
uniform lowp sampler2D s_tex3;
uniform lowp sampler2D s_tex4;
uniform mediump vec4 c1;
uniform mediump vec4 c5;
varying lowp vec4 colorVarying;
varying mediump vec2 TexCoord[3];

void main()
{
    lowp vec4 col, col2;
    col  = mix(texture2D(s_tex0, TexCoord[0]), texture2D(s_tex3, TexCoord[0]), c1);
    col2 = mix(texture2D(s_tex1, TexCoord[1]), texture2D(s_tex4, TexCoord[1]), c1);
    gl_FragColor = colorVarying * max(col, col2);
}
)";

// 4: first cross-fade modulated by the brightness of the second.
static const char *const kNeonPixelShader4 = R"(
uniform sampler2D s_tex0;
uniform sampler2D s_tex1;
uniform sampler2D s_tex3;
uniform sampler2D s_tex4;
uniform lowp vec4 c1;
uniform lowp vec4 c5;
varying lowp vec4 colorVarying;
varying mediump vec2 TexCoord[3];

void main()
{
    lowp vec4 col, col2;
    lowp float sv;
    col  = mix(texture2D(s_tex0, TexCoord[0]), texture2D(s_tex3, TexCoord[0]), c1);
    col2 = c5 * mix(texture2D(s_tex1, TexCoord[1]), texture2D(s_tex4, TexCoord[1]), c1);
    sv = (col2.r + col2.g + col2.b) * c5.w;
    gl_FragColor = colorVarying * sv * col;
}
)";

// 5: alpha keyed from the red channel.
static const char *const kNeonPixelShader5 = R"(
uniform sampler2D s_tex0;
varying lowp vec4 colorVarying;
varying mediump vec2 TexCoord[3];

void main()
{
    gl_FragColor = texture2D(s_tex0, TexCoord[0]);
    gl_FragColor.a = (gl_FragColor.r > 0.0) ? 1.0 : 0.0;
    gl_FragColor = colorVarying * gl_FragColor;
}
)";

static const char *const kNeonVertexShaders[2] = {kNeonVertexShader0, kNeonVertexShader1};
static const char *const kNeonPixelShaders[6] = {kNeonPixelShader0, kNeonPixelShader0,
                                                 kNeonPixelShader2, kNeonPixelShader3,
                                                 kNeonPixelShader4, kNeonPixelShader5};

// Uniform names looked up per program, in the original's table order.
static const char *const kNeonUniformNames[22] = {
    "WorldViewProj",
    "matTex0a",
    "matTex0b",
    "matTex1a",
    "matTex1b",
    "matTex2a",
    "matTex2b",
    "s_tex0",
    "s_tex1",
    "s_tex2",
    "s_tex3",
    "s_tex4",
    "s_tex5",
    "c0",
    "c1",
    "c2",
    "c5",
    "c7",
    "c8",
    "c9",
    "c10",
    "c25",
};

} // namespace render
} // namespace frozen
